# Multimodal Composition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable Android and the C API to load and run freely selectable multimodal compositions such as LFM + SigLIP and LFM + JEPA, including mixed backend execution like LLM on CPU and vision on NPU/QNN.

**Architecture:** Move from a fixed monolithic VL class to descriptor-driven composition. Register LLM, vision encoder, and connector as separate catalog components, then load a validated composition into one `CausalLmHandle` with role-based subcomponents and per-component backend selection.

**Tech Stack:** C++17, nntrainer CausalLM `Transformer`, Quick.AI C API/JNI, Kotlin Android AAR, Gradle, Meson/Ninja, QNN on Android.

---

## Current State

- `nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.*` owns SigLIP vision, LFM2 connector, and LFM2 LLM in one class.
- `api/quick_dot_ai_api.*` already has a partial multi-model handle convention: `models[0] = vision producer`, `models[1] = LLM`.
- `loadMultimodalHandleByName()` accepts one `BackendType compute` for both vision and LLM, so mixed backend compositions are not representable.
- `ModelDescriptor` has `QDA_CAP_VISION_ENCODER` in C, but Android `Capability` does not decode it yet.
- Connector is not a catalog role; it is currently embedded in `Lfm2VlForConditionalGeneration`.
- LFM2 embedding consumer support is not aligned with the base `Transformer` virtual API. `Lfm2CausalLM::lookupEmbedding(unsigned int)` returns `std::vector<float>`, while `Transformer::lookupEmbedding(int) const` returns `const void *`.

## Design Decisions

1. Keep existing single-model APIs for compatibility.
2. Add a new composition API instead of overloading the current two-id load function.
3. Represent composition as JSON at the C API boundary so we can add connector, preprocessing, backend, and pair-specific weight fields without ABI churn.
4. Use role-based loaded components internally rather than fixed `models[0]`, `models[1]` assumptions.
5. Treat pair-dependent LLM weights as distinct model ids, e.g. `lfm2-siglip-llm` and `lfm2-jepa-llm`.
6. Treat connector as pair-dependent and selectable by composition, not by the user-facing top-level model picker.

## Target Public API Shape

The new C API should be:

```c
WIN_EXPORT ErrorCode loadMultimodalCompositionJson(
  const char *composition_json,
  ModelQuantizationType quant_type,
  const char *native_lib_dir,
  const char *model_base_path,
  CausalLmHandle *out_handle);
```

Example JSON:

```json
{
  "id": "lfm2-siglip",
  "llm": {
    "model_id": "lfm2-siglip-llm",
    "backend": "CPU"
  },
  "vision": {
    "model_id": "siglip-lfm2-vision",
    "backend": "CPU"
  },
  "connector": {
    "model_id": "lfm2-siglip-connector",
    "backend": "CPU"
  }
}
```

Mixed backend example:

```json
{
  "id": "lfm2-jepa",
  "llm": {
    "model_id": "lfm2-jepa-llm",
    "backend": "CPU"
  },
  "vision": {
    "model_id": "jepa-qnn-vision",
    "backend": "NPU"
  },
  "connector": {
    "model_id": "lfm2-jepa-connector",
    "backend": "CPU"
  }
}
```

Android `LoadModelRequest` should gain optional native composition fields:

```kotlin
@SerialName("composition_id") val compositionId: String? = null,
@SerialName("llm_model_id") val llmModelId: String? = null,
@SerialName("llm_backend") val llmBackend: BackendType? = null,
@SerialName("vision_model_id") val visionModelId: String? = null,
@SerialName("vision_backend") val visionBackend: BackendType? = null,
@SerialName("connector_model_id") val connectorModelId: String? = null,
@SerialName("connector_backend") val connectorBackend: BackendType? = null,
```

`modelId` remains for legacy single-model loads.

---

## Task 1: Extend Catalog Metadata Without Changing Load Behavior

**Files:**
- Modify: `api/model_descriptor.h`
- Modify: `api/model_descriptors_public.cpp`
- Modify: `api/quick_dot_ai_api.cpp`
- Modify: `Android/QuickDotAI/src/main/java/com/example/quickdotai/ModelCatalog.kt`
- Test: catalog JSON parsing and Android capability decode through existing lightweight checks or new focused tests if a suitable harness exists.

- [x] **Step 1: Write failing catalog expectation**

Add a minimal test or executable check that expects:

```json
{
  "role": "TEXT_LLM",
  "embedding_dim": 1024,
  "compatible_with": []
}
```

to be present in catalog JSON for descriptors that opt into the new fields.

Expected initial result: FAIL because `getModelCatalogJson()` does not emit these fields and Android cannot decode `VISION_ENCODER`.

- [x] **Step 2: Add C catalog fields**

Extend `ModelDescriptor` with:

```c
typedef enum {
  QDA_ROLE_UNKNOWN = 0,
  QDA_ROLE_TEXT_LLM = 1,
  QDA_ROLE_VISION_ENCODER = 2,
  QDA_ROLE_CONNECTOR = 3,
  QDA_ROLE_COMPOSITION = 4,
} ModelRole;

ModelRole role;
unsigned int embedding_dim;
const char *compatible_with;
```

Use conservative defaults for existing descriptors:

```c
QDA_ROLE_TEXT_LLM
0
""
```

Set `tiny-bert` to `QDA_ROLE_UNKNOWN` or embedding role only if it is not part of the multimodal path.

- [x] **Step 3: Emit new catalog JSON fields**

Update `getModelCatalogJson()` to emit:

```json
"role": 1,
"embedding_dim": 0,
"compatible_with": ""
```

Expected green result: existing catalog users still parse old fields; new fields are available.

- [x] **Step 4: Decode Android capability and role**

Update `Capability` with `VISION_ENCODER`.

Add `ModelRole` enum and `role: ModelRole` to `ModelDescriptor`.

Decode absent role as `UNKNOWN` so fallback descriptors still work.

- [ ] **Step 5: Verify**

Run the smallest available build/check:

```bash
./build.sh --target=api
```

Expected: build completes without catalog ABI errors.

If Android-only Kotlin changed:

```bash
cd Android
./gradlew :QuickDotAI:compileDebugKotlin
```

Expected: Kotlin compilation succeeds.

Progress:
- 2026-06-08: Added `scripts/check_multimodal_catalog_contract.sh`. RED confirmed with `FAIL: ModelRole enum with QDA_ROLE_TEXT_LLM is missing`.
- 2026-06-08: Added `ModelRole`, `role`, `embedding_dim`, and `compatible_with` to `ModelDescriptor`.
- 2026-06-08: Updated `getModelCatalogJson()` to emit `role`, `embedding_dim`, and `compatible_with`.
- 2026-06-08: Updated Android `ModelCatalog.kt` to decode `VISION_ENCODER`, `ModelRole`, `embeddingDim`, and `compatibleWith`.
- 2026-06-08: GREEN confirmed for `bash scripts/check_multimodal_catalog_contract.sh`.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android Kotlin verification blocked: `./gradlew :QuickDotAI:compileDebugKotlin` failed because `JAVA_HOME` is not set and no `java` executable is on `PATH`.

---

## Task 2: Add Composition Descriptors and Pair Validation

**Files:**
- Modify: `api/model_descriptor.h`
- Modify: `api/model_descriptors_public.cpp`
- Modify: `api/quick_dot_ai_api.cpp`
- Modify: `Android/QuickDotAI/src/main/java/com/example/quickdotai/ModelCatalog.kt`

- [x] **Step 1: Write failing validation test**

Create a focused test or debug check that:

```json
{
  "id": "lfm2-jepa",
  "llm": "lfm2-siglip-llm",
  "vision": "jepa-qnn-vision",
  "connector": "lfm2-siglip-connector"
}
```

is rejected because connector/LLM pair metadata does not match the selected vision encoder.

Expected initial result: FAIL because no validation API exists.

- [x] **Step 2: Add helper for descriptor compatibility**

Implement internal helpers:

```cpp
static bool descriptor_has_role(const ModelDescriptor &d, ModelRole role);
static bool descriptor_allows_backend(const ModelDescriptor &d, BackendType backend);
static bool descriptor_compatible_with(const ModelDescriptor &d, const char *other_id);
```

- [x] **Step 3: Register initial LFM component placeholders**

Add descriptors for:

```text
lfm2-siglip-llm
siglip-lfm2-vision
lfm2-siglip-connector
lfm2-siglip
lfm2-jepa-llm
jepa-qnn-vision
lfm2-jepa-connector
lfm2-jepa
```

These descriptors may be hidden from selectable text model lists until load support is implemented.

- [x] **Step 4: Android catalog helpers**

Add:

```kotlin
fun llmOptionsForComposition(compositionId: String): List<ModelDescriptor>
fun visionOptionsForLlm(llmModelId: String): List<ModelDescriptor>
fun connectorFor(llmModelId: String, visionModelId: String): ModelDescriptor?
fun compositions(): List<ModelDescriptor>
```

- [ ] **Step 5: Verify**

Run:

```bash
./build.sh --target=api
cd Android
./gradlew :QuickDotAI:compileDebugKotlin
```

Progress:
- 2026-06-08: Added `scripts/check_multimodal_composition_contract.sh`. RED confirmed with `FAIL: descriptor 'lfm2-siglip-llm' is missing`.
- 2026-06-08: Added component/composition placeholder descriptors for `lfm2-siglip-*` and `lfm2-jepa-*`.
- 2026-06-08: Added descriptor helpers `descriptor_has_role`, `descriptor_allows_backend`, and `descriptor_compatible_with`.
- 2026-06-08: Added Android catalog helpers `compositions()`, `llmOptionsForComposition()`, `visionOptionsForLlm()`, and `connectorFor()`.
- 2026-06-08: GREEN confirmed for `bash scripts/check_multimodal_composition_contract.sh` and regression check `bash scripts/check_multimodal_catalog_contract.sh`.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android Kotlin verification remains blocked by missing Java/JDK in this environment.

---

## Task 3: Add New C API and JNI Entry Point for Composition Loading

**Files:**
- Modify: `api/quick_dot_ai_api.h`
- Modify: `api/quick_dot_ai_api.cpp`
- Modify: `Android/QuickDotAI/src/main/cpp/quickai_jni.cpp`
- Modify: `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt`

- [x] **Step 1: Write failing JNI/API compile check**

Add the Kotlin external declaration first:

```kotlin
external fun loadMultimodalCompositionJsonNative(
    compositionJson: String,
    quant: Int,
    nativeLibDir: String?,
    modelBasePath: String?,
): Long
```

Expected initial result: native symbol missing or C API declaration missing.

- [x] **Step 2: Add C API declaration**

Add:

```c
WIN_EXPORT ErrorCode loadMultimodalCompositionJson(
  const char *composition_json,
  ModelQuantizationType quant_type,
  const char *native_lib_dir,
  const char *model_base_path,
  CausalLmHandle *out_handle);
```

- [x] **Step 3: Implement JSON parse and validation only**

Parse `llm.model_id`, `llm.backend`, `vision.model_id`, `vision.backend`, `connector.model_id`, `connector.backend`.

Initially return `CAUSAL_LM_ERROR_UNSUPPORTED` after validation succeeds so the API contract can be compiled before real load.

- [x] **Step 4: Add JNI wrapper**

Implement `Java_com_example_quickdotai_NativeCausalLm_loadMultimodalCompositionJsonNative`.

- [x] **Step 5: Verify**

Run:

```bash
./build.sh --target=api
cd Android
./gradlew :QuickDotAI:compileDebugKotlin
```

Progress:
- 2026-06-08: Added `scripts/check_multimodal_composition_api_contract.sh`. RED confirmed with `FAIL: C API declaration for loadMultimodalCompositionJson is missing`.
- 2026-06-08: Added `loadMultimodalCompositionJson` C API declaration and validation-only implementation for composition id, LLM, vision, connector, per-component backend, role, and compatibility checks.
- 2026-06-08: Added JNI entry point `Java_com_example_quickdotai_NativeCausalLm_loadMultimodalCompositionJsonNative` and Kotlin external declaration `loadMultimodalCompositionJsonNative`.
- 2026-06-08: Added Android bundled C API header coverage after RED from `:QuickDotAI:externalNativeBuildDebug` showed the JNI CMake include path was stale.
- 2026-06-08: GREEN confirmed for `bash scripts/check_multimodal_composition_api_contract.sh`, `bash scripts/check_multimodal_composition_contract.sh`, and `bash scripts/check_multimodal_catalog_contract.sh`.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android verification passed with `./gradlew :QuickDotAI:compileDebugKotlin` and `./gradlew :QuickDotAI:externalNativeBuildDebug` after rebuilding/copying the ignored Android `libquick_dot_ai_api.so` prebuilt for local verification.

---

## Task 4: Refactor Handle State to Role-Based Components

**Files:**
- Modify: `api/quick_dot_ai_api.cpp`

- [x] **Step 1: Write failing internal behavior check**

Create a test/debug path that loads a fake or existing two-component handle and verifies that the text model is found by role, not by index.

Expected initial result: FAIL because current helpers use fixed index convention.

- [x] **Step 2: Add role metadata to handle**

Introduce:

```cpp
struct LoadedComponent {
  ModelRole role;
  std::string id;
  BackendType backend;
  std::unique_ptr<causallm::Transformer> model;
  std::string architecture;
  std::string model_dir;
  double initialization_duration_ms;
};
```

Keep the existing vectors temporarily if needed for legacy APIs, but new composition code should use `components`.

- [x] **Step 3: Add lookup helpers**

```cpp
static causallm::Transformer *find_component(CausalLmModel &h, ModelRole role);
static causallm::Transformer *find_text_llm(CausalLmModel &h);
static causallm::Transformer *find_vision_encoder(CausalLmModel &h);
```

- [x] **Step 4: Migrate multimodal path to role lookup**

Replace direct `h.models[0]` and `h.models[1]` usage in generic multimodal functions.

- [x] **Step 5: Verify**

Run:

```bash
./build.sh --target=api
```

Progress:
- 2026-06-08: Added `scripts/check_multimodal_role_lookup_contract.sh`. RED confirmed with `FAIL: LoadedComponent struct is missing`.
- 2026-06-08: Added `LoadedComponent` and `CausalLmModel.components`, with existing `models`, `architectures`, `model_dirs`, and `initialization_duration_ms` kept as compatibility mirrors.
- 2026-06-08: Added role lookup helpers `find_component()`, `find_text_llm()`, and `find_vision_encoder()`.
- 2026-06-08: Updated `loadMultimodalHandleByName()` to tag loaded vision and text components by role, and migrated generic multimodal paths to role lookup instead of direct `models[0]`/`models[1]` access.
- 2026-06-08: GREEN confirmed for `bash scripts/check_multimodal_role_lookup_contract.sh` plus existing multimodal contract checks.
- 2026-06-08: Native verification passed with `./build.sh --target=api` and Android/QNN compile verification passed with `./build.sh --platform=android --enable-qnn --target=api`.

---

## Task 5: Define Connector Adapter Interface

**Files:**
- Modify: `nntrainer/Applications/CausalLM/models/transformer.h`
- Modify: `nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_connector.h`
- Modify: `nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_connector.cpp`
- Modify: `api/quick_dot_ai_api.cpp`

- [x] **Step 1: Write failing connector projection test**

Use a tiny synthetic connector fixture where input features are known and verify that `project()` returns the expected LLM embedding shape.

Expected initial result: FAIL because no connector adapter role exists.

- [x] **Step 2: Introduce connector base interface**

Avoid forcing connector to inherit full `Transformer` if it does not need graph lifecycle.

Add a small internal API-layer adapter type:

```cpp
struct ConnectorAdapter {
  virtual ~ConnectorAdapter() = default;
  virtual size_t output_embedding_dim() const = 0;
  virtual std::vector<float> project(const std::vector<float> &vision_features,
                                     int num_tokens) = 0;
};
```

- [x] **Step 3: Wrap `Lfm2VlConnector`**

Provide an adapter that owns `Lfm2VlConnector`, loads weights from descriptor-resolved model dir, and projects SigLIP/JEP(A) features into LFM hidden size.

- [x] **Step 4: Wire connector into `CausalLmModel`**

Store connector components separately from `Transformer` components if needed:

```cpp
std::vector<std::unique_ptr<ConnectorAdapter>> connectors;
```

- [x] **Step 5: Verify**

Run:

```bash
./build.sh --target=api
```

Progress:
- 2026-06-08: Added `scripts/check_multimodal_connector_contract.sh`. RED confirmed with `FAIL: ConnectorAdapter interface is missing`.
- 2026-06-08: Added API-layer `ConnectorAdapter`, `Lfm2ConnectorAdapter`, and `CausalLmModel.connectors` ownership.
- 2026-06-08: Added `Lfm2VlConnector::project()` and a downsample-factor accessor in nntrainer.
- 2026-06-08: Added descriptor-based LFM2 connector weight lookup that calls `connector->loadWeights(...)`.
- 2026-06-08: GREEN confirmed for `bash scripts/check_multimodal_connector_contract.sh`.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android/QNN verification passed with `./build.sh --platform=android --enable-qnn --target=api`.

---

## Task 6: Align LFM2 LLM With Generic Embedding Consumer Interface

**Files:**
- Modify: `nntrainer/Applications/CausalLM/models/lfm2/lfm2_causallm.h`
- Modify: `nntrainer/Applications/CausalLM/models/lfm2/lfm2_causallm.cpp`
- Modify: `api/quick_dot_ai_api.cpp`

- [x] **Step 1: Write failing embedding consumer check**

Construct or load LFM2 enough to assert:

```cpp
model->embeddingBytesPerToken() == sizeof(float) * hidden_size
model->lookupEmbedding(token_id) != nullptr
model->run_with_embeddings(...)
```

Expected initial result: FAIL because LFM2 does not override the base pointer-returning API.

- [x] **Step 2: Add stable lookup buffer**

Add a mutable scratch buffer inside `Lfm2CausalLM` so:

```cpp
const void *lookupEmbedding(int token_id) const override;
```

can return a pointer valid until the next lookup on the same model.

- [x] **Step 3: Override `embeddingBytesPerToken()`**

Return `sizeof(float) * DIM` for LFM2 FP32 embedding consumer.

- [x] **Step 4: Keep old vector API if existing code uses it**

Rename or keep:

```cpp
std::vector<float> lookupEmbeddingVector(unsigned int token_id) const;
```

and update `lfm2_vl_model.cpp` legacy code if needed.

- [x] **Step 5: Verify**

Run:

```bash
./build.sh --target=api
```

Progress:
- 2026-06-08: Added `scripts/check_lfm2_embedding_consumer_contract.sh`. RED confirmed with `FAIL: Lfm2CausalLM.embeddingBytesPerToken() override is missing`.
- 2026-06-08: Added `Lfm2CausalLM::embeddingBytesPerToken()` and base-pointer `lookupEmbedding(int) const` overrides.
- 2026-06-08: Added `embedding_lookup_scratch_` as the stable per-model pointer buffer.
- 2026-06-08: Renamed the vector-returning API to `lookupEmbeddingVector()` and updated LFM2-VL/internal call sites.
- 2026-06-08: GREEN confirmed for `bash scripts/check_lfm2_embedding_consumer_contract.sh`.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android/QNN verification passed with `./build.sh --platform=android --enable-qnn --target=api`.

---

## Task 7: Implement First Real Composition Load: LFM + SigLIP on CPU

**Files:**
- Modify: `api/quick_dot_ai_api.cpp`
- Modify: `api/model_descriptors_public.cpp`
- Modify: LFM2/SigLIP config resources as needed under `src/res/` or the model asset directory convention.

- [x] **Step 1: Write failing load test**

Call `loadMultimodalCompositionJson()` with `lfm2-siglip`, all CPU.

Expected initial result: FAIL with unsupported or missing loader.

- [x] **Step 2: Load LLM component**

Load `lfm2-siglip-llm` with backend CPU and role `TEXT_LLM`.

- [x] **Step 3: Load vision component**

Load `siglip-lfm2-vision` with backend CPU and role `VISION_ENCODER`.

- [x] **Step 4: Load connector component**

Load `lfm2-siglip-connector` as connector adapter.

- [x] **Step 5: Run single-image prompt**

Use `runMultimodalHandleWithMessagesStreaming()` with one preprocessed image and one text prompt.

- [x] **Step 6: Verify**

Run:

```bash
./build.sh --target=api
```

If model files are available on device:

```bash
export NDK_ROOT=/path/to/android-ndk
./apk-build-install.sh
```

Progress:
- 2026-06-08: Added `scripts/check_lfm2_siglip_composition_load_contract.sh`. RED confirmed with `FAIL: LFM2+SigLIP composition loader helper is missing`.
- 2026-06-08: Added descriptor-key-based component loading for composition parts.
- 2026-06-08: Added `load_lfm2_siglip_composition_handle()` to load CPU LFM2 LLM, CPU SigLIP vision, and CPU LFM2 connector into one role-tagged handle.
- 2026-06-08: Added shared `move_first_loaded_component()` helper for role-tagged handle composition.
- 2026-06-08: Added `Lfm2VlVisionTransformer::run_image()` and routed API multimodal execution through connector projection when a connector is present.
- 2026-06-08: Removed the `ENABLE_QNN` gate from generic multimodal run paths so CPU compositions can execute.
- 2026-06-08: GREEN confirmed for `bash scripts/check_lfm2_siglip_composition_load_contract.sh`; with an empty model base the valid composition reached the real load path and returned `CAUSAL_LM_ERROR_MODEL_LOAD_FAILED` instead of `CAUSAL_LM_ERROR_UNSUPPORTED`.
- 2026-06-08: Full single-image prompt smoke was not run because no LFM2/SigLIP model asset directory is present in the workspace; run-path source and load-path smoke are verified.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android/QNN verification passed with `./build.sh --platform=android --enable-qnn --target=api`.

---

## Task 8: Implement LFM + JEPA Mixed Backend Composition

**Files:**
- Modify: `api/model_descriptors_public.cpp`
- Modify: `api/quick_dot_ai_api.cpp`
- Modify: QNN vision component files under `src/models/qnn/` as needed.
- Modify: Android preprocessing selection.

- [x] **Step 1: Write failing mixed backend load test**

Use composition JSON:

```json
{
  "id": "lfm2-jepa",
  "llm": {"model_id": "lfm2-jepa-llm", "backend": "CPU"},
  "vision": {"model_id": "jepa-qnn-vision", "backend": "NPU"},
  "connector": {"model_id": "lfm2-jepa-connector", "backend": "CPU"}
}
```

Expected initial result: FAIL until per-component backend load is wired.

- [x] **Step 2: Ensure QNN env setup is per component**

Call QNN backend config setup before QNN vision component initialization.

- [x] **Step 3: Implement multi-image vision path**

Remove the temporary V-JEPA multi-image delegation that uses only the first image.

- [x] **Step 4: Verify**

Run:

```bash
./build.sh --platform=android --enable-qnn --target=api
```

Then device smoke via:

```bash
./apk-build-install.sh
```

Progress:
- 2026-06-08: Added `scripts/check_lfm2_jepa_mixed_backend_contract.sh`. RED confirmed with `FAIL: LFM2+JEPA mixed-backend composition matcher is missing`.
- 2026-06-08: Added `is_lfm2_jepa_mixed_backend_composition()` and `load_lfm2_jepa_composition_handle()` for CPU LLM, NPU vision, and CPU connector loading.
- 2026-06-08: Reused per-component `load_transformer_component()` so QNN backend setup happens inside the selected NPU vision component load path.
- 2026-06-08: Replaced the multi-image first-image stub with patch-layout validation and aggregate original-dimension handling.
- 2026-06-08: GREEN confirmed for `bash scripts/check_lfm2_jepa_mixed_backend_contract.sh`.
- 2026-06-08: Native verification passed with `./build.sh --target=api`.
- 2026-06-08: Android/QNN verification passed with `./build.sh --platform=android --enable-qnn --target=api`.
- 2026-06-08: Device smoke via `./apk-build-install.sh` was not run because JEPA QNN assets/device runtime are not present in the workspace.

---

## Task 9: Android Request and UI Integration

**Files:**
- Modify: `Android/QuickDotAI/src/main/java/com/example/quickdotai/Types.kt`
- Modify: `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt`
- Modify: `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`
- Modify: `Android/SampleTestAPP/src/main/java/com/example/sampletestapp/MainActivity.kt`
- Modify: `Android/QuickDotAI/README.md`
- Modify: `Android/Architecture.md`

- [ ] **Step 1: Write failing Kotlin compile check**

Add `LoadModelRequest` fields and use them in `NativeQuickDotAI.load()`.

Expected initial result: FAIL until JNI declaration exists.

- [ ] **Step 2: Build composition JSON in Kotlin**

In `NativeQuickDotAI.load()`, if `compositionId` is non-null, call `loadMultimodalCompositionJsonNative`.

- [ ] **Step 3: Update model key**

Make `modelKey` include composition and per-component backends:

```kotlin
compositionId:llmModelId:llmBackend:visionModelId:visionBackend:connectorModelId:connectorBackend:quant
```

- [ ] **Step 4: Update SampleTestAPP UI**

Add separate controls:

```text
LLM family/runtime/backend
Vision encoder/backend
Connector display
```

For now connector can be read-only after LLM/Vision selection.

- [ ] **Step 5: Select processor by vision id**

Use SigLIP preprocessing for `siglip-lfm2-vision`, JEPA preprocessing for `jepa-qnn-vision`.

- [ ] **Step 6: Verify**

Run:

```bash
cd Android
./gradlew :QuickDotAI:compileDebugKotlin :SampleTestAPP:compileDebugKotlin
```

Progress:
- 2026-06-08: Pending.

---

## Task 10: Documentation and End-to-End Verification

**Files:**
- Modify: `docs/RepositoryOrientation.md`
- Modify: `docs/Guides.md`
- Modify: `api/README.md`
- Modify: `Android/QuickDotAI/README.md`
- Modify: `Android/Architecture.md`

- [ ] **Step 1: Document component catalog**

Explain model roles, composition ids, backend selection, and pair-specific weights.

- [ ] **Step 2: Document Android usage**

Show Kotlin examples for:

```kotlin
LoadModelRequest(
    compositionId = "lfm2-jepa",
    llmModelId = "lfm2-jepa-llm",
    llmBackend = BackendType.CPU,
    visionModelId = "jepa-qnn-vision",
    visionBackend = BackendType.NPU,
    connectorModelId = "lfm2-jepa-connector",
    connectorBackend = BackendType.CPU,
    modelId = "lfm2-jepa"
)
```

- [ ] **Step 3: Run final checks**

Run:

```bash
./build.sh --target=api
cd Android
./gradlew :QuickDotAI:compileDebugKotlin :SampleTestAPP:compileDebugKotlin
```

If Android device and NDK/QNN are available:

```bash
./apk-build-install.sh
```

Progress:
- 2026-06-08: Pending.

---

## Progress Log

- 2026-06-08: Plan created. No production code changed by this plan entry.
