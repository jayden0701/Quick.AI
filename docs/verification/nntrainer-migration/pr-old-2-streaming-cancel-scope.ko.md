# PR-OLD-2 Streaming and Cancel Scope Report

Date: 2026-06-11

Status: scope proposal. PR-OLD-2a implementation/verification is tracked in
`docs/verification/nntrainer-migration/pr-old-2a-streaming-callback.ko.md`.

Sources consulted:

```text
Main base: f2c0e6ae30ceb1ff418064f02b2f95804f294d24
OLD branch: quickdotai_api_refact
OLD commits:
  81b303e4684dd5e39105da68c6d3174c66af37a1
  1752be2672fbfd25a9bf9b17d387a1721aeb3a07
  67a2836c3654c160656994ccd3695a4022e17090
  b45774adf7743e44bb696a7221384ad96b623065
  4f7c560721da2f63fcc97a6409ecd0a1134712c0
```

## Main Baseline

Current main has incremental stdout emission inside the CausalLM runtime, but
does not have a public streaming callback API or cross-thread cancellation API.

Observed main behavior:

- `Applications/CausalLM/models/causal_lm.cpp`
  - `CausalLM::registerOutputs()` decodes pending token ids and prints decoded
    text to `std::cout` when `log_output == true`.
  - Generation stops only on EOS or `NUM_TO_GENERATE`.
  - There is no external stop flag check in the generation loop.
- `Applications/CausalLM/api/causal_lm_api.cpp`
  - The C API uses one global `g_model` and `g_mutex`.
  - `runModel()` holds `g_mutex`, optionally applies chat template, calls
    `g_model->run(..., g_verbose)`, and returns the final output string.
- `Applications/CausalLM/api/causal_lm_api.h`
  - Exports `runModel(const char *, const char **)`.
  - Does not export handle API, streaming callback API, or cancel API.

The current API is therefore final-output based. Existing "streaming" in the
CLI is stdout emitted from the inference loop, not a callback contract.

## OLD Generic Runtime Ideas

The relevant OLD commits mix generic runtime changes with QuickAI product API,
JNI, and Android app code. Only the generic runtime ideas are candidates.

Generic ideas worth preserving:

- `Applications/CausalLM/api/streamer.{h,cpp}` with a small `BaseStreamer`
  vtable and helper calls such as `streamer_put()` and `streamer_end()`.
- `Applications/CausalLM/api/callback_streamer.{h,cpp}` with a
  `CausalLmTokenCallback` adapter.
- `CausalLM::setStreamer(BaseStreamer *)` as a synchronous, non-owning streamer
  attachment.
- `CausalLM::registerOutputs()` routing decoded deltas to the attached streamer.
- Callback nonzero return causing cooperative cancellation.
- `CausalLM::requestStop()` backed by `std::atomic<bool>` for cross-thread
  cancellation.
- Later QuickDotAI behavior shows a useful RAII attach/detach pattern and a
  cancel path that must not take the run mutex.

Excluded from PR-OLD-2:

- `quick_dot_ai_api.*`
- OLD handle-based QuickAI API
- JNI/Kotlin/Android service wrappers
- NDJSON HTTP streaming behavior
- message-format streaming API changes
- multimodal streaming

## Minimal PR Proposal

Keep main's architecture. Do not introduce the OLD QuickAI handle API.

Proposed PR-OLD-2a:

- Add generic streamer plumbing under `Applications/CausalLM/api`.
- Add CausalLM model-level streamer attachment.
- Add `runModelStreaming()` to `causal_lm_api.*`, matching main's global-model
  API style.
- Treat callback nonzero as cooperative cancellation for the current synchronous
  run.
- Keep existing `runModel()` behavior unchanged.

Proposed PR-OLD-2b:

- Add explicit cross-thread `cancelModel()`.
- Use an atomic active-run pointer or equivalent lifetime guard.
- Ensure `cancelModel()` never takes `g_mutex`, because `runModelStreaming()`
  will hold it during inference.

This split is recommended. PR-OLD-2a can be reviewed as streaming callback
plumbing, while PR-OLD-2b can focus on lifetime and thread-safety.

## Candidate API Shape

Names are provisional and should be checked against nntrainer naming style
before implementation.

```c
typedef int (*CausalLmTokenCallback)(const char *delta, void *user_data);

ErrorCode runModelStreaming(const char *inputTextPrompt,
                            const char **outputText,
                            CausalLmTokenCallback callback,
                            void *user_data);

ErrorCode cancelModel(void);
```

Policy:

- `runModelStreaming()` should return `CAUSAL_LM_ERROR_INVALID_PARAMETER` for
  null prompt, output pointer, or callback.
- Callback nonzero return should stop generation cooperatively.
- A callback-requested stop should not be reported as inference failure unless
  the runtime itself failed.
- `cancelModel()` before model load should return `CAUSAL_LM_ERROR_NOT_INITIALIZED`.
- Callbacks run on the inference thread. They must not call load/run/metrics
  APIs that need the same global mutex.

## Candidate Files

Potential files to add:

- `Applications/CausalLM/api/streamer.h`
- `Applications/CausalLM/api/streamer.cpp`
- `Applications/CausalLM/api/callback_streamer.h`
- `Applications/CausalLM/api/callback_streamer.cpp`

Potential files to edit:

- `Applications/CausalLM/api/causal_lm_api.h`
- `Applications/CausalLM/api/causal_lm_api.cpp`
- `Applications/CausalLM/models/causal_lm.h`
- `Applications/CausalLM/models/causal_lm.cpp`
- `Applications/CausalLM/meson.build`
- `Applications/CausalLM/api/test_api.cpp` only if a CLI demonstration is added

Avoid touching `quick_dot_ai_api.*`.

## Test Plan

Tests or compile checks that should fail before PR-OLD-2 and pass after:

- C API compile test references:
  - `CausalLmTokenCallback`
  - `runModelStreaming()`
  - `cancelModel()`
- Streamer adapter unit:
  - `streamer_put()` invokes callback with deltas.
  - A nonzero callback return is sticky and suppresses later callback calls.
  - `streamer_end()` is invoked once.
- Streaming runtime test:
  - `runModelStreaming()` receives at least one callback delta.
  - Concatenated callback deltas match final output where token buffering allows.
- Callback cancellation test:
  - Callback returns nonzero after first useful delta.
  - Run stops before max token count.
  - A following normal run is not pre-cancelled.
- Cross-thread cancellation test for PR-OLD-2b:
  - Run in one thread.
  - Call `cancelModel()` from another thread.
  - Verify no `g_mutex` deadlock and generation stops.
- Negative tests:
  - null prompt, output pointer, or callback returns
    `CAUSAL_LM_ERROR_INVALID_PARAMETER`.
  - cancel before load returns `CAUSAL_LM_ERROR_NOT_INITIALIZED`.

## Risks and Design Questions

- `cancelModel()` cannot take `g_mutex` if a running inference holds it.
- `streamer_` is a non-owning pointer; it must be attached only for the
  synchronous run and detached through RAII or equivalent cleanup.
- `streamer_end()` and detach semantics must be clear for exceptions.
- Callback is called while inference is active. Re-entering load/run/metrics
  APIs from the callback can deadlock unless explicitly designed otherwise.
- Cancellation while `pending_ids_` holds incomplete decoded fragments may drop
  buffered text unless the implementation flushes carefully.
- A cancelled run may leave KV-cache/session state partially advanced. The PR
  must define whether the model remains reusable as a continuation or only for
  a fresh next run.
