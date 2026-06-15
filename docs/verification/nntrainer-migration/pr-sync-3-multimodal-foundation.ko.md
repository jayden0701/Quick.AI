# PR-SYNC-3 Verification Report

Date: 2026-06-12 KST

## Scope

- PR/work unit: generic multimodal foundation hooks for CausalLM and LFM2
- Worktree: `/home/jrock/nntrainer-pr-sync-3-multimodal-foundation`
- Branch: `migration/pr-sync-3-multimodal-foundation`
- Base:
  - `baff1b6f5f9f2b0808386625149ee0f1df353125 causallm: add LFM2 text model`
- Local commit:
  - `34ae1841d92bebf8e40c4d992079344ac0dffd9a causallm: add multimodal foundation hooks`

Source commits consulted:

- `c3a44e11 Fix : deliver externally-fed inputs to QNN graph input tensors`
- `12d6ca9d feat(transformer): add model-agnostic multimodal embedding interface`
- `00a44e01 fix(transformer): add virtual getKvLen() to base for generic composer`
- `5c07ad06 fix(transformer): tolerate missing tokenizer_file (vision-encoder sub-models)`
- `c1b8a569 [CausalLM] Make KV cache binding overridable in SentenceTransformer`
- `32b0b559 [CausalLM] Support run function for LFM in use_embedding`
- `18754b1b feat(transformer): add expectedPixelElems + imagePlaceholderTokenId hooks`

## Implemented Behavior

- Adds model-agnostic multimodal hooks on the base CausalLM/Transformer path:
  - raw embedding row lookup,
  - embedding row byte width,
  - externally supplied embedding run path,
  - pixel count and image placeholder metadata hooks,
  - generic KV length hook.
- Allows Transformer construction without `tokenizer_file`, which is required
  for non-text submodels such as future vision encoders.
- Makes SentenceTransformer cache binding names and slot count overridable.
- Adds nntrainer graph/tensor plumbing for externally supplied QNN graph input
  tensors and QNN activation allocation.
- Adds LFM2 `use_embedding` support:
  - sidecar embedding loading for `FP32`, `FP16`, `Q4_0`, and `Q6_K`,
  - FP32 lookup helper for generated-token embeddings,
  - raw row lookup hook for generic multimodal composition,
  - tokenizer-null and generation-window guards,
  - precomputed embedding prefill and follow-up token generation.
- Extends LFM2 tests to cover the new foundation hooks and the risky
  `use_embedding` sidecar cases.

Excluded:

- Full LFM2-VL model tree, SigLIP/V-JEPA vision models, image-file prompt
  handling, connector model code, and composition loader.
- Tokenizer cache, BPE, WordPiece, and tokenizer_loader changes. These remain
  intentionally deferred to the final tokenizer stage.
- Quick.AI product API, `quick_dot_ai_api`, Android app/AAR/SampleTestAPP code,
  and ProductRatings code.
- Any replacement of nntrainer `causal_lm_api` with Quick.AI-specific wrappers.

Changed files:

- `Applications/CausalLM/models/causal_lm.h`
- `Applications/CausalLM/models/transformer.{h,cpp}`
- `Applications/CausalLM/models/sentence_transformer.{h,cpp}`
- `Applications/CausalLM/models/lfm2/lfm2_causallm.{h,cpp}`
- `nntrainer/graph/network_graph.h`
- `nntrainer/layers/input_layer.cpp`
- `nntrainer/models/neuralnet.cpp`
- `nntrainer/tensor/{manager.h,memory_pool.h,tensor_pool.h,tensor_pool.cpp}`
- `test/unittest/models/unittest_causallm_lfm2.cpp`

## Review Fixes

Code-quality review found several real LFM2 runtime risks. They were fixed
before the final commit:

- `run_with_embeddings()` now rejects prefills that would leave no generation
  window or overflow `ids_history`/KV positions.
- `run()` and `run_with_embeddings()` now reject tokenizer-less decode paths
  before `registerOutputs()` can dereference a null tokenizer.
- LFM2 sidecar loading now supports `FP16`; rows are converted to FP32 for the
  model run path.
- Quantized raw hooks are consistent: `embeddingBytesPerToken()` and
  `lookupEmbedding(int)` now expose the same loaded row storage for `Q4_0` and
  `Q6_K`.
- Manual Q4/Q6 decode was removed; LFM2 now uses nntrainer's
  `dequantize_row_q4_0()` and `dequantize_row_q6_K()` helpers.
- Tests were added first for:
  - generation-window rejection,
  - tokenizer-null rejection,
  - FP16 sidecar load,
  - raw quantized row exposure,
  - Q6_K reference dequant behavior.

Spec re-review result: compliant.

Code-quality re-review result: approved / ready to merge.

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| whitespace | `git diff --check baff1b6f5f9f2b0808386625149ee0f1df353125..HEAD` | PASS | no output |
| x86 reconfigure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | commit `34ae1841` |
| x86 build | `ninja -C build` | PASS | rebuilt CausalLM and LFM2 targets after reconfigure |
| unit tests | `meson test -C build unittest_causallm_models unittest_causal_conv1d_layer unittest_custom_multiply unittest_nntrainer_cpu_backend unittest_cancel_api unittest_chat_template --print-errorlogs` | PASS | 6/6 |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent Seoul answer |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-pr-sync-3-multimodal-foundation -Denable-transformer=true` | PASS | produced generic nntrainer Android package |
| CausalLM NDK app | `ndk-build -C Applications/CausalLM/jni` | BLOCKED_ENV | missing `Applications/CausalLM/lib/libtokenizers_android_c.a`, before source compile |
| NDK syntax probe | NDK clang `-fsyntax-only` for `transformer.cpp`, `sentence_transformer.cpp`, `causal_lm.cpp`, `lfm2_causallm.cpp` | PASS | validates changed CausalLM sources against Android headers |

Android package warnings:

- The Android package build still emits pre-existing C++20-extension warnings
  from `nntrainer/layers/conv2d_layer.cpp` and
  `nntrainer/layers/pooling2d_layer.cpp`.

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| tiny LFM2 fixture | x86 unit test | generated test fixtures | `unittest_causallm_models` | PASS | covers use_embedding bounds, tokenizer guard, FP16/Q4/Q6 sidecar behavior |
| LFM2 use_embedding converted model | x86/Android | not found locally | not run | PENDING_MODEL_FILES | see missing files |
| LFM2-VL / multimodal converted model | x86/Android | not part of this PR | not run | DEFERRED_TO_NEXT_PR | this PR only adds foundation hooks |

Android CLI on `R3CX80H8Y0F` was not run for PR-SYNC-3 standalone because no
converted LFM2 `use_embedding` or LFM2-VL package is available in the current
local/device model inventory, and the direct CausalLM NDK app path is blocked by
the missing tokenizer static archive. Generic nntrainer Android package and
Android syntax probes passed.

## Missing Model Files

| Model | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| LFM2 use_embedding text package | local x86 or `/sdcard/Download/aistudio-mobile/models` converted package | `config.json`, `generation_config.json`, `nntr_config.json`, tokenizer, weights, `embedding_bin_path` sidecar | run x86/Android CLI after package is provided |
| LFM2-VL package | future PR model directory | full LLM, vision, connector, tokenizer, image prompt fixture | deferred to LFM2-VL/image-file PR |

## Debugging Notes

- Direct `ndk-build -C Applications/CausalLM/jni` remains blocked by a
  pre-existing missing tokenizer archive:

```text
Android NDK: ERROR: ... tokenizers_c: LOCAL_SRC_FILES points to a missing file
Android NDK: Check that .../Applications/CausalLM/jni/../lib/libtokenizers_android_c.a exists
```

- This happens before changed CausalLM sources compile, so Android source
  compatibility was checked with syntax-only NDK clang probes and the generic
  nntrainer Android package build.
- QNN activation allocator routing is intentionally graph-wide in this PR
  because it mirrors the `sync_v0.4.0` source concept from `c3a44e11`. Review
  accepted this as non-blocking for PR-SYNC-3, but mixed CPU/QNN E2E with
  FSU/cache-pool scenarios must be validated in a later QNN runtime work unit.

## Review Status

- Spec re-review after final fixes: compliant.
- Code-quality re-review after final fixes: approved / ready to merge.
