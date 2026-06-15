# PR-OLD-3 Verification Report

Date: 2026-06-11

## Scope

- PR/work unit: CausalLM EmbeddingLayer sidecar LUT support
- Worktree: `/home/jrock/nntrainer-pr-old-embedding-sidecar`
- Branch: `migration/pr-old-embedding-sidecar`
- Local commits:
  - `458b0c8d causallm: support embedding sidecar LUT`
  - `9fa9514b causallm: pass embedding sidecar config into graphs`
  - `311cc4fb causallm: resolve sidecar config paths from model dir`
- Source commits consulted:
  - OLD `76dc25a6`: UINT4 quantized embedding sidecar path
  - OLD `e0a68c75`: raw UINT16 sidecar path
  - OLD `b64ac377`: signed 4-bit sidecar support
  - OLD `e9692a60`: OLD merge result
- Files changed:
  - `Applications/CausalLM/layers/embedding_layer.{h,cpp}`
  - `Applications/CausalLM/models/transformer.*`
  - model/config wiring under CausalLM model construction
  - focused sidecar unit tests

## Implemented Behavior

- Adds `quantized_lut_path` sidecar property to CausalLM embedding.
- Supports raw UINT16 sidecar embedding rows.
- Supports JSON manifest based packed unsigned 4-bit `ufixed8` LUT.
- Supports JSON manifest based packed signed 4-bit `sfixed4` LUT.
- Supports output requantization via sidecar scale/offset metadata where
  required by the model configuration.
- Resolves sidecar paths relative to the model/config directory so Android
  model packages are not forced to use host absolute paths.
- Keeps generic UINT4/QINT4 tensor dtype migration out of scope because current
  main already has the tensor dtype layer needed for standard weight loading.

Excluded:

- tokenizer cache, BPE, WordPiece, tokenizer_loader
- Quick.AI product API
- Android app/service/AAR code
- global layer-name lowercasing policy changes

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 configure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | cumulative validation |
| nntrainer x86 build | `ninja -C build` | PASS | cumulative validation |
| focused unit | `meson test -C build unittest_embedding_sidecar_lut --print-errorlogs` | PASS | cumulative validation |
| CausalLM model regression | `meson test -C build unittest_causallm_models --print-errorlogs` | PASS | cumulative validation |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output answered Seoul |
| whitespace | `git diff --check f2c0e6ae30ceb1ff418064f02b2f95804f294d24..HEAD` | PASS | cumulative validation |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | regression smoke; package uses managed `Q6_K` embedding, not sidecar |
| sidecar LUT fixtures | x86 unit | generated temporary files | `unittest_embedding_sidecar_lut` | PASS | covers raw UINT16, unsigned 4-bit, signed 4-bit, and invalid manifest cases |
| Android QNN Gemma sidecar models | Android CLI | `/sdcard/Download/aistudio-mobile/models` | cumulative Quick.AI smoke | PARTIAL | Android `quick_dot_ai_test` CPU models passed; QNN build blocked by SDK root |

## Missing Model Files and Environment Gaps

| Model/env | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| QNN sidecar model full device run | `/sdcard/Download/aistudio-mobile/models` plus QNN SDK build support | QNN SDK root not configured on host | provide QNN SDK path, then rerun affected QNN model |

## Debugging Notes

- The original OLD title made this look like a broad UINT4/signed dtype port.
  Main already has the generic tensor dtype machinery, so the PR boundary was
  narrowed to sidecar LUT loading and graph/config wiring.
- Path resolution was kept in nntrainer CausalLM model setup because Android
  model packages must be relocatable under
  `/sdcard/Download/aistudio-mobile/models`.
- Full device QNN validation remains environment-blocked, not code-failed.
