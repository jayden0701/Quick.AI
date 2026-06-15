# PR-OLD-2a Verification Report

Date: 2026-06-11

Worktree:

```text
/home/jrock/nntrainer-pr-old-streaming
```

Branch and local commit:

```text
migration/pr-old-streaming
82c41e6e causallm: add streaming callback API
```

## Scope

- PR/work unit: generic CausalLM streaming callback plumbing and callback-driven
  cooperative stop for the current synchronous run.
- Source commits consulted:
  - OLD `81b303e4684dd5e39105da68c6d3174c66af37a1`
  - OLD `1752be2672fbfd25a9bf9b17d387a1721aeb3a07`
  - OLD `67a2836c3654c160656994ccd3695a4022e17090`
  - OLD `b45774adf7743e44bb696a7221384ad96b623065`
- Files changed:
  - `Applications/CausalLM/api/streamer.{h,cpp}`
  - `Applications/CausalLM/api/callback_streamer.{h,cpp}`
  - `Applications/CausalLM/api/causal_lm_callback.h`
  - `Applications/CausalLM/api/causal_lm_api.{h,cpp}`
  - `Applications/CausalLM/api/unittest_callback_streamer.cpp`
  - `Applications/CausalLM/models/causal_lm.{h,cpp}`
  - `Applications/CausalLM/meson.build`
  - `Applications/CausalLM/jni/Android.mk`
- Excluded:
  - `quick_dot_ai_api.*`
  - handle-based QuickAI API
  - JNI/Kotlin/Android product wrappers
  - explicit cross-thread `cancelModel()`; this remains PR-OLD-2b
  - tokenizer cache/BPE/WordPiece/tokenizer_loader

## Implemented Behavior

- Adds a product-neutral `BaseStreamer` C ABI with null-safe helper wrappers.
- Adds `CallbackStreamer`, adapting `CausalLmTokenCallback` into `BaseStreamer`.
- Adds `runModelStreaming(const char *, const char **, CausalLmTokenCallback,
  void *)` to `causal_lm_api`.
- Adds `CausalLM::setStreamer(BaseStreamer *)`.
- Routes decoded output deltas from `CausalLM::registerOutputs()` to the
  attached streamer.
- Treats nonzero callback return as a sticky cooperative stop request for the
  current synchronous run.
- Calls `streamer_end()` through an RAII guard when the run exits.
- Keeps `runModel()` behavior unchanged for non-streaming callers.

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 configure | `meson build -Denable-transformer=true` | PASS | `build/meson-logs/meson-log.txt` |
| nntrainer x86 build | `ninja -C build` | PASS | 734/734, final rerun: `ninja: no work to do.` |
| CausalLM focused Meson unit | `meson test -C build unittest_callback_streamer --print-errorlogs` | PASS | `build/meson-logs/testlog.txt`, 1/1 |
| C API header syntax | `cc -std=c11 -IApplications/CausalLM/api -x c -fsyntax-only ...` | PASS | direct syntax check |
| Android CausalLM build | `./Applications/CausalLM/build_android.sh --cache` | PASS | fresh run; `streamer.cpp` compiled into `causallm_core` and `nntr_quantize`; `nntrainer_causallm`, `libcausallm_core.so` produced |
| Android CausalLM API lib | `./Applications/CausalLM/build_api_lib.sh` | PASS | fresh run; `callback_streamer.cpp`, `model_config.cpp`, and `causal_lm_api.cpp` compiled; `libcausallm_api.so` produced |
| CausalLM x86 runtime | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output: `The capital of Korea is **Seoul**.` |

Focused unit result:

```text
1/1 nntrainer:unittests / unittest_callback_streamer OK
Ok: 1
Fail: 0
```

## Runtime Results

The x86 runtime used the same local model path as PR-OLD-1. The model package
needed the same local `tokenizer_file` correction in
`/home/jrock/Quick.AI/gemma4_it_x86/nntr_config.json`.

Passing x86 command:

```bash
cd /home/jrock/nntrainer-pr-old-streaming
timeout 300s env \
  LD_LIBRARY_PATH=/home/jrock/nntrainer-pr-old-streaming/build/Applications/CausalLM:/home/jrock/nntrainer-pr-old-streaming/build/nntrainer:$LD_LIBRARY_PATH \
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

Fresh Android artifacts were pushed to:

```text
/data/local/tmp/nntrainer/causallm
```

with:

```text
LD_LIBRARY_PATH=/data/local/tmp/nntrainer/causallm
NNTR_NUM_THREADS=4
```

Android CLI runtime:

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | command above | PASS | coherent answer: Seoul |
| `function_gemma` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models/function_gemma` | `./nntrainer_causallm /sdcard/Download/aistudio-mobile/models/function_gemma` | PASS | exit 0; generated function-call style output for London temperature prompt; 98 prefill tokens, 12 generated tokens |
| `gemma4_cpu` | Android CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models/gemma4_cpu` | `./nntrainer_causallm /sdcard/Download/aistudio-mobile/models/gemma4_cpu` | PASS | exit 0; generated arithmetic explanation and `7/12`; 32 prefill tokens, 256 generated tokens |
| `gauss-3.8-*` | Android CLI | `R3CX80H8Y0F` | not run | SKIP_DEVICE_UNSUPPORTED | expected unsupported on this device |

## Missing Model Files

No x86 model file is missing after the local tokenizer path fix. Android
availability follows the Phase 0 model inventory. Some Android model
directories are present but not directly runnable with current main CLI:

| Model | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| `qwen3-0.6b` | `/sdcard/Download/aistudio-mobile/models/qwen3-0.6b` | config contains a host absolute tokenizer path | fix model config or add a temporary runtime config before smoke |
| `gauss-3.6-qnn` | `/sdcard/Download/aistudio-mobile/models/gauss-3.6-qnn` | none confirmed | main CLI lacks matching registered architecture in current stage |
| `tiny_bert` | `/sdcard/Download/aistudio-mobile/models/tiny_bert` | none confirmed | `MultilingualTinyBert` is excluded on Android in main |

## Debugging Notes

- The user-requested exact x86 command initially failed before code compile
  because the host lacked default nntrainer native build dependencies. After
  installing/resolving the required packages, `meson build
  -Denable-transformer=true` passed.
- `runModelStreaming()` is synchronous and uses the existing global model mutex.
  The public callback contract documents that callbacks must not re-enter CausalLM
  APIs that need the same mutex.
- This PR intentionally does not add cross-thread `cancelModel()`. That remains
  a separate lifetime/thread-safety PR.

## Residual Risk

- There is no full model-level `runModelStreaming()` integration test yet that
  asserts callback deltas against final output. Current automated coverage is
  focused on the callback streamer adapter and build coverage.
- A read-only subagent spec review found no blocking issue in the staged
  PR-OLD-2a diff. It did flag that `causal_lm_api.h` now includes
  `causal_lm_callback.h`, while current CausalLM Meson files do not define
  installed-header packaging for the CausalLM API headers. This is not a new
  regression for the normal in-tree/app build, but external installed-header
  consumption should be verified or formalized before publishing an SDK package.
- Callback cancellation stops the generation loop cooperatively but does not
  define reusable KV-cache/session semantics after a partial generation. Treat
  that as part of the later explicit cancel/lifecycle PR.
- Android CLI verification has not yet been repeated for every available model
  directory. `function_gemma` and `gemma4_cpu` passed; `qwen3-0.6b`,
  `tiny_bert`, and Gauss/QNN variants remain blocked or skipped for the reasons
  listed in `Missing Model Files`.
