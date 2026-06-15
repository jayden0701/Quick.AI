# PR-SYNC-5a Verification Report

Date: 2026-06-12 KST

## Scope

- PR/work unit: LFM2-VL converter and resource reproducibility
- Worktree: `/home/jrock/nntrainer-pr-sync-5a-lfm2-vl-converters`
- Branch: `migration/pr-sync-5a-lfm2-vl-converters`
- Base:
  - `879f04185ed6a0d729f9ad9816e62edf4db4e327 causallm: add LFM2-VL vision tower`
- Local commit:
  - `443506b7933650f3f96d4fd03f4f5ffa55a64014 causallm: add LFM2-VL conversion resources`

Source commits consulted:

- `9f954843 [CausalLM] Fix LFM2-VL LM converter for hybrid conv/attn architecture`
- `0b52d15e [CausalLM] Add LFM2-VL weight converters and config templates`
- `313229d1 Restore verified LFM2-VL LM converter order`
- `3dbd4d97 [CausalLM] Write vision position embedding in convert_vision_hf.py`

## Implemented Behavior

- Adds LFM2-VL conversion resources under
  `Applications/CausalLM/res/lfm2-vl`.
- Adds converters for:
  - language model weights,
  - vision tower weights,
  - multimodal connector weights,
  - embedding table weights.
- Adds `nntr_config.json` template with relative filenames for later runtime
  wiring.
- Adds scoped README that documents conversion only and explicitly defers
  image-file runtime, image preprocessing, ARM numerics, and app raw-pixel APIs.
- Documents two current runtime caveats so this converter-only PR does not
  overclaim runtime readiness:
  - `embedding_bin_path` is not yet resolved relative to model directory by the
    current runtime.
  - LiquidAI Hugging Face `config.json` may use `text_config.conv_L_cache == 3`,
    while the current nntrainer LFM2 graph accepts `2`.
- Adds `unittest_lfm2_vl_converters`, a Python Meson test that verifies:
  - required resource files exist,
  - LFM2-VL hybrid layer pattern is fixed,
  - FFN `w3` is written before `w1`,
  - causal-conv kernels flip/permute as expected,
  - vision position embedding is written before encoder blocks,
  - connector and embedding byte-size constants match the expected package,
  - runtime config paths remain relative.
  - README caveats for `embedding_bin_path` and `conv_L_cache` stay present.

Excluded:

- `nntr_causallm` factory/CLI/runtime registration for
  `Lfm2VlForConditionalGeneration`.
- In-binary image loading/preprocessing and `image_util.h`.
- Android ARM numerics fixes in shared layers.
- `runFromPixels()`, `getLM()`, and app monolithic path helpers.
- `vision/gguf_to_nntrainer.py` and `vision/naflex_preprocess.py`.
- Tokenizer cache, BPE, WordPiece, and tokenizer_loader changes.
- V-JEPA, Quick.AI/product code, Android app/sample code.

Changed files:

- `Applications/CausalLM/meson.build`
- `Applications/CausalLM/res/lfm2-vl/README.md`
- `Applications/CausalLM/res/lfm2-vl/convert_connector.py`
- `Applications/CausalLM/res/lfm2-vl/convert_embedding.py`
- `Applications/CausalLM/res/lfm2-vl/convert_lm.py`
- `Applications/CausalLM/res/lfm2-vl/convert_vision_hf.py`
- `Applications/CausalLM/res/lfm2-vl/nntr_config.json`
- `test/unittest/models/unittest_lfm2_vl_converters.py`

## TDD Notes

RED:

```text
meson test -C build unittest_lfm2_vl_converters --print-errorlogs
```

Result before adding resources: FAIL. The test reported the missing
`README.md`, `nntr_config.json`, and four converter scripts.

GREEN:

```text
meson test -C build unittest_lfm2_vl_converters --no-rebuild --print-errorlogs
```

Result after adding resources: PASS.

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| Python syntax | `python3 -m py_compile ...` | PASS | four converters plus Python test |
| converter unit test | `meson test -C build unittest_lfm2_vl_converters --no-rebuild --print-errorlogs` | PASS | 1/1 |
| whitespace | `git diff --check 879f04185ed6a0d729f9ad9816e62edf4db4e327..HEAD` | PASS | no output |
| x86 reconfigure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | native x86 |
| x86 build | `ninja -C build` | PASS | full CausalLM build |
| focused unit tests | `meson test -C build unittest_lfm2_vl_converters unittest_lfm2_vl_vision_transformer unittest_causallm_models unittest_chat_template --print-errorlogs` | PASS | 4/4 |
| x86 Gemma4 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | answered Seoul |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-pr-sync-5a-lfm2-vl-converters -Denable-transformer=true` | PASS | generic nntrainer Android package; Meson skips app path |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| LFM2-VL converted package | x86/Android | converter-only PR | not run | SKIP_NOT_AFFECTED | no runtime behavior changed |

## Notes

- This PR intentionally improves reproducibility without claiming LFM2-VL
  runtime readiness.
- A code-quality re-review after the runtime-caveat README/test fix reported no
  Critical/P1/P2 findings.
- The first Android package attempt in this worktree failed because
  `subprojects/iniparser` had not been initialized in the new worktree. After
  `git submodule update --init subprojects/iniparser`, Android package build
  passed. This was a worktree setup issue, not a source issue.
- Device `R3CX80H8Y0F` has `/sdcard/Download/aistudio-mobile/models/lfm2-vl`,
  but actual image runtime remains for later PRs.
- Android package still emits pre-existing C++20-extension warnings from
  `nntrainer/layers/conv2d_layer.cpp` and
  `nntrainer/layers/pooling2d_layer.cpp`.
