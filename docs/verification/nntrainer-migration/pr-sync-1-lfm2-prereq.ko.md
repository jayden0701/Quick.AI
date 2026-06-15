# PR-SYNC-1 Verification Report

Date: 2026-06-12 KST

## Scope

- PR/work unit: LFM2 prerequisite kernels and layers
- Worktree: `/home/jrock/nntrainer-pr-sync-1-lfm2-prereq`
- Branch: `migration/pr-sync-1-lfm2-prereq`
- Base: `50f90a26 test: cover CausalLM accessor hooks`
- Local commit:
  - `b1674b2eff2daf6d45729e5146ad82579ff154b9 Port LFM2 prerequisite kernels and layers`

Source commits represented:

- `a53f89c0 cpu_backend: add causal depthwise conv1d kernels`
- `31e92d39 causallm: add LFM2 support layers`
- `9fd46c0f causallm: support hybrid decoder configs`
- `605c6629 [CausalLM] Apply thread_manager in causalm_conv1d_layer.cpp`

Deferred to PR-SYNC-2:

- `a79ad18e causallm: add LFM2 CausalLM model`
- `46cad449 causallm: fix untied LFM2 lm head scope`
- `0e9fcb9b [CausalLM] Update LFM2 config reading`

## Implemented Behavior

- Adds x86 and ARM causal depthwise Conv1D kernel-size-3 backend functions.
- Adds backend unit coverage for prefill/decode behavior.
- Adds `CausalConv1DLayer` with rolling Conv1D state for decode and cached
  chunk continuation.
- Adds `CustomMultiplyLayer` with bidirectional broadcast support.
- Adds focused layer tests for:
  - left-side broadcast forwarding,
  - multi-axis forwarding broadcast,
  - left-height-broadcast incremental forwarding,
  - multi-axis incremental forwarding broadcast,
  - scalar-height incremental forwarding broadcast,
  - `CustomMultiplyLayer` reset-input-dimension broadcast recomputation,
  - disabling unsafe in-place aliasing,
  - `CausalConv1DLayer` `from > 0 && to - from > 1` state continuation,
  - `CausalConv1DLayer` one-token prefill ignoring stale state,
  - `CausalConv1DLayer` FP16 activation rejection,
  - `CausalConv1DLayer` batch and input-dimension update resizing
    `conv_state`.
- Wires prerequisite hybrid decoder config tolerance without adding LFM2 model
  files.

Excluded:

- LFM2 text model registration/graph
- LFM2-VL and V-JEPA
- Quick.AI product/API code
- Android product scripts
- tokenizer cache/BPE/WordPiece/tokenizer_loader

## Review Fixes

Code review found and this commit fixes:

- PLUGGABLE initializer shape for shared builds.
- ARMv7 portability by using the existing `VFMAQ_F32` compatibility path.
- `CustomMultiplyLayer` left-broadcast forwarding.
- `CustomMultiplyLayer` output-shaped multi-axis broadcast where both inputs
  expand different axes, for both full and incremental forwarding.
- `CustomMultiplyLayer` incremental left-height broadcast slicing.
- Unsafe `CustomMultiplyLayer` in-place aliasing for broadcast-expanded outputs.
- `CausalConv1DLayer` using global `to` instead of local `to - from` and losing
  state continuity for cached multi-token chunks.
- `CausalConv1DLayer` one-token prefill reading stale `MAX_LIFESPAN`
  `conv_state` instead of using zero causal history.
- `CausalConv1DLayer` FP16 activation tensors being accepted despite using
  FP32-only raw buffer access internally. This PR now rejects non-FP32
  activation/runtime tensors until a real FP16 path is implemented.
- `CausalConv1DLayer` `conv_state` not resizing through `setBatch()` or
  `updateTensorsByInputDimensions()`, which could make runtime batch/input
  dimension changes access stale state shapes.
- `CustomMultiplyLayer` missing `updateTensorsByInputDimensions()`, causing
  reset-input-dimension graph paths to throw.
- `CustomMultiplyLayer` scalar-height output incremental slices expanding to
  `to - from` and requesting a shared tensor larger than the output buffer.

## Build Results

| Check | Command | Result | Notes |
|---|---|---|---|
| x86 configure | `meson setup build --reconfigure -Denable-transformer=true` | PASS | native x86 |
| x86 build | `ninja -C build` | PASS | full build |
| unit tests | `meson test -C build unittest_custom_multiply unittest_causal_conv1d_layer unittest_nntrainer_cpu_backend unittest_causallm_models --print-errorlogs` | PASS | 4/4 |
| CausalLM x86 runtime | `NNTR_NUM_THREADS=4 ./build/Applications/CausalLM/nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | coherent Seoul answer |
| shared/plugin configure | `meson setup build-shared-pr-sync-1 --reconfigure -Denable-transformer=true -Ddefault_library=shared` | PASS | covers `PLUGGABLE` compile path |
| shared/plugin build | `ninja -C build-shared-pr-sync-1 Applications/CausalLM/layers/libcausal_conv1d_layer.so Applications/CausalLM/layers/libcustom_multiply.so Applications/CausalLM/unittest_custom_multiply Applications/CausalLM/unittest_causal_conv1d_layer` | PASS | no rebuild needed post-amend |
| shared/plugin tests | `./build-shared-pr-sync-1/Applications/CausalLM/unittest_custom_multiply` and `./build-shared-pr-sync-1/Applications/CausalLM/unittest_causal_conv1d_layer` | PASS | 7/7 and 5/5 |
| ARMv7 compile probe | direct `clang++ --target=armv7a-linux-androideabi29 ... neon_impl.cpp` | PASS | validates new NEON path portability |
| Android package | `./tools/package_android.sh /home/jrock/nntrainer-pr-sync-1-lfm2-prereq -Denable-transformer=true` | PASS | produced `nntrainer_for_android.tar.gz` and arm64 `libnntrainer.so` |
| whitespace | `git diff --check 50f90a26..HEAD` | PASS | no output |

## Runtime Results

| Model | Platform | Device/path | Command | Result | Notes |
|---|---|---|---|---|---|
| `gemma4_it_x86` | x86 CLI | `/home/jrock/Quick.AI/gemma4_it_x86` | `nntr_causallm /home/jrock/Quick.AI/gemma4_it_x86` | PASS | answered `The capital of Korea is **Seoul**.` |

## Missing Model Files

- LFM2 text model runtime was not executed in this PR because the LFM2 model
  class/files are intentionally deferred to PR-SYNC-2.
- LFM2-VL/V-JEPA runtime was not executed because those model implementations
  are out of scope for PR-SYNC-1.

## Handoff Notes

- Final independent review found no blocking issues for PR-SYNC-1.
- PR-SYNC-2 must explicitly handle registration/loading for the new layers.
  Current `Transformer::registerCustomLayers()` does not register
  `causal_conv1d` or `custom_multiply`, and directory plugin discovery only
  discovers libraries matching `*layer.so`. The PR-SYNC-1 shared build currently
  produces `libcausal_conv1d_layer.so` and `libcustom_multiply.so`, so LFM2 text
  model integration must either register both layers in the model path or adjust
  plugin loading/naming intentionally.

## Debugging Notes

- The first Android package attempt in this worktree previously failed because
  submodules were not initialized. After `git submodule update --init
  --recursive`, Android package build passed. The final worktree is clean.
- `CustomMultiplyLayer` keeps accepting `inplace` properties for config
  compatibility, but `initializeInPlace()` now returns `NONE` because broadcast
  expansion makes either input unsafe as a general in-place target.
- `CausalConv1DLayer` uses vectorized prefill only when `from == 0`; cached
  multi-token chunks use sequential decode to preserve rolling state.
- `from == 0 && to - from == 1` is treated as prefill, not decode: it computes
  with zero history and resets state to `[0, x0]`.
