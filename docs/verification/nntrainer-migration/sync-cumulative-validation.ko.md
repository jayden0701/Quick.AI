# Sync Cumulative Validation Report

Date: 2026-06-12 KST

## Scope

- Worktree: `/home/jrock/nntrainer-sync-cumulative`
- Branch: `migration/sync-cumulative-validation`
- Base cumulative commit:
  - `50f90a26378395942fa0db920476d3c1b1bf0536 test: cover CausalLM accessor hooks`
- Current cumulative commit:
  - `64aa176a6a4bfe575295cd8988914167454d4bce causallm: add LFM2-VL conversion resources`

This branch is the integration base for sync-series PRs after PR-OLD-1..6 and
PR-SYNC-1..5a.

Included sync PRs:

- PR-SYNC-1:
  `b1674b2eff2daf6d45729e5146ad82579ff154b9 Port LFM2 prerequisite kernels and layers`
- PR-SYNC-2:
  `baff1b6f5f9f2b0808386625149ee0f1df353125 causallm: add LFM2 text model`
- PR-SYNC-3:
  `1651f66d834ed86ec4163b312847b40386f928d2 causallm: add multimodal foundation hooks`
- PR-SYNC-4:
  `b24b3a556901b919dfa4a3327a8b86fced303281 causallm: add LFM2-VL vision tower`
- PR-SYNC-5a:
  `64aa176a6a4bfe575295cd8988914167454d4bce causallm: add LFM2-VL conversion resources`

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| x86 reconfigure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | native x86 |
| x86 build | `ninja -C build` | PASS | full build |
| whitespace | `git diff --check 50f90a26378395942fa0db920476d3c1b1bf0536..HEAD` | PASS | no output |
| unit tests | `meson test -C build unittest_lfm2_vl_converters unittest_lfm2_vl_vision_transformer unittest_causallm_models unittest_causal_conv1d_layer unittest_custom_multiply unittest_nntrainer_cpu_backend unittest_cancel_api unittest_chat_template --print-errorlogs` | PASS | 8/8 |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | answered Seoul |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-sync-cumulative -Denable-transformer=true` | PASS | generic nntrainer Android package; Meson skips app path |
| NDK syntax probe | NDK clang `-fsyntax-only` for PR-SYNC-4 LFM2-VL connector/model/vision sources | PASS | validates new CausalLM sources against Android headers |
| CausalLM NDK app | `ndk-build -C Applications/CausalLM/jni APP_ABI=arm64-v8a` | BLOCKED_ENV | missing `Applications/CausalLM/lib/libtokenizers_android_c.a`, before source compile |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| LFM2-VL structural fixture | x86 unit test | generated test fixtures | `unittest_lfm2_vl_vision_transformer` | PASS | 22 tests cover ViT config/file validation, connector shape checks, prompt merge guards |
| LFM2-VL converter resources | x86 unit test | source fixtures | `unittest_lfm2_vl_converters` | PASS | verifies converter files, tensor ordering, relative config paths, and current runtime caveats |
| LFM2-VL converted model | Android | `/sdcard/Download/aistudio-mobile/models/lfm2-vl` on `R3CX80H8Y0F` | not run | DEFERRED_TO_NEXT_PR | model package exists, but image CLI/factory/preprocessing path is not part of PR-SYNC-4/5a |

## Android Model Inventory Update

`R3CX80H8Y0F` currently has an LFM2-VL package:

```text
/sdcard/Download/aistudio-mobile/models/lfm2-vl/config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/generation_config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_connector.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_embedding.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_lm.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_vision.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/nntr_config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/sample.png
/sdcard/Download/aistudio-mobile/models/lfm2-vl/special_tokens_map.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/tokenizer.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/tokenizer_config.json
```

This inventory is enough to plan the next LFM2-VL runtime PR, but not enough
to count the PR-SYNC-4/5a cumulative state as Android runtime-passed: these
PRs deliberately exclude `main.cpp`/factory image-file CLI registration and
image preprocessing artifacts, and direct CausalLM app build is blocked by the
missing tokenizer archive before the new sources compile.

## Notes

- This branch now contains the completed OLD cumulative base plus PR-SYNC-1,
  PR-SYNC-2, PR-SYNC-3, PR-SYNC-4, and PR-SYNC-5a.
- LFM2 text converted model runtime remains `PENDING_MODEL_FILES` as recorded in
  `pr-sync-2-lfm2-text.ko.md`.
- LFM2 `use_embedding` / LFM2-VL runtime remains `DEFERRED_TO_NEXT_PR` as
  recorded in `pr-sync-3-multimodal-foundation.ko.md` and
  `pr-sync-4-lfm2-vl.ko.md`.
- PR-SYNC-5a adds converter/resource reproducibility only. It documents that
  `embedding_bin_path` model-dir-relative resolution and `conv_L_cache == 3`
  compatibility remain runtime-wiring follow-ups.
- Direct CausalLM NDK app build remains separately blocked by missing
  `Applications/CausalLM/lib/libtokenizers_android_c.a`; generic nntrainer
  Android package build passed.
- PR-SYNC-3's graph-wide QNN activation allocator route is accepted as
  source-parity behavior for this PR, but mixed CPU/QNN E2E remains a required
  later validation item.
- Android package warnings remain pre-existing C++20-extension warnings from
  `nntrainer/layers/conv2d_layer.cpp` and
  `nntrainer/layers/pooling2d_layer.cpp`.
