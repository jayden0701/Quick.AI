# PR-SYNC-2 Verification Report

Date: 2026-06-12 KST

## Scope

- PR/work unit: LFM2 text CausalLM model
- Worktree: `/home/jrock/nntrainer-pr-sync-2-lfm2-text`
- Branch: `migration/pr-sync-2-lfm2-text`
- Base:
  - `b1674b2eff2daf6d45729e5146ad82579ff154b9 Port LFM2 prerequisite kernels and layers`
- Local commit:
  - `468b1be7a5f82309c144051d4ddddaf9d5ff35b5 causallm: add LFM2 text model`

Source commits represented:

- `a79ad18e causallm: add LFM2 CausalLM model`
- `46cad449 causallm: fix untied LFM2 lm head scope`
- `0e9fcb9b [CausalLM] Update LFM2 config reading`

## Implemented Behavior

- Adds `Lfm2ForCausalLM` text model registration through the existing
  CausalLM model selection path.
- Adds LFM2 model sources and Meson integration:
  - `Applications/CausalLM/models/lfm2/lfm2_causallm.cpp`
  - `Applications/CausalLM/models/lfm2/lfm2_causallm.h`
  - `Applications/CausalLM/models/lfm2/meson.build`
- Adds LFM2 tiny model tests for:
  - hybrid `conv` + `full_attention` graph construction,
  - tied output head sharing from `embedding0`,
  - untied `embedding_layer` + standalone `lm_head`,
  - tied embedding sidecar-path rejection,
  - hybrid KV cache input binding order for leading conv layers,
  - tiny prefill runtime logits,
  - unsupported conv option rejection,
  - `system_prompt.kvcache` rejection for conv-state LFM2 models.
- Extends CausalLM runtime input construction so models can expose only the
  KV-cache layers that actually exist in the compiled graph. The default path
  preserves all-layer behavior for existing decoder-only models; LFM2 overrides
  it to expose only `attention` and `full_attention` layer ids.
- Updates `Applications/CausalLM/jni/Android.mk` so CausalLM NDK app builds see
  LFM2 and the PR-SYNC-1 custom layer sources.
- Registers LFM2 in the CLI, shared C API, and quantizer factory paths.

Excluded:

- LFM2-VL, SigLIP, V-JEPA, or any image/multimodal input path.
- Quick.AI product/API code.
- tokenizer cache/BPE/WordPiece/tokenizer_loader.
- Android product packaging outside nntrainer.

## Review Fixes

Initial review found the following issues, all addressed in the final commit:

- Hybrid LFM2 runtime cache inputs were positionally wrong when conv layers
  preceded attention layers. The fix adds named cache input binding helpers and
  an LFM2 `getKVCacheLayerIds()` override.
- Android CausalLM NDK source list missed LFM2 and PR-SYNC-1 layer sources.
  `Android.mk` now includes the LFM2 directory/source and the
  `causal_conv1d`/`custom_multiply` source files.
- LFM2 embedding setup bypassed the common embedding config helper. It now uses
  `buildEmbeddingLayerProperties(...)` and rejects
  `embedding_file_name + tie_word_embeddings`.
- Parsed but unsupported conv fields are now rejected explicitly:
  `conv_dim_out != hidden_size`, `conv_L_cache != 2`, `conv_bias=true`.
- Tests now include a tiny LFM2 prefill path, not only graph construction.
- The tied LM-head regression test checks the actual `shared_from=embedding0`
  property.
- LFM2 now rejects `system_prompt.kvcache` for conv-layer models until the
  cache format can persist `causal_conv1d` state in addition to attention
  KV-cache tensors.
- LFM2 factory registration was added to `causal_lm_api.cpp` and
  `quantize.cpp`, so model construction is not limited to `nntr_causallm`.
- Android `nntr_quantize` source/include lists were updated with LFM2 and the
  PR-SYNC-1 custom layer sources so the Android quantize target stays aligned
  with `quantize.cpp`.

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| whitespace | `git diff --check b1674b2eff2daf6d45729e5146ad82579ff154b9..HEAD` | PASS | no output |
| unit tests | `meson test -C build unittest_causallm_models unittest_custom_multiply unittest_causal_conv1d_layer --print-errorlogs` | PASS | 3/3 |
| x86 build | `ninja -C build` | PASS | no work after final amend |
| API/quantize build | `ninja -C build Applications/CausalLM/test_api Applications/CausalLM/nntr_quantize Applications/CausalLM/nntr_causallm` | PASS | validates API and quantizer registration compile paths |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent Seoul answer |
| shared build | `ninja -C build-shared-pr-sync-2 Applications/CausalLM/nntr_causallm Applications/CausalLM/unittest_causallm_models Applications/CausalLM/test_api Applications/CausalLM/nntr_quantize` | PASS | shared `libcausallm.so`, API, quantizer paths |
| shared unit test | `meson test -C build-shared-pr-sync-2 unittest_causallm_models --print-errorlogs` | PASS | 1/1 after rerun; first post-commit run hit non-LFM2 temporary EmbeddingGemma fixture file miss, then standalone and full rerun passed |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-pr-sync-2-lfm2-text -Denable-transformer=true` | PASS | generic nntrainer Android package; app skipped by existing Meson path |
| CausalLM NDK app | `ndk-build -C Applications/CausalLM/jni` | BLOCKED_ENV | missing `Applications/CausalLM/lib/libtokenizers_android_c.a`, before source compile |
| NDK syntax probe | NDK clang `-fsyntax-only` for `main.cpp`, `api/causal_lm_api.cpp`, `quantize.cpp`, `lfm2_causallm.cpp`, `causal_conv1d_layer.cpp`, `custom_multiply.cpp` | PASS | validates LFM2 Android include/source wiring despite tokenizer archive blocker |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| tiny LFM2 fixture | x86 unit test | generated test fixtures | `unittest_causallm_models` | PASS | constructs graph and runs prefill logits |
| LFM2 text converted model | x86/Android | not found locally | not run | PENDING_MODEL_FILES | see missing files |

## Missing Model Files

| Model | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| LFM2 text | local x86 or `/sdcard/Download/aistudio-mobile/models` converted package | `config.json`, `generation_config.json`, `nntr_config.json`, tokenizer, weights for a text-only LFM2 package | user/device follow-up after converted files are available |

The local tree only exposed an LFM2-VL nntrainer config under
`Applications/CausalLM/res/lfm2-vl/nntr_config.json`; no text-only converted
LFM2 runtime package was found during this work unit.

## TDD / Mutation Notes

- A tied/untied output-head test was added after spec review found the original
  type-only assertion too weak.
- The KV-cache binding helper was introduced behind a test that first failed to
  compile without the helper API.
- Unsupported conv option tests failed before the validation checks were added.
- The tied sidecar-path test was strengthened with message matching after a
  mutation showed a weaker `EXPECT_THROW` could pass on a lower-level layer
  property failure instead of the intended explicit LFM2 guard.
- `RejectsSystemPromptKVCacheWithConvLayers` was added before the guard and
  failed with `Expected LFM2 system_prompt.kvcache to be rejected`; after the
  guard, `unittest_causallm_models` passed.

## Debugging Notes

- Direct CausalLM NDK app build remains blocked by a pre-existing missing
  tokenizer static archive:

```text
Android NDK: ERROR: ... tokenizers_c: LOCAL_SRC_FILES points to a missing file
Android NDK: Check that .../Applications/CausalLM/jni/../lib/libtokenizers_android_c.a exists
```

- This blocker occurs before compiling `main.cpp` or LFM2 sources, so it does
  not invalidate the `Android.mk` LFM2 include/source fix. The relevant NDK
  compile fronts were checked separately with `-fsyntax-only`.
- `./tools/package_android.sh` is still a valid nntrainer Android package
  verification for this PR because Meson currently skips app builds on Android.

## Review Status

- Spec re-review after final fixes: compliant.
- Code-quality re-review after final fixes: ready to merge.
