# PR-SYNC-5b Verification Report

Date: 2026-06-12 KST

## Scope

- PR/work unit: LFM2-VL generic runtime wiring
- Worktree: `/home/jrock/nntrainer-pr-sync-5b-lfm2-vl-runtime`
- Branch: `migration/pr-sync-5b-lfm2-vl-runtime`
- Base:
  - `443506b7933650f3f96d4fd03f4f5ffa55a64014 causallm: add LFM2-VL conversion resources`
- Local commit:
  - `5b05ea49c839d8da686c68dbc4f6fa8b087dbd39 causallm: wire LFM2-VL runtime paths`

## Implemented Behavior

- Registers `Lfm2VlForConditionalGeneration` through the existing CausalLM
  factory path.
- Makes `Lfm2VlForConditionalGeneration` `Transformer`-compatible so generic
  CausalLM runners can construct and call it.
- Adds shared CausalLM runtime config path helpers for resolving relative
  `nntr_config.json` paths against the selected model directory.
- Extends `nntr_causallm` path resolution to cover:
  - `model_file_name`
  - `tokenizer_file`
  - `embedding_file_name`
  - `ple_file_name`
  - `embedding_bin_path`
  - `vision_model_file`
  - `connector_model_file`
  - `image_path`
  - `image_tensor_path`
- Updates LFM2-VL weight loading so the LM, vision tower, and connector weight
  paths resolve from the model package directory.
- Adds a generic `Transformer::run()` entry for LFM2-VL that accepts only a
  preprocessed raw FP32 tensor through `nntr_config.json` `image_tensor_path`.
- Updates LFM2-VL resources and README to state that PNG/JPEG/BMP decoding and
  preprocessing from `image_path` are deferred.

Excluded:

- PNG/JPEG/BMP image-file decode and in-binary preprocessing.
- Android ARM numerics fixes beyond package-build coverage.
- App-oriented `runFromPixels()` / raw-pixel C API.
- Tokenizer cache, BPE, WordPiece, and tokenizer_loader changes.
- Quick.AI product wrapper changes.

Changed files:

- `Applications/CausalLM/main.cpp`
- `Applications/CausalLM/meson.build`
- `Applications/CausalLM/runtime_config.cpp`
- `Applications/CausalLM/runtime_config.h`
- `Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.cpp`
- `Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.h`
- `Applications/CausalLM/res/lfm2-vl/README.md`
- `Applications/CausalLM/res/lfm2-vl/nntr_config.json`
- `test/unittest/layers/unittest_lfm2_vl_vision_transformer.cpp`
- `test/unittest/models/unittest_lfm2_vl_converters.py`

Affected models:

- LFM2-VL: generic factory/config path wiring added.
- Gemma4: regression-tested because `main.cpp` path resolution changed.

## TDD Notes

RED:

```text
ninja -C build Applications/CausalLM/unittest_lfm2_vl_vision_transformer
```

Result before implementation: FAIL. The added test included
`runtime_config.h`, which did not exist yet.

GREEN:

```text
meson test -C build unittest_lfm2_vl_vision_transformer --no-rebuild --print-errorlogs
```

Result after implementation: PASS.

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| whitespace | `git diff --check 443506b7933650f3f96d4fd03f4f5ffa55a64014..HEAD` | PASS | no output |
| x86 reconfigure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | native x86 |
| x86 build | `ninja -C build` | PASS | full CausalLM build |
| focused unit tests | `meson test -C build unittest_lfm2_vl_converters unittest_lfm2_vl_vision_transformer unittest_causallm_models unittest_chat_template --print-errorlogs` | PASS | 4/4 |
| x86 Gemma4 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | answered Seoul |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-pr-sync-5b-lfm2-vl-runtime -Denable-transformer=true` | PASS | generic nntrainer Android package; Meson skips app path |
| CausalLM NDK app | `ndk-build -C Applications/CausalLM/jni APP_ABI=arm64-v8a` | BLOCKED_ENV | missing `Applications/CausalLM/lib/libtokenizers_android_c.a` |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| LFM2-VL structural fixture | x86 unit test | generated test fixtures | `unittest_lfm2_vl_vision_transformer` | PASS | verifies config path resolution, factory compatibility, and missing `image_tensor_path` guard |
| LFM2-VL converted package | Android | `/sdcard/Download/aistudio-mobile/models/lfm2-vl` on `R3CX80H8Y0F` | not run | PENDING_INPUT_TENSOR | device package has `sample.png`, but its `nntr_config.json` does not yet provide `image_tensor_path`; image decode is deferred |

## Android Device Observation

`R3CX80H8Y0F` has the expected LFM2-VL package under
`/sdcard/Download/aistudio-mobile/models/lfm2-vl`, including LM, vision,
connector, embedding, tokenizer, `sample.png`, and `nntr_config.json`.

The current device `nntr_config.json` does not contain `image_tensor_path`.
That is not a failure of this PR because PR-SYNC-5b intentionally supports only
preprocessed raw FP32 image tensors. The next image-preprocessing PR should
either generate/provide that tensor for CLI testing or add the image-file
decode path from `image_path`.

## Review Notes

- Spec review found one packaging risk before commit: `runtime_config.cpp/h`
  were untracked. They are included in
  `5b05ea49c839d8da686c68dbc4f6fa8b087dbd39`.
- Code-quality review after commit is tracked separately in the migration
  session notes.

## Notes

- `nntr_causallm` no longer prefixes `model_path` manually after config path
  resolution; Gemma4 regression confirms existing relative-model packages still
  run.
- Android package still emits pre-existing C++20-extension warnings from
  `nntrainer/layers/conv2d_layer.cpp` and
  `nntrainer/layers/pooling2d_layer.cpp`.
