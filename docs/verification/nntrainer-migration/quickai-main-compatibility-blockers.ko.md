# Quick.AI Main nntrainer Compatibility Blockers

Date: 2026-06-11

Purpose: record blockers found while trying to build Quick.AI against a
main-based nntrainer validation worktree. These are not PR-OLD-1 defects.

Validation setup:

```text
Quick.AI temp worktree: /home/jrock/Quick.AI-pr-old-1-validation
nntrainer symlink:      /home/jrock/nntrainer-migration-validation
xgrammar symlink:       /home/jrock/Quick.AI/xgrammar
```

Command:

```bash
./build.sh --platform=x86 --target=src,api,api-test --clean
```

## Blocker 1: `CausalLM/tokenizers` Directory Assumption

Quick.AI `src/meson.build` assumes this sync-era path exists:

```text
nntrainer/Applications/CausalLM/tokenizers
```

Current main-based nntrainer does not have that directory. Meson fails because
`find` receives a non-existent path with `check: true`.

Temporary validation-only patch:

- Build the `find` root list dynamically.
- Add `CausalLM/tokenizers` only if the directory exists.

Recommended real fix:

- Make Quick.AI source discovery tolerate main's CausalLM layout.
- Do not add an empty directory to nntrainer just to satisfy Quick.AI.

## Blocker 2: Missing `CausalLM/third_party` Include Path

Current main uses minja as a submodule. Its headers include:

```cpp
#include <nlohmann/json.hpp>
```

The header exists under:

```text
nntrainer/Applications/CausalLM/third_party/nlohmann/json.hpp
```

Quick.AI top-level build only added:

```text
Applications/CausalLM/third_party/minja/include
```

Temporary validation-only patch:

- Add `Applications/CausalLM/third_party` to top-level CausalLM include args.

Recommended real fix:

- Mirror nntrainer main's CausalLM include directories when Quick.AI compiles
  CausalLM sources directly.

## Blocker 3: Quick.AI API Depends on sync-only CausalLM Hooks

After the two temporary build-harness patches, `chat_template.cpp` compiled.
The next failure was `api/quick_dot_ai_api.cpp` against main CausalLM headers.

Missing hooks observed:

```text
causallm::multimodal_pointer
Transformer::getTokenizer()
Transformer::getVocabSize()
Transformer::initialize(std::string native_lib_dir)
Transformer::getOutput()
Transformer::setXGrammar()
Transformer::resetXGrammar()
Transformer::embeddingBytesPerToken()
Transformer::setStreamer()
Transformer::requestStop()
SentenceTransformer::getEmbeddingDim()
```

Classification:

| Missing hook | Likely owner |
|---|---|
| `setStreamer`, `requestStop`, `getOutput` through generic base access | PR-OLD-2 streaming/cancel or adjacent generic CausalLM API |
| `getTokenizer`, `getVocabSize` | XGrammar/logits-mask design, avoid direct product coupling |
| `setXGrammar`, `resetXGrammar` | Generic logits-mask/provider interface, not direct XGrammar dependency in nntrainer main |
| `multimodal_pointer`, `embeddingBytesPerToken` | sync-followup generic multimodal contract |
| `initialize(native_lib_dir)` | QNN/native-lib delivery design, likely sync-followup |
| `SentenceTransformer::getEmbeddingDim` | embedding API design, probably separate narrow PR |

Recommended migration handling:

- Do not upstream `quick_dot_ai_api.*` as-is.
- Implement only generic nntrainer hooks in small PRs.
- Keep Quick.AI product orchestration in the top-level repo.
- Keep Android CLI runtime blocked until the generic hooks needed by Quick.AI
  are implemented or Quick.AI is adapted away from sync-only assumptions.

## Impact on PR-OLD-1

PR-OLD-1 standalone verification passes. Quick.AI runtime verification for
PR-OLD-1 cannot complete yet because Quick.AI does not currently build against
main-based nntrainer. This is expected to improve as PR-OLD-2 and later generic
hook PRs land in the validation branch.
