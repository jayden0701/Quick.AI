# OLD Cumulative Validation Report

Date: 2026-06-11

## Scope

- Work unit: PR-OLD-1..6 cumulative validation branch
- Worktree: `/home/jrock/nntrainer-old-cumulative`
- Branch: `migration/old-cumulative-validation`
- Base: `f2c0e6ae30ceb1ff418064f02b2f95804f294d24`
- Head: `50f90a26 test: cover CausalLM accessor hooks`
- Quick.AI validation worktree:
  `/home/jrock/Quick.AI-old-cumulative-validation`
- Quick.AI validation branch: `validation/old-cumulative-quickai`
- Quick.AI validation base:
  `0e25e35d3eae099f318a166fb3aad6fb23d80aa0`
- Quick.AI validation patches:
  - `30f4a4a validation: adapt QuickAI to main-based nntrainer`
  - `5dd7bb5 validation: serialize tool logits processor setup`

The Quick.AI validation worktree uses `nntrainer` as a symlink to
`/home/jrock/nntrainer-old-cumulative`. The symlink is intentionally not a
source patch and must not be committed as a migration change.

## Included PR Commits

| Order | Cumulative commit | Source PR branch | Feature |
|---|---:|---|---|
| 1 | `608278b4` | `migration/pr-old-chattemplate` | HF array-form chat template |
| 2 | `2b40a6b9` | `migration/pr-old-streaming` | CausalLM streaming callback API |
| 3 | `97127a6f` | `migration/pr-old-cancel` | explicit cooperative cancel API |
| 4 | `8e9dd262` | `migration/pr-old-embedding-sidecar` | CausalLM embedding sidecar LUT |
| 5 | `a28fa1a8` | `migration/pr-old-qnn-memory` | QNN MemoryPool planned-offset allocation |
| 6 | `bd64b5e2` | `migration/pr-old-logits-processor` | generic logits processor hook |
| 7 | `3466a14f` | `migration/pr-old-embedding-sidecar` | sidecar path/model config wiring |
| 8 | `ef6a25be` | `migration/pr-old-causallm-accessors` | tokenizer and embedding-dim accessors |
| 9 | `50f90a26` | `migration/pr-old-causallm-accessors` | accessor unit-test coverage |

## Conflict Resolution Notes

- `Applications/CausalLM/meson.build`
  - Kept focused test targets:
    - `unittest_chat_template`
    - `unittest_callback_streamer`
    - `unittest_cancel_api`
    - `unittest_embedding_sidecar_lut`
    - `unittest_causallm_models`
- `test/unittest/meson.build`
  - Kept `unittest_memory_pool`.
- `Applications/CausalLM/models/causal_lm.{h,cpp}`
  - Kept streaming/cancel API surface:
    `setStreamer`, `requestStop`, `prepareForRun`,
    `prepareStopRequestForRun`.
  - Kept logits processor API surface:
    `setLogitsProcessor`, `resetLogitsProcessor`.
- `Applications/CausalLM/models/transformer.h`
  - Kept generic `LogitsProcessor`, `getVocabSize()`, and
    `getTokenizer()` hooks without importing Quick.AI product API.
- `Applications/CausalLM/models/sentence_transformer.h`
  - Kept `getEmbeddingDim()` as a narrow accessor needed by Quick.AI
    embedding integration.
- Worktree setup required initializing the minja submodule:
  `Applications/CausalLM/third_party/minja` at `021c2293`.

## nntrainer Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 configure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | `build/meson-logs/meson-log.txt` |
| nntrainer x86 build | `ninja -C build` | PASS | console build finished |
| focused unit tests | `meson test -C build unittest_chat_template unittest_callback_streamer unittest_cancel_api unittest_embedding_sidecar_lut unittest_memory_pool unittest_causallm_models --print-errorlogs` | PASS | `build/meson-logs/testlog.txt` |
| post-accessor focused unit | `meson test -C build unittest_causallm_models --print-errorlogs` | PASS | `build/meson-logs/testlog.txt` |
| x86 CausalLM runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output answered `Seoul` |
| whitespace | `git diff --check f2c0e6ae30ceb1ff418064f02b2f95804f294d24..HEAD` | PASS | no output |

Fresh recheck at `2026-06-11 23:47:44 KST`:

```bash
ninja -C build &&
meson test -C build \
  unittest_chat_template \
  unittest_callback_streamer \
  unittest_cancel_api \
  unittest_embedding_sidecar_lut \
  unittest_memory_pool \
  unittest_causallm_models \
  --print-errorlogs &&
timeout 240 env NNTR_NUM_THREADS=4 \
  ./build/Applications/CausalLM/nntr_causallm \
  /home/jrock/Quick.AI/gemma4_it_x86 &&
git diff --check f2c0e6ae30ceb1ff418064f02b2f95804f294d24..HEAD
```

Result: PASS. Meson reported `Ok: 6`, Gemma4 generated
`The capital of Korea is **Seoul**.<turn|>`, and `git diff --check` produced no
output.

Focused unit result recorded for the cumulative branch:

```text
1/6 nntrainer:unittests / unittest_memory_pool           OK
2/6 nntrainer:unittests / unittest_embedding_sidecar_lut OK
3/6 nntrainer:unittests / unittest_callback_streamer     OK
4/6 nntrainer:unittests / unittest_chat_template         OK
5/6 nntrainer:unittests / unittest_cancel_api            OK
6/6 nntrainer:unittests / unittest_causallm_models       OK

Ok: 6
Fail: 0
```

Gemma4 x86 smoke output excerpt:

```text
The capital of Korea is **Seoul**.<turn|>

prefill: 13 tokens
generation: 9 tokens
```

## Quick.AI Build Results

Quick.AI was validated from the baseline worktree because current top-level
Quick.AI has unrelated in-flight app/API work.

| Check | Command | Result | Notes |
|---|---|---|---|
| Quick.AI x86 build | `./build.sh --platform=x86 --target=src,api,api-test` | PASS | built against symlinked cumulative nntrainer |
| Quick.AI x86 runtime | `LD_LIBRARY_PATH=... NNTR_NUM_THREADS=4 ./builddir_x86/src/quick_dot_ai /home/jrock/Quick.AI/gemma4_it_x86` | PASS | generated coherent Seoul answer |
| Quick.AI Android native | `./build.sh --platform=android --target=src,api,api-test` | PASS | non-QNN build |
| Android native install | `ANDROID_SERIAL=R3CX80H8Y0F ./install_android.sh` | PASS | installed native libs/tools to `/data/local/tmp/Quick.AI` |
| Android app build | `./gradlew :QuickDotAI:assembleDebug :SampleTestAPP:assembleDebug` | PASS | produced AAR/APK |
| Android app install | `ANDROID_SERIAL=R3CX80H8Y0F ./gradlew :SampleTestAPP:installDebug` | PASS | required one prior uninstall because existing package signature differed |
| Android QNN native build | `./build.sh --platform=android --enable-qnn --target=src,api,api-test` | BLOCKED_ENV | nntrainer requires `-Dqnn-sdk-root=<path-to-qcom-qnn-sdk>`; SDK root was not available in environment |

Fresh Quick.AI x86 recheck at `2026-06-11 23:48:05 KST`:

```bash
./build.sh --platform=x86 --target=src,api,api-test &&
timeout 240 env \
  LD_LIBRARY_PATH=/home/jrock/Quick.AI-old-cumulative-validation/nntrainer/builddir_x86/nntrainer:/home/jrock/Quick.AI-old-cumulative-validation/nntrainer/builddir_x86/api/ccapi:/home/jrock/Quick.AI-old-cumulative-validation/builddir_x86/src:/home/jrock/Quick.AI-old-cumulative-validation/builddir_x86/api:$LD_LIBRARY_PATH \
  NNTR_NUM_THREADS=4 \
  ./builddir_x86/src/quick_dot_ai \
  /home/jrock/Quick.AI/gemma4_it_x86
```

Result: PASS. Quick.AI generated
`The capital of Korea is **Seoul**.<turn|>`.

Fresh Android non-QNN recheck at `2026-06-11 23:48:56 KST`:

```bash
./build.sh --platform=android --target=src,api,api-test &&
ANDROID_SERIAL=R3CX80H8Y0F ./install_android.sh
```

Result: PASS. The build was up to date and `install_android.sh` pushed
`libcausallm.so`, `libquick_dot_ai.so`, `quick_dot_ai`,
`libquick_dot_ai_api.so`, `quick_dot_ai_test`, nntrainer libraries, and
`libc++_shared.so` to `/data/local/tmp/Quick.AI`.

Fresh Android CLI recheck at `2026-06-11 23:48:56 KST`:

```bash
adb -s R3CX80H8Y0F shell "
  cd /data/local/tmp/Quick.AI &&
  export LD_LIBRARY_PATH=/data/local/tmp/Quick.AI:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test qwen3-0.6b \
    'What is the capital of Korea?' \
    true W4A32 false /sdcard/Download/aistudio-mobile/models
"
adb -s R3CX80H8Y0F shell "
  cd /data/local/tmp/Quick.AI &&
  export LD_LIBRARY_PATH=/data/local/tmp/Quick.AI:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test gemma4_cpu \
    'What is the capital of Korea?' \
    true W4A32 false /sdcard/Download/aistudio-mobile/models
"
adb -s R3CX80H8Y0F shell "
  cd /data/local/tmp/Quick.AI &&
  export LD_LIBRARY_PATH=/data/local/tmp/Quick.AI:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test function_gemma \
    'Return a JSON object.' \
    true W4A32 false /sdcard/Download/aistudio-mobile/models
"
```

Result: PASS for process/runtime. `qwen3-0.6b` returned bounded JSON with
`"count": 10`, `gemma4_cpu` returned JSON containing the input query, and
`function_gemma` still exits 0 with valid metrics but keeps the existing output
quality note.

Fresh Android app recheck at `2026-06-11 23:49:21 KST`:

```bash
cd /home/jrock/Quick.AI-old-cumulative-validation/Android
mkdir -p QuickDotAI/prebuilt_libs
cp ../install_libs/*.so QuickDotAI/prebuilt_libs/
ANDROID_SERIAL=R3CX80H8Y0F \
  ./gradlew \
    :QuickDotAI:assembleDebug \
    :SampleTestAPP:assembleDebug \
    :SampleTestAPP:installDebug
```

Result: PASS. Gradle reported `BUILD SUCCESSFUL in 3s` and installed
`SampleTestAPP-debug.apk` on `SM-S936U - 15`. The existing warning about
`local.properties` `sdk.dir` still appears but does not block the build.

Quick.AI validation patch summary:

```text
30f4a4a validation: adapt QuickAI to main-based nntrainer
 api-app/test_api.cpp              |   5 +-
 api/quick_dot_ai_api.cpp          | 218 ++++++++++++++++++++++++++++++-------
 meson.build                       |  18 ++-
 src/meson.build                   |  32 ++++--
 src/models/qnn/quick_dot_ai_qnn.h |  16 ++-
```

Additional validation patch:

```text
5dd7bb5 validation: serialize tool logits processor setup
 api/quick_dot_ai_api.cpp | 85 +++++++++++++++++++++++++----------------------
```

Important Quick.AI adaptation choices:

- Use main-based CausalLM adapter helpers instead of broad sync-only
  `Transformer` hooks.
- Keep Quick.AI product API outside nntrainer.
- Route XGrammar integration through the generic nntrainer logits processor
  hook.
- Call `requestStop()` when the XGrammar matcher completes or terminates, so
  post-completion grammar exceptions do not occur.
- Use selected text-generation model index instead of hard-coded
  `h.models[0]`.
- Do not reset the logits processor immediately before running.
- Disable experimental multimodal paths unless
  `QUICKAI_ENABLE_EXPERIMENTAL_MULTIMODAL_NNTRAINER_API` is set; default is
  unsupported rather than calling sync-only APIs.
- Make Quick.AI Meson tolerate main's CausalLM layout:
  `third_party`, `huggingface_tokenizer.cpp`, `kv_cache_manager.cpp`, missing
  `tokenizers`, and excluded `unittest_*.cpp`.
- Add missing QNN header state such as `native_lib_dir_` in the Quick.AI
  adapter.
- Keep `runModelHandleWithTool()` from reading `h.models` or attaching
  XGrammar/logits processor state outside `h.mtx`; `run_on_handle()` now
  selects the text model and attaches/detaches the scoped grammar processor
  under the same run lock.

Code-review fix verification after `5dd7bb5`:

```bash
./build.sh --platform=x86 --target=src,api,api-test &&
timeout 240 env \
  LD_LIBRARY_PATH=/home/jrock/Quick.AI-old-cumulative-validation/nntrainer/builddir_x86/nntrainer:/home/jrock/Quick.AI-old-cumulative-validation/nntrainer/builddir_x86/api/ccapi:/home/jrock/Quick.AI-old-cumulative-validation/builddir_x86/src:/home/jrock/Quick.AI-old-cumulative-validation/builddir_x86/api:$LD_LIBRARY_PATH \
  NNTR_NUM_THREADS=4 \
  ./builddir_x86/src/quick_dot_ai \
  /home/jrock/Quick.AI/gemma4_it_x86 &&
./build.sh --platform=android --target=src,api,api-test &&
ANDROID_SERIAL=R3CX80H8Y0F ./install_android.sh &&
timeout 900 adb -s R3CX80H8Y0F shell "
  cd /data/local/tmp/Quick.AI &&
  export LD_LIBRARY_PATH=/data/local/tmp/Quick.AI:\$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test qwen3-0.6b \
    'What is the capital of Korea?' \
    true W4A32 false /sdcard/Download/aistudio-mobile/models
"
```

Result: PASS. x86 Gemma4 generated Seoul, Android build/install passed, and
Android `qwen3-0.6b` tool smoke returned bounded JSON with `"count": 10`.

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | nntrainer x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent answer: Seoul |
| `gemma4_it_x86` | Quick.AI x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `./builddir_x86/src/quick_dot_ai /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent answer: Seoul |
| `qwen3-0.6b` | Android Quick.AI test CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models` | `./quick_dot_ai_test qwen3-0.6b 'What is the capital of Korea?' true W4A32 false /sdcard/Download/aistudio-mobile/models` | PASS | exit 0; JSON output respected bounded schema |
| `gemma4_cpu` | Android Quick.AI test CLI | same | `./quick_dot_ai_test gemma4_cpu 'What is the capital of Korea?' true W4A32 false /sdcard/Download/aistudio-mobile/models` | PASS | exit 0; JSON output contained the query |
| `function_gemma` | Android Quick.AI test CLI | same | `./quick_dot_ai_test function_gemma 'Return a JSON object.' true W4A32 false /sdcard/Download/aistudio-mobile/models` | PASS_RUNTIME_WITH_QUALITY_NOTE | exit 0; generated function-style text, but content quality is odd and should be reviewed separately |
| `qwen3-0.6b` | Android direct Quick.AI CLI | direct model dir | `./quick_dot_ai /sdcard/Download/aistudio-mobile/models/qwen3-0.6b 'Return one short sentence.'` | FAIL_MODEL_CONFIG | model config contains stale host tokenizer path `/home/jayden/Quick_AI/5_1_release/Quick.AI/test_models/qwen3-0.6b/tokenizer.json` |
| `gemma4_cpu` | Android direct Quick.AI CLI | direct model dir | `./quick_dot_ai /sdcard/Download/aistudio-mobile/models/gemma4_cpu` | PASS | direct CLI path works for this model |
| Gauss3.8 family | Android | `R3CX80H8Y0F` | not run | SKIP_DEVICE_UNSUPPORTED | expected unsupported on this device |

The required Android migration smoke path is `quick_dot_ai_test` with explicit
model base path. The direct `quick_dot_ai` qwen3 failure is tracked as a model
config/path issue, not as evidence that the cumulative nntrainer branch failed.

## Missing Model Files and Environment Gaps

| Item | Required path/env | Status | Owner follow-up |
|---|---|---|---|
| QNN SDK root | `qnn-sdk-root` Meson option or equivalent SDK env | BLOCKED_ENV | provide Qualcomm QNN SDK path before `--enable-qnn` build can be required |
| `qwen3-0.6b` direct CLI tokenizer | `/sdcard/Download/aistudio-mobile/models/qwen3-0.6b/.../tokenizer.json` or relative config | FAIL_MODEL_CONFIG | clean model config or add direct-CLI path rebasing |
| `function_gemma` output quality | available model files | REVIEW_NEEDED | runtime exits 0, but generated text should be reviewed against expected schema/tool behavior |

No missing x86 model files were found for `gemma4_it_x86`.

## Debugging Notes

- First cumulative configure attempts failed when the worktree did not have
  `Applications/CausalLM/third_party/minja/include`.
  Fix:
  `git submodule update --init --recursive Applications/CausalLM/third_party/minja`.
- Quick.AI initially failed against main-based nntrainer because it expected
  sync-only CausalLM directory layout and sync-only C++ hooks. The validation
  patch adapts Quick.AI without importing Quick.AI product API into nntrainer.
- `quick_dot_ai_test` initially produced invalid tool-schema bounds because the
  hard-coded schema used placeholder text. The validation patch sets actual
  numeric `minimum` and `maximum` values.
- Android app install initially failed with
  `INSTALL_FAILED_UPDATE_INCOMPATIBLE` because an existing
  `com.example.sampletestapp` package had a different signature. Uninstalling
  the package and reinstalling the debug APK resolved it.
- Gradle emits a warning that `local.properties` contains a non-existing
  `sdk.dir`, but the environment-provided SDK path is sufficient and the build
  succeeds.

## Current Decision

The OLD cumulative branch is ready to serve as the integration base for the
next OLD residual audit, with one environment caveat: QNN-enabled Quick.AI
native build remains blocked until the QNN SDK root is supplied.
