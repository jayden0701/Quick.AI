# Phase 0 Android Model Availability Report

Date: 2026-06-11

Device: `R3CX80H8Y0F`

Model base path:

```text
/sdcard/Download/aistudio-mobile/models
```

Command:

```bash
adb -s R3CX80H8Y0F shell \
  "find /sdcard/Download/aistudio-mobile/models -maxdepth 3 -type f \
   \( -name config.json -o -name generation_config.json -o -name nntr_config.json -o -name tokenizer.json -o -name '*.bin' -o -name '*.serialized' -o -name '*.so' \) \
   2>/dev/null | sort"
```

## Available Model Directories

```text
function_gemma
gauss-3.6-qnn
gauss-3.8-qnn
gauss-3.8-vencoder-qnn
gauss-3.8-vit-qnn
gemma-4-E2B-it
gemma-4-e2b-qnn
gemma4_cpu
gemma4_it_x86
lfm2-vl
lfm2-vl-split-bak
qwen3-0.6b
tiny_bert
```

## Runtime Policy

- x86 is build-only for this migration.
- Runtime model checks run through Android CLI on `R3CX80H8Y0F`.
- Gauss3.8-family models are known unsupported on this device and should be
  recorded as `SKIP_DEVICE_UNSUPPORTED`, not as regressions.
- If a PR affects a model family whose files are absent from the model base
  path, record `PENDING_MODEL_FILES` in that PR verification report.

## High-Priority Runtime Smoke Targets

| Model id / directory | Purpose | Status |
|---|---|---|
| `qwen3-0.6b` | CPU CausalLM baseline, chat/template/tool regression | available |
| `function_gemma` | FunctionGemma/chat-template regression | available |
| `gemma4_cpu` | CPU Gemma4 regression | available |
| `gemma-4-e2b-qnn` | QNN Gemma4 regression | available |
| `lfm2-vl` | LFM2-VL monolithic/siglip path | available |
| `tiny_bert` | tokenizer/WordPiece final phase | available |
| `gauss-3.8-qnn` | Gauss3.8 QNN | `SKIP_DEVICE_UNSUPPORTED` |
| `gauss-3.8-vencoder-qnn` | Gauss3.8 vision encoder | `SKIP_DEVICE_UNSUPPORTED` |
| `gauss-3.8-vit-qnn` | Gauss3.8 ViT | `SKIP_DEVICE_UNSUPPORTED` |

## Observed Files

Representative files found:

```text
/sdcard/Download/aistudio-mobile/models/function_gemma/config.json
/sdcard/Download/aistudio-mobile/models/function_gemma/generation_config.json
/sdcard/Download/aistudio-mobile/models/function_gemma/nntr_config.json
/sdcard/Download/aistudio-mobile/models/function_gemma/nntr_gemma3_270m_q40_embdfp32_ARM.bin
/sdcard/Download/aistudio-mobile/models/function_gemma/tokenizer.json
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/config.json
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/gemma-4-E2B-it-textonly-untied-foldedple-merged-quantized.serialized.bin
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/gemma_4_E2B_embed_4bit_vocabwise_quantized.bin
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/gemma_4_E2B_ple_4bit_vocabwise_quantized.bin
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/generation_config.json
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/nntr_config.json
/sdcard/Download/aistudio-mobile/models/gemma-4-e2b-qnn/tokenizer.json
/sdcard/Download/aistudio-mobile/models/gemma4_cpu/config.json
/sdcard/Download/aistudio-mobile/models/gemma4_cpu/generation_config.json
/sdcard/Download/aistudio-mobile/models/gemma4_cpu/nntr_config.json
/sdcard/Download/aistudio-mobile/models/gemma4_cpu/nntr_gemma4_q40_embdq6k_ARM.bin
/sdcard/Download/aistudio-mobile/models/gemma4_cpu/tokenizer.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/generation_config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_connector.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_embedding.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_lm.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_vision.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/nntr_config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/tokenizer.json
/sdcard/Download/aistudio-mobile/models/qwen3-0.6b/config.json
/sdcard/Download/aistudio-mobile/models/qwen3-0.6b/generation_config.json
/sdcard/Download/aistudio-mobile/models/qwen3-0.6b/nntr_config.json
/sdcard/Download/aistudio-mobile/models/qwen3-0.6b/nntr_qwen3_0.6b_q40_embdq6k.bin
/sdcard/Download/aistudio-mobile/models/qwen3-0.6b/tokenizer.json
/sdcard/Download/aistudio-mobile/models/tiny_bert/config.json
/sdcard/Download/aistudio-mobile/models/tiny_bert/generation_config.json
/sdcard/Download/aistudio-mobile/models/tiny_bert/nntr_config.json
/sdcard/Download/aistudio-mobile/models/tiny_bert/tinybert_q40_arm.bin
/sdcard/Download/aistudio-mobile/models/tiny_bert/tokenizer.json
```
