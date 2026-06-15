# PR-OLD-6 Verification Report

Date: 2026-06-11

## Scope

- PR/work unit: CausalLM tokenizer and embedding-dimension accessors required
  by Quick.AI validation
- Worktree: `/home/jrock/nntrainer-pr-old-causallm-accessors`
- Branch: `migration/pr-old-causallm-accessors`
- Local commit:
  - `2f4838e0 causallm: expose model tokenizer and embedding dimension`
- Files changed:
  - `Applications/CausalLM/models/transformer.h`
  - `Applications/CausalLM/models/sentence_transformer.h`
  - `test/unittest/models/unittest_causallm_qwen2.cpp`

## Implemented Behavior

- Adds `Transformer::getTokenizer()` returning the owned tokenizer pointer.
- Adds `SentenceTransformer::getEmbeddingDim()` returning `DIM`.
- Adds focused tests that:
  - a tiny Qwen2 CausalLM exposes its tokenizer,
  - a tiny Qwen2.5 embedding model reports the expected embedding dimension.

Excluded:

- tokenizer cache/BPE/WordPiece/tokenizer_loader
- tokenizer ownership transfer
- Quick.AI product API
- multimodal embedding/tensor contract

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| submodule setup | `git submodule update --init --recursive Applications/CausalLM/third_party/minja` | PASS | minja available |
| nntrainer x86 configure | `meson setup build -Denable-transformer=true` | PASS | `build/meson-logs/meson-log.txt` |
| nntrainer x86 build | `ninja -C build` | PASS | full build completed |
| focused unit | `meson test -C build unittest_causallm_models --print-errorlogs` | PASS | accessor tests included |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output answered Seoul |
| whitespace | `git diff --check` | PASS | no output |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent answer: Seoul |
| Quick.AI cumulative | x86/Android | validation worktree | see `old-cumulative-validation.ko.md` | PASS_WITH_ENV_GAP | integration needs these accessors; QNN SDK remains environment-blocked |

## Missing Model Files

No additional model files were required for this accessor-only PR.

## Debugging Notes

- The first test attempted to assert that `getTokenizer()` returns null before
  initialization. That expectation was wrong for current main: the constructor
  path creates the tokenizer during parameter setup. The final test only asserts
  that the tokenizer is present.
- This PR intentionally avoids `getVocabSize()` because that belongs to
  PR-OLD-5 logits processor integration.
