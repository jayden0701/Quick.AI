# nntrainer OLD_NNTRAINER to sync_v0.4.0 to main audit

Date: 2026-06-11

Status: draft for review

## 목적

이 문서는 `/home/jrock/Quick.AI/nntrainer/OLD_NNTRAINER/.git`의 내부망
브랜치 변경이 외부 공개 nntrainer의 `sync_v0.4.0`에 어떻게 반영되었는지
비판적으로 추적하고, 현재 `main` 기준으로 어떤 변경을 upstream PR로 다시
쪼개야 하는지 정리한다.

핵심 결론부터 말하면, `sync_v0.4.0`은 내부망 변경을 신뢰할 수 있는 형태로
보존한 브랜치가 아니다. `2e8eaa9d`에서 너무 큰 tree 통합을 한 뒤, 그 위에
`main`과 이미 겹치는 변경, LFM2/LFM2-VL/V-JEPA 같은 신규 작업, DDTree/QNN/
safetensors 계열 변경이 섞였다. 따라서 `sync_v0.4.0`을 그대로 rebase 또는 merge
하면 안 되고, 내부망 브랜치를 원본 의도 기준으로 삼아 기능 단위로 다시
port해야 한다.

## 기준 브랜치와 커밋

OLD_NNTRAINER 기준 브랜치는 `quickdotai_api_refact`다.

확인 결과:

```text
OLD_NNTRAINER HEAD              e9692a609d5d652cce04e8c2f8d486c785a0bb1f
OLD_NNTRAINER quickdotai_api_refact
                                e9692a609d5d652cce04e8c2f8d486c785a0bb1f
OLD_NNTRAINER quickAI_merge     e9692a609d5d652cce04e8c2f8d486c785a0bb1f
OLD_NNTRAINER start commit      dcacb60c3f5a2e8d6037309f4c0e9de156010f92
```

`quickdotai_api_refact`와 `quickAI_merge`는 현재 같은 커밋을 가리킨다.
따라서 이 보고서의 OLD 범위는 다음이다.

```text
dcacb60c3f5a2e8d6037309f4c0e9de156010f92^..e9692a609d5d652cce04e8c2f8d486c785a0bb1f
```

외부 nntrainer 기준:

```text
main          f2c0e6ae30ceb1ff418064f02b2f95804f294d24
sync_v0.4.0   92f8ce201545f4dff22bf4171b942568c291995c
merge-base    3c331fb26f4f8bec1420b2d39731ba7e75124c35
large import  2e8eaa9d16fe5f97c0c501b1c043c3818a90319f
```

## 수치 요약

OLD 내부망 범위:

```text
commit count: 143
205 files changed, 141137 insertions(+), 7564 deletions(-)
```

OLD 범위의 dirstat:

```text
55.8% Applications/
43.1% Applications/CausalLM/
13.2% Applications/CausalLM/models/
12.7% Applications/CausalLM/layers/
30.3% nntrainer/
12.7% nntrainer/tensor/
8.8%  nntrainer/tensor/cpu_backend/
8.8%  nntrainer/layers/
```

`2e8eaa9d` 단일 import 커밋:

```text
115 files changed, 133008 insertions(+), 1684 deletions(-)
```

`2e8eaa9d^..sync_v0.4.0`:

```text
commit count: 79
```

`main..sync_v0.4.0`:

```text
192 files changed, 140449 insertions(+), 4483 deletions(-)
```

`main..sync_v0.4.0` dirstat:

```text
51.9% Applications/
50.8% Applications/CausalLM/
16.5% Applications/CausalLM/models/
11.0% Applications/CausalLM/layers/
34.8% nntrainer/
20.4% nntrainer/tensor/
16.0% nntrainer/tensor/cpu_backend/
```

## Tree-level 관찰

OLD 최종 tree와 `sync_v0.4.0` 최종 tree를 archive로 풀어 비교하면:

```text
OLD files:       1726
sync files:      1840
common files:    1720
OLD-only files:  6
sync-only files: 120
```

OLD 최종 tree와 최초 공개 import인 `2e8eaa9d` tree를 비교하면:

```text
import files:       1780
common old/import:  1720
OLD not import:     6
import not OLD:     60
```

즉 최초 공개 import 시점에도 OLD의 최종 파일 대부분은 들어갔지만, 이미 공개
main 쪽 base drift와 섞인 상태였다. 이 때문에 `2e8eaa9d` tree 자체를 "내부망
원본"으로 보면 안 된다.

OLD-only 6개는 대부분 작업 메모 또는 patch 파일이다.

```text
CLAUDE.md
gemma-model-path.md
how-to-use-litert-lm-guide.md
nntrainer_pr3867_jinja2_compat.patch
streaming.diff
user-request.md
```

이 숫자만 보면 sync가 OLD를 거의 포함한 것처럼 보인다. 그러나 이것은
오해를 부를 수 있다. 공통 파일 1720개 중 실제 내용이 다른 파일이 많고,
`sync_v0.4.0`에는 OLD 이후의 public/main 계열 작업이 대량으로 섞였다.
따라서 "파일이 존재한다"는 사실은 "올바른 방식으로 반영되었다"는 뜻이
아니다.

특히 sync-only 주요 파일군은 다음이다.

```text
Applications/CausalLM/models/lfm2/*
Applications/CausalLM/models/lfm2/lfm2-vl/*
Applications/CausalLM/models/vjepa2_vit/*
Applications/CausalLM/layers/vjepa_*
Applications/CausalLM/res/lfm2-vl/*
Applications/CausalLM/res/vjepa2/*
nntrainer/ddtree/*
nntrainer/qnn/*
nntrainer/utils/safetensors_util.*
test/unittest/ddtree_*
```

이들은 대부분 OLD 내부망 초기 변경 그 자체라기보다, 외부 공개 브랜치에서
후속으로 얹힌 기능이거나 현재 `main`과 이미 겹치는 기능이다.

## 가장 중요한 구조적 문제

`main`의 CausalLM API 구조는 아직 다음 파일들을 가진다.

```text
Applications/CausalLM/api/README.md
Applications/CausalLM/api/causal_lm_api.cpp
Applications/CausalLM/api/causal_lm_api.h
Applications/CausalLM/api/model_config.cpp
Applications/CausalLM/api/model_config_internal.h
Applications/CausalLM/api/test_api.cpp
```

반면 `sync_v0.4.0`은 `main` 기준으로 다음과 같은 diff를 만든다.

```text
D Applications/CausalLM/api/README.md
D Applications/CausalLM/api/causal_lm_api.cpp
D Applications/CausalLM/api/causal_lm_api.h
D Applications/CausalLM/api/model_config.cpp
D Applications/CausalLM/api/model_config_internal.h
D Applications/CausalLM/api/test_api.cpp
A Applications/CausalLM/api/quick_dot_ai_api.cpp
A Applications/CausalLM/api/quick_dot_ai_api.h
A Applications/CausalLM/api/streamer.cpp
A Applications/CausalLM/api/streamer.h
```

이것이 가장 큰 구조적 경고 신호다.

Quick.AI 현재 문서 기준으로는 `quick_dot_ai_api.*`가 Quick.AI 최상위 repo의
public deployment API다. nntrainer는 CausalLM execution plane으로 남는 것이
더 맞다. 따라서 `sync_v0.4.0`처럼 nntrainer의 기존 CausalLM API를 삭제하고
Quick.AI API로 교체하는 방식은 main 병합 전략으로 부적절하다.

결론:

```text
quick_dot_ai_api.* 자체는 nntrainer upstream PR 대상이 아니다.
필요한 runtime hook만 nntrainer에 일반화해서 넣어야 한다.
```

## OLD 내부망 변경군 분류

### 1. QuickAI Android/product stack

대표 OLD 커밋:

```text
dcacb60c add Android Project
6f4d73e6 add LauncherApp, QuickAIService, ClientApp
dbfdd69a QuickAI: REST service + handle-based causal_lm_api for parallel models
61f1eb28 QuickAI: extract QuickDotAI AAR and add SampleTestAPP
61378fb2 QuickDotAI: add multimodal (image) runMultimodal* API to AAR
7f18a189 Support NativeChatSession functionality
2f71f6ca QuickAI first release
```

성격:

- Android app, service, client app, AAR, SampleTestAPP, JNI bridge, REST server.
- 제품 릴리즈 레이어에 가깝다.
- 현재 Quick.AI 최상위 repo의 `Android/`, `api/`, `src/`, `qnn/`가 이 책임을
  가져간 상태다.

sync 반영 상태:

- `2e8eaa9d`에서는 대부분 제거 또는 축소되었다.
- 그러나 `Applications/QuickAI/QuickDotAI/.gitignore` 같은 잔재가 남았다.
- `quick_dot_ai_api.*`는 nntrainer 안으로 들어갔고, 이것이 구조적 오염이 되었다.

main 기준 판단:

- upstream 제외.
- QuickAI repo에 남겨야 한다.
- nntrainer에는 Android product, AAR packaging, REST service, model catalog
  deployment API를 넣지 않는 것이 맞다.

### 2. CausalLM handle API, streaming API, QuickDotAI API 전환

대표 OLD 커밋:

```text
dbfdd69a QuickAI: REST service + handle-based causal_lm_api for parallel models
81b303e4 CausalLM: add streaming C API + QuickAI JNI wiring for Qwen3
1752be26 CausalLM: switch test_api to the handle-based streaming API
2c9ee80d Switch from causal_lm_api to quick_dot_ai_api across the entire stack
2fd800f3 Applications/CausalLM/api: sync with Quick.AI
ab2525ab quick_dot_ai_api: support dynamic model path
5e12b108 [Refactor] Unify API to handle-based OpenAI message format
4f7c5607 feat(streaming): Add Streaming API with messages format support
```

성격:

- Quick.AI deployment API를 만들기 위한 작업이다.
- streaming callback, handle lifecycle, dynamic model path, OpenAI-style messages
  입력이 섞여 있다.
- 일부는 nntrainer runtime에도 유용한 추상화일 수 있지만, API 표면 자체는
  Quick.AI 제품 레이어다.

sync 반영 상태:

- `2e8eaa9d`에서 기존 `causal_lm_api.*`를 삭제하고 `quick_dot_ai_api.*`로
  교체했다.
- 이후 `sync_v0.4.0`도 이 구조를 계속 들고 간다.

비판:

- main의 기존 API 구조와 정면 충돌한다.
- PR 리뷰 관점에서 "nntrainer CausalLM API 개선"이 아니라 "Quick.AI 제품 API
  이식"으로 보인다.
- upstream에 넣을 수 있는 작은 단위가 아니다.

main 기준 판단:

- `quick_dot_ai_api.*`는 upstream 제외.
- 필요한 경우 다음만 별도 검토한다.
  - CausalLM 내부 streamer abstraction
  - cancel/cooperative cancellation hook
  - message 기반 입력을 CausalLM sample app에서 처리하는 최소 기능
  - model path를 외부에서 주입하는 generic loader 개선

### 3. Chat template, minja, OpenAI tools

대표 OLD 커밋:

```text
5f0a53a6 Support HF-chat template feature
dd83ccab Add Enhanced Chat Template, and update nntrainer
2639f431 feat(chat_template): Support array-form chat_template and streaming API
```

성격:

- Hugging Face style chat template.
- minja 기반 renderer.
- OpenAI `tools`/`functions`를 template context로 넘기는 기능.
- FunctionGemma/Gemma4 계열에 필요했던 기반.

sync 반영 상태:

- `2e8eaa9d`에서 minja header를 직접 vendor 파일처럼 넣었다.
- 이후 sync는 chat template 관련 rebase commit을 별도로 더 쌓았다.

main 반영 상태:

`main`에는 이미 다음 계열이 들어가 있다.

```text
17820970 [CausalLM] add ChatTemplate
5bc1cc8b [Chat_Tempalte] Update Chat template to support jinja
96b96213 [ChatTemplate] Fix the chat_template mismatch for function & tool call
952939b0 [ChatTemplate] Remove BuiltIn ChatTemplate for FunctionGemma
c400bb78 causallm: finish chat template CI adaptation
```

또한 `main`은 `Applications/CausalLM/third_party/minja`를 submodule 또는
외부 dependency 형태로 유지한다. sync처럼 header를 직접 복사한 형태를 그대로
가져오면 main 구조와 맞지 않는다.

main 기준 판단:

- 대부분 skip.
- sync/OLD 쪽에서 main에 없는 작은 delta만 확인한다.
- 현재 후보는 `a6fd285b [ChatTemplate] Extend minja engine - Adds "capitalize" as
  global filter` 정도다.
- 이 경우도 minja vendoring 방식이 아니라 main의 minja integration 방식에 맞춰
  최소 patch로 가져와야 한다.

### 4. Tokenizer cache, BPE, WordPiece, tokenizer_loader

대표 OLD 커밋:

```text
f31096a2 [Tokenizer] Add tokenizer cache utilities
d64ce8c8 [Tokenizer] Add WordPiece tokenizer support
1c22c29e [Tokenizer] Add BPE tokenizer support
61049fb5 [Tokenizer] Integrate compact tokenizers with build system
9c19da17 fix(tokenizer): handle missing tokenizer_file gracefully
```

성격:

- compact tokenizer support.
- BPE/WordPiece native tokenizer.
- tokenizer cache.
- `tokenizer_loader`로 tokenizer 종류 선택.

sync 반영 상태:

- `Applications/CausalLM/tokenizers/bpe_tokenizer.cpp`
- `Applications/CausalLM/tokenizers/wordpiece_tokenizer.cpp`
- `Applications/CausalLM/tokenizers/tokenizer_cache_util.h`
- `Applications/CausalLM/models/tokenizer_loader.*`

main 반영 상태:

- `main`에는 현재 이 compact tokenizer 파일들이 보이지 않는다.
- `main`은 chat template/Gemma4는 갖고 있지만, OLD의 BPE/WordPiece/tokenizer
  loader 계열은 별도 port 후보로 남아 있다.

main 기준 판단:

- OLD 기반 upstream 후보.
- 단, `2e8eaa9d`처럼 API 교체와 함께 넣으면 안 된다.
- 별도 PR로 나누는 것이 좋다.

권장 PR:

```text
PR-tokenizer-1: tokenizer cache utility only
PR-tokenizer-2: WordPiece tokenizer
PR-tokenizer-3: BPE tokenizer
PR-tokenizer-4: tokenizer_loader integration
```

실제로는 리뷰 부담을 줄이기 위해 2개 PR 정도로 합칠 수 있다.

### 5. ThreadManager, OpenMP 제거, ggml kernel tuning

대표 OLD 커밋:

```text
3fb02bd6 Introduce unified Thread Manager
cfd37ba1 Replace bs thread pool into new thread pool
67589c58 Replace openmp into new thread pool
4537b7de Optimize ggml kernel
2a8d2b47 Refactor ThreadManager
```

성격:

- OpenMP/bs_thread_pool 의존도를 줄이고 nntrainer 내부 ThreadManager를 사용.
- Android/Windows build, ggml kernel 경로와 연결된다.

sync 반영 상태:

- `nntrainer/utils/thread_manager.*`가 존재한다.
- 일부 CausalLM/LFM2 path에서 thread_manager 사용 commit이 추가되었다.

main 반영 상태:

`main`에도 ThreadManager 계열이 이미 들어가 있다.

```text
909f1102 Introduce unified Thread Manager
9c902f48 Refactor ThreadManager
e9db649d Apply code review
65462bb5 Fix bug for 1 thread
```

main 기준 판단:

- 대부분 skip.
- OLD의 `67589c58 Replace openmp into new thread pool`은 너무 넓고 Android.mk/
  Windows binary 삭제까지 포함한다. 그대로 port하면 위험하다.
- 필요한 경우 특정 kernel 또는 특정 CausalLM layer에서 ThreadManager를 쓰는
  작은 delta만 별도 검토한다.

### 6. Memory allocator, QNN/FSU memory leak

대표 OLD 커밋:

```text
8dbe0dae feat(allocator): add CpuMemAllocator and RpcMemAllocator
18c116a0 fix(memory): plug FSU/NPU memory leak via allocator integration
```

성격:

- CPU/RPC memory allocator.
- FSU/NPU memory leak mitigation.
- memory_pool/tensor_pool/manager와 관련된다.

sync 반영 상태:

- `2e8eaa9d`와 sync 모두 memory_pool 계열 변경을 포함한다.
- 이후 sync에는 QNN 관련 추가 수정이 더 들어갔다.

관련 sync 후속 커밋:

```text
5a0f2e17 Change memory_pool.cpp for QNN
c3a44e11 Fix : deliver externally-fed inputs to QNN graph input tensors
```

main 반영 상태:

- `main`에도 QNN context와 memory 관련 구조가 이미 일부 존재한다.
- 그러나 OLD의 allocator abstraction이 main에 온전히 들어갔는지는 추가 비교가
  필요하다.

main 기준 판단:

- 후보이나 위험도가 높다.
- public API나 QuickAI handle과 분리해서, memory ownership bug fix로만
  재구성해야 한다.
- 테스트 없이 port하면 안 된다.

권장 PR:

```text
PR-memory-1: allocator abstraction only, no QNN behavior change
PR-memory-2: FSU/NPU leak fix with reproduction test
PR-qnn-1: externally-fed input delivery fix
```

### 7. UINT4/signed 4bit embedding

대표 OLD 커밋:

```text
76dc25a6 Update Embedding Layer to support UINT4 Quatized Weight
b64ac377 Support signed 4bit
```

성격:

- CausalLM embedding layer quantization support.
- UINT4/signed 4bit weight handling.

sync 반영 상태:

- `Applications/CausalLM/layers/embedding_layer.*`에 큰 변경으로 들어가 있다.

main 반영 상태:

- main에는 safetensors quantization, typed external tensor mapping, RMSNorm fix 등
  관련 변경이 이미 많다.
- 단, OLD의 signed 4bit embedding semantics가 main에 완전히 들어갔는지는 별도
  diff가 필요하다.

main 기준 판단:

- 후보.
- 그러나 main의 최신 quantization path와 충돌 가능성이 높다.
- safetensors series 이후의 main 구조 위에서 다시 설계해야 한다.

### 8. Gemma4, FunctionGemma, RMSNorm

대표 OLD/sync 커밋:

```text
10c4feac Add Gemma4/Function Gemma - Add models and skip_prefill optimization
9575cb58 Add Gemma4 and optimizations
e1223a8e [CausalLM] Port Gemma4 to chat template base
229a553b [cpu_backend] Implement FP16 RMSNorm fallback
1e141019 Fix FP16 RMS norm overflow and implement missing fallback/NEON specializations
cceca5b1 disable rms_norm_wrt_width_fp16_intrinsic in rms_norm.cpp
```

main 반영 상태:

`main`에 이미 더 정리된 형태로 들어가 있다.

```text
18518354 Add Gemma4 and optimizations
ed866334 [CausalLM] Port Gemma4 to chat template base
86e51368 [CausalLM] Fix Gemma4 doxygen briefs
0983f807 [CausalLM] Fix RMSNorm FP16 overflow and typed external tensor mapping
```

`git cherry -v main sync_v0.4.0`에서도 일부 Gemma4/TFLite/tensor_api/DDTree test
커밋은 patch-equivalent로 표시된다.

main 기준 판단:

- skip.
- sync의 RMSNorm workaround를 다시 가져오면 main의 최신 `0983f807` 방향을
  되돌릴 위험이 있다.
- FunctionGemma에 필요한 chat-template delta만 별도 확인한다.

### 9. XGrammar hook

대표 OLD 커밋:

```text
5b19bf0c feat(xgrammar): Add XGrammar virtual methods and getVocabSize to Transformer base class
```

성격:

- Transformer base에 XGrammar 관련 virtual method를 추가.
- Quick.AI의 constrained decoding과 연결된다.

sync 반영 상태:

- OLD의 XGrammar hook 자체가 `sync_v0.4.0`의 눈에 띄는 독립 PR 단위로는 잘
  보이지 않는다.
- Quick.AI 현재 repo에는 XGrammar manager/wrapper가 최상위 `src/xgrammar/`에
  존재한다.
- 현재 Quick.AI 쪽 별도 브랜치에는 CPU CausalLM generation에 XGrammar mask를
  적용하는 commit이 따로 존재한다.

main 기준 판단:

- 설계 결정이 필요하다.
- nntrainer upstream이 XGrammar를 직접 dependency로 받을지, 아니면 grammar mask
  provider interface만 받을지 정해야 한다.
- 지금 형태 그대로는 Quick.AI product coupling이 강하다.

권장 방향:

```text
1. nntrainer에는 external logits mask provider interface만 둔다.
2. XGrammar manager/compiler/cache는 Quick.AI repo에 둔다.
3. CausalLM sampler가 optional mask callback을 호출하게 한다.
```

### 10. Multimodal API

대표 OLD 커밋:

```text
61378fb2 QuickDotAI: add multimodal (image) runMultimodal* API to AAR
c9d04158 Enable image input in SampleTestAPP chat tab
280bf472 Route chat image inputs through multimodal run
```

성격:

- OLD에서는 대부분 Android/AAR public API 레이어였다.
- nntrainer runtime 쪽 generic composition은 아직 충분히 정리되지 않았다.

sync 후속 커밋:

```text
12d6ca9d feat(transformer): add model-agnostic multimodal embedding interface
00a44e01 fix(transformer): add virtual getKvLen()
5c07ad06 fix(transformer): tolerate missing tokenizer_file
7feb453f [CausalLM] Port LFM2-VL multimodal model onto #3963 base
18754b1b feat(transformer): add expectedPixelElems + imagePlaceholderTokenId hooks
92f8ce20 feat(lfm2-vl): add runFromPixels + getLM for app monolithic path
```

문서 기준 방향:

- Quick.AI는 control plane.
- nntrainer는 execution plane.
- public C API가 component pointer나 execution order를 소유하지 않아야 한다.
- 최종 방향은 generic multimodal composition runtime이다.

main 기준 판단:

- OLD의 Android/AAR multimodal API는 upstream 제외.
- sync의 Transformer-level multimodal hook은 후보이나, 현재는 LFM2-VL bring-up에
  맞춘 ad-hoc hook이 섞여 있다.
- 먼저 generic tensor/embedding contract를 설계하고, 그 뒤 LFM2-VL을 얹어야 한다.

## `2e8eaa9d` import 방식 비판

`2e8eaa9d`는 다음 이유로 future PR의 기준이 되면 안 된다.

### 1. PR 단위가 너무 크다

단일 커밋이 `115 files`, `133k insertions` 규모다. 게다가 CausalLM API 삭제,
QuickAI API 추가, tokenizer, chat template, Gemma4, quantization, memory,
nntrainer core 변경이 한 번에 섞였다.

### 2. main API를 QuickAI API로 교체했다

기존 `causal_lm_api.*`를 삭제하고 `quick_dot_ai_api.*`를 추가했다. 이는
upstream nntrainer의 public shape를 Quick.AI 제품 API로 바꾸는 것이며, 현재
Quick.AI repo 구조와도 맞지 않는다.

### 3. OLD 변경과 public base drift가 섞였다

OLD 최종 tree와 `2e8eaa9d` tree를 비교하면 OLD에 없던 public/main 계열 파일이
많이 보인다.

예:

```text
Applications/CausalLM/models/deberta_v2/*
Applications/CausalLM/models/timm_vit/*
Applications/CausalLM/stb_image.inc
Applications/CausalLM/third_party/nlohmann/json.hpp
api/ccapi/src/tensor_api_*
nntrainer/qnn_context.*
nntrainer/tensor/cpu_backend/compute_ops.*
nntrainer/utils/safetensors_util.*
```

이 자체가 나쁜 것은 아니다. 공개 main 위에 port하려면 당연히 base drift가 있다.
문제는 이 상태에서 QuickAI 변경까지 한 커밋으로 덮어썼기 때문에 어떤 변경이
main-origin이고 어떤 변경이 QuickAI-origin인지 리뷰하기 어렵다는 점이다.

### 4. chat template/minja 통합 방식이 main과 다르다

OLD/import는 minja header를 직접 tree에 넣는 형태가 보인다. main은 이미
`third_party/minja`를 별도 dependency 형태로 갖고 있다. 따라서 sync 방식을
그대로 가져오면 dependency 관리 방식이 어긋난다.

### 5. test/data payload가 PR을 압도한다

예를 들어 `Applications/CausalLM/res/tiny-bert/tokenizer.json` 같은 큰 resource가
초기 import에 포함된다. 기능 PR이라면 테스트 fixture와 모델 resource를 엄격히
분리해야 한다.

## `sync_v0.4.0` 후속 변경 비판

`sync_v0.4.0`은 `2e8eaa9d` 이후 79개 커밋을 더 쌓았다. 이 중에는 좋은 기능도
있지만, upstream 병합 기준으로는 다음 문제가 있다.

### 1. 이미 main에 있는 기능을 다시 들고 있다

main에는 이미 다음이 있다.

```text
Gemma4
ChatTemplate/minja
ThreadManager
QNN context
safetensors utility
DDTree
stb_image
DeBERTa/TiMM ViT support
```

sync에는 이 기능들의 오래된 형태, 다른 path, 또는 rebase 중간 형태가 같이
남아 있다.

### 2. DDTree path가 main과 다르다

main:

```text
nntrainer/utils/ddtree/*
```

sync:

```text
nntrainer/ddtree/*
```

main에 이미 들어간 DDTree를 sync의 path 이동 형태로 다시 가져오면 불필요한
churn이 된다. DDTree는 skip해야 한다.

### 3. merge conflict 품질 신호가 있다

sync에는 다음 커밋이 있다.

```text
baea966f fix(causallm): resolve committed merge-conflict markers from vjepa ViT commit
```

이것은 해당 구간을 그대로 신뢰하면 안 된다는 강한 신호다. 특히 V-JEPA와
LFM2-VL 주변은 cherry-pick보다 재작성/재검증이 맞다.

### 4. LFM2/LFM2-VL/V-JEPA가 QuickAI API 교체 위에 얹혀 있다

LFM2 계열 자체는 upstream 후보일 수 있다. 하지만 현재 sync에서는
`quick_dot_ai_api` 교체, generic multimodal hook, model-specific runFromPixels,
Android ARM numerics fix, converter fix가 섞인 상태다.

따라서 LFM2 계열은 sync commit을 그대로 PR로 내면 안 되고, `main`에서 다음
순서로 다시 쪼개야 한다.

```text
1. LFM2 prerequisite kernels/layers
2. LFM2 CausalLM model
3. generic multimodal embedding contract
4. LFM2-VL vision/connector/model
5. image input/preprocessing/converters
6. Android ARM numerics fix
```

## main 기준 skip 목록

다음은 main에 이미 들어갔거나, sync 형태로 가져오면 안 되는 항목이다.

```text
Gemma4 base model
Gemma4 chat-template port
RMSNorm FP16 overflow fix
ThreadManager base
DDTree
safetensors utility and quantization series
QNN context base
DeBERTa/TiMM ViT public-main side changes
QuickAI Android app/service/AAR/SampleTestAPP
quick_dot_ai_api.* as nntrainer API replacement
prebuilt native libraries
large product resource payloads
```

## main 기준 port 후보

### OLD-derived 후보

우선순위 높은 OLD-derived 후보:

```text
Tokenizer cache, BPE, WordPiece, tokenizer_loader
signed/UINT4 embedding support after safetensors-main compatibility check
allocator/memory ownership fixes if still missing in main
generic mask-provider hook for XGrammar-style constrained decoding
small chat-template delta such as capitalize filter if missing in main
```

주의:

- 이것들은 OLD 히스토리를 기준으로 원래 의도를 추적해야 한다.
- `2e8eaa9d`나 sync 최종 tree에서 복사하면 안 된다.
- main의 현재 파일 구조에 맞춰 새 patch로 만들어야 한다.

### sync-only 후보

OLD 원본이 아니라 sync 후속에서 생긴 후보:

```text
LFM2 CPU kernels and support layers
LFM2 CausalLM model
generic multimodal embedding interface
LFM2-VL model
LFM2-VL converters/config/docs
V-JEPA 2.1 ViT-B video encoder
QNN externally-fed input fix
```

이들은 OLD 내부망 복원 작업 이후에 다루는 것이 좋다.

## 권장 작업 순서

### Phase 1. OLD 기준 audit matrix 작성

각 OLD 커밋을 다음 분류로 표기한다.

```text
product-only
already-main
port-candidate
sync-only-followup
obsolete
needs-design
```

보고서의 1차 분류는 충분하지 않다. 실제 PR 작업 전에는 commit 단위 matrix가
필요하다.

### Phase 2. main에서 OLD-derived 작은 PR부터 시작

권장 첫 작업은 LFM2가 아니라 OLD 내부망 원본에 있던 작은 기능이다.

추천 순서:

```text
1. tokenizer cache/BPE/WordPiece/tokenizer_loader
2. signed/UINT4 embedding support 여부 diff
3. XGrammar를 직접 넣지 않는 generic logits-mask hook 설계
4. allocator/memory leak fix가 main에 남아 있는지 재현 기반 확인
```

이 순서가 사용자 요구의 "먼저 과거 내부망 내용을 현재 main에 맞추기"와 맞다.

### Phase 3. sync 후속 기능 분할

OLD-derived 작업이 정리된 뒤 LFM2/LFM2-VL/V-JEPA를 다룬다.

권장 PR 순서:

```text
PR-LFM2-1: causal depthwise conv1d kernels and LFM2 layers
PR-LFM2-2: LFM2 CausalLM model
PR-MM-1: generic multimodal embedding/tensor contract
PR-LFM2VL-1: LFM2-VL vision tower and connector
PR-LFM2VL-2: LFM2-VL model, converters, config, docs
PR-VJEPA-1: V-JEPA layers
PR-VJEPA-2: V-JEPA model and converters
PR-QNN-1: externally-fed QNN input fix
```

## 검토가 필요한 설계 질문

1. nntrainer upstream에 `quick_dot_ai_api.*`를 절대 넣지 않는다는 방향을 확정할지.
2. XGrammar를 nntrainer dependency로 받을지, 아니면 generic logits-mask provider만
   둘지.
3. multimodal composition의 최종 소유권을 nntrainer runtime으로 옮길지.
4. LFM2-VL을 fat monolithic model로 먼저 upstream할지, generic composition
   runtime 이후에 넣을지.
5. signed/UINT4 embedding support가 main의 safetensors quantization series와 어떤
   방식으로 합쳐져야 하는지.
6. OLD allocator/QNN memory fix가 현재 main에서 재현되는 실제 bug인지.

## 결론

`sync_v0.4.0`은 병합 대상 브랜치가 아니라 참고 자료다. 내부망 원본 의도는
`OLD_NNTRAINER/.git`의 `quickdotai_api_refact`에서 추적해야 하고, `sync_v0.4.0`은
그 의도가 public main 위에서 어떻게 왜곡되었는지 확인하는 보조 자료로 써야 한다.

가장 큰 왜곡은 `quick_dot_ai_api.*`를 nntrainer CausalLM API로 교체한 것이다.
현재 Quick.AI 아키텍처 문서 기준으로 이 API는 Quick.AI 최상위 repo의 public
deployment API이며, nntrainer upstream에는 runtime hook과 model/layer 기능만
일반화해서 넣는 것이 맞다.

따라서 다음 실제 작업은 `main`에서 새 브랜치를 만들고, OLD-derived 변경 중 아직
main에 없는 작은 기능부터 port하는 것이다. 첫 후보는 tokenizer cache/BPE/
WordPiece/tokenizer_loader 계열이다. LFM2/LFM2-VL/V-JEPA는 그 다음 단계에서
sync 후속 기능으로 별도 분할해야 한다.
