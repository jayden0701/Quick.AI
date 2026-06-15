# nntrainer Migration Progress Summary

Date: 2026-06-12 KST

## 한줄 요약

OLD_NNTRAINER `quickdotai_api_refact`에서 가져와야 할 비-Android/비-product
핵심 기능은 PR-OLD-1..6으로 분할해 main 기반 누적 브랜치에 반영했고,
Quick.AI x86/Android/app 검증까지 한 번 통과했다. 이후 `sync_v0.4.0`
잔여분은 LFM2/LFM2-VL 중심으로 PR-SYNC-1..5b까지 분할했다.

현재 멈춘 지점은 **PR-SYNC-5b 단독 브랜치 검증 완료, 누적 브랜치 반영 전**이다.

## 현재 브랜치 상태

| 구분 | Worktree | Branch | Head | 상태 |
|---|---|---|---|---|
| OLD 누적 | `/home/jrock/nntrainer-old-cumulative` | `migration/old-cumulative-validation` | `50f90a26` | PR-OLD-1..6 누적 완료 |
| sync 누적 | `/home/jrock/nntrainer-sync-cumulative` | `migration/sync-cumulative-validation` | `64aa176a` | PR-SYNC-1..5a 누적 완료, 5b 미반영 |
| PR-SYNC-5b 단독 | `/home/jrock/nntrainer-pr-sync-5b-lfm2-vl-runtime` | `migration/pr-sync-5b-lfm2-vl-runtime` | `5b05ea49` | 단독 구현/검증/리뷰 완료 |
| Quick.AI OLD 검증 | `/home/jrock/Quick.AI-old-cumulative-validation` | `validation/old-cumulative-quickai` | `5dd7bb5` | OLD 누적 Quick.AI 검증용 패치 포함 |

기준 main:

```text
f2c0e6ae30ceb1ff418064f02b2f95804f294d24
```

Quick.AI 검증 기준:

```text
0e25e35d3eae099f318a166fb3aad6fb23d80aa0
```

## 완료된 OLD_NNTRAINER 이식

OLD_NNTRAINER의 `quickdotai_api_refact` 범위에서 Android/product 코드는
제외하고, nntrainer main에 들어갈 수 있는 공통 기능만 분리했다.

| PR | 누적 commit | 내용 | 상태 |
|---|---:|---|---|
| PR-OLD-1 | `608278b4` | Hugging Face array-form chat template 지원 | 완료 |
| PR-OLD-2a | `2b40a6b9` | CausalLM streaming callback API | 완료 |
| PR-OLD-2b | `97127a6f` | 명시적 cooperative cancel API | 완료 |
| PR-OLD-3 | `8e9dd262`, `3466a14f` | embedding sidecar LUT 및 config/path wiring | 완료 |
| PR-OLD-4 | `a28fa1a8` | QNN MemoryPool planned-offset allocation | 완료 |
| PR-OLD-5 | `bd64b5e2` | generic logits processor hook | 완료 |
| PR-OLD-6 | `ef6a25be`, `50f90a26` | `Transformer::getTokenizer()`, `SentenceTransformer::getEmbeddingDim()` accessor 및 테스트 | 완료 |

OLD 누적 검증 결과:

| 검증 | 결과 | 비고 |
|---|---|---|
| nntrainer x86 configure/build | PASS | `meson setup build --reconfigure -Denable-transformer=true`, `ninja -C build` |
| nntrainer focused unit tests | PASS | chat template, streaming, cancel, embedding sidecar, memory pool, CausalLM models |
| nntrainer Gemma4 x86 runtime | PASS | `The capital of Korea is **Seoul**.` |
| Quick.AI x86 build/runtime | PASS | main 기반 nntrainer에 맞춘 validation patch 포함 |
| Quick.AI Android native build/install | PASS | `R3CX80H8Y0F` 설치까지 확인 |
| Android app build/install | PASS | `QuickDotAI`, `SampleTestAPP` assemble/install |
| Android QNN native build | BLOCKED_ENV | `-Dqnn-sdk-root=<path-to-qcom-qnn-sdk>` 필요 |

상세 보고서:

- `docs/verification/nntrainer-migration/old-cumulative-validation.ko.md`
- `docs/verification/nntrainer-migration/pr-old-*.ko.md`
- `docs/verification/nntrainer-migration/quickai-main-compatibility-blockers.ko.md`

## 완료된 sync_v0.4.0 분할 이식

`sync_v0.4.0`은 병합 대상이 아니라 참고 소스로만 사용했다. 큰 import
commit `2e8eaa9d...`를 그대로 가져오지 않고, 현재 main 구조에 맞춰
기능 단위 PR로 다시 만들었다.

| PR | Branch / Head | 내용 | 누적 반영 | 검증 상태 |
|---|---|---|---|---|
| PR-SYNC-1 | `migration/pr-sync-1-lfm2-prereq` / `b1674b2e` | LFM2 prerequisite kernels/layers: causal depthwise conv1d, custom multiply, hybrid decoder tolerance | 반영됨 | PASS |
| PR-SYNC-2 | `migration/pr-sync-2-lfm2-text` / `468b1be7` | LFM2 text CausalLM model, hybrid KV binding, API/quantize registration | 반영됨 | PASS |
| PR-SYNC-3 | `migration/pr-sync-3-multimodal-foundation` / `34ae1841` | generic multimodal hooks, LFM2 `use_embedding`, external QNN input plumbing | 반영됨 | PASS |
| PR-SYNC-4 | `migration/pr-sync-4-lfm2-vl` / `879f0418` | LFM2-VL vision tower, connector, composition skeleton | 반영됨 | PASS |
| PR-SYNC-5a | `migration/pr-sync-5a-lfm2-vl-converters` / `443506b7` | LFM2-VL converters/resources/config template | 반영됨 | PASS |
| PR-SYNC-5b | `migration/pr-sync-5b-lfm2-vl-runtime` / `5b05ea49` | LFM2-VL generic factory/runtime path wiring, `image_tensor_path` raw FP32 input | 아직 미반영 | 단독 PASS |

sync 누적 브랜치 현재 상태:

```text
/home/jrock/nntrainer-sync-cumulative
branch: migration/sync-cumulative-validation
head:   64aa176a6a4bfe575295cd8988914167454d4bce
scope:  OLD cumulative + PR-SYNC-1..5a
```

PR-SYNC-5b는 아직 이 누적 브랜치에 cherry-pick하지 않았다.

## PR-SYNC-5b에서 방금 끝낸 작업

PR-SYNC-5b는 LFM2-VL을 `main.cpp`에 특수분기로 넣지 않고 기존
`Factory` / `Transformer` 경로에 태우는 작업이다.

구현한 내용:

- `Lfm2VlForConditionalGeneration`을 `Transformer` subclass로 변경.
- `main.cpp`에 `Lfm2VlForConditionalGeneration` factory 등록 추가.
- `Applications/CausalLM/runtime_config.{h,cpp}` 추가.
- `nntr_config.json` 상대 경로 resolve 범위를 확장:
  `model_file_name`, `tokenizer_file`, `embedding_file_name`,
  `ple_file_name`, `embedding_bin_path`, `vision_model_file`,
  `connector_model_file`, `image_path`, `image_tensor_path`.
- LFM2-VL weight loading이 LM/vision/connector 파일을 model directory
  기준으로 해석하도록 수정.
- generic `run(prompt, ...)`은 `image_tensor_path`가 비어 있으면 명시적
  error를 내고, 현재는 preprocessed raw FP32 tensor만 받도록 제한.
- README/config/test를 5b 범위에 맞게 업데이트.

5b 검증 결과:

| 검증 | 결과 | 비고 |
|---|---|---|
| `git diff --check 443506b7..HEAD` | PASS | 공백 문제 없음 |
| x86 reconfigure/build | PASS | `meson setup build --reconfigure -Denable-transformer=true`, `ninja -C build` |
| focused tests | PASS | `unittest_lfm2_vl_converters`, `unittest_lfm2_vl_vision_transformer`, `unittest_causallm_models`, `unittest_chat_template` |
| Gemma4 x86 runtime | PASS | `The capital of Korea is **Seoul**.` |
| Android generic package | PASS | `./tools/package_android.sh ... -Denable-transformer=true` |
| CausalLM NDK app | BLOCKED_ENV | `Applications/CausalLM/lib/libtokenizers_android_c.a` 없음 |
| Android LFM2-VL CLI | PENDING_INPUT_TENSOR | 기기 package에는 `sample.png`만 있고 `image_tensor_path` 없음 |

5b review:

- Spec review: Critical/P1/P2 functional issue 없음.
- Code-quality review: Critical/P1/P2 issue 없음.
- 리뷰에서 남긴 잔여 test gap: 실제 LFM2-VL full E2E inference는 아직
  raw FP32 tensor fixture와 full package/runtime 준비가 필요하다.

상세 보고서:

- `docs/verification/nntrainer-migration/pr-sync-5b-lfm2-vl-runtime.ko.md`

## Android/device 상태

연결 확인:

```text
R3CN80CW3FY device
R3CX80H8Y0F device
```

`R3CX80H8Y0F`의 LFM2-VL package:

```text
/sdcard/Download/aistudio-mobile/models/lfm2-vl/config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/generation_config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_connector.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_embedding.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_lm.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/lfm2_vl_450m_vision.bin
/sdcard/Download/aistudio-mobile/models/lfm2-vl/nntr_config.json
/sdcard/Download/aistudio-mobile/models/lfm2-vl/sample.png
/sdcard/Download/aistudio-mobile/models/lfm2-vl/tokenizer.json
```

현재 device `nntr_config.json`에는 `image_tensor_path`가 없다. PR-SYNC-5b는
이미지 파일 decode를 하지 않고 raw FP32 tensor path만 받으므로, 이 모델의
Android CLI 실행은 다음 image preprocessing 작업 전까지 `PENDING_INPUT_TENSOR`
상태다.

## 현재 blocker와 의도적으로 미룬 것

| 항목 | 상태 | 설명 |
|---|---|---|
| CausalLM Android NDK app build | BLOCKED_ENV | `Applications/CausalLM/lib/libtokenizers_android_c.a`가 없어 source compile 전 NDK가 중단됨 |
| Quick.AI Android QNN build | BLOCKED_ENV | QNN SDK root 경로 필요 |
| LFM2 text full runtime | PENDING_MODEL_FILES | text-only LFM2 converted package가 local/device inventory에 없음 |
| LFM2-VL Android/x86 full runtime | PENDING_INPUT_TENSOR / PENDING_PREPROCESSING | full package는 device에 있지만 raw FP32 tensor 또는 image decode path가 아직 없음 |
| Tokenizer cache/BPE/WordPiece/tokenizer_loader | DEFERRED_FINAL_STAGE | 사용자 지시에 따라 마지막 단계로 미룸 |
| Gauss3.8 계열 device test | SKIP_DEVICE_UNSUPPORTED | 해당 기기에서 작동하지 않을 것으로 사용자와 합의 |

## 문서화된 세부 보고서

주요 entry point:

- `docs/nntrainer-main-migration-verification-plan.ko.md`
- `docs/nntrainer-old-to-sync-main-audit.ko.md`
- `docs/nntrainer-migration-doubts.ko.md`
- `docs/verification/nntrainer-migration/sync-cumulative-validation.ko.md`
- `docs/verification/nntrainer-migration/old-cumulative-validation.ko.md`

PR별 보고서:

- `docs/verification/nntrainer-migration/pr-old-1-chattemplate-array-form.ko.md`
- `docs/verification/nntrainer-migration/pr-old-2a-streaming-callback.ko.md`
- `docs/verification/nntrainer-migration/pr-old-2b-cancel-model.ko.md`
- `docs/verification/nntrainer-migration/pr-old-3-embedding-sidecar-verification.ko.md`
- `docs/verification/nntrainer-migration/pr-old-4-qnn-memory.ko.md`
- `docs/verification/nntrainer-migration/pr-old-5-logits-processor.ko.md`
- `docs/verification/nntrainer-migration/pr-old-6-causallm-accessors.ko.md`
- `docs/verification/nntrainer-migration/pr-sync-1-lfm2-prereq.ko.md`
- `docs/verification/nntrainer-migration/pr-sync-2-lfm2-text.ko.md`
- `docs/verification/nntrainer-migration/pr-sync-3-multimodal-foundation.ko.md`
- `docs/verification/nntrainer-migration/pr-sync-4-lfm2-vl.ko.md`
- `docs/verification/nntrainer-migration/pr-sync-5a-lfm2-vl-converters.ko.md`
- `docs/verification/nntrainer-migration/pr-sync-5b-lfm2-vl-runtime.ko.md`

## 다음에 할 일

중단된 지점에서 이어가면 순서는 다음이 맞다.

1. PR-SYNC-5b를 `/home/jrock/nntrainer-sync-cumulative`에 cherry-pick.
2. sync 누적 브랜치에서 다시 검증:
   - `git diff --check`
   - `meson setup build --reconfigure -Denable-transformer=true`
   - `ninja -C build`
   - 관련 unit tests
   - Gemma4 x86 runtime
   - Android generic package
3. `sync-cumulative-validation.ko.md`를 PR-SYNC-5b 포함 상태로 업데이트.
4. 다음 LFM2-VL runtime 작업을 분리:
   - Android ARM numerics/compatibility 검증 작업
   - image preprocessing 또는 raw FP32 tensor fixture 기반 CLI E2E 작업
   - 이후 Quick.AI `causal_lm_api` 경유 integration 작업
5. sync_v0.4.0의 남은 non-tokenizer 변경을 다시 audit.
6. 마지막에 tokenizer cache/BPE/WordPiece/tokenizer_loader 계열을 별도 PR
   묶음으로 검토.

## 진행률 감각

- OLD_NNTRAINER의 main 이식 대상 핵심 기능: 완료.
- OLD 누적 + Quick.AI baseline validation: 완료.
- sync_v0.4.0 LFM2/LFM2-VL 구조 이식: 상당 부분 완료.
- LFM2-VL 실제 이미지 E2E runtime: 아직 진행 중, 5b는 그 직전의 generic
  runtime path 준비 단계.
- Tokenizer 계열: 의도적으로 미착수.
