# PR-OLD-1 Verification Report

Date: 2026-06-11

Worktree:

```text
/home/jrock/nntrainer-pr-old-chattemplate
```

Branch:

```text
migration/pr-old-chattemplate
```

Local commit:

```text
1fc02dfd causallm: support array chat templates
```

## Scope

- PR/work unit: ChatTemplate HF array-form support
- Source commits consulted:
  - OLD `2639f431463ecc59cc07a1684dc0585206f53761`
- Files changed:
  - `Applications/CausalLM/chat_template.cpp`
  - `Applications/CausalLM/meson.build`
  - `test/unittest/models/unittest_chat_template.cpp`
- Affected models:
  - Any model whose `tokenizer_config.json` uses HF array-form
    `chat_template`, especially configs shaped as
    `[{ "name": "default", "template": "..." }, ...]`.
  - Runtime smoke priority after integration: `qwen3-0.6b`,
    `function_gemma`, `gemma4_cpu`.

## Implemented Behavior

- `tokenizer_config.chat_template` now accepts string, object, or array form.
- Array form is converted into the existing `template_map` path.
- Valid array entries must be objects with string `name` and string `template`.
- Existing default/tool-use/template-name selection logic is reused.
- Existing string and object forms are preserved.
- Malformed array entries are rejected instead of silently skipped.

Excluded:

- minja submodule changes
- `quick_dot_ai_api.*`
- Android/product/AAR code
- tokenizer cache/BPE/WordPiece/tokenizer_loader

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 configure | `meson build -Denable-transformer=true` | PASS | `build/meson-logs/meson-log.txt` |
| nntrainer x86 build | `ninja -C build` | PASS | stdout, final rerun: `ninja: no work to do.` |
| CausalLM focused Meson unit | `meson test -C build unittest_chat_template --print-errorlogs` | PASS | `build/meson-logs/testlog.txt`, 1/1 |
| CausalLM focused manual unit | `c++ ... unittest_chat_template.cpp Applications/CausalLM/chat_template.cpp ... && ./build/unittest_chat_template_manual` | PASS | 9/9 tests |
| nntrainer Android package | `./tools/package_android.sh -Dmmap-read=false -Dprefix=/home/jrock/nntrainer-pr-old-chattemplate/.android_stage` | PASS | `builddir/android_build_result` created |
| CausalLM x86 runtime | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| Quick.AI x86 | `./build.sh --platform=x86 --target=src,api,api-test --clean` in temporary Quick.AI validation worktree | FAIL_INTEGRATION_MAIN_GAP | nntrainer core built, `chat_template.cpp` compiled after temporary harness fixes, then Quick.AI API failed against missing main CausalLM hooks |
| Android native Quick.AI | not run | BLOCKED_BY_X86_INTEGRATION | do not start Android integration while x86 integration fails |
| Android app | not run | BLOCKED_BY_X86_INTEGRATION | do not start app integration while native integration fails |

Focused manual unit command:

```bash
cd /home/jrock/nntrainer-pr-old-chattemplate
c++ -std=c++17 \
  -IApplications/CausalLM \
  -IApplications/CausalLM/third_party \
  -IApplications/CausalLM/third_party/minja/include \
  -Isubprojects/googletest/googletest/include \
  -Isubprojects/googletest/googlemock/include \
  test/unittest/models/unittest_chat_template.cpp \
  Applications/CausalLM/chat_template.cpp \
  build/subprojects/googletest/libgtest_main.a \
  build/subprojects/googletest/libgtest.a \
  -pthread \
  -o build/unittest_chat_template_manual
./build/unittest_chat_template_manual
```

Focused manual unit result:

```text
9 tests from ChatTemplateTest
PASSED 9 tests
```

## Runtime Results

Standalone nntrainer x86 runtime was re-run with the user-requested Gemma4 x86
model package. The model package initially failed because
`/home/jrock/Quick.AI/gemma4_it_x86/nntr_config.json` contained an absolute
`tokenizer_file` path from another machine:
`/home/jayden/Quick_AI/5_1_release/Quick.AI/test_models/gemma4_it_x86/tokenizer.json`.
For local verification, that field was updated to
`/home/jrock/Quick.AI/gemma4_it_x86/tokenizer.json`.

Passing command:

```bash
cd /home/jrock/nntrainer-pr-old-chattemplate
timeout 300s env \
  LD_LIBRARY_PATH=/home/jrock/nntrainer-pr-old-chattemplate/build/Applications/CausalLM:/home/jrock/nntrainer-pr-old-chattemplate/build/nntrainer:$LD_LIBRARY_PATH \
  ./build/Applications/CausalLM/nntr_causallm \
  /home/jrock/Quick.AI/gemma4_it_x86
```

Observed output:

```text
<bos><|turn>user
The capital of Korea is<turn|>
<|turn>model

The capital of Korea is **Seoul**.<turn|>
```

The first raw-prompt attempt with `"What is 1/3 + 1/4? Answer briefly."`
exited successfully but generated only an end token because the prompt was not
chat-template formatted. The passing run omitted argv[2], letting the CLI apply
`chat_input` through the model chat template.

Android CLI inference was not re-run for this PR during the x86 verification
update. Earlier Android-side work remains tracked separately.

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | command above | PASS | coherent answer: Seoul |
| `qwen3-0.6b` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models` | not re-run in this update | PENDING_ANDROID_CLI | affected by chat template; run after Android validation pass resumes |
| `function_gemma` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models` | not re-run in this update | PENDING_ANDROID_CLI | affected by chat template |
| `gemma4_cpu` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models` | not re-run in this update | PENDING_ANDROID_CLI | affected by chat template |

## Missing Model Files

No missing files were found for `gemma4_it_x86` after fixing the local
`tokenizer_file` path. Android model file status remains as recorded in the
Phase 0 device snapshot.

| Model | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| none for this standalone stage | n/a | n/a | n/a |

## Review Findings and Resolution

Two independent read-only reviews were run.

Resolved:

- POSIX-only `<unistd.h>`/`getpid()` in the new test was removed.
- Tests were expanded to cover:
  - array default selection
  - array `tool_use` selection
  - explicit `Options::template_name`
  - string-form regression
  - object-form regression
  - missing template rejection
  - non-object array entry rejection
  - non-string name rejection
  - non-string template rejection
- Temporary test directory now includes a sanitized suite/test name and a
  monotonic timestamp suffix.

Recorded residual risk:

- Existing `selectTemplate()` fallback chooses any string entry if no
  `default`, no selected `tool_use`, and no explicit `template_name` are usable.
  For array-form inputs, conversion to a JSON object means this fallback follows
  object key ordering rather than original array order. This PR keeps the main
  behavior unchanged to avoid broadening scope; if stricter multi-template
  semantics are desired, handle it as a separate ChatTemplate policy PR.

## Debugging Notes

- The user-requested exact x86 command initially failed at Meson configure due
  to missing host build dependencies. After installing/resolving
  `tensorflow2-lite-dev`, `libjsoncpp-dev`, `libcurl4-openssl-dev`,
  `nnstreamer-dev`, `ml-api-common-dev`, `ml-inference-api-dev`,
  `libgtest-dev`, `libgmock-dev`, and `libiniparser-dev`, the exact configure
  passed.
- The first `nntr_causallm` runtime attempt failed because the model package's
  `nntr_config.json` referenced another machine's absolute tokenizer path. The
  path was corrected locally for verification.
- A raw prompt generated only `<turn|>`, so the pass/fail runtime criterion was
  evaluated with the CLI's default `chat_input` path, which applies the model
  chat template and produced a coherent answer.

## Quick.AI Integration Attempt

Quick.AI integration was tested in a separate temporary top-level worktree to
avoid modifying the dirty main checkout:

```text
/home/jrock/Quick.AI-pr-old-1-validation
```

The temporary worktree used:

```text
nntrainer -> /home/jrock/nntrainer-migration-validation
xgrammar   -> /home/jrock/Quick.AI/xgrammar
```

Attempts:

1. `./build.sh --platform=x86 --target=src,api,api-test --clean`
   - Result: failed during Quick.AI Meson configure.
   - Root cause: `src/meson.build` always passes
     `Applications/CausalLM/tokenizers` to `find`, but current main does not
     have that directory.
2. Temporary validation-only harness patch:
   - Skip `CausalLM/tokenizers` in `src/meson.build` if the directory is absent.
   - Result: build progressed to compiling `chat_template.cpp`.
3. Next failure:
   - Root cause: top-level Quick.AI include args did not include
     `Applications/CausalLM/third_party`, but main minja includes
     `<nlohmann/json.hpp>`.
4. Temporary validation-only harness patch:
   - Add `Applications/CausalLM/third_party` to top-level CausalLM include args.
   - Result: `chat_template.cpp` compiled.
5. Final x86 integration blocker:
   - `api/quick_dot_ai_api.cpp` depends on CausalLM APIs that are not present in
     main:
     - `causallm::multimodal_pointer`
     - `Transformer::getTokenizer()`
     - `Transformer::getVocabSize()`
     - `Transformer::initialize(std::string)`
     - `Transformer::getOutput()`
     - `Transformer::setXGrammar()`
     - `Transformer::resetXGrammar()`
     - `Transformer::embeddingBytesPerToken()`
     - `Transformer::setStreamer()`
     - `Transformer::requestStop()`
     - `SentenceTransformer::getEmbeddingDim()`

Conclusion:

- PR-OLD-1 itself is not the source of the integration failure.
- Quick.AI currently still assumes several sync/QuickAI-specific CausalLM hooks.
- Before Android CLI runtime can be executed on main-based nntrainer, Quick.AI
  needs a main-compat integration adaptation and PR-OLD-2/related generic hooks
  need design/implementation.
