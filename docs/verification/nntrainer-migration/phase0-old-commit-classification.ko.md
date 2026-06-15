# Phase 0 OLD Commit Classification Report

Date: 2026-06-11

Source:

```text
OLD git:    /home/jrock/Quick.AI/nntrainer/OLD_NNTRAINER/.git
OLD branch: quickdotai_api_refact
OLD HEAD:   e9692a609d5d652cce04e8c2f8d486c785a0bb1f
OLD range:  dcacb60c3f5a2e8d6037309f4c0e9de156010f92^..quickdotai_api_refact
Main base:  f2c0e6ae30ceb1ff418064f02b2f95804f294d24
```

## Summary

`quickdotai_api_refact` 범위에는 143개 커밋이 있다. 대부분은 Android
앱/service, QuickDotAI AAR, SampleTestAPP, prebuilt binary, Gradle/toolchain
수정, model-list UI wiring, Quick.AI 제품 API surface에 해당한다.

이 범위는 nntrainer main에 그대로 replay하면 안 된다. 특히
`quick_dot_ai_api.*`는 nntrainer main으로 가져오지 않고,
`Applications/CausalLM/api/causal_lm_api.*`를 main의 공개 API 기준으로
유지한다. QuickDotAI 커밋 안에 숨어 있는 runtime 아이디어는 generic
CausalLM hook으로만 재설계 후보가 될 수 있다.

Tokenizer cache, BPE, WordPiece, tokenizer_loader는 실제 후보 기능이지만
검증이 덜 되었으므로 migration 마지막 단계로 보류한다.

## Classification

| OLD commit(s) / group | Classification | Proposal |
|---|---:|---|
| `dcacb60c`, `6f4d73e6` Android project, LauncherApp, QuickAIService, ClientApp | `product-only` | 제외. Android product scaffold는 nntrainer main 범위가 아니다. |
| `8efcc8b6`, `c24b2eb6`, `c64c1cd4`, `f5ad2831`, `fdb72047` request docs, LiteRT guide, Gemma path, reference diff | `obsolete` | 감사 자료로만 유지. PR로 이식하지 않는다. |
| `dbfdd69a` REST service/JNI/product worker side | `product-only` | REST service, model registry, HTTP protocol, Kotlin/JNI product glue 제외. |
| Hidden part of `dbfdd69a`: handle-based CausalLM model state | `product-owned` | Quick.AI handle lifecycle은 Quick.AI validation patch에서만 유지한다. nntrainer main에는 아직 generic handle lifecycle PR을 만들지 않는다. |
| `81b303e4`, `1752be26` streamer/callback C API and CausalLM streaming hook | `completed` | PR-OLD-2a로 generic streamer/callback만 포트했다. QuickAI JNI 제외. |
| `fd23f1f7` NDJSON HTTP streaming in LauncherApp | `product-only` | Product REST behavior이므로 제외. |
| `f670a69c` disables built-in Qwen3 config registration | `obsolete` | main의 built-in config registration과 config loading 구조를 유지한다. |
| `b7a6eb99`, `f2778ab4`, `61f2952e`, `b6cff7ed`, `a647b8a9`, `ce33473d` prebuilt native libs/AAR blobs | `product-only` | binary artifact 전부 제외. |
| Gradle/toolchain/build fixes: `af6275db`, `f10db298`, `a4c08be6`, `c09bc1e1`, `dd909985`, `7a6c00ab`, `25c40232`, `fffa6f80`, `60b8ca49`, `24afa4f2`, `7e4d9f27`, `63a22a2e`, `c5eaddc6`, `4b730832`, `1230d740`, `8b8bea8a`, `c5a98fe1`, `087c84eb`, `a52bc6b7`, `9635f18d` | `product-only` | Android/product packaging과 build plumbing이므로 제외. |
| `2c9ee80d`, `0fecf066`, `2fd800f3`, `ab2525ab`, `5e12b108`, `4f7c5607` QuickDotAI API switch/refactor | `product-only` | `quick_dot_ai_api.*`는 포트하지 않는다. 숨은 runtime hook만 별도 후보로 분리한다. |
| Hidden in QuickDotAI API: unload/destroy/cancel, per-handle metrics, dynamic model path, message input, QNN KV cache | `product-owned` | Quick.AI validation patch에서 main 기반 nntrainer와 맞췄다. nntrainer OLD PR로는 streamer/cancel/logits/accessor만 추출했다. |
| `61f1eb28`, `e86c3775`, `059b2f91`, `5bcbebd4` AAR/SampleTestAPP/Android QuickDotAI movement | `product-only` | 제외. `5bcbebd4`는 Android QuickDotAI가 top-level Quick.AI로 이동했음을 강화한다. |
| Chat/session wrapper series `5c208165` through `1e6b7028`, plus UI fixes `edf8669b`, `990db7db`, `dde7d889`, `097d20f0`, `51ae989f`, `b800c23a` | `product-only` | Kotlin/API/UI session wrapper 제외. runtime KV-cache lifecycle은 필요 시 generic 설계 대상. |
| `61378fb2`, `faeef588`, `c9d04158`, `280bf472` multimodal AAR/image picker/app routing | `product-only` | Android/AAR/SampleTestAPP image API 제외. |
| `7a324559`, `d602b287`, `f10c45e9`, `bde2f051` OLD multimodal/runtime `run_image`/multi-model ideas | `sync-followup` | OLD의 `void * + size` hook은 포트하지 않는다. 이후 sync generic multimodal contract를 참고한다. |
| `67a2836c`, `b45774ad`, `dfac73bb` cancel while run / cancel before unload | `completed` | PR-OLD-2b로 CausalLM cooperative cancellation/runtime hook만 포트했다. Android/JNI wrapper 제외. |
| `b191b778`, `7e385c07`, `3fb02bd6`, `cfd37ba1`, `67589c58`, `4537b7de`, `2a8d2b47` ThreadManager, OpenMP replacement, TinyBERT/CPU model pieces | `already-main` | main에 ThreadManager와 TinyBERT/BERT 관련 지원이 이미 있다. OLD broad commit replay 금지. |
| `8dbe0dae` allocator abstraction | `already-main` | main의 `MemAllocator`/vendor allocator 구조가 더 최신이다. OLD `CpuMemAllocator`/`RpcMemAllocator` 형태를 직접 포트하지 않는다. |
| `18c116a0` FSU/NPU memory leak via allocator integration | `completed` | PR-OLD-4로 main allocator 구조에 맞춰 QNN planned-offset allocation/ownership bugfix만 포트했다. |
| `76dc25a6`, `e0a68c75`, `a935a220`, `b64ac377`, `e9692a60` UINT4/signed 4-bit embedding | `completed` | PR-OLD-3로 broad dtype port 대신 CausalLM EmbeddingLayer sidecar LUT support로 좁혀 포트했다. |
| `f31096a2`, `d64ce8c8`, `1c22c29e`, `61049fb5`, `3069e7b4`, `9c19da17` tokenizer cache, WordPiece, BPE, tokenizer_loader | `defer-tokenizer` | 최종 단계로 보류. cache, WordPiece, BPE, loader integration으로 쪼갠다. |
| `5f0a53a6`, `dd83ccab`, `10c4feac` broad HF chat template/minja/Gemma4/FunctionGemma | `already-main` | main에 더 정리된 chat-template/minja/Gemma4 작업이 있다. OLD minja header vendor 금지. |
| `2639f431` array-form `chat_template` support | `completed` | PR-OLD-1로 작은 missing delta만 포트했다. |
| `5b19bf0c` XGrammar methods and `getVocabSize()` | `completed` | PR-OLD-5로 direct XGrammar dependency 없이 generic logits processor/provider hook만 포트했다. |
| `d401da59` Android install script NDK preflight | `optional-helper` | core runtime migration PR이 아니다. install failure가 재발할 때만 별도 helper patch로 검토한다. |
| `93adaf0c`, `77acac01`, `df63ccbe`, `9a08cb08`, `9b0123c5`, `20488371`, UI merge/fix commits `da2bdddd` through `9986442f` | `obsolete` | temporary, failed, conflict/UI/build-fix churn. runtime idea는 위 항목에서만 다룬다. |

## Completed OLD PR Status

The first OLD migration pass has progressed beyond the initial proposal below.
Current completed branches are:

| PR | Branch | Status | Notes |
|---|---|---|---|
| PR-OLD-1 | `migration/pr-old-chattemplate` | DONE | `2639f431` array-form `chat_template` delta ported. |
| PR-OLD-2a | `migration/pr-old-streaming` | DONE | generic streaming callback surface ported without Quick.AI product API. |
| PR-OLD-2b | `migration/pr-old-cancel` | DONE | cooperative cancel API/lifetime guard ported. |
| PR-OLD-3 | `migration/pr-old-embedding-sidecar` | DONE | OLD UINT4/signed/raw sidecar need narrowed to CausalLM embedding sidecar LUT support. |
| PR-OLD-4 | `migration/pr-old-qnn-memory` | DONE | `18c116a0` memory ownership issue narrowed to QNN planned-offset allocation. |
| PR-OLD-5 | `migration/pr-old-logits-processor` | DONE | `5b19bf0c` XGrammar need represented as generic logits processor hook. |
| PR-OLD-6 | `migration/pr-old-causallm-accessors` | DONE | narrow `getTokenizer()` and `getEmbeddingDim()` accessors added for Quick.AI integration. |

The cumulative validation branch is:

```text
/home/jrock/nntrainer-old-cumulative
migration/old-cumulative-validation
HEAD 50f90a26 test: cover CausalLM accessor hooks
```

Quick.AI integration was validated from:

```text
/home/jrock/Quick.AI-old-cumulative-validation
validation/old-cumulative-quickai
HEAD 5dd7bb5 validation: serialize tool logits processor setup
previous 30f4a4a validation: adapt QuickAI to main-based nntrainer
```

Remaining OLD-derived candidates before moving to `sync_v0.4.0`:

- Tokenizer cache / WordPiece / BPE / tokenizer_loader: intentionally deferred
  to the final tokenizer stage.
- `d401da59` install script NDK preflight: optional helper only; do not block
  runtime migration unless install failures reappear.
- OLD multimodal `void * + size` API ideas: not ported; superseded by later
  sync multimodal contract work.
- Product/API/Android packaging commits: excluded by policy.

## Historical First PR Proposal

### PR-OLD-1: ChatTemplate HF array-form support

Source: `2639f431`

Status: completed as `migration/pr-old-chattemplate`.

Scope:

- Add HF array-form `chat_template` parsing in main `ChatTemplate::Load()`.
- Preserve current main string/object behavior.
- Add focused test fixture.
- Do not touch minja submodule.
- Do not add `quick_dot_ai_api.*`.

This is the safest first PR because it is a small, isolated missing behavior and
does not depend on Android/product code or tokenizer-cache work.

### PR-OLD-2: Generic CausalLM streaming and cooperative cancel

Sources: runtime pieces of `81b303e4`, `1752be26`, `67a2836c`,
`b45774ad`, and hidden streaming pieces from `4f7c5607`

Status: completed as `migration/pr-old-streaming` and
`migration/pr-old-cancel`.

Scope to design before implementation:

- Keep `causal_lm_api.*` as the C API surface.
- Add generic streamer/callback shape only if main lacks equivalent behavior.
- Add cooperative cancellation semantics with explicit thread-safety contract.
- Exclude QuickAI JNI, NDJSON HTTP streaming, Android service code, and
  `quick_dot_ai_api.*`.

### PR-OLD-3: Signed/UINT4 embedding support probe

Sources: `76dc25a6`, `e0a68c75`, `a935a220`, `b64ac377`, `e9692a60`

Status: completed as narrowed sidecar LUT support in
`migration/pr-old-embedding-sidecar`.

Scope:

- First confirm whether current main Q4_0/safetensors path already covers the
  required model files.
- If missing, add tests or binary-format fixture before implementation.
- Port only the missing signed/UINT4 embedding behavior.

### Final Tokenizer Stage

Sources: `f31096a2`, `d64ce8c8`, `1c22c29e`, `61049fb5`, `3069e7b4`,
`9c19da17`

Deferred by policy:

- Tokenizer cache
- WordPiece
- BPE
- tokenizer_loader integration

This stage needs token-id fixtures and cache format ownership before code work.

## Explicit Exclusions

- Android app/service/AAR/SampleTestAPP/product packaging
- prebuilt `.so`/AAR binaries
- Quick.AI REST/NDJSON HTTP behavior
- Kotlin/JNI product wrappers
- `quick_dot_ai_api.*`
- OLD direct minja header vendoring
- OLD direct multimodal `void * + size` contract
- broad merge/fix churn commits
