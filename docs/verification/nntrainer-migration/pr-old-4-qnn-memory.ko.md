# PR-OLD-4 Verification Report

Date: 2026-06-11

## Scope

- PR/work unit: QNN/RPC MemoryPool allocation by planned offset
- Worktree: `/home/jrock/nntrainer-pr-old-qnn-memory`
- Branch: `migration/pr-old-qnn-memory`
- Local commit:
  - `040efba4 tensor: allocate qnn memory by planned offset`
- Source commits consulted:
  - OLD `8dbe0dae feat(allocator): add CpuMemAllocator and RpcMemAllocator`
  - OLD `18c116a0 fix(memory): plug FSU/NPU memory leak via allocator integration`
- Files changed:
  - `nntrainer/tensor/memory_pool.cpp`
  - `nntrainer/tensor/memory_pool.h`
  - `test/unittest/memory/unittest_memory_pool.cpp`
  - `test/unittest/meson.build`

## Implemented Behavior

- Keeps normal CPU/GPU-SVM `allocate()` as one contiguous pool.
- Adds QNN-specific `allocate()` behavior when the allocator name is `qnn`:
  allocate one backend buffer per distinct planned offset.
- Shares a single backend buffer for multiple memory requests that map to the
  same planned offset, sized by the largest aliasing request.
- Tracks QNN-owned buffers in `owned_buffers_`, so `deallocate()` releases all
  buffers even when `mem_pool` remains null.
- Preserves a successful planned layout across `deallocate()` so a pool can be
  reallocated without rebuilding the request list.
- Invalidates stale layout metadata when a new request set starts.
- Resets stale weight-gradient request counts when a pool is reused.

Excluded:

- broad allocator abstraction copy from OLD
- QNN graph input delivery changes from later `sync_v0.4.0`
- Android product code
- tokenizer work

## Build Results

| Check | Command | Result | Log |
|---|---|---|---|
| nntrainer x86 configure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | cumulative validation |
| nntrainer x86 build | `ninja -C build` | PASS | cumulative validation |
| focused unit | `meson test -C build unittest_memory_pool --print-errorlogs` | PASS | cumulative validation |
| cumulative focused units | `meson test -C build unittest_chat_template unittest_callback_streamer unittest_cancel_api unittest_embedding_sidecar_lut unittest_memory_pool unittest_causallm_models --print-errorlogs` | PASS | cumulative validation |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | output answered Seoul |
| whitespace | `git diff --check f2c0e6ae30ceb1ff418064f02b2f95804f294d24..HEAD` | PASS | cumulative validation |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | regression smoke |
| `qwen3-0.6b` | Android Quick.AI test CLI | `R3CX80H8Y0F`, `/sdcard/Download/aistudio-mobile/models` | `quick_dot_ai_test qwen3-0.6b ...` | PASS | cumulative validation, CPU path |
| QNN affected models | Android QNN | `R3CX80H8Y0F` | not run | BLOCKED_ENV | QNN SDK root not available for host build |

## Missing Model Files and Environment Gaps

| Model/env | Required path | Missing files | Owner follow-up |
|---|---|---|---|
| QNN SDK | host SDK path passed to Meson as `qnn-sdk-root` | SDK root not available in environment | provide SDK path and rerun `./build.sh --platform=android --enable-qnn --target=src,api,api-test` |

## Debugging Notes

- QNN allocation intentionally keys off `allocator_->getName() == "qnn"`
  rather than replacing the generic pool planner.
- Unit tests use a counting allocator and forced-offset planner to prove
  distinct-offset allocation, same-offset aliasing, double-allocation rejection,
  replan rejection while allocated, reallocation after deallocate, and stale
  request metadata reset.
- The later sync QNN graph input delivery patch remains a separate candidate
  because this PR only fixes MemoryPool allocation ownership/layout behavior.
