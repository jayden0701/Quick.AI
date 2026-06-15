# nntrainer Main Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `OLD_NNTRAINER/.git`의 `quickdotai_api_refact` 변경을 Android/product code 없이 nntrainer `main`에 기능 단위로 이식하고, 그 뒤 `sync_v0.4.0`의 아직 병합되지 않은 기능을 PR 단위로 검증하며 가져온다.

**Architecture:** `sync_v0.4.0`은 병합 대상이 아니라 참고 자료로만 사용한다. 모든 PR은 `main`에서 새로 시작하고, Quick.AI 앱 검증은 이전 PR들이 누적된 별도 integration validation branch에서 수행한다. Tokenizer cache/BPE/WordPiece/tokenizer_loader 계열은 검증이 덜 되었으므로 전체 migration의 마지막 순서로 둔다.

**Quick.AI validation baseline:** Quick.AI top-level에는 별도 진행 중인 기능이
섞일 수 있으므로, nntrainer migration 검증 중 Quick.AI 쪽 동작이 어색하거나
현재 HEAD 변화와 충돌할 때는 `0e25e35d3eae099f318a166fb3aad6fb23d80aa0`
(`feat(android): add configurable model base path and refine model selection UI`)
기준으로 checkout/worktree를 만들어 검증한다.

**Tech Stack:** nntrainer Meson/Ninja, C++17 CausalLM, Android NDK/Gradle, Quick.AI native API, Android device `R3CX80H8Y0F`.

---

## 기준

### Source branches

```text
OLD source git: /home/jrock/Quick.AI/nntrainer/OLD_NNTRAINER/.git
OLD branch:     quickdotai_api_refact
OLD HEAD:       e9692a609d5d652cce04e8c2f8d486c785a0bb1f
OLD range:      dcacb60c3f5a2e8d6037309f4c0e9de156010f92^..quickdotai_api_refact

External main:  f2c0e6ae30ceb1ff418064f02b2f95804f294d24
sync_v0.4.0:    92f8ce201545f4dff22bf4171b942568c291995c
large import:   2e8eaa9d16fe5f97c0c501b1c043c3818a90319f
Quick.AI validation baseline:
                 0e25e35d3eae099f318a166fb3aad6fb23d80aa0
```

### Non-negotiable rules

- Do not merge `sync_v0.4.0` into `main`.
- Do not replay `2e8eaa9d` as a PR.
- Do not upstream Android app/service/AAR/SampleTestAPP/product packaging from OLD.
- Do not replace nntrainer `Applications/CausalLM/api/causal_lm_api.*` with `quick_dot_ai_api.*`.
- Quick.AI's `quick_dot_ai_api` must call into nntrainer through
  `causal_lm_api`; product-specific wrappers stay outside nntrainer, while
  generic runtime capabilities currently hidden in `quick_dot_ai_api` may be
  moved into `causal_lm_api` or CausalLM runtime classes as separate PRs.
- Do not port features already in `main` unless a narrow missing delta is proven.
- Keep tokenizer cache/BPE/WordPiece/tokenizer_loader until the final stage.
- Every PR or work unit must have a verification report.

## Worktree Strategy

Use separate worktrees. Do not do PR work directly in the current dirty
`/home/jrock/Quick.AI/nntrainer` checkout.

```bash
export QAI_ROOT=/home/jrock/Quick.AI
export NNTR_CURRENT=$QAI_ROOT/nntrainer
export OLD_GIT=$QAI_ROOT/nntrainer/OLD_NNTRAINER/.git

git -C "$NNTR_CURRENT" fetch --all --tags
git -C "$NNTR_CURRENT" worktree add ../nntrainer-main-migration-main main
```

For each PR:

```bash
cd /home/jrock/Quick.AI
git -C nntrainer worktree add ../nntrainer-pr-<pr-id> main
cd ../nntrainer-pr-<pr-id>
git switch -c <pr-id>
```

Maintain one cumulative validation branch:

```bash
cd /home/jrock/Quick.AI
git -C nntrainer worktree add ../nntrainer-migration-validation main
cd ../nntrainer-migration-validation
git switch -c migration/nntrainer-main-validation
```

After each PR branch passes standalone nntrainer checks, merge or cherry-pick
that PR into `migration/nntrainer-main-validation` and run Quick.AI integration
checks there. This avoids false failures caused by testing Quick.AI app against
an isolated PR that lacks previous migration dependencies.

## Phase 0 Tracking Documents

Detailed Phase 0 artifacts:

- `docs/nntrainer-old-to-sync-main-audit.ko.md`: OLD/sync/main comparative
  audit.
- `docs/verification/nntrainer-migration/phase0-old-commit-classification.ko.md`:
  OLD commit classification and first PR order.
- `docs/verification/nntrainer-migration/phase0-android-model-availability.ko.md`:
  Android model availability on `R3CX80H8Y0F`.
- `docs/verification/nntrainer-migration/pr-old-2-streaming-cancel-scope.ko.md`:
  PR-OLD-2 streaming/cancel scope proposal.
- `docs/verification/nntrainer-migration/pr-old-2a-streaming-callback.ko.md`:
  PR-OLD-2a streaming callback implementation verification.
- `docs/verification/nntrainer-migration/pr-old-2b-cancel-model.ko.md`:
  PR-OLD-2b explicit cancel/lifetime verification.
- `docs/verification/nntrainer-migration/pr-old-3-embedding-sidecar-scope.ko.md`:
  PR-OLD-3 CausalLM embedding sidecar LUT scope decision.
- `docs/verification/nntrainer-migration/pr-old-3-embedding-sidecar-verification.ko.md`:
  PR-OLD-3 CausalLM embedding sidecar LUT implementation verification.
- `docs/verification/nntrainer-migration/pr-old-4-qnn-memory.ko.md`:
  PR-OLD-4 QNN MemoryPool planned-offset allocation verification.
- `docs/verification/nntrainer-migration/pr-old-5-logits-processor.ko.md`:
  PR-OLD-5 generic logits processor hook verification.
- `docs/verification/nntrainer-migration/pr-old-6-causallm-accessors.ko.md`:
  PR-OLD-6 narrow CausalLM accessor verification.
- `docs/verification/nntrainer-migration/old-cumulative-validation.ko.md`:
  PR-OLD-1..6 누적 validation branch의 nntrainer/Quick.AI 검증 결과.
- `docs/verification/nntrainer-migration/quickai-main-compatibility-blockers.ko.md`:
  Quick.AI top-level blockers found when building against main-based nntrainer.
- `docs/verification/nntrainer-migration/pr-sync-1-lfm2-prereq.ko.md`:
  PR-SYNC-1 LFM2 prerequisite kernels/layers verification.
- `docs/verification/nntrainer-migration/sync-cumulative-validation.ko.md`:
  Sync cumulative branch verification after PR-SYNC-1..5a.
- `docs/verification/nntrainer-migration/pr-sync-2-lfm2-text.ko.md`:
  PR-SYNC-2 LFM2 text model verification.
- `docs/verification/nntrainer-migration/pr-sync-3-multimodal-foundation.ko.md`:
  PR-SYNC-3 generic multimodal foundation hook verification.
- `docs/verification/nntrainer-migration/pr-sync-4-lfm2-vl.ko.md`:
  PR-SYNC-4 LFM2-VL vision tower/connector structural support verification.
- `docs/verification/nntrainer-migration/pr-sync-5a-lfm2-vl-converters.ko.md`:
  PR-SYNC-5a LFM2-VL converter/resource reproducibility verification.
- `docs/nntrainer-migration-doubts.ko.md`: unresolved policy/design questions.

Completed OLD PR order and current next stage:

1. PR-OLD-1: ChatTemplate HF array-form support.
2. PR-OLD-2a: generic CausalLM streaming callback and callback-driven
   cooperative stop for the current synchronous run.
3. PR-OLD-2b: explicit cross-thread cancel/lifetime contract.
4. PR-OLD-3: CausalLM EmbeddingLayer sidecar LUT support.
5. PR-OLD-4: QNN/RPC MemoryPool allocation by planned offset.
6. PR-OLD-5: generic logits processor/provider hook for XGrammar-style
   constrained decoding, without an XGrammar dependency in nntrainer.
7. PR-OLD-6: narrow accessors needed by Quick.AI integration:
   `Transformer::getTokenizer()` and `SentenceTransformer::getEmbeddingDim()`.
8. Minja `capitalize`: verify only. Current main's minja submodule already
   contains it; no code PR unless a target branch lacks that submodule commit.
9. OLD residual audit: no remaining non-product, non-tokenizer OLD runtime PR
   was found after PR-OLD-1..6.
10. PR-SYNC-1: LFM2 prerequisite kernels and layers.
11. Sync cumulative validation branch: `migration/sync-cumulative-validation`
    at `b1674b2eff2daf6d45729e5146ad82579ff154b9`.
12. PR-SYNC-2: LFM2 CausalLM text model, implemented locally as
    `migration/pr-sync-2-lfm2-text`
    (`468b1be7a5f82309c144051d4ddddaf9d5ff35b5`), final spec/code-quality
    re-review passed.
13. Next active stage after PR-SYNC-2 review: merge PR-SYNC-2 into
    `migration/sync-cumulative-validation` and rerun cumulative nntrainer
    validation. Completed at
    `baff1b6f5f9f2b0808386625149ee0f1df353125`.
14. PR-SYNC-3: generic multimodal foundation hooks, implemented locally as
    `migration/pr-sync-3-multimodal-foundation`
    (`34ae1841d92bebf8e40c4d992079344ac0dffd9a`), final spec/code-quality
    re-review passed.
15. Sync cumulative validation branch now includes PR-SYNC-3 at
    `1651f66d834ed86ec4163b312847b40386f928d2`; x86 build/runtime, relevant
    unit tests, diff check, and Android package passed.
16. PR-SYNC-4: LFM2-VL vision tower/connector structural support, implemented
    locally as `migration/pr-sync-4-lfm2-vl`
    (`879f04185ed6a0d729f9ad9816e62edf4db4e327`), final spec/code-quality
    re-review passed.
17. Sync cumulative validation branch now includes PR-SYNC-4 at
    `b24b3a556901b919dfa4a3327a8b86fced303281`; x86 build/runtime, relevant
    unit tests, diff check, Android package, and Android NDK syntax probes
    passed. Direct CausalLM NDK app remains `BLOCKED_ENV` because
    `Applications/CausalLM/lib/libtokenizers_android_c.a` is missing.
18. PR-SYNC-5a: LFM2-VL converter/resource reproducibility, implemented
    locally as `migration/pr-sync-5a-lfm2-vl-converters`
    (`443506b7933650f3f96d4fd03f4f5ffa55a64014`), final code-quality
    re-review passed.
19. Sync cumulative validation branch now includes PR-SYNC-5a at
    `64aa176a6a4bfe575295cd8988914167454d4bce`; x86 build/runtime, 8 relevant
    unit tests, diff check, and Android package passed.
20. Next sync stage: PR-SYNC-5b LFM2-VL generic runtime wiring
    (`nntr_causallm`/factory/config path support), without tokenizer cache or
    app raw-pixel APIs.
21. Final tokenizer stage: tokenizer cache, WordPiece, BPE, tokenizer_loader.

## Verification Contract

Each PR/work unit must produce:

```text
docs/verification/nntrainer-migration/<pr-id>.ko.md
```

The report must include:

```markdown
# <pr-id> Verification Report

## Scope
- PR/work unit:
- Source commits consulted:
- Files changed:
- Affected models:

## Build Results
| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 | `meson build -Denable-transformer=true` then `ninja -C build` | PASS/FAIL | path |
| nntrainer x86 runtime | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS/FAIL/SKIP | output/log |
| Quick.AI x86 | `./build.sh --platform=x86 --target=src,api,api-test --clean` | PASS/FAIL/SKIP | path |
| Android native | `./build.sh --platform=android --target=src,api,api-test` | PASS/FAIL | path |
| Android QNN native | `./build.sh --platform=android --enable-qnn --target=src,api,api-test` | PASS/FAIL/BLOCKED_ENV | path |
| Android app | `./gradlew :QuickDotAI:assembleDebug :SampleTestAPP:assembleDebug` | PASS/FAIL | path |

## Runtime Results
| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|

Runtime rows include:

- x86 `nntr_causallm` execution with `/home/jrock/Quick.AI/gemma4_it_x86`.
  The pass criterion is not exact text matching; the generated text must be
  coherent/plausible for the prompt.
- Android CLI runs on `R3CX80H8Y0F` with models under
  `/sdcard/Download/aistudio-mobile/models`. Gauss3.8-family models on this
  device are recorded as `SKIP_DEVICE_UNSUPPORTED`.

## Missing Model Files
| Model | Required path | Missing files | Owner follow-up |
|---|---|---|---|

## Debugging Notes
- Failure:
- Root cause:
- Fix:
- Retest:
```

If model files are missing, record them in `Missing Model Files`. The build can
still pass, but the runtime result remains `PENDING_MODEL_FILES` rather than
`PASS`.

## Standard Verification Commands

### 1. nntrainer x86 build and runtime

Run in the nntrainer PR worktree.

```bash
cd /home/jrock/nntrainer-pr-<pr-id>

meson build -Denable-transformer=true
ninja -C build
```

Expected:

```text
build completes with exit code 0.
```

Then run the produced CausalLM CLI against the x86 Gemma4 package:

```bash
./build/Applications/CausalLM/nntr_causallm \
  /home/jrock/Quick.AI/gemma4_it_x86
```

If the binary path differs in a local Meson layout, find the `nntr_causallm`
target under `build/` and record the exact path in the report. The run passes
only when it exits successfully and prints a coherent answer. For the local
Gemma4 package, do not pass a raw prompt as argv[2] for this smoke test:
`main.cpp` treats argv[2] as raw text and bypasses chat-template application,
which can produce an immediate `<turn|>` stop token. The no-argv prompt path
uses `nntr_config.json.chat_input` and should answer that Korea's capital is
Seoul.

For CausalLM-related PRs, also run relevant unit tests:

```bash
meson test -C build --suite unittests --print-errorlogs
```

If the PR only touches a focused CausalLM test target, run the specific target
first, then the full relevant suite.

### 2. Quick.AI x86 build

Run from the Quick.AI root after pointing the submodule or validation checkout
at the PR result. If current Quick.AI HEAD contains unrelated in-progress app
work that obscures nntrainer migration failures, use a separate Quick.AI
worktree at `0e25e35d3eae099f318a166fb3aad6fb23d80aa0` and point that
worktree's `nntrainer` submodule to the cumulative validation branch.

```bash
cd /home/jrock/Quick.AI
./build.sh --platform=x86 --target=src,api,api-test --clean
```

Expected artifacts:

```text
builddir_x86/src/quick_dot_ai
builddir_x86/src/libquick_dot_ai.so
builddir_x86/api/libquick_dot_ai_api.so
builddir_x86/api-app/quick_dot_ai_test
```

### 3. Android native build

```bash
cd /home/jrock/Quick.AI
export NDK_ROOT=${NDK_ROOT:?set NDK_ROOT to Android NDK path}
export ANDROID_NDK=$NDK_ROOT

./build.sh --platform=android --target=src,api,api-test
./apk_install_android.sh
```

Expected artifacts:

```text
install_libs/libnntrainer.so
install_libs/libquick_dot_ai.so
install_libs/libquick_dot_ai_api.so
install_libs/quick_dot_ai_test
```

If the work unit directly affects QNN behavior, also run the QNN-enabled build:

```bash
./build.sh --platform=android --enable-qnn --target=src,api,api-test
```

If Meson reports that `qnn-sdk-root` is missing and no Qualcomm QNN SDK root is
available on the host, record this as `BLOCKED_ENV`, not as a source failure.

### 4. Android app build

```bash
cd /home/jrock/Quick.AI
mkdir -p Android/QuickDotAI/prebuilt_libs
cp install_libs/*.so Android/QuickDotAI/prebuilt_libs/

cd Android
./gradlew :QuickDotAI:assembleDebug :SampleTestAPP:assembleDebug
```

Expected:

```text
BUILD SUCCESSFUL
```

### 5. Android install on R3CX80H8Y0F

```bash
cd /home/jrock/Quick.AI/Android
export ANDROID_SERIAL=R3CX80H8Y0F
./gradlew :SampleTestAPP:installDebug
```

Expected:

```text
BUILD SUCCESSFUL
```

### 6. Android CLI runtime on R3CX80H8Y0F

`install_android.sh` creates `/data/local/tmp/Quick.AI/run_test.sh`, but the
explicit command below is preferred in reports because it records the exact
binary, library path, model base path, and arguments used.

```bash
cd /home/jrock/Quick.AI
export ANDROID_SERIAL=R3CX80H8Y0F
export DEVICE_QAI_DIR=/data/local/tmp/Quick.AI
export DEVICE_MODEL_BASE=/sdcard/Download/aistudio-mobile/models

./install_android.sh

adb -s "$ANDROID_SERIAL" shell "
  cd $DEVICE_QAI_DIR &&
  export LD_LIBRARY_PATH=$DEVICE_QAI_DIR:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test qwen3-0.6b 'Return one short sentence.' true W4A32 false $DEVICE_MODEL_BASE
"
```

For each affected model id whose files are present under
`/sdcard/Download/aistudio-mobile/models`, run the same CLI shape:

```bash
export MODEL_ID=qwen3-0.6b

adb -s "$ANDROID_SERIAL" shell "
  cd $DEVICE_QAI_DIR &&
  export LD_LIBRARY_PATH=$DEVICE_QAI_DIR:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test $MODEL_ID 'Return one short sentence.' true W4A32 false $DEVICE_MODEL_BASE
"
```

For JSON/chat-template/XGrammar-adjacent changes, run this Android CLI shape:

```bash
adb -s "$ANDROID_SERIAL" shell "
  cd $DEVICE_QAI_DIR &&
  export LD_LIBRARY_PATH=$DEVICE_QAI_DIR:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test --json-smoke qwen3-0.6b $DEVICE_MODEL_BASE
"
```

For direct tool/XGrammar-adjacent changes, run this Android CLI shape:

```bash
adb -s "$ANDROID_SERIAL" shell "
  cd $DEVICE_QAI_DIR &&
  export LD_LIBRARY_PATH=$DEVICE_QAI_DIR:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test --tool-json qwen3-0.6b $DEVICE_MODEL_BASE \
    '{\"prompt\":\"Return the exact tool JSON.\",\"tool_name\":\"migration_schema\",\"tool_schema\":{\"type\":\"object\",\"properties\":{\"status\":{\"type\":\"string\",\"enum\":[\"ok\"]}},\"required\":[\"status\"]}}' \
    W4A32
"
```

For QNN affected changes, use affected QNN model ids such as
`gemma4-e2b-qnn` or `vjepa-qnn` only when the required model files and QNN
assets exist on the device.

Gauss3.8-family models are known not to work on `R3CX80H8Y0F`. If an affected
model set includes Gauss3.8, record it as `SKIP_DEVICE_UNSUPPORTED` with the
device id and do not treat it as a regression for this migration.

## Affected Model Matrix

Each PR must fill this matrix before verification starts.

| Change type | Required model tests |
|---|---|
| Chat template/minja | Android CLI: `qwen3-0.6b`, `gemma4-cpu`, `function_gemma` if available |
| Logits mask/XGrammar hook | Android CLI: `qwen3-0.6b` JSON smoke, direct tool smoke |
| UINT4/signed 4bit embedding | Android CLI: models using W4A32 or signed/UINT4 embedding weights |
| Memory allocator/QNN ownership | Android CLI: CPU model smoke plus `gemma4-e2b-qnn` if available |
| LFM2 base | LFM2 text model if converted files exist; otherwise unit tests plus missing model report |
| Generic multimodal interface | LFM2-VL/SigLIP composition if available; otherwise structural tests |
| LFM2-VL | `lfm2-siglip` or monolithic LFM2-VL files if available |
| V-JEPA | Android CLI: `vjepa-qnn` if available |
| Tokenizer cache/BPE/WordPiece | Android CLI: TinyBERT/BERT tokenizer fixtures, Qwen tokenizer regression, model smoke |

## Debugging Rule

Do not start the next PR/work unit while the current one has a failed required
build or a failed available-model runtime test.

Allowed states:

```text
PASS
SKIP_NOT_AFFECTED
SKIP_DEVICE_UNSUPPORTED
PENDING_MODEL_FILES
DEFERRED_TO_NEXT_PR
BLOCKED_ENV
```

Disallowed states:

```text
FAIL
UNKNOWN
```

If a test fails:

1. Reproduce with the shortest command.
2. Identify the first bad commit in the current PR branch.
3. Fix only the failing scope.
4. Re-run the failed command.
5. Re-run all required commands for that PR.
6. Update the verification report with the failure, root cause, fix, and retest.

## Migration Order

The order below intentionally deviates from pure OLD-first sequencing in one
place: tokenizer work is OLD-derived, but it is moved to the final phase because
it needs extra validation.

### Phase 0: Migration Control Documents

**Files:**

- Already created: `docs/nntrainer-old-to-sync-main-audit.ko.md`
- Create/update per work unit: `docs/verification/nntrainer-migration/<pr-id>.ko.md`

- [x] **Step 0.1: Create commit classification matrix**

Create a table mapping OLD commits to one of:

```text
product-only
already-main
port-candidate
defer-tokenizer
sync-followup
obsolete
needs-design
```

Source command:

```bash
git --git-dir=/home/jrock/Quick.AI/nntrainer/OLD_NNTRAINER/.git \
  log --reverse --date=short \
  --pretty=format:'%h %ad %s' \
  dcacb60c3f5a2e8d6037309f4c0e9de156010f92^..quickdotai_api_refact
```

- [x] **Step 0.2: Create Android model availability matrix**

For `R3CX80H8Y0F`:

```bash
adb -s R3CX80H8Y0F shell \
  "find /sdcard/Download/aistudio-mobile/models -maxdepth 3 -type f \
   \\( -name config.json -o -name generation_config.json -o -name nntr_config.json -o -name tokenizer.json -o -name '*.bin' \\) 2>/dev/null | sort"
```

Record missing model files in the first verification report.

### Phase 1: OLD_NNTRAINER to main, Android/product removed, tokenizer deferred

#### PR-OLD-1: ChatTemplate/minja small delta only

Status: completed as `migration/pr-old-chattemplate`.

**Source commits:**

```text
5f0a53a6 Support HF-chat template feature
dd83ccab Add Enhanced Chat Template, and update nntrainer
2639f431 feat(chat_template): Support array-form chat_template and streaming API
a6fd285b [ChatTemplate] Extend minja engine - Adds "capitalize" as global filter
```

**Port rule:**

- Do not vendor minja headers from OLD/sync.
- Use `main`'s existing `Applications/CausalLM/third_party/minja` integration.
- Port only missing behavior, likely `capitalize` filter or array-form template gaps if still absent.

**Files to inspect:**

```text
Applications/CausalLM/chat_template.cpp
Applications/CausalLM/chat_template.h
Applications/CausalLM/third_party/minja
Applications/CausalLM/models/gemma4/*
```

**Verification focus:**

```text
qwen3-0.6b JSON smoke
gemma4-cpu/function_gemma if model files exist
Android app build
```

#### PR-OLD-2a/2b: Runtime streaming/cancel hooks

Status: completed as `migration/pr-old-streaming` and
`migration/pr-old-cancel`.

**Source commits:**

```text
81b303e4 CausalLM: add streaming C API + QuickAI JNI wiring for Qwen3
67a2836c support cancel while run
b45774ad support cancel while run
4f7c5607 feat(streaming): Add Streaming API with messages format support
```

**Port rule:**

- Do not port Quick.AI C API surface.
- Only port generic CausalLM runtime hooks if `main` lacks them and Quick.AI
  needs them.
- Keep streaming callback and explicit cross-thread cancellation as separate
  reviewable patches.

**Files to inspect:**

```text
Applications/CausalLM/models/causal_lm.cpp
Applications/CausalLM/models/causal_lm.h
Applications/CausalLM/api/causal_lm_api.cpp
Applications/CausalLM/api/causal_lm_api.h
```

**Verification focus:**

```text
qwen3-0.6b streaming smoke
Quick.AI app run/cancel manual or scripted test if UI automation exists
```

#### PR-OLD-3: CausalLM EmbeddingLayer sidecar LUT support

Status: completed as `migration/pr-old-embedding-sidecar`.

**Source commits:**

```text
76dc25a6 Update Embedding Layer to support UINT4 Quatized Weight
b64ac377 Support signed 4bit
```

**Port rule:**

- Start from `main`'s current embedding and safetensors/quantization structure.
- Do not copy OLD `embedding_layer.*` wholesale.
- First prove whether `main` already covers the dtype path.

**Files to inspect:**

```text
Applications/CausalLM/layers/embedding_layer.cpp
Applications/CausalLM/layers/embedding_layer.h
Applications/CausalLM/quantize.cpp
nntrainer/tensor/uint4_tensor.cpp
nntrainer/tensor/int4_tensor.cpp
nntrainer/utils/safetensors_util.cpp
```

**Verification focus:**

```text
unit tests for dtype loading if available
qwen3-0.6b W4A32 smoke if model files exist
any signed/UINT4 embedding model provided locally
```

#### PR-OLD-4: Memory allocator and ownership fixes

Status: completed as `migration/pr-old-qnn-memory`.

**Source commits:**

```text
8dbe0dae feat(allocator): add CpuMemAllocator and RpcMemAllocator
18c116a0 fix(memory): plug FSU/NPU memory leak via allocator integration
```

**Port rule:**

- Do not combine allocator abstraction with QNN behavior changes in one PR.
- If `main` already has an equivalent fix, skip.
- If a leak or ownership bug cannot be reproduced, document and do not port.

**Files to inspect:**

```text
nntrainer/mem_allocator.cpp
nntrainer/mem_allocator.h
nntrainer/tensor/manager.h
nntrainer/tensor/memory_pool.cpp
nntrainer/tensor/memory_pool.h
nntrainer/tensor/tensor_pool.cpp
nntrainer/tensor/tensor_pool.h
```

**Verification focus:**

```text
nntrainer memory unit tests
qwen3-0.6b load/unload smoke
gemma4-e2b-qnn load/unload on R3CX80H8Y0F if assets exist
```

#### PR-OLD-5: Generic logits-mask provider hook for structured decoding

Status: completed as `migration/pr-old-logits-processor`.

**Source commits:**

```text
5b19bf0c feat(xgrammar): Add XGrammar virtual methods and getVocabSize to Transformer base class
```

**Port rule:**

- Do not add XGrammar as an nntrainer dependency.
- Add only a generic optional logits-mask/provider interface if needed.
- Keep XGrammar manager/compiler/cache in Quick.AI top-level `src/xgrammar`.

**Files to inspect:**

```text
Applications/CausalLM/models/transformer.h
Applications/CausalLM/models/causal_lm.cpp
Applications/CausalLM/models/causal_lm.h
Applications/CausalLM/models/*/*causallm*.cpp
```

**Verification focus:**

```text
qwen3-0.6b normal generation
qwen3-0.6b JSON smoke
qwen3-0.6b direct tool smoke if Quick.AI integration provides XGrammar
```

#### PR-OLD-6: CausalLM accessors for Quick.AI integration

Status: completed as `migration/pr-old-causallm-accessors`.

**Port rule:**

- Keep this PR narrow.
- Add only accessors that are generic and already owned by CausalLM model
  classes.
- Do not add tokenizer cache/loader behavior here.
- Do not add multimodal pointer APIs here.

**Files to inspect:**

```text
Applications/CausalLM/models/transformer.h
Applications/CausalLM/models/sentence_transformer.h
test/unittest/models/unittest_causallm_qwen2.cpp
```

**Verification focus:**

```text
unittest_causallm_models
Quick.AI cumulative x86/Android build
Android CLI qwen3-0.6b regression
```

### Phase 2: sync_v0.4.0 unmerged functionality

#### PR-SYNC-1: LFM2 prerequisite kernels and layers

**Status:** implemented locally as `migration/pr-sync-1-lfm2-prereq`
(`b1674b2eff2daf6d45729e5146ad82579ff154b9`). Verification report:
`docs/verification/nntrainer-migration/pr-sync-1-lfm2-prereq.ko.md`

**Source commits:**

```text
a53f89c0 cpu_backend: add causal depthwise conv1d kernels
31e92d39 causallm: add LFM2 support layers
9fd46c0f causallm: support hybrid decoder configs
605c6629 [CausalLM] Apply thread_manager in causalm_conv1d_layer.cpp
```

**Port rule:**

- Keep this PR model-light: kernels, layers, config support only.
- Do not include LFM2-VL, V-JEPA, QuickAI API, or tokenizer work.

**Files to inspect:**

```text
Applications/CausalLM/layers/causal_conv1d_layer.*
Applications/CausalLM/layers/custom_multiply.*
Applications/CausalLM/layers/shared_fully_connected_layer.*
Applications/CausalLM/layers/meson.build
nntrainer/tensor/cpu_backend/*
```

**Verification focus:**

```text
unit tests for new kernels/layers
full x86 build
Gemma4 x86 CLI smoke
shared/plugin build for PLUGGABLE path
ARMv7 compile probe
Android native package build
```

#### PR-SYNC-2: LFM2 CausalLM text model

**Status:** implemented locally as `migration/pr-sync-2-lfm2-text`
(`468b1be7a5f82309c144051d4ddddaf9d5ff35b5`). Verification report:
`docs/verification/nntrainer-migration/pr-sync-2-lfm2-text.ko.md`.
Final spec/code-quality re-review passed and the work is included in
`migration/sync-cumulative-validation`.

**Source commits:**

```text
a79ad18e causallm: add LFM2 CausalLM model
46cad449 causallm: fix untied LFM2 lm head scope
0e9fcb9b [CausalLM] Update LFM2 config reading
```

**Port rule:**

- Build on PR-SYNC-1.
- Text LFM2 only.
- Do not include LFM2-VL vision tower or image input.

**Files to inspect:**

```text
Applications/CausalLM/models/lfm2/lfm2_causallm.*
Applications/CausalLM/models/lfm2/meson.build
Applications/CausalLM/models/meson.build
Applications/CausalLM/main.cpp
```

**Verification focus:**

```text
LFM2 text model if files exist
registration/loading for causal_conv1d and custom_multiply layers
Android CLI qwen3-0.6b regression smoke
Android app build
```

#### PR-SYNC-3: Multimodal/LFM2-VL foundation only

**Status:** implemented locally as
`migration/pr-sync-3-multimodal-foundation`
(`34ae1841d92bebf8e40c4d992079344ac0dffd9a`). Verification report:
`docs/verification/nntrainer-migration/pr-sync-3-multimodal-foundation.ko.md`.
Final spec/code-quality re-review passed and the work is included in
`migration/sync-cumulative-validation`.

**Source commits:**

```text
c3a44e11 Fix : deliver externally-fed inputs to QNN graph input tensors
12d6ca9d feat(transformer): add model-agnostic multimodal embedding interface
00a44e01 fix(transformer): add virtual getKvLen()
5c07ad06 fix(transformer): tolerate missing tokenizer_file
c1b8a569 [CausalLM] Make KV cache binding overridable in SentenceTransformer
32b0b559 [CausalLM] Support run function for LFM in use_embedding
```

**Port rule:**

- Keep this as a foundation PR, not the full LFM2-VL model tree.
- Preserve PR-OLD streaming/cancel/logits/accessor behavior in `causal_lm.*` and
  `transformer.*`.
- Treat QNN externally-fed input delivery as part of the multimodal foundation,
  but do not reintroduce the old memory-pool patch if PR-OLD-4 already covers
  the required allocation behavior.
- Do not include full vision tower, image preprocessing, converters, V-JEPA, or
  tokenizer work.

**Files to inspect:**

```text
nntrainer/graph/network_graph.h
nntrainer/layers/input_layer.cpp
nntrainer/models/neuralnet.cpp
Applications/CausalLM/models/transformer.h
Applications/CausalLM/models/transformer.cpp
Applications/CausalLM/models/sentence_transformer.*
Applications/CausalLM/models/causal_lm.*
Applications/CausalLM/models/lfm2/lfm2_causallm.*
```

**Verification focus:**

```text
existing text models still run
structural unit tests for multimodal hooks
Android app build
```

#### PR-SYNC-4: LFM2-VL vision tower and connector

**Source commits:**

```text
7feb453f [CausalLM] Port LFM2-VL multimodal model onto #3963 base
e92307b7 [CausalLM] Fix LFM2-VL ViT FP32 KV cache, USE_EMBEDDING path, and shared weight loading
```

**Port rule:**

- Build on PR-SYNC-2 and PR-SYNC-3.
- Include vision tower and connector implementation.
- Keep image file input and converter scripts for the next PR unless needed for tests.

**Files to inspect:**

```text
Applications/CausalLM/models/lfm2/lfm2-vl/vision/*
Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_connector.*
Applications/CausalLM/models/lfm2/lfm2-vl/meson.build
test/unittest/layers/unittest_lfm2_vl_vision_transformer.cpp
```

**Verification focus:**

```text
unittest_lfm2_vl_vision_transformer
LFM2 text regression
Android native build
Android app build
```

#### PR-SYNC-5a: LFM2-VL converter/resource reproducibility

**Status:** completed locally as
`migration/pr-sync-5a-lfm2-vl-converters`
(`443506b7933650f3f96d4fd03f4f5ffa55a64014`). Cumulative validation branch
includes it at `64aa176a6a4bfe575295cd8988914167454d4bce`.

**Source commits:**

```text
9f954843 [CausalLM] Fix LFM2-VL LM converter for hybrid conv/attn architecture
0b52d15e [CausalLM] Add LFM2-VL weight converters and config templates
313229d1 Restore verified LFM2-VL LM converter order
3dbd4d97 [CausalLM] Write vision position embedding in convert_vision_hf.py
```

**Port rule:**

- Add converter scripts, `nntr_config.json` template, and README only.
- Do not claim current runtime readiness.
- Document current limitations:
  - `embedding_bin_path` is not yet resolved relative to model directory.
  - current LFM2 parser accepts `text_config.conv_L_cache == 2`; LiquidAI HF
    config may use `3`.

**Files to inspect:**

```text
Applications/CausalLM/res/lfm2-vl/*
test/unittest/models/unittest_lfm2_vl_converters.py
```

**Verification focus:**

```text
unittest_lfm2_vl_converters
LFM2-VL structural regression
x86 Gemma4 regression
Android package
```

#### PR-SYNC-5b: LFM2-VL generic runtime wiring

**Source commits to inspect:**

```text
45988882 feat(lfm2-vl): portable path resolution and complete nntr_config
00dbb6ea Remove legacy image_tensor_path
```

**Port rule:**

- Wire `Lfm2VlForConditionalGeneration` through generic CausalLM runtime and
  `nntr_causallm` only where it fits the main branch factory/config structure.
- Add portable model-directory path resolution, including `embedding_bin_path`.
- Keep image-file preprocessing, Android ARM numerics, raw-pixel APIs, and app
  monolithic helpers out of this PR unless review proves they are inseparable.
- Do not include tokenizer cache/BPE/WordPiece/tokenizer_loader changes.

**Files to inspect:**

```text
Applications/CausalLM/main.cpp
Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.*
Applications/CausalLM/models/lfm2/lfm2_causallm.*
Applications/CausalLM/res/lfm2-vl/nntr_config.json
```

**Verification focus:**

```text
LFM2-VL config loading unit/structural tests
x86 Gemma4 regression
Android package
Android CLI LFM2-VL stays DEFERRED unless image input is included
```

#### PR-SYNC-5c: LFM2-VL Android ARM numerics

**Source commits to inspect:**

```text
9d21f8f2 fix(lfm2-vl): correct Android ARM vision-tower numerics
```

**Port rule:**

- Isolate numerics fixes from runtime/user-facing wiring.
- Prove the fix with focused tests or Android device output where model files
  exist.

**Verification focus:**

```text
LFM2-VL vision/unit regression
Android package
R3CX80H8Y0F LFM2-VL affected path if runtime is available by then
```

#### PR-SYNC-5d: LFM2-VL image-file input and preprocessing

**Source commits to inspect:**

```text
5accdb6f feat(lfm2-vl): add real image file input with in-binary preprocessing
```

**Port rule:**

- Add image-file CLI/input preprocessing only after generic runtime wiring is
  settled.
- Document exact preprocessing assumptions and image tensor layout.
- Use `/sdcard/Download/aistudio-mobile/models/lfm2-vl/sample.png` on
  `R3CX80H8Y0F` if runtime and model files are available.

**Files to inspect:**

```text
Applications/CausalLM/image_util.h
Applications/CausalLM/stb_image.inc
Applications/CausalLM/main.cpp
Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_model.*
```

**Verification focus:**

```text
LFM2-VL image test on x86/Android if model files exist
x86 Gemma4 regression
Android package
```

#### PR-SYNC-5e: app raw-pixel helpers, only if accepted as generic API

**Source commits to inspect:**

```text
908ee9cd feat(lfm2-vl-vit): add runFromPixels raw-NCHW entry
92f8ce20 feat(lfm2-vl): add runFromPixels + getLM for app monolithic path
```

**Port rule:**

- Treat these as app/integration conveniences first, not automatic upstream
  nntrainer API.
- Upstream only the generic part that clearly belongs in `causal_lm_api` or
  model runtime; otherwise leave it for Quick.AI-side wrappers.

#### PR-SYNC-6: V-JEPA 2.1 ViT-B

**Source commits:**

```text
e1dc4059 [CausalLM] Add vjepa project modules to merge the video frames
9d505253 [Application/CausalLM] Add V-JEPA 2.1 ViT-B video encoder
baea966f fix(causallm): resolve committed merge-conflict markers from vjepa ViT commit
```

**Port rule:**

- Re-read this area carefully; sync had committed conflict markers.
- Do not cherry-pick blindly.
- Separate V-JEPA layers from model/converter if needed.

**Files to inspect:**

```text
Applications/CausalLM/layers/vjepa_*
Applications/CausalLM/models/vjepa2_vit/*
Applications/CausalLM/res/vjepa2/*
Applications/CausalLM/layers/mha_core.*
```

**Verification focus:**

```text
V-JEPA unit/structural tests
vjepa-qnn on R3CX80H8Y0F if assets exist
Android app build
```

#### PR-SYNC-7: QNN externally-fed input fix

**Status:** folded into PR-SYNC-3 unless device validation shows it must be
split again.

**Original source commits:**

```text
5a0f2e17 Change memory_pool.cpp for QNN
c3a44e11 Fix : deliver externally-fed inputs to QNN graph input tensors
```

**Port rule:**

- `5a0f2e17` overlaps with PR-OLD-4 memory ownership work and should not be
  cherry-picked blindly.
- `c3a44e11` should move with the multimodal foundation PR because it is needed
  by externally-fed inputs.
- Require device verification or mark `PENDING_MODEL_FILES` if QNN model assets
  are unavailable.

**Files to inspect:**

```text
nntrainer/tensor/memory_pool.cpp
nntrainer/tensor/memory_pool.h
nntrainer/qnn/*
src/models/qnn/*
```

**Verification focus:**

```text
gemma4-e2b-qnn on R3CX80H8Y0F if available
vjepa-qnn on R3CX80H8Y0F if available
CPU qwen3 regression
```

### Phase 3: Deferred tokenizer work, final stage

Tokenizer work is last by explicit project decision.

#### PR-TOKENIZER-1: Tokenizer verification fixtures and cache utility

**Source commits:**

```text
f31096a2 [Tokenizer] Add tokenizer cache utilities
```

**Port rule:**

- First add validation fixtures or tests that compare token ids against known
  tokenizer outputs.
- Do not integrate loader behavior until fixtures pass.

**Files to inspect:**

```text
Applications/CausalLM/tokenizers/tokenizer_cache_util.h
Applications/CausalLM/tokenizers_cpp.h
test/unittest/*
```

**Verification focus:**

```text
tokenizer unit tests
qwen3-0.6b smoke
Android app build
```

#### PR-TOKENIZER-2: WordPiece tokenizer

**Source commits:**

```text
d64ce8c8 [Tokenizer] Add WordPiece tokenizer support
```

**Port rule:**

- Keep BPE out of this PR.
- Test TinyBERT/BERT token ids with fixed input strings.

**Files to inspect:**

```text
Applications/CausalLM/tokenizers/wordpiece_tokenizer.cpp
Applications/CausalLM/tokenizers_cpp.h
Applications/CausalLM/res/tiny-bert/*
```

**Verification focus:**

```text
TinyBERT/BERT tokenizer fixtures
Android CLI TinyBERT model smoke if model files exist
Android CLI qwen3 regression
```

#### PR-TOKENIZER-3: BPE tokenizer

**Source commits:**

```text
1c22c29e [Tokenizer] Add BPE tokenizer support
```

**Port rule:**

- Keep loader auto-selection out until BPE tests pass.
- Test known vocab/merges/tokenizer.json cases.

**Files to inspect:**

```text
Applications/CausalLM/tokenizers/bpe_tokenizer.cpp
Applications/CausalLM/tokenizers_cpp.h
```

**Verification focus:**

```text
BPE tokenizer fixtures
qwen tokenizer regression
Android CLI qwen3-0.6b model smoke
```

#### PR-TOKENIZER-4: tokenizer_loader integration

**Source commits:**

```text
61049fb5 [Tokenizer] Integrate compact tokenizers with build system
9c19da17 fix(tokenizer): handle missing tokenizer_file gracefully
```

**Port rule:**

- Integrate only after cache, WordPiece, and BPE are verified.
- Preserve existing HuggingFace tokenizer behavior.
- Missing `tokenizer_file` should be tolerated only for models that do not need
  text tokenization, such as vision encoder sub-models.

**Files to inspect:**

```text
Applications/CausalLM/models/tokenizer_loader.cpp
Applications/CausalLM/models/tokenizer_loader.h
Applications/CausalLM/models/transformer.cpp
Applications/CausalLM/meson.build
Applications/CausalLM/jni/Android.mk
```

**Verification focus:**

```text
qwen3-0.6b normal generation
TinyBERT/WordPiece if files exist
BPE fixture tests
vision encoder no-tokenizer-file load path if present
Android app build and install
```

## Explicit Skip List

Skip these from OLD/sync unless a later review proves a narrow missing delta:

```text
Applications/QuickAI/**
Applications/CausalLM/QuickAI/**
Android app/service/AAR/SampleTestAPP product code from OLD
prebuilt native libraries
quick_dot_ai_api.* inside nntrainer
Gemma4 base model already in main
Gemma4 chat-template port already in main
RMSNorm FP16 overflow fixes already in main
ThreadManager base already in main
DDTree already in main under nntrainer/utils/ddtree
safetensors utility and quantization series already in main
```

## Stop Conditions

Stop and review before continuing if any of these happens:

- A PR needs to delete or rename `Applications/CausalLM/api/causal_lm_api.*`.
- A PR needs to add `quick_dot_ai_api.*` to nntrainer.
- A PR changes Android product code while claiming to be an nntrainer upstream PR.
- A PR combines OLD-derived work with LFM2/LFM2-VL/V-JEPA work.
- A PR combines tokenizer changes with non-tokenizer runtime changes.
- A required build fails after two fix attempts.
- An available affected model fails on Android CLI on `R3CX80H8Y0F`.
- A model runtime test is skipped without either a `Missing Model Files` entry
  or a `SKIP_DEVICE_UNSUPPORTED` note for Gauss3.8-family models on
  `R3CX80H8Y0F`.

## Completion Criteria

The migration is complete only when:

- All non-skipped OLD-derived nntrainer runtime changes are ported or explicitly
  marked obsolete/already-main.
- Android/product OLD code is not present in nntrainer PRs.
- All selected sync-only features are ported after OLD-derived work.
- Tokenizer cache/BPE/WordPiece/tokenizer_loader work is completed last.
- Every PR/work unit has a verification report.
- The cumulative validation branch passes:

```bash
cd /home/jrock/Quick.AI
./build.sh --platform=x86 --target=src,api,api-test --clean
export NDK_ROOT=${NDK_ROOT:?set NDK_ROOT}
export ANDROID_NDK=$NDK_ROOT
./build.sh --platform=android --target=src,api,api-test
./apk_install_android.sh
mkdir -p Android/QuickDotAI/prebuilt_libs
cp install_libs/*.so Android/QuickDotAI/prebuilt_libs/
cd Android
./gradlew :QuickDotAI:assembleDebug :SampleTestAPP:assembleDebug
export ANDROID_SERIAL=R3CX80H8Y0F
./gradlew :SampleTestAPP:installDebug
```

and x86 build checks pass while all available affected-model runtime tests pass
through Android CLI on `R3CX80H8Y0F`.
