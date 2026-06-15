# Quick.AI/nntrainer Multimodal Composition Pluggability

Date: 2026-06-09

이 문서는 `vision encoder + LLM` composition이 현재 코드에서 얼마나
pluggable한지, CPU/NPU vision encoder가 어떻게 합쳐지는지, 그리고 두 모델의
소유권이 Quick.AI와 nntrainer 중 어디에 있는지를 정리한다.

기준 커밋:

- Quick.AI: `19b4527` .. `3161e42`
- nntrainer: `a303271e`, `40384839`, `3ab77293`

## Short Answer

현재 구조는 pluggable하게 확장하기 위한 기반을 갖췄지만, 실제 loader는 아직
완전히 임의 조합을 지원하지 않는다.

| 질문 | 현재 답 |
|---|---|
| LFM 말고 다른 LLM도 임의 vision encoder와 붙일 수 있는가? | 아직 아니다. 공통 interface는 생겼지만 실제 loader는 `lfm2-siglip`, `lfm2-jepa` 두 조합만 hard-code로 지원한다. |
| CPU vision과 NPU vision은 유사하게 합쳐지는가? | execution layer에서는 같은 role/interface로 합쳐진다. 다만 NPU vision은 QNN build/runtime/assets가 필요하다. |
| 두 모델을 들고 있는 곳은 어디인가? | composition handle과 component ownership은 Quick.AI C API 쪽이다. 실제 model class와 연산 구현은 nntrainer 쪽이다. |

## Current Supported Compositions

현재 descriptor-driven composition loader가 실제 load path로 보내는 조합은 두
개다.

| Composition | LLM | Vision | Connector | Backend tuple |
|---|---|---|---|---|
| `lfm2-siglip` | `lfm2-siglip-llm` | `siglip-lfm2-vision` | `lfm2-siglip-connector` | CPU / CPU / CPU |
| `lfm2-jepa` | `lfm2-jepa-llm` | `jepa-qnn-vision` | `lfm2-jepa-connector` | CPU / NPU / CPU |

Catalog descriptor는 더 일반적인 형태를 가진다.

```cpp
// api/model_descriptor.h
typedef enum {
  QDA_ROLE_UNKNOWN = 0,
  QDA_ROLE_TEXT_LLM = 1,
  QDA_ROLE_VISION_ENCODER = 2,
  QDA_ROLE_CONNECTOR = 3,
  QDA_ROLE_COMPOSITION = 4,
} ModelRole;

typedef struct {
  const char *id;
  const char *family;
  const char *display_name;
  RuntimeKind runtime;
  unsigned int backend_mask;
  unsigned int capabilities;
  const char *config_name;
  const char *arch_string;
  ModelRole role;
  unsigned int embedding_dim;
  const char *compatible_with;
} ModelDescriptor;
```

예를 들어 현재 public descriptor는 LFM2/SigLIP 쌍을 이렇게 선언한다.

```cpp
// api/model_descriptors_public.cpp
{"lfm2-siglip-llm", "lfm2", "LFM2 for SigLIP", QDA_RUNTIME_NATIVE,
 B(0) | B(1), 0, nullptr, "Lfm2ForCausalLM", QDA_ROLE_TEXT_LLM, 1024,
 "siglip-lfm2-vision,lfm2-siglip-connector,lfm2-siglip"},

{"siglip-lfm2-vision", "siglip", "SigLIP for LFM2", QDA_RUNTIME_NATIVE,
 B(0) | B(1), QDA_CAP_VISION_ENCODER, nullptr, "Lfm2VlVisionTransformer",
 QDA_ROLE_VISION_ENCODER, 768,
 "lfm2-siglip-llm,lfm2-siglip-connector,lfm2-siglip"},

{"lfm2-siglip-connector", "lfm2", "LFM2 SigLIP Connector",
 QDA_RUNTIME_NATIVE, B(0), 0, nullptr, nullptr, QDA_ROLE_CONNECTOR, 1024,
 "lfm2-siglip-llm,siglip-lfm2-vision,lfm2-siglip"},
```

## Why It Is Not Fully Arbitrary Yet

`loadMultimodalCompositionJson()` parses JSON, checks role/backend/compatibility,
then dispatches only to the two known load functions.

```cpp
// api/quick_dot_ai_api.cpp
if (is_lfm2_siglip_cpu_composition(composition_id, llm, vision, connector)) {
  return load_lfm2_siglip_composition_handle(
    llm, vision, connector, quant_type, native_lib_dir, model_base_path,
    out_handle);
}

if (is_lfm2_jepa_mixed_backend_composition(composition_id, llm, vision,
                                           connector)) {
  return load_lfm2_jepa_composition_handle(
    llm, vision, connector, quant_type, native_lib_dir, model_base_path,
    out_handle);
}

return CAUSAL_LM_ERROR_UNSUPPORTED;
```

따라서 descriptor layer는 확장 가능하지만, 현재 runtime loader는 아직
generic "any LLM + any vision encoder" loader가 아니다. 새 조합을 실제로
지원하려면 descriptor만 추가해서는 부족하고, loader dispatch와 connector
adapter도 맞춰야 한다.

## Common nntrainer Interface

nntrainer의 `Transformer` base class에는 composition을 위한 공통 virtual API가
추가되어 있다.

```cpp
// nntrainer/Applications/CausalLM/models/transformer.h
virtual multimodal_pointer
run_image(const WSTR prompt, multimodal_pointer image, int image_height,
          int image_width, bool do_sample = false,
          const WSTR system_prompt = "", const WSTR tail_prompt = "",
          bool log_output = true);

virtual size_t embeddingBytesPerToken() const { return 0; }

virtual const void *lookupEmbedding(int token_id) const {
  (void)token_id;
  return nullptr;
}

virtual std::pair<float, int> get_embedding_info() { return {1.0f, 0}; }

virtual void run_with_embeddings(const void *prefill_embeds, size_t n_tokens,
                                 std::vector<int> seed_tokens, bool do_sample,
                                 bool log_output);

virtual void set_quant_param(float scale, int offset) {
  (void)scale;
  (void)offset;
}
```

이 interface 기준으로 보면:

- vision encoder는 `run_image()`로 image embedding을 만든다.
- LLM은 `lookupEmbedding()`과 `run_with_embeddings()`로 text embedding과
  image embedding이 합쳐진 prefill input을 소비한다.
- vision encoder는 필요하면 `set_quant_param()`으로 LLM embedding quant space에
  맞춘 출력을 만들 수 있다.

LFM2 LLM은 이 consumer API를 구현한다.

```cpp
// nntrainer/Applications/CausalLM/models/lfm2/lfm2_causallm.cpp
size_t Lfm2CausalLM::embeddingBytesPerToken() const {
  return embedding_weight_cached_ ? sizeof(float) * DIM : 0;
}

const void *Lfm2CausalLM::lookupEmbedding(int token_id) const {
  if (token_id < 0)
    return nullptr;

  try {
    embedding_lookup_scratch_ =
      lookupEmbeddingVector(static_cast<unsigned int>(token_id));
    return embedding_lookup_scratch_.data();
  } catch (...) {
    embedding_lookup_scratch_.clear();
    return nullptr;
  }
}

void Lfm2CausalLM::run_with_embeddings(const void *inputs_embeds,
                                       size_t n_tokens,
                                       std::vector<int> seed_tokens,
                                       bool do_sample, bool log_output);
```

SigLIP CPU vision은 `Lfm2VlVisionTransformer::run_image()`로 producer API를
구현한다.

```cpp
// nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/vision/lfm2_vl_vision_transformer.cpp
Lfm2VlVisionTransformer::run_image(const WSTR prompt,
                                   multimodal_pointer image,
                                   int image_height,
                                   int image_width,
                                   bool,
                                   const WSTR,
                                   const WSTR,
                                   bool log_output) {
  // preprocessed image tensor -> vision features
}
```

## Quick.AI Owns the Composition Handle

두 모델을 하나의 composition으로 묶어 들고 있는 곳은 Quick.AI C API의
`CausalLmModel` handle이다.

```cpp
// api/quick_dot_ai_api.cpp
struct LoadedComponent {
  ModelRole role = QDA_ROLE_UNKNOWN;
  std::string id;
  BackendType backend = CAUSAL_LM_BACKEND_CPU;
  std::unique_ptr<causallm::Transformer> model;
  std::string architecture;
  std::string model_dir;
  double initialization_duration_ms = 0.0;
};

struct CausalLmModel {
  std::mutex mtx;
  std::vector<LoadedComponent> components;
  std::vector<std::unique_ptr<ConnectorAdapter>> connectors;
  std::vector<causallm::Transformer *> models;
  std::vector<std::string> architectures;
  std::vector<std::string> model_dirs;
  bool initialized = false;
  int kv_len = 0;
};
```

Role lookup도 Quick.AI 쪽에서 한다.

```cpp
static causallm::Transformer *find_text_llm(CausalLmModel &h) {
  return find_component(h, QDA_ROLE_TEXT_LLM);
}

static causallm::Transformer *find_vision_encoder(CausalLmModel &h) {
  return find_component(h, QDA_ROLE_VISION_ENCODER);
}
```

정리하면:

- Quick.AI: handle, role-based component ownership, JSON composition parsing,
  compatibility validation, JNI/Kotlin request flow.
- nntrainer: concrete model classes, `Transformer` virtual API, actual
  inference implementation.

## Execution Flow

실행 흐름은 backend와 무관하게 role 기반이다.

```text
runMultimodalHandle*
  -> find_vision_encoder(handle)
  -> find_text_llm(handle)
  -> vision->set_quant_param(llm->get_embedding_info())
  -> vision->run_image(...)
  -> connector->project(...)          # connector가 있을 때
  -> llm->lookupEmbedding(text token)
  -> splice image embeddings into prefill embeddings
  -> llm->run_with_embeddings(...)
```

Quick.AI의 실제 code path는 다음 형태다.

```cpp
// api/quick_dot_ai_api.cpp
auto info = llm->get_embedding_info();
vision->set_quant_param(info.first, info.second);

auto image_embeds =
  vision->run_image(std::string(prompt ? prompt : ""), image_in,
                    originalHeight, originalWidth, false, "", "",
                    g_verbose);

auto *connector = find_connector_adapter(h);
if (connector != nullptr) {
  auto projected = connector->project(vision_features,
                                      static_cast<int>(num_tokens));
  return copy_float_vector_to_multimodal_pointer(projected);
}

return image_embeds;
```

LLM 쪽에서는 text token embedding과 image embedding을 하나의 prefill tensor로
합친 뒤 generation을 시작한다.

```cpp
// api/quick_dot_ai_api.cpp
const size_t bpt = llm->embeddingBytesPerToken();
std::vector<uint8_t> combined(n_total * bpt);

for (int token_id : text_ids_before_image) {
  const void *e = llm->lookupEmbedding(token_id);
  std::memcpy(dst, e, bpt);
  dst += bpt;
}

std::memcpy(dst, image_embeds.first, n_image * bpt);
dst += n_image * bpt;

llm->run_with_embeddings(combined.data(), n_total, text_ids,
                         false, g_verbose);
```

## CPU Vision vs NPU Vision

CPU vision과 NPU vision은 composition execution layer에서는 거의 같은 방식으로
합쳐진다. 둘 다 `QDA_ROLE_VISION_ENCODER` component이고, `Transformer *` base
pointer를 통해 `run_image()`가 호출된다.

차이는 load/build/runtime 조건이다.

```cpp
// CPU composition matcher
return composition_id == "lfm2-siglip" &&
       llm.backend == CAUSAL_LM_BACKEND_CPU &&
       vision.backend == CAUSAL_LM_BACKEND_CPU &&
       connector.backend == CAUSAL_LM_BACKEND_CPU;

// Mixed backend composition matcher
return composition_id == "lfm2-jepa" &&
       llm.backend == CAUSAL_LM_BACKEND_CPU &&
       vision.backend == CAUSAL_LM_BACKEND_NPU &&
       connector.backend == CAUSAL_LM_BACKEND_CPU;
```

NPU/QNN vision은 Android build에서 `--enable-qnn`이 필요하고, QNN backend
extension config와 model assets가 있어야 한다. 현재 checkout 기준으로
`jepa-qnn-vision` descriptor는 존재하지만 JEPA QNN concrete `run_image()`
구현 파일은 public source tree에 보이지 않는다. 따라서 실제 JEPA NPU 실행은
빌드에 포함되는 QNN/plugin/model asset 쪽 구현이 있어야 성립한다.

## C API Example

CPU SigLIP composition:

```json
{
  "id": "lfm2-siglip",
  "llm": {"model_id": "lfm2-siglip-llm", "backend": "CPU"},
  "vision": {"model_id": "siglip-lfm2-vision", "backend": "CPU"},
  "connector": {"model_id": "lfm2-siglip-connector", "backend": "CPU"}
}
```

Mixed CPU/NPU JEPA composition:

```json
{
  "id": "lfm2-jepa",
  "llm": {"model_id": "lfm2-jepa-llm", "backend": "CPU"},
  "vision": {"model_id": "jepa-qnn-vision", "backend": "NPU"},
  "connector": {"model_id": "lfm2-jepa-connector", "backend": "CPU"}
}
```

Minimal C-style load example:

```c
CausalLmHandle handle = NULL;

const char *composition_json =
  "{"
  "\"id\":\"lfm2-jepa\","
  "\"llm\":{\"model_id\":\"lfm2-jepa-llm\",\"backend\":\"CPU\"},"
  "\"vision\":{\"model_id\":\"jepa-qnn-vision\",\"backend\":\"NPU\"},"
  "\"connector\":{\"model_id\":\"lfm2-jepa-connector\",\"backend\":\"CPU\"}"
  "}";

ErrorCode ec = loadMultimodalCompositionJson(
  composition_json,
  CAUSAL_LM_QUANTIZATION_UNKNOWN,
  native_lib_dir,
  model_base_path,
  &handle);

if (ec != CAUSAL_LM_ERROR_NONE) {
  // Handle invalid descriptor/backend/compatibility/model-load failure.
}
```

## Android Kotlin Example

Android에서는 `LoadModelRequest`에 composition field를 채우면
`NativeQuickDotAI.load()`가 JSON을 만들고 JNI의
`loadMultimodalCompositionJsonNative()`로 넘긴다.

```kotlin
val request = LoadModelRequest(
    modelId = "lfm2-jepa",
    backend = BackendType.CPU,
    quantization = QuantizationType.UNKNOWN,
    modelBasePath = "/sdcard/Download/aistudio-mobile/models",
    nativeLibDir = context.applicationInfo.nativeLibraryDir,

    compositionId = "lfm2-jepa",
    llmModelId = "lfm2-jepa-llm",
    llmBackend = BackendType.CPU,
    visionModelId = "jepa-qnn-vision",
    visionBackend = BackendType.NPU,
    connectorModelId = "lfm2-jepa-connector",
    connectorBackend = BackendType.CPU,
)

val engine = NativeQuickDotAI(context)
val loadResult = engine.load(request)
```

Vision preprocessing도 vision model id로 선택된다.

```kotlin
private fun processorForVisionModel(visionModelId: String): NativeImageProcessor =
    when (visionModelId) {
        "siglip-lfm2-vision" -> SigLipNaFlexImageProcessor()
        "jepa-qnn-vision", ModelIds.VJEPA_QNN -> JepaImageProcessor()
        else -> LlavaNextNativeImageProcessor(appContext)
    }
```

## What a New Pluggable Pair Needs

새로운 `vision encoder + LLM` 조합을 추가하려면 다음 조건이 필요하다.

1. LLM descriptor
   - role: `QDA_ROLE_TEXT_LLM`
   - compatible_with에 vision/connector/composition id 포함
   - concrete LLM이 `embeddingBytesPerToken()`, `lookupEmbedding()`,
     `run_with_embeddings()`를 구현해야 한다.

2. Vision descriptor
   - role: `QDA_ROLE_VISION_ENCODER`
   - supported backend mask 설정
   - concrete vision model이 `run_image()`를 구현해야 한다.

3. Connector descriptor and adapter
   - role: `QDA_ROLE_CONNECTOR`
   - vision output dimension을 LLM hidden size로 project해야 한다.
   - 현재 adapter는 LFM2 connector 전용이다.

4. Composition descriptor
   - role: `QDA_ROLE_COMPOSITION`
   - compatible_with에 정확한 LLM/vision/connector component id 포함

5. Loader dispatch
   - 현재처럼 hard-code matcher를 추가하거나,
   - 별도 generic composition loader를 만들어 descriptor만으로 load하도록
     확장해야 한다.

예시 skeleton:

```cpp
static bool is_new_pair_composition(
  const std::string &composition_id,
  const CompositionComponentSpec &llm,
  const CompositionComponentSpec &vision,
  const CompositionComponentSpec &connector) {
  return composition_id == "new-llm-new-vision" &&
         llm.model_id == "new-llm-for-vision" &&
         vision.model_id == "new-vision-encoder" &&
         connector.model_id == "new-pair-connector" &&
         llm.backend == CAUSAL_LM_BACKEND_CPU &&
         connector.backend == CAUSAL_LM_BACKEND_CPU;
}
```

이 skeleton만으로는 충분하지 않다. `new-llm-for-vision`이 embedding consumer
API를 구현하지 않으면 `execute_multimodal()`에서 unsupported 또는 inference
failure가 난다. `new-vision-encoder`가 `run_image()`를 구현하지 않아도 같은
문제가 난다.

## Practical Conclusion

현재 코드는 "완전히 pluggable한 runtime"보다는 "pluggable runtime으로 가는
중간 단계"다. 이미 좋은 기반은 있다.

- descriptor role/compatibility/backend validation
- role-based handle component ownership
- nntrainer base `Transformer` composition interface
- LFM2 embedding consumer implementation
- CPU SigLIP and NPU JEPA composition examples

하지만 실제 임의 조합을 지원하려면 loader dispatch를 더 일반화하고, connector
adapter를 LFM2 전용에서 descriptor/architecture 기반으로 확장해야 한다.
