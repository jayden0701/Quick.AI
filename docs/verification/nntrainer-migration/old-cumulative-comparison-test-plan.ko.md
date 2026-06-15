# OLD Cumulative Comparison Test Plan

Date: 2026-06-15 KST

## 목적

OLD_NNTRAINER `quickdotai_api_refact`에서 main 기반 nntrainer로 옮긴
PR-OLD-1..6이 기능적으로 빠짐없이 이식되었는지 확인한다. 단순히 build가
되는지보다, OLD 원본에서 의도한 runtime behavior가 main 이식본과 Quick.AI
validation branch에서 같은 의미로 재현되는지를 검증한다.

## 비교 대상

| 대상 | 위치 / branch | 목적 |
|---|---|---|
| OLD 원본 | `/home/jrock/Quick.AI/nntrainer/OLD_NNTRAINER/.git`, `quickdotai_api_refact` | 내부망 원본 behavior 기준 |
| nntrainer 이식본 | `ojs/migration/old-cumulative-validation` | main 기반 PR-OLD-1..6 누적 결과 |
| Quick.AI validation | `ojs/validation/old-cumulative-quickai` | Quick.AI wrapper가 main 기반 nntrainer로 동작하는지 확인 |

관련 원격 branch:

```text
nntrainer ojs/migration/pr-old-chattemplate
nntrainer ojs/migration/pr-old-streaming
nntrainer ojs/migration/pr-old-cancel
nntrainer ojs/migration/pr-old-embedding-sidecar
nntrainer ojs/migration/pr-old-qnn-memory
nntrainer ojs/migration/pr-old-logits-processor
nntrainer ojs/migration/pr-old-causallm-accessors
nntrainer ojs/migration/old-cumulative-validation
Quick.AI ojs/validation/old-cumulative-quickai
```

## 판정 기준

| 결과 | 의미 |
|---|---|
| PASS | OLD 원본과 main 이식본/Quick.AI validation이 같은 기능 의미를 보이고, crash/hang/regression 없음 |
| PASS_WITH_DIFFERENCE | 출력 문자열은 다르지만 기능 의미가 동일하고 차이를 설명할 수 있음 |
| FAIL | 기능 누락, crash, hang, 잘못된 path resolution, API contract 깨짐 |
| BLOCKED_ENV | QNN SDK, tokenizer archive, device, model file 등 환경 부재 |
| PENDING_MODEL_FILES | 검증에 필요한 model/config/weight/tokenizer 파일 부재 |

생성형 모델 출력은 exact text match가 아니라 다음 기준으로 본다.

- prompt에 맞는 의미 있는 답변인지
- schema/tool calling에서는 JSON/schema를 지키는지
- streaming 조각을 합친 결과가 non-streaming 결과와 같은 의미인지
- cancel 후 handle이 재사용 가능한지

## 공통 준비

### 1. 깨끗한 nntrainer 이식본 clone/worktree

```bash
git clone https://github.com/jayden0701/nntrainer.git /tmp/nntrainer-old-cumulative-check
cd /tmp/nntrainer-old-cumulative-check
git checkout ojs/migration/old-cumulative-validation
git submodule update --init --recursive
```

### 2. nntrainer 기본 검증

```bash
meson setup build -Denable-transformer=true
ninja -C build
meson test -C build \
  unittest_chat_template \
  unittest_callback_streamer \
  unittest_cancel_api \
  unittest_embedding_sidecar_lut \
  unittest_memory_pool \
  unittest_causallm_models \
  --print-errorlogs
timeout 240 env NNTR_NUM_THREADS=4 \
  ./build/Applications/CausalLM/nntr_causallm \
  /home/jrock/Quick.AI/gemma4_it_x86
```

Expected:

- build exit code 0
- Meson tests all pass
- Gemma4 answers Korea's capital as Seoul or an equivalent coherent answer

### 3. Quick.AI validation branch 준비

```bash
git clone https://github.com/jayden0701/Quick.AI.git /tmp/quickai-old-cumulative-check
cd /tmp/quickai-old-cumulative-check
git checkout ojs/validation/old-cumulative-quickai
git submodule update --init --recursive
```

If validating against the local nntrainer cumulative worktree, replace the
submodule with a symlink only in the test checkout:

```bash
rm -rf nntrainer
ln -s /tmp/nntrainer-old-cumulative-check nntrainer
```

Do not commit that symlink.

## 기능별 비교 테스트

### PR-OLD-1: ChatTemplate HF array-form support

목표:

- Hugging Face style array-form chat template이 OLD 원본과 main 이식본에서
  같은 prompt structure로 변환되는지 확인한다.

테스트 케이스:

| Case | 입력 | 비교 항목 |
|---|---|---|
| single user | `[{role:user, content:"Hello"}]` | BOS/EOS/turn marker 위치 |
| system + user | system prompt 포함 | system prompt 위치 |
| multi-turn | user/assistant/user | 이전 assistant 답변 보존 |
| tool message | tool 또는 function message 포함 | unsupported면 명확한 error, supported면 동일 formatting |
| empty system | system 빈 문자열 | 불필요한 marker 추가 여부 |

검증 방법:

1. OLD 원본에서 같은 tokenizer/config로 formatted prompt를 dump한다.
2. `ojs/migration/pr-old-chattemplate` 또는 cumulative branch에서 dump한다.
3. whitespace-only 차이를 제외하고 구조를 비교한다.
4. Quick.AI가 chat template을 거치는 모델에서 runtime smoke를 돌린다.

PASS 기준:

- role ordering과 turn marker가 OLD와 동등하다.
- unsupported message type은 crash가 아니라 명확한 error로 끝난다.

### PR-OLD-2a: Streaming callback

목표:

- streaming callback이 token 단위 또는 chunk 단위 output을 안정적으로
  전달하고, callback output을 합친 결과가 non-streaming output과 같은
  의미인지 확인한다.

테스트 케이스:

| Case | 확인 |
|---|---|
| basic streaming | callback이 1회 이상 호출됨 |
| stream concat | callback chunks를 합친 문자열이 최종 output과 같은 의미 |
| repeated streaming | 같은 handle로 5회 연속 실행 |
| stream then normal | streaming 실행 후 non-streaming 실행 |
| callback stop | callback 내부 stop 요청 시 hang 없이 종료 |

Quick.AI 측 추가 확인:

- `runModelHandleStreaming()` 또는 이에 대응하는 JNI/API path에서 callback
  sequence가 정상 전달되는지 확인한다.
- Android UI/sample path가 있다면 token이 UI에 누적되는지 확인한다.

PASS 기준:

- callback 누락 없음.
- 반복 실행 후 stale streamer가 남지 않음.
- stop 후 재실행 가능.

### PR-OLD-2b: Explicit cancel

목표:

- generation 중 외부 thread/API에서 cancel을 호출했을 때 cooperative stop이
  동작하고, handle lifetime이 깨지지 않는지 확인한다.

테스트 케이스:

| Case | 확인 |
|---|---|
| long generation cancel | 긴 prompt/큰 `num_to_generate`에서 cancel 후 종료 |
| idle cancel | run 중이 아닐 때 cancel해도 crash 없음 |
| double cancel | cancel 2회 이상 호출해도 crash 없음 |
| cancel then rerun | cancel 후 같은 handle로 정상 재실행 |
| streaming cancel | streaming 중 cancel 후 streamer detach |
| Android cancel | `R3CX80H8Y0F`에서 native/API cancel path 실행 |

PASS 기준:

- cancel 후 process가 hang하지 않음.
- 다음 run이 정상 동작.
- output이 partial이어도 API error/return contract가 문서화된 범위 안에 있음.

### PR-OLD-3: Embedding sidecar LUT

목표:

- embedding sidecar 파일을 사용하는 모델이 cwd와 무관하게 sidecar를 찾고,
  dtype/shape/path error를 명확히 처리하는지 확인한다.

테스트 케이스:

| Case | 확인 |
|---|---|
| relative sidecar path | model directory 기준으로 resolve |
| absolute sidecar path | absolute path 유지 |
| cwd outside model | 다른 cwd에서 실행해도 sidecar load |
| missing sidecar | 명확한 load failure |
| corrupt sidecar | size/shape error |
| Gemma4 runtime | `/home/jrock/Quick.AI/gemma4_it_x86` coherent Seoul answer |

명령 예:

```bash
cd /tmp
timeout 240 env NNTR_NUM_THREADS=4 \
  /tmp/nntrainer-old-cumulative-check/build/Applications/CausalLM/nntr_causallm \
  /home/jrock/Quick.AI/gemma4_it_x86
```

PASS 기준:

- cwd가 model directory가 아니어도 실행된다.
- missing/corrupt case가 silent fallback하지 않는다.

### PR-OLD-4: QNN MemoryPool planned-offset allocation

목표:

- QNN/RPC memory allocation이 planned offset 기준으로 안정적으로 동작하는지
  실기기에서 확인한다. 이 항목은 CPU unit test만으로 충분하지 않다.

필수 환경:

- Qualcomm QNN SDK root
- Android device `R3CX80H8Y0F`
- QNN 실행 가능한 model package

테스트 케이스:

| Case | 확인 |
|---|---|
| QNN native build | `./build.sh --platform=android --enable-qnn --target=src,api,api-test` |
| install | `ANDROID_SERIAL=R3CX80H8Y0F ./install_android.sh` |
| single run | QNN model 1회 실행 |
| repeated run | 같은 model 20회 반복 |
| reload | load/unload 또는 process restart 후 재실행 |
| cache path | KV/cache 저장/복원 기능이 있으면 같이 검증 |
| mixed models | QNN model 실행 후 CPU model 실행, 반대 순서도 확인 |

PASS 기준:

- memory allocation 관련 crash, RPC error, stale buffer error 없음.
- 반복 실행에서 RSS나 device memory가 비정상적으로 증가하지 않음.

현재 상태:

- 이전 검증에서는 QNN SDK root 부재로 `BLOCKED_ENV`.

### PR-OLD-5: Generic logits processor hook

목표:

- Quick.AI XGrammar/tool calling이 nntrainer의 generic
  `causallm::LogitsProcessor` hook을 통해 동작하는지 확인한다.

테스트 케이스:

| Case | 확인 |
|---|---|
| valid JSON schema | output이 valid JSON |
| bounded integer | `minimum`/`maximum` 같은 schema 제약 준수 |
| completion stop | grammar completed/terminated 후 generation stop |
| reset after tool | tool run 뒤 일반 run에 grammar 영향 없음 |
| invalid schema | 명확한 error |
| repeated tool runs | 같은 handle로 10회 반복 |
| Android tool run | `quick_dot_ai_test function_gemma` 또는 equivalent |

Android 예:

```bash
adb -s R3CX80H8Y0F shell "
  cd /data/local/tmp/Quick.AI &&
  export LD_LIBRARY_PATH=/data/local/tmp/Quick.AI:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test function_gemma \
    'Return a JSON object.' \
    true W4A32 false /sdcard/Download/aistudio-mobile/models
"
```

PASS 기준:

- output JSON parse 성공.
- schema 위반 없음.
- tool run 후 non-tool run이 정상.
- post-completion grammar exception 없음.

### PR-OLD-6: CausalLM accessors

목표:

- Quick.AI가 필요한 narrow accessor가 main 기반 nntrainer에서 충분한지
  확인한다.

테스트 케이스:

| Accessor | 확인 |
|---|---|
| `Transformer::getTokenizer()` | tokenizer pointer가 null이 아니고 tokenization path가 정상 |
| `SentenceTransformer::getEmbeddingDim()` | embedding API output dimension과 일치 |

Quick.AI 측 확인:

- embedding encode API가 있으면 x86/Android에서 shape와 dim을 확인한다.
- tokenizer를 직접 쓰는 wrapper가 null dereference 없이 동작하는지 확인한다.

PASS 기준:

- accessor가 Quick.AI wrapper에서 필요한 범위만 제공한다.
- concrete model cast나 sync-only API를 요구하지 않는다.

## Quick.AI 통합 비교

### x86

```bash
cd /tmp/quickai-old-cumulative-check
./build.sh --platform=x86 --target=src,api,api-test
timeout 240 env \
  LD_LIBRARY_PATH=/tmp/quickai-old-cumulative-check/nntrainer/builddir_x86/nntrainer:/tmp/quickai-old-cumulative-check/nntrainer/builddir_x86/api/ccapi:/tmp/quickai-old-cumulative-check/builddir_x86/src:/tmp/quickai-old-cumulative-check/builddir_x86/api:$LD_LIBRARY_PATH \
  NNTR_NUM_THREADS=4 \
  ./builddir_x86/src/quick_dot_ai \
  /home/jrock/Quick.AI/gemma4_it_x86
```

PASS 기준:

- build exit code 0.
- output이 Korea capital에 대해 Seoul 또는 동등한 의미의 답변.

### Android native

```bash
cd /tmp/quickai-old-cumulative-check
./build.sh --platform=android --target=src,api,api-test
ANDROID_SERIAL=R3CX80H8Y0F ./install_android.sh
```

기기 runtime 최소 세트:

| Model/API | 목적 |
|---|---|
| `qwen3-0.6b` | basic text generation |
| `gemma4_cpu` | Gemma4 sidecar/regression |
| `function_gemma` | XGrammar/logits processor |
| embedding encode API | accessor/embedding dim |
| streaming API | callback detach/reuse |
| cancel API | cooperative stop/reuse |

Gauss3.8 계열은 이 기기에서 작동하지 않을 것으로 보고
`SKIP_DEVICE_UNSUPPORTED`로 기록한다.

### Android app

```bash
cd /tmp/quickai-old-cumulative-check/Android
mkdir -p QuickDotAI/prebuilt_libs
cp ../install_libs/*.so QuickDotAI/prebuilt_libs/
ANDROID_SERIAL=R3CX80H8Y0F \
  ./gradlew \
    :QuickDotAI:assembleDebug \
    :SampleTestAPP:assembleDebug \
    :SampleTestAPP:installDebug
```

PASS 기준:

- Gradle build/install 성공.
- Sample app에서 basic text generation과 cancel/streaming UI path가 있으면
  smoke 확인.

## OLD 원본과 직접 비교해야 할 산출물

가능하면 아래 항목을 OLD 원본과 main 이식본에서 같은 입력으로 저장한다.

| 산출물 | OLD | main 이식본 | 비교 기준 |
|---|---|---|---|
| formatted chat prompt | required | required | marker/role ordering |
| non-streaming output | required | required | semantic equivalence |
| streaming chunks | required | required | concat equivalence |
| cancel timing/result | required | required | no hang, reusable handle |
| sidecar resolved path | required | required | same model-dir semantics |
| logits processor JSON | required | required | schema validity |
| embedding dim/output shape | required | required | same dimension |
| QNN run logs | if env available | if env available | no RPC/memory error |

## 결과 기록 template

```markdown
## OLD Cumulative Comparison Run

- Date:
- Tester:
- OLD source commit:
- nntrainer cumulative commit:
- Quick.AI validation commit:
- Device:
- Model base path:

| Area | Result | Evidence | Notes |
|---|---|---|---|
| Clean nntrainer build |  |  |  |
| nntrainer focused tests |  |  |  |
| Gemma4 x86 |  |  |  |
| ChatTemplate comparison |  |  |  |
| Streaming comparison |  |  |  |
| Cancel comparison |  |  |  |
| Embedding sidecar |  |  |  |
| QNN MemoryPool |  |  |  |
| LogitsProcessor/XGrammar |  |  |  |
| Accessors/embedding API |  |  |  |
| Quick.AI x86 |  |  |  |
| Quick.AI Android native |  |  |  |
| Quick.AI Android app |  |  |  |

### Failures / Blockers

| ID | Area | Symptom | Root cause | Owner | Next action |
|---|---|---|---|---|---|
```

## 우선순위

1. QNN MemoryPool 실기기 검증: CPU test로 대체하기 어렵다.
2. XGrammar/logits processor 반복 실행: tool completion과 reset 문제가
   제품 영향이 크다.
3. streaming/cancel stress: thread/lifetime regression을 잡기 좋다.
4. embedding sidecar path 검증: cwd와 package layout 차이에서 깨질 수 있다.
5. ChatTemplate direct prompt 비교: 작은 차이가 model behavior 차이로
   이어질 수 있다.
6. accessor/embedding API: Quick.AI wrapper가 sync-only API를 다시 요구하지
   않는지 확인한다.
