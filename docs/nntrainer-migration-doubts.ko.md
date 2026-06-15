# nntrainer Migration Doubts and Questions

Date: 2026-06-11

This document records unresolved policy, design, and verification questions
found during the OLD `quickdotai_api_refact` to nntrainer `main` migration.
It is intentionally separate from implementation reports so it can be reviewed
periodically without blocking unrelated PRs.

## Current Decisions

| Topic | Current working decision |
|---|---|
| Android product code | Do not port to nntrainer main. Android is used for verification only. |
| `quick_dot_ai_api.*` | Keep as Quick.AI product API outside nntrainer. It should call nntrainer through `causal_lm_api`. |
| nntrainer public C API | Preserve and extend `Applications/CausalLM/api/causal_lm_api.*` when a generic API is needed. Generic runtime logic currently embedded in Quick.AI wrappers may move here. |
| Tokenizer cache/BPE/WordPiece/tokenizer_loader | Defer to the final migration stage. |
| x86 runtime | Required for nntrainer PR verification: build with `meson build -Denable-transformer=true`, `ninja -C build`, then run `nntr_causallm` with `/home/jrock/Quick.AI/gemma4_it_x86` and check for coherent output. |
| Android runtime | Run CLI tests on `R3CX80H8Y0F` with models under `/sdcard/Download/aistudio-mobile/models`. |
| Gauss3.8 on `R3CX80H8Y0F` | Record as `SKIP_DEVICE_UNSUPPORTED`, not as a regression. |
| Quick.AI baseline for migration validation | Use `0e25e35d3eae099f318a166fb3aad6fb23d80aa0` when current Quick.AI HEAD has unrelated in-flight work. |
| QNN-enabled Quick.AI build | Non-QNN Android build is required now; QNN build is `BLOCKED_ENV` until a Qualcomm QNN SDK root is available. |

## Open Questions

| ID | Area | Question | Default until answered | Impact |
|---|---|---|---|---|
| Q-001 | API ownership | Should `quick_dot_ai_api.*` be upstreamed into nntrainer? | ANSWERED: no as a product wrapper. Quick.AI keeps `quick_dot_ai_api`, but it must depend on nntrainer through `causal_lm_api`. | Prevents product API upstreaming while preserving Quick.AI integration. |
| Q-002 | Model lifecycle | Should nntrainer expose handle-based parallel model lifecycle, or should that remain entirely in Quick.AI? | ANSWERED: Quick.AI owns the product-facing handles; if a lifecycle feature is generic and required by `causal_lm_api`, extract only that generic part into nntrainer. | Affects streaming/cancel design and future API shape. |
| Q-003 | Streaming API | Should streaming be C callback in `causal_lm_api.*`, C++ `BaseStreamer`, or both? | ANSWERED: implement generic runtime streaming/cancel in CausalLM and expose the product-needed surface through `causal_lm_api`; Quick.AI then consumes it from `quick_dot_ai_api`. | Determines PR-OLD-2 scope and ABI surface. |
| Q-004 | Cancellation | Does main need cancel-before-unload semantics, and what thread-safety contract should be promised? | Implement only cooperative cancellation with explicit lifetime rules; avoid forced async destruction. | Prevents data races and undefined model teardown behavior. |
| Q-005 | Embedding format | Is OLD signed/UINT4 embedding format still required after main's safetensors/Q4_0 work? | Do not port until a required model file fails on main or a fixture proves the gap. | Determines whether PR-OLD-3 is implementation or skip report. |
| Q-006 | FSU/NPU allocator leak | Does OLD `18c116a0` memory leak reproduce on current main with QNN assets? | ANSWERED: required, but split into a separate PR. Reproduce and verify independently. | Becomes a narrow allocator bugfix PR, not part of streaming/chat-template PRs. |
| Q-007 | XGrammar | Should main accept a direct XGrammar dependency, or only a generic logits-mask/provider interface? | Avoid direct dependency; prefer generic provider if needed. | Affects structured-output/tool-calling integration. |
| Q-008 | Multimodal contract | Should OLD `run_image` be discarded in favor of sync's later generic embedding/tensor contract? | Discard OLD `void * + size` hook; revisit with sync follow-up. | Affects LFM2-VL/Gauss multimodal PR shape. |
| Q-009 | Tokenizer cache ownership | Who owns tokenizer cache file format compatibility and fixtures? | Defer; require token-id fixtures and cache compatibility docs before code. | Blocks final tokenizer stage. |
| Q-010 | minja `capitalize` | Should `|capitalize` be fixed by upstream minja PR/submodule bump, local fork commit, or deferred until a model proves it is required? | ANSWERED: make this a minja-only patch. Do not mix it into ChatTemplate array-form support. | Prevents PR-OLD-1 from changing third-party dependency policy. |
| Q-011 | ChatTemplate malformed array | For array-form `chat_template`, should malformed entries throw immediately or be skipped? | Throw on missing/non-string `name` or `template`, matching strict config parsing. | Affects PR-OLD-1 test expectation and HF compatibility. |
| Q-012 | Sync follow-up scope | Which sync-only features should run after OLD-first stage: LFM2, LFM2-VL, V-JEPA, QNN input delivery, generic multimodal? | Keep separate from OLD migration until early OLD PRs land. | Prevents scope creep in OLD PRs. |
| Q-013 | Quick.AI build compatibility | Quick.AI top-level `src/meson.build` assumes sync's `Applications/CausalLM/tokenizers` directory and omits main's `Applications/CausalLM/third_party` include path. Should this be fixed as a Quick.AI main-compat build patch before more nntrainer PR runtime validation? | ANSWERED: nntrainer PRs are verified with nntrainer's own build/runtime first. Quick.AI must also build on the cumulative validation branch, but Quick.AI build gaps are reported separately from standalone nntrainer PR status. | Prevents Quick.AI harness issues from hiding nntrainer PR quality, while keeping final product integration mandatory. |
| Q-014 | Quick.AI API vs main CausalLM hooks | Quick.AI `quick_dot_ai_api.cpp` depends on sync-only hooks such as `getTokenizer`, `getVocabSize`, `initialize(native_lib_dir)`, XGrammar setters, streamer/cancel, multimodal pointer, and embedding dimension helpers. Which of these should become generic nntrainer PRs vs remain Quick.AI-only adaptation? | ANSWERED for OLD stage: streamer/cancel are PR-OLD-2a/2b; XGrammar is represented by generic PR-OLD-5 `LogitsProcessor` plus Quick.AI adapter; `getTokenizer` and `getEmbeddingDim` are PR-OLD-6; `native_lib_dir_` remains Quick.AI adapter state; multimodal remains sync-followup/disabled in baseline validation. | Quick.AI cumulative validation can proceed without importing `quick_dot_ai_api.*` into nntrainer. |
| Q-015 | QNN SDK availability | Should `./build.sh --platform=android --enable-qnn` be a hard gate when no QNN SDK root is configured on the host? | Treat as `BLOCKED_ENV` until the SDK root is supplied. Keep non-QNN Android native build, install, app build/install, and Android CLI runtime as required gates. | Prevents an environment-only failure from blocking OLD CPU/runtime PRs while preserving QNN follow-up. |
| Q-016 | Android direct qwen CLI model path | `quick_dot_ai /sdcard/.../qwen3-0.6b` fails because the device model config points to `/home/jayden/.../tokenizer.json`. Should this block migration? | No for the current validation contract. Required path is `quick_dot_ai_test <model-id> ... <model-base>`, which passed. Track direct CLI model-config cleanup separately. | Avoids misclassifying stale model package metadata as a nntrainer regression. |
| Q-017 | `function_gemma` structured output quality | Android `function_gemma` exits 0 under `quick_dot_ai_test`, but the generated content is odd. Is runtime pass enough? | Record as `PASS_RUNTIME_WITH_QUALITY_NOTE`. Revisit expected schema/tool behavior in a separate Quick.AI/XGrammar quality pass. | Keeps migration progress moving while preserving the quality concern. |
| Q-018 | Quick.AI tool-run handle locking | `runModelHandleWithTool()` originally attached XGrammar/logits processor state before `run_on_handle()` acquired `h.mtx`. Is the validation branch PR-ready? | ANSWERED: fixed in Quick.AI validation commit `5dd7bb5`. `run_on_handle()` now chooses the text model and attaches/detaches scoped grammar state under the run lock. | Removes a same-handle race from the validation patch before using it as the OLD cumulative Quick.AI baseline. |

## Items to Revisit After Each PR

- Did the PR introduce or require a public API change?
- Did Android CLI runtime expose a missing model file or unsupported device case?
- Did any check fail only because Quick.AI validation branch lacks previous PRs?
- Did a feature classified as `already-main` show a real missing behavior under test?
- Did an implementation depend on tokenizer cache/BPE/WordPiece/tokenizer_loader,
  and therefore need to be deferred?
- Did a failed QNN command fail because code regressed, or because
  `qnn-sdk-root` is absent?
- Did a failed Android direct model path use stale absolute host paths while
  the required `quick_dot_ai_test` model-base-path flow still passes?
