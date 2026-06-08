#include "generate_qnn_utils.h"
#include "android_memory_allocator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <model.h>
#include <tokenizers_cpp.h>

std::mt19937 rng;

namespace {

constexpr int kQnnKvNumColumns = 128;

constexpr float kRopeQuantScale = 3.051804378628731e-05f;
constexpr int kRopeQuantOffset = -32768;

uint16_t quantize_rope_value(double value, double attention_factor) {
  const double q =
    (value * attention_factor) / kRopeQuantScale - kRopeQuantOffset;
  if (q <= 0.0)
    return 0;
  if (q >= 65535.0)
    return 65535;
  return static_cast<uint16_t>(std::lrint(q));
}

} // namespace

std::tuple<uint16_t *, uint16_t *>
get_cos_sin(int context_size, int pos_dim, const double theta,
            const std::string &rope_type, double partial_rotary_factor,
            double rope_scaling_factor, int rope_head_dim) {
  double attention_factor = 1.0;
  const double scaling_factor =
    rope_scaling_factor > 0.0 ? rope_scaling_factor : 1.0;
  const int frequency_dim = rope_head_dim > 0 ? rope_head_dim : pos_dim * 2;
  // inv_freq indexes angle pairs, so the exponent advances by 2/head_dim.
  const double exponent = 2.0 / static_cast<double>(frequency_dim);
  std::vector<double> inv_freq(pos_dim, 0.0);

  if (rope_type == "default") {
    for (int j = 0; j < pos_dim; j++)
      inv_freq[j] = 1.0 / std::pow(theta, j * exponent);
  } else if (rope_type == "linear" || rope_type == "proportional") {
    int rotary_freq_count = pos_dim;

    if (rope_type == "proportional") {
      double proportion = partial_rotary_factor;
      if (proportion < 0.0)
        proportion = 0.0;
      if (proportion > 1.0)
        proportion = 1.0;
      rotary_freq_count = static_cast<int>(
        std::floor(proportion * static_cast<double>(frequency_dim) / 2.0));
      rotary_freq_count = std::max(0, std::min(pos_dim, rotary_freq_count));
    }

    for (int j = 0; j < rotary_freq_count; j++)
      inv_freq[j] = (1.0 / std::pow(theta, j * exponent)) / scaling_factor;
  } else {
    for (int j = 0; j < pos_dim; j++)
      inv_freq[j] = 1.0 / std::pow(theta, j * exponent);
  }

  // Partial RoPE (Gemma 4 full attention uses partial_rotary_factor=0.25):
  // only the first `effective_dim` lanes carry an actual rotation; the
  // remaining lanes are identity (cos=1, sin=0) so multiplication is
  // pass-through. Without this the non-rotary lanes get a small bogus
  // rotation that compounds across the 35 transformer layers and the
  // model collapses into repetition after a few dozen tokens.
  int effective_dim = pos_dim;
  if (partial_rotary_factor > 0.0 && partial_rotary_factor < 1.0) {
    effective_dim =
      static_cast<int>(std::floor(pos_dim * partial_rotary_factor));
    if (effective_dim < 0)
      effective_dim = 0;
    if (effective_dim > pos_dim)
      effective_dim = pos_dim;
  }

  uint16_t *cos_val =
    (uint16_t *)allocate(sizeof(uint16_t) * context_size * pos_dim);
  uint16_t *sin_val =
    (uint16_t *)allocate(sizeof(uint16_t) * context_size * pos_dim);

  // Quantized identity values for non-rotary lanes.
  const uint16_t cos_one = quantize_rope_value(1.0, attention_factor);
  const uint16_t sin_zero = quantize_rope_value(0.0, attention_factor);

  for (int i = 0; i < context_size; i++) {
    for (int j = 0; j < effective_dim; j++) {
      const double freq = i * inv_freq[j];
      cos_val[i * pos_dim + j] =
        quantize_rope_value(std::cos(freq), attention_factor);
      sin_val[i * pos_dim + j] =
        quantize_rope_value(std::sin(freq), attention_factor);
    }
    for (int j = effective_dim; j < pos_dim; j++) {
      cos_val[i * pos_dim + j] = cos_one;
      sin_val[i * pos_dim + j] = sin_zero;
    }
  }
  return std::make_tuple(cos_val, sin_val);
}

bool qnn_starts_with(const std::string &value, const std::string &prefix) {
  return value.compare(0, prefix.size(), prefix) == 0;
}

int find_tensor_index_or_minus_one(const TensorInfoList &tensor_infos,
                                   const std::string &tensor_name) {
  for (size_t idx = 0; idx < tensor_infos.size(); idx++) {
    if (tensor_infos[idx].name == tensor_name) {
      return static_cast<int>(idx);
    }
  }
  return -1;
}

std::string kv_output_to_input_name(const std::string &output_name) {
  if (output_name.size() >= 4 &&
      output_name.compare(output_name.size() - 4, 4, "_out") == 0) {
    return output_name.substr(0, output_name.size() - 4) + "_in";
  }
  return output_name;
}

int get_kv_row_length(const TensorInfo &tensor_info, bool is_key,
                      const std::string &tensor_name) {
  if (tensor_info.dimensions.size() < 2) {
    throw std::runtime_error("Unexpected KV dims for " + tensor_name);
  }

  if (is_key) {
    return tensor_info.dimensions.back();
  }

  return tensor_info.dimensions[tensor_info.dimensions.size() - 2];
}

void copy_kv_cache_window(uint8_t *dest, int dest_row_length,
                          const uint8_t *src, int src_row_length,
                          int history_length, bool is_key, int num_columns) {
  if (dest == nullptr || src == nullptr || history_length <= 0 ||
      dest_row_length <= 0 || src_row_length <= 0) {
    return;
  }

  const int available_history = std::min(history_length, src_row_length);
  const int copy_length = std::min(available_history, dest_row_length);
  const int src_start = available_history - copy_length;
  const bool align_to_tail =
    history_length >= src_row_length && dest_row_length > copy_length;
  const int dest_start = align_to_tail ? dest_row_length - copy_length : 0;

  if (is_key) {
    for (int col = 0; col < num_columns; ++col) {
      std::memcpy(dest + col * dest_row_length + dest_start,
                  src + col * src_row_length + src_start, copy_length);
    }
  } else {
    std::memcpy(dest + dest_start * num_columns, src + src_start * num_columns,
                copy_length * num_columns);
  }
}

std::vector<QnnKvOutputBinding> build_kv_output_bindings(
  const TensorInfoList &outputs,
  const std::unordered_map<std::string, int> &generation_kv_index_by_name,
  const std::string &graph_name, int kv_per_layer) {
  std::vector<QnnKvOutputBinding> bindings;
  for (size_t idx = 0; idx < outputs.size(); idx++) {
    const auto &name = outputs[idx].name;
    if (!qnn_starts_with(name, "past_")) {
      continue;
    }

    const auto input_name = kv_output_to_input_name(name);
    auto it = generation_kv_index_by_name.find(input_name);
    if (it == generation_kv_index_by_name.end()) {
      throw std::runtime_error(graph_name +
                               " KV output has no generation input: " + name);
    }

    const int kv_index = it->second;
    bindings.push_back({static_cast<int>(idx), kv_index,
                        kv_index / kv_per_layer,
                        qnn_starts_with(name, "past_key_")});
  }
  return bindings;
}

void append_outputs_to_kv_cache(const std::vector<IO_TensorType> &step_outputs,
                                const std::vector<QnnKvOutputBinding> &bindings,
                                const std::vector<uint8_t *> &kvs,
                                const std::vector<int> &kv_row_lengths,
                                int target_position, int rows,
                                int src_row_length,
                                const std::string &graph_name,
                                const std::vector<int> *kv_columns) {
  for (const auto &binding : bindings) {
    if (binding.output_index < 0 ||
        binding.output_index >= static_cast<int>(step_outputs.size()) ||
        binding.kv_index < 0 ||
        binding.kv_index >= static_cast<int>(kvs.size()) ||
        binding.layer_index < 0 ||
        binding.layer_index >= static_cast<int>(kv_row_lengths.size())) {
      throw std::runtime_error(graph_name +
                               " output KV binding is out of range");
    }
  }

#pragma omp parallel for
  for (int binding_idx = 0; binding_idx < static_cast<int>(bindings.size());
       binding_idx++) {
    const auto &binding = bindings[binding_idx];
    const int dest_row_length = kv_row_lengths[binding.layer_index];
    const int num_columns =
      kv_columns ? (*kv_columns)[binding.layer_index] : kQnnKvNumColumns;
    auto output = std::get<uint8_t *>(step_outputs[binding.output_index]);
    auto dest = kvs[binding.kv_index];

    int target_idx = target_position;
    const int valid_before = std::min(target_position, dest_row_length);
    const int shift = valid_before + rows - dest_row_length;
    if (shift > 0) {
      target_idx = valid_before - shift;
      if (binding.is_key) {
        for (int col = 0; col < num_columns; ++col) {
          uint8_t *col_base = dest + col * dest_row_length;
          std::memmove(col_base, col_base + shift, dest_row_length - shift);
        }
      } else {
        std::memmove(dest, dest + shift * num_columns,
                     (dest_row_length - shift) * num_columns);
      }
    }

    if (binding.is_key) {
      process_key(output, rows, num_columns, dest, target_idx, dest_row_length,
                  src_row_length);
    } else {
      process_value(output, rows, num_columns, dest, target_idx);
    }
  }
}

void process_key(uint8_t *pointer, int row, int column, uint8_t *dest, int idx,
                 int dest_row_length, int src_row_length) {
  // format: 1:1:col:row
  for (int i = 0; i < column; i++) {
    std::memcpy(dest + i * dest_row_length + idx, pointer + i * src_row_length,
                row);
  }
}

void process_value(uint8_t *pointer, int row, int column, uint8_t *dest,
                   int idx) {
  // format: 1:1:row:col
  for (int i = 0; i < row; i++) {
    std::memcpy(dest + (idx + i) * column, pointer + i * column, column);
  }
}

void fill_attention_mask_with_length(int rows, int columns, int length,
                                     uint16_t *attention_mask) {

  int index = 0;
  // TODO use std::fill_n?
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (i >= length) {
        attention_mask[index] = std::numeric_limits<uint16_t>::min();
      } else if (j >= columns - rows && j <= columns - rows + i) {
        attention_mask[index] = std::numeric_limits<uint16_t>::max();
      } else {
        attention_mask[index] = std::numeric_limits<uint16_t>::min();
      }
      index++;
    }
  }
}

void fill_attention_mask_with_prev_length(int rows, int columns, int length,
                                          uint16_t *attention_mask) {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < length; j++) {
      attention_mask[i * columns + j] = std::numeric_limits<uint16_t>::max();
    }
  }
}

uint16_t *get_zero_memory(int size, int zero_point) {
  uint16_t *memory = (uint16_t *)allocate(size * sizeof(uint16_t));
  for (int i = 0; i < size; i++)
    memory[i] = zero_point;
  return memory;
}

void fill_generation_inputs_common(
  uint16_t *generation_attention_mask, int generation_attention_mask_elements,
  uint16_t *generation_sliding_attention_mask,
  int generation_sliding_attention_mask_elements,
  int generation_full_kv_past_length, int generation_sliding_kv_past_length,
  uint16_t *generation_position_ids_cos, uint16_t *generation_position_ids_sin,
  const uint16_t *position_ids_cos, const uint16_t *position_ids_sin,
  int pos_dim, uint16_t *generation_swa_position_ids_cos,
  uint16_t *generation_swa_position_ids_sin,
  const uint16_t *swa_position_ids_cos, const uint16_t *swa_position_ids_sin,
  int swa_pos_dim, int position, int rope_cache_seq_len) {
  if (position < 0 || position >= rope_cache_seq_len) {
    throw std::runtime_error("Generation position is out of rope cache");
  }

  std::fill_n(generation_attention_mask, generation_attention_mask_elements, 0);
  std::fill_n(generation_sliding_attention_mask,
              generation_sliding_attention_mask_elements, 0);

  generation_attention_mask[generation_attention_mask_elements - 1] =
    std::numeric_limits<uint16_t>::max();
  generation_sliding_attention_mask[generation_sliding_attention_mask_elements -
                                    1] = std::numeric_limits<uint16_t>::max();

  for (int i = 0; i < position && i < generation_full_kv_past_length; i++) {
    generation_attention_mask[i] = std::numeric_limits<uint16_t>::max();
  }
  for (int i = 0; i < position && i < generation_sliding_kv_past_length; i++) {
    generation_sliding_attention_mask[i] = std::numeric_limits<uint16_t>::max();
  }

  std::memcpy(generation_position_ids_cos,
              position_ids_cos + position * pos_dim,
              pos_dim * sizeof(uint16_t));
  std::memcpy(generation_position_ids_sin,
              position_ids_sin + position * pos_dim,
              pos_dim * sizeof(uint16_t));
  std::memcpy(generation_swa_position_ids_cos,
              swa_position_ids_cos + position * swa_pos_dim,
              swa_pos_dim * sizeof(uint16_t));
  std::memcpy(generation_swa_position_ids_sin,
              swa_position_ids_sin + position * swa_pos_dim,
              swa_pos_dim * sizeof(uint16_t));
}

void fill_generation_inputs(
  float *generation_sample, int current_token,
  uint16_t *generation_attention_mask, int generation_attention_mask_elements,
  uint16_t *generation_sliding_attention_mask,
  int generation_sliding_attention_mask_elements,
  int generation_full_kv_past_length, int generation_sliding_kv_past_length,
  uint16_t *generation_position_ids_cos, uint16_t *generation_position_ids_sin,
  const uint16_t *position_ids_cos, const uint16_t *position_ids_sin,
  int pos_dim, uint16_t *generation_swa_position_ids_cos,
  uint16_t *generation_swa_position_ids_sin,
  const uint16_t *swa_position_ids_cos, const uint16_t *swa_position_ids_sin,
  int swa_pos_dim, int position, int rope_cache_seq_len) {
  generation_sample[0] = current_token;
  fill_generation_inputs_common(
    generation_attention_mask, generation_attention_mask_elements,
    generation_sliding_attention_mask,
    generation_sliding_attention_mask_elements, generation_full_kv_past_length,
    generation_sliding_kv_past_length, generation_position_ids_cos,
    generation_position_ids_sin, position_ids_cos, position_ids_sin, pos_dim,
    generation_swa_position_ids_cos, generation_swa_position_ids_sin,
    swa_position_ids_cos, swa_position_ids_sin, swa_pos_dim, position,
    rope_cache_seq_len);
}

void fill_generation_inputs_incremental(
  float *generation_sample, int current_token,
  uint16_t *generation_attention_mask, int generation_attention_mask_elements,
  uint16_t *generation_sliding_attention_mask,
  int generation_sliding_attention_mask_elements,
  int generation_full_kv_past_length, int generation_sliding_kv_past_length,
  uint16_t *generation_position_ids_cos, uint16_t *generation_position_ids_sin,
  const uint16_t *position_ids_cos, const uint16_t *position_ids_sin,
  int pos_dim, uint16_t *generation_swa_position_ids_cos,
  uint16_t *generation_swa_position_ids_sin,
  const uint16_t *swa_position_ids_cos, const uint16_t *swa_position_ids_sin,
  int swa_pos_dim, int position, int rope_cache_seq_len) {
  if (position < 0 || position >= rope_cache_seq_len) {
    throw std::runtime_error("Generation position is out of rope cache");
  }

  generation_sample[0] = current_token;

  const uint16_t visible = std::numeric_limits<uint16_t>::max();
  generation_attention_mask[generation_attention_mask_elements - 1] = visible;
  generation_sliding_attention_mask[generation_sliding_attention_mask_elements -
                                    1] = visible;

  const int newly_visible = position - 1;
  if (newly_visible >= 0 && newly_visible < generation_full_kv_past_length) {
    generation_attention_mask[newly_visible] = visible;
  }
  if (newly_visible >= 0 && newly_visible < generation_sliding_kv_past_length) {
    generation_sliding_attention_mask[newly_visible] = visible;
  }

  std::memcpy(generation_position_ids_cos,
              position_ids_cos + position * pos_dim,
              pos_dim * sizeof(uint16_t));
  std::memcpy(generation_position_ids_sin,
              position_ids_sin + position * pos_dim,
              pos_dim * sizeof(uint16_t));
  std::memcpy(generation_swa_position_ids_cos,
              swa_position_ids_cos + position * swa_pos_dim,
              swa_pos_dim * sizeof(uint16_t));
  std::memcpy(generation_swa_position_ids_sin,
              swa_position_ids_sin + position * swa_pos_dim,
              swa_pos_dim * sizeof(uint16_t));
}

void fill_generation_inputs_u16(
  uint16_t *generation_attention_mask, int generation_attention_mask_elements,
  uint16_t *generation_sliding_attention_mask,
  int generation_sliding_attention_mask_elements,
  int generation_full_kv_past_length, int generation_sliding_kv_past_length,
  uint16_t *generation_position_ids_cos, uint16_t *generation_position_ids_sin,
  const uint16_t *position_ids_cos, const uint16_t *position_ids_sin,
  int pos_dim, uint16_t *generation_swa_position_ids_cos,
  uint16_t *generation_swa_position_ids_sin,
  const uint16_t *swa_position_ids_cos, const uint16_t *swa_position_ids_sin,
  int swa_pos_dim, int position, int rope_cache_seq_len) {
  fill_generation_inputs_common(
    generation_attention_mask, generation_attention_mask_elements,
    generation_sliding_attention_mask,
    generation_sliding_attention_mask_elements, generation_full_kv_past_length,
    generation_sliding_kv_past_length, generation_position_ids_cos,
    generation_position_ids_sin, position_ids_cos, position_ids_sin, pos_dim,
    generation_swa_position_ids_cos, generation_swa_position_ids_sin,
    swa_position_ids_cos, swa_position_ids_sin, swa_pos_dim, position,
    rope_cache_seq_len);
}

int sample(uint16_t *pointer, int length, int *tokens, int number_of_tokens,
           float logit_scale, int logit_offset, float repetition_penalty,
           float temperature, float top_p, int top_k,
           float final_logit_softcapping) {
  if (pointer == nullptr || length <= 0)
    return 0;

  const int candidate_limit =
    (top_k > 0) ? std::min(top_k, length) : length;

  // Keep only the largest raw logits. Reuse thread-local buffers so the
  // per-token sampling path does not allocate on every decode step.
  using RawCandidate = std::pair<int, int>; // raw qlogit, token id
  thread_local std::vector<RawCandidate> heap;
  thread_local std::vector<std::pair<int, float>> candidates;
  thread_local std::vector<float> weights;
  heap.clear();
  candidates.clear();
  weights.clear();
  heap.reserve(candidate_limit);
  candidates.reserve(candidate_limit);
  weights.reserve(candidate_limit);

  const auto min_heap_cmp = [](const RawCandidate &a,
                               const RawCandidate &b) {
    return a.first > b.first;
  };

  for (int i = 0; i < candidate_limit; ++i)
    heap.emplace_back(pointer[i], i);
  std::make_heap(heap.begin(), heap.end(), min_heap_cmp);

  for (int i = candidate_limit; i < length; ++i) {
    if (heap.front().first < pointer[i]) {
      std::pop_heap(heap.begin(), heap.end(), min_heap_cmp);
      heap.back() = {pointer[i], i};
      std::push_heap(heap.begin(), heap.end(), min_heap_cmp);
    }
  }

  const bool use_softcap = final_logit_softcapping > 0.0f;
  const float inv_softcap = use_softcap ? 1.0f / final_logit_softcapping : 0.0f;
  const bool use_temperature = temperature > 1e-5f;
  const float inv_temperature = use_temperature ? 1.0f / temperature : 1.0f;

  for (const auto &element : heap) {
    float logit = (static_cast<float>(element.first) + logit_offset) * logit_scale;
    if (use_softcap)
      logit = final_logit_softcapping * std::tanh(logit * inv_softcap);
    if (use_temperature)
      logit *= inv_temperature;
    candidates.emplace_back(element.second, logit);
  }

  if (repetition_penalty != 1.0f) {
    for (int i = 0; i < number_of_tokens; ++i) {
      const int token_id = tokens[i];
      for (auto &candidate : candidates) {
        if (candidate.first != token_id)
          continue;
        if (candidate.second > 0.0f)
          candidate.second /= repetition_penalty;
        else
          candidate.second *= repetition_penalty;
        break;
      }
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  const float max_logit = candidates.front().second;
  float sum_exp = 0.0f;
  weights.resize(candidates.size());
  for (size_t i = 0; i < candidates.size(); ++i) {
    weights[i] = std::exp(candidates[i].second - max_logit);
    sum_exp += weights[i];
  }
  if (sum_exp <= 0.0f)
    sum_exp = 1.0f;

  float cumulative_prob = 0.0f;
  size_t nucleus_size = 0;
  const float nucleus_threshold = (top_p > 0.0f && top_p < 1.0f) ? top_p : 1.0f;
  while (nucleus_size < weights.size() && cumulative_prob < nucleus_threshold) {
    cumulative_prob += weights[nucleus_size] / sum_exp;
    ++nucleus_size;
  }
  if (nucleus_size == 0)
    nucleus_size = 1;

  weights.resize(nucleus_size);
  std::discrete_distribution<int> dist(weights.begin(), weights.end());
  return candidates[dist(rng)].first;
}
