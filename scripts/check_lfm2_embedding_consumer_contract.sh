#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

lfm2_h="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2_causallm.h"
lfm2_cpp="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2_causallm.cpp"
vl_cpp="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.cpp"

rg -q "size_t embeddingBytesPerToken\\(\\) const override;" "$lfm2_h" ||
  fail "Lfm2CausalLM.embeddingBytesPerToken() override is missing"

rg -q "const void \\*lookupEmbedding\\(int token_id\\) const override;" "$lfm2_h" ||
  fail "Lfm2CausalLM.lookupEmbedding(int) base override is missing"

rg -q "std::vector<float> lookupEmbeddingVector\\(unsigned int token_id\\) const;" "$lfm2_h" ||
  fail "Lfm2CausalLM.lookupEmbeddingVector() helper is missing"

rg -q "mutable std::vector<float> embedding_lookup_scratch_;" "$lfm2_h" ||
  fail "Lfm2CausalLM stable lookup scratch buffer is missing"

rg -q "size_t Lfm2CausalLM::embeddingBytesPerToken\\(\\) const" "$lfm2_cpp" ||
  fail "Lfm2CausalLM.embeddingBytesPerToken() implementation is missing"

rg -q "const void \\*Lfm2CausalLM::lookupEmbedding\\(int token_id\\) const" "$lfm2_cpp" ||
  fail "Lfm2CausalLM.lookupEmbedding(int) implementation is missing"

rg -Uq "std::vector<float> Lfm2CausalLM::lookupEmbeddingVector\\([[:space:]]*unsigned int token_id\\) const" "$lfm2_cpp" ||
  fail "Lfm2CausalLM.lookupEmbeddingVector() implementation is missing"

rg -q "embedding_weight_cached_ \\? sizeof\\(float\\) \\* DIM : 0" "$lfm2_cpp" ||
  fail "embeddingBytesPerToken() must report zero until embedding weights are cached"

rg -q "lookupEmbeddingVector" "$vl_cpp" ||
  fail "Legacy LFM2-VL code was not updated to the vector lookup helper"

if rg -q "std::vector<float> lookupEmbedding\\(unsigned int token_id\\)" "$lfm2_h"; then
  fail "unsigned vector lookup still hides the Transformer lookupEmbedding() override"
fi

echo "PASS: LFM2 embedding consumer contract is present"
