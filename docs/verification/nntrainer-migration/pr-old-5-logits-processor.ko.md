# PR-OLD-5 Verification Report

Date: 2026-06-11

## Scope

- PR/work unit: generic CausalLM logits processor hook
- Worktree: `/home/jrock/nntrainer-pr-old-logits-processor`
- Branch: `migration/pr-old-logits-processor`
- Local commit:
  - `5ea1917c feat: add generic CausalLM logits processor hook`
- Source commit consulted:
  - OLD `5b19bf0c feat(xgrammar): Add XGrammar virtual methods and getVocabSize to Transformer base class`
- Files changed:
  - `Applications/CausalLM/models/transformer.h`
  - `Applications/CausalLM/models/causal_lm.h`
  - `Applications/CausalLM/models/causal_lm.cpp`
  - `test/unittest/models/unittest_causallm_qwen2.cpp`

## Implemented Behavior

- Adds product-neutral `causallm::LogitsProcessor` interface.
- Adds `Transformer::getVocabSize()`.
- Adds virtual no-op `setLogitsProcessor()` and `resetLogitsProcessor()` on
  `Transformer`.
- Implements logits processor attachment/reset in `CausalLM`.
- Invokes `process(logits, vocab_size, batch_index)` after repetition penalty
  and bad-word masking, before greedy or sampling token selection.
- Invokes `acceptToken(token_id, batch_index)` after token selection.
- Keeps XGrammar compiler/cache/matcher dependencies outside nntrainer; Quick.AI
  adapts XGrammar to this generic hook.

Excluded:

- direct XGrammar dependency in nntrainer
- Quick.AI product API
- tokenizer cache/BPE/WordPiece/tokenizer_loader
- multimodal hooks

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 configure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | cumulative validation |
| nntrainer x86 build | `ninja -C build` | PASS | cumulative validation |
| focused unit | `meson test -C build unittest_causallm_models --print-errorlogs` | PASS | cumulative validation |
| cumulative focused units | `meson test -C build unittest_chat_template unittest_callback_streamer unittest_cancel_api unittest_embedding_sidecar_lut unittest_memory_pool unittest_causallm_models --print-errorlogs` | PASS | cumulative validation |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output answered Seoul |
| Quick.AI Android JSON/tool smoke | `quick_dot_ai_test ...` on `R3CX80H8Y0F` | PASS_RUNTIME | cumulative validation |
| whitespace | `git diff --check f2c0e6ae30ceb1ff418064f02b2f95804f294d24..HEAD` | PASS | cumulative validation |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | regression smoke |
| `qwen3-0.6b` | Android Quick.AI test CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models` | `quick_dot_ai_test qwen3-0.6b 'What is the capital of Korea?' true W4A32 false ...` | PASS | output was bounded JSON after Quick.AI schema fix |
| `function_gemma` | Android Quick.AI test CLI | same | `quick_dot_ai_test function_gemma 'Return a JSON object.' true W4A32 false ...` | PASS_RUNTIME_WITH_QUALITY_NOTE | exit 0, but output content is odd |

## Missing Model Files

No model file is missing for the x86 Gemma4 regression. Android direct
`qwen3-0.6b` CLI still has a stale absolute tokenizer path in the device model
config, but the required `quick_dot_ai_test` model-base-path flow passed.

## Debugging Notes

- The hook is non-owning; Quick.AI must keep its XGrammar adapter alive for the
  duration of generation.
- The processor is called after bad-word masking. This allows a structured
  decoder to force a token even if another processor had previously favored a
  different token, and the unit test covers a forced bad-word id.
- Quick.AI validation initially reset the processor immediately before run,
  which disabled structured decoding. The validation branch removes that reset.
- Quick.AI validation requests cooperative stop when XGrammar says generation
  is completed or terminated, preventing matcher calls after completion.
