# PR-OLD-2b Verification Report

Date: 2026-06-11

Worktree:

```text
/home/jrock/nntrainer-pr-old-cancel
```

Branch:

```text
migration/pr-old-cancel
```

Local commit:

```text
84e9c5a7 causallm: add explicit cancel API
```

Base:

```text
82c41e6e causallm: add streaming callback API
```

## Scope

- PR/work unit: explicit cross-thread CausalLM cancellation API and active-run
  lifetime guard.
- Source commits consulted:
  - OLD `67a2836c3654c160656994ccd3695a4022e17090`
  - OLD `b45774adf7743e44bb696a7221384ad96b623065`
- Files changed:
  - `Applications/CausalLM/api/causal_lm_api.h`
  - `Applications/CausalLM/api/causal_lm_api.cpp`
  - `Applications/CausalLM/api/unittest_cancel_api.cpp`
  - `Applications/CausalLM/models/causal_lm.h`
  - `Applications/CausalLM/models/causal_lm.cpp`
  - `Applications/CausalLM/meson.build`

## Implemented Behavior

- Adds public `cancelModel(void)` to the product-neutral CausalLM C API.
- `cancelModel()` returns `CAUSAL_LM_ERROR_NOT_INITIALIZED` before model load.
- `cancelModel()` returns `CAUSAL_LM_ERROR_NONE` when a model is loaded but no
  run is active.
- During an active CausalLM run, `cancelModel()` requests cooperative stop
  through `CausalLM::requestStop()`.
- `cancelModel()` does not take the global run/load mutex `g_mutex`; it uses a
  separate active-run pointer mutex.
- Active run publish/clear and cancellation dereference share
  `g_active_model_mutex`, preventing model replacement from freeing an object
  while `cancelModel()` is about to call `requestStop()`.
- `CausalLM::stop_requested_` is now atomic.
- Existing PR-OLD-2a callback-driven stop now routes through `requestStop()`.
- Run-start cancellation ordering is explicit:
  - C API calls `CausalLM::prepareForRun()` before publishing the active run.
  - `CausalLM::run()` calls `prepareStopRequestForRun()`, which does not clear
    a cancel request that arrived after active publication.

## Excluded

- `quick_dot_ai_api.*`
- Android/Kotlin/JNI product wrappers
- handle-based QuickAI API
- tokenizer cache/BPE/WordPiece/tokenizer_loader

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| TDD red: missing API/method | compile `unittest_cancel_api.cpp` before production changes | PASS_RED | failed on missing `cancelModel` and missing `CausalLM::requestStop` |
| TDD red: active publish race | `unittest_cancel_api --gtest_filter=*CancelAfterActivePublish*` before ordering fix | PASS_RED | failed before `prepareForRun()` / `prepareStopRequestForRun()` |
| TDD red: active pointer lifetime | `unittest_cancel_api --gtest_filter=*ModelReplacementWaits*` before active mutex | PASS_RED | failed because model replacement completed while cancel held selected active pointer |
| whitespace | `git diff --check` | PASS | no output |
| nntrainer x86 configure | `meson build -Denable-transformer=true` | PASS | `build/meson-logs/meson-log.txt` |
| nntrainer x86 build | `ninja -C build` | PASS | final rerun compiled 30 targets after cleanup |
| focused Meson unit | `meson test -C build unittest_cancel_api unittest_callback_streamer --print-errorlogs` | PASS | 2/2 OK |
| CausalLM x86 runtime | `./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |
| Android CausalLM build | `./Applications/CausalLM/build_android.sh --cache` | PASS | `nntrainer_causallm`, `libcausallm_core.so`, `nntr_quantize`, `nntr_safetensors_info` produced |
| Android CausalLM API lib | `./Applications/CausalLM/build_api_lib.sh` | PASS | `libcausallm_api.so` produced |

Focused unit result:

```text
1/2 nntrainer:unittests / unittest_callback_streamer OK
2/2 nntrainer:unittests / unittest_cancel_api        OK
Ok: 2
Fail: 0
```

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent answer: Seoul |
| `function_gemma` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models/function_gemma` | `LD_LIBRARY_PATH=. NNTR_NUM_THREADS=4 ./nntrainer_causallm .../function_gemma` | PASS | exit 0; function-call style output for London temperature prompt |
| `gemma4_cpu` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models/gemma4_cpu` | `LD_LIBRARY_PATH=. NNTR_NUM_THREADS=4 ./nntrainer_causallm .../gemma4_cpu` | PASS | exit 0; generated arithmetic reasoning and `7/12`; truncated by 256-token generation limit |
| `gauss-3.8-*` | Android CLI | `R3CX80H8Y0F` | not run | SKIP_DEVICE_UNSUPPORTED | expected unsupported on this device |

Android artifacts were pushed to:

```text
/data/local/tmp/nntrainer/causallm
```

## Review Results

- Spec review round 1: FAIL.
  - Found lost-cancel race between active publication and `CausalLM::run()`
    clearing `stop_requested_`.
  - Found test coverage gaps for loaded/no-active success, active cancel, and
    active lifetime.
- Fix:
  - Added `prepareForRun()` / `prepareStopRequestForRun()`.
  - Added fake-model unit tests for missing states.
- Spec review round 2: FAIL.
  - Found raw active pointer lifetime race if `cancelModel()` selected a model
    just before active run ended and `loadModel()` replaced `g_model`.
- Fix:
  - Added `g_active_model_mutex` and regression test
    `ModelReplacementWaitsForInFlightCancelDereference`.
- Spec review round 3: PASS.
  - Reviewer confirmed public API, no `g_mutex` in `cancelModel()`, active
    publish/clear/cancel coordination, atomic stop request, callback stop
    preservation, and focused tests.

## Missing Model Files

No additional model file was needed for this PR. The local x86 model package
still relies on the existing path-corrected
`/home/jrock/Quick.AI/gemma4_it_x86/nntr_config.json`.

## Debugging Notes

- Initial Meson configure failure in this worktree was environmental:
  `Applications/CausalLM/third_party/minja/include` was absent because the
  submodule was not initialized in the new worktree. Running
  `git submodule update --init --recursive Applications/CausalLM/third_party/minja subprojects/googletest subprojects/iniparser`
  fixed configure.
- A fake-model test initially hung only under Meson because Meson sets
  `MALLOC_PERTURB_`. Root cause: the fake test model constructed real
  `CausalLM` JSON setup unnecessarily. The final test uses an `ENABLE_TEST`
  lightweight protected constructor, avoiding real tokenizer/model setup.
- The final Android build still emits a pre-existing warning in
  `embedding_pooling_layer.cpp` about `%d` vs `size_t`; this PR does not modify
  that code.

## Residual Risk

- `cancelModel()` is still cooperative. It can only stop at generation-loop
  polling points after the current kernel/model step returns.
- The public C API remains singleton/global-state based. This PR preserves that
  model rather than introducing per-handle lifetime management.
- Installed-header packaging for the CausalLM API is still not formalized; this
  is inherited from PR-OLD-2a and should be handled before publishing a
  standalone SDK package.
