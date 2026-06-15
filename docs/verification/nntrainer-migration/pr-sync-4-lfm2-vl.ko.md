# PR-SYNC-4 Verification Report

Date: 2026-06-12 KST

## Scope

- PR/work unit: LFM2-VL vision tower, connector, and composition skeleton
- Worktree: `/home/jrock/nntrainer-pr-sync-4-lfm2-vl`
- Branch: `migration/pr-sync-4-lfm2-vl`
- Base:
  - `34ae1841d92bebf8e40c4d992079344ac0dffd9a causallm: add multimodal foundation hooks`
- Local commit:
  - `879f04185ed6a0d729f9ad9816e62edf4db4e327 causallm: add LFM2-VL vision tower`

Source commits consulted:

- `7feb453f [CausalLM] Port LFM2-VL multimodal model onto #3963 base`
- `e92307b7 [CausalLM] Fix LFM2-VL ViT FP32 KV cache, USE_EMBEDDING path, and shared weight loading`
- `4c661e56` was inspected, but README/converter/runtime docs were deferred.

## Implemented Behavior

- Adds the LFM2-VL connector:
  - pixel-unshuffle / space-to-depth over ViT patch features,
  - layer-norm + two-layer MLP connector,
  - configured `downsample_factor` support,
  - input-size and patch-grid validation.
- Adds the LFM2-VL vision transformer:
  - SigLIP-style ViT graph construction,
  - rectangular `image_height` / `image_width` patch grid support,
  - FP32 external KV cache placeholders and `allocateAndBindVitKVCache()`,
  - exact raw FP32 tensor file-size validation before inference,
  - cached output features for downstream connector use.
- Adds a composition class for LFM2-VL structure:
  - owns ViT, connector, and LFM2 CausalLM submodels,
  - normalizes a single image marker in the prompt,
  - rejects multiple tokenized image placeholders,
  - rejects multimodal `batch_size != 1` for this structural PR,
  - uses PR-SYNC-3 LFM2 `use_embedding` hook for merged embeddings.
- Adds Meson wiring for the new LFM2-VL tree and a structural unit test target.

Excluded:

- `main.cpp` image-file CLI and factory/runtime registration for
  `Lfm2VlForConditionalGeneration`.
- Converter scripts, resource README/docs/scripts/configs, sample image assets.
- Tokenizer cache, BPE, WordPiece, and tokenizer_loader changes. These remain
  intentionally deferred to the final tokenizer stage.
- V-JEPA, raw video/pixel app path, Android product code, Quick.AI wrappers,
  ProductRatings, and AAR/sample app code.
- Core `network_graph.cpp`, `neuralnet.cpp`, `mha_core.*`, and
  `transformer.cpp` changes from sync commits.
- Any new public API in existing non-LFM2-VL files. An early
  `Lfm2CausalLM::getGeneratedIds()` accessor was removed before final commit.

Changed files:

- `Applications/CausalLM/meson.build`
- `Applications/CausalLM/models/lfm2/meson.build`
- `Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_connector.{h,cpp}`
- `Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.{h,cpp}`
- `Applications/CausalLM/models/lfm2/lfm2-vl/meson.build`
- `Applications/CausalLM/models/lfm2/lfm2-vl/vision/lfm2_vl_vision_transformer.{h,cpp}`
- `Applications/CausalLM/models/lfm2/lfm2-vl/vision/meson.build`
- `test/unittest/layers/unittest_lfm2_vl_vision_transformer.cpp`

## Review Fixes

Subagent review found real issues before final commit. They were fixed with
targeted tests first:

- Multiple `<image>` placeholders could splice past `image_embeds`.
  The prompt is now normalized to one canonical marker, and tokenized image
  placeholder count must be exactly one.
- Multimodal `batch_size > 1` was not implemented end-to-end.
  The VL path now rejects non-1 batch size explicitly.
- `downsample_factor` was configurable in the model but not honored by
  `Lfm2VlConnector::outTokens()`. The connector now stores and validates the
  configured factor.
- `loadImageTensor()` accepted non-FP32-aligned file sizes. It now rejects
  byte lengths not divisible by `sizeof(float)`.
- The full VL composition used square `image_size` for both patch dimensions.
  It now follows the ViT's `image_height` / `image_width` fallback logic.
- Standalone ViT setup accepted invalid patch grids and could divide by zero.
  `patch_size == 0` and non-divisible image dimensions are rejected.
- ViT `run()` accepted files with trailing bytes after the expected tensor.
  It now requires the raw tensor file size to match exactly.
- Debug-only environment hooks and unconditional ViT logging were removed; quiet
  `log_output=false` now stays quiet.
- `incremental_inference()` outputs from the ViT path are deleted after copying.

Final spec review result: compliant.

Final code-quality review result: no Critical/P1/P2 findings.

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| whitespace | `git diff --check 34ae1841d92bebf8e40c4d992079344ac0dffd9a..HEAD` | PASS | no output |
| x86 reconfigure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | commit `879f0418` |
| x86 build | `ninja -C build` | PASS | full CausalLM build after reconfigure |
| unit tests | `meson test -C build unittest_lfm2_vl_vision_transformer unittest_causallm_models unittest_causal_conv1d_layer unittest_custom_multiply unittest_nntrainer_cpu_backend unittest_chat_template --print-errorlogs` | PASS | 6/6 |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent Seoul answer |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-pr-sync-4-lfm2-vl -Denable-transformer=true` | PASS | produced generic nntrainer Android package |
| CausalLM NDK app | `ndk-build -C Applications/CausalLM/jni` | BLOCKED_ENV | missing `Applications/CausalLM/lib/libtokenizers_android_c.a`, before source compile |
| NDK syntax probe | NDK clang `-fsyntax-only` for new LFM2-VL connector/model/vision sources | PASS | validates changed CausalLM sources against Android headers |

Android package warnings:

- The Android package build still emits pre-existing C++20-extension warnings
  from `nntrainer/layers/conv2d_layer.cpp` and
  `nntrainer/layers/pooling2d_layer.cpp`.

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| LFM2-VL structural fixture | x86 unit test | generated test fixtures | `unittest_lfm2_vl_vision_transformer` | PASS | 22 tests cover config parsing, invalid patch grid, quiet ViT run, connector validation, prompt merge guards, and file-size checks |
| LFM2-VL converted model | Android | `/sdcard/Download/aistudio-mobile/models/lfm2-vl` on `R3CX80H8Y0F` | not run | DEFERRED_TO_NEXT_PR | model files exist on-device, but this structural PR intentionally excludes image CLI/factory wiring and raw-image preprocessing path |
| LFM2-VL converted model | x86 | not found locally | not run | PENDING_MODEL_FILES | no local x86 package or preprocessed FP32 image tensor fixture |

Android CLI on `R3CX80H8Y0F` was not run for PR-SYNC-4 standalone. The device
does contain an LFM2-VL model package:

```text
/sdcard/Download/aistudio-mobile/models/lfm2-vl
```

However, this PR is intentionally structural: `main.cpp`/factory image-file
CLI support, converter/runtime scripts, and image preprocessing artifacts are
deferred. The direct CausalLM NDK app path is also blocked before source
compile by the missing tokenizer static archive. Generic nntrainer Android
package and Android syntax probes passed.

## Missing Model Files

| Model | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| LFM2-VL x86 package | local converted package | LLM weights, vision weights, connector weights, embedding sidecar, `config.json`, `generation_config.json`, `nntr_config.json`, tokenizer, preprocessed FP32 image tensor | run x86 CLI after package is provided |
| LFM2-VL Android runtime input | `/sdcard/Download/aistudio-mobile/models/lfm2-vl` | raw/preprocessed FP32 image tensor path for the current nntr_causallm image CLI shape; package currently has `sample.png` | run Android CLI after image CLI/factory/preprocessing PR is available |
| LFM2-VL CLI wiring | future PR | `main.cpp`/factory image-file path support | deferred by scope |

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
- LFM2-VL is still structural in this PR. Full runtime is intentionally deferred
  until image CLI/factory wiring, converted model files, and image preprocessing
  artifacts are available.

## Review Status

- Final spec re-review: compliant.
- Final code-quality re-review: no Critical/P1/P2 findings.
