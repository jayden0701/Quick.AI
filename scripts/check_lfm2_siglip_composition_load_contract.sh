#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

api_cpp="$root_dir/api/quick_dot_ai_api.cpp"
api_lib="$root_dir/builddir_x86/api/libquick_dot_ai_api.so"
vision_h="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/vision/lfm2_vl_vision_transformer.h"
vision_cpp="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/vision/lfm2_vl_vision_transformer.cpp"

rg -q "load_lfm2_siglip_composition_handle" "$api_cpp" ||
  fail "LFM2+SigLIP composition loader helper is missing"

rg -q "load_transformer_component\\(llm" "$api_cpp" ||
  fail "composition loader does not load the LLM component"

rg -q "load_transformer_component\\(vision" "$api_cpp" ||
  fail "composition loader does not load the vision component"

rg -q "load_lfm2_connector_adapter\\(connector.descriptor" "$api_cpp" ||
  fail "composition loader does not load the connector adapter"

rg -q "h->connectors.push_back" "$api_cpp" ||
  fail "composition loader does not store connector adapters on the handle"

rg -q "input_embedding_dim\\(\\) const" "$api_cpp" ||
  fail "connector adapter does not expose input embedding dimensions"

rg -q "find_connector_adapter" "$api_cpp" ||
  fail "multimodal run path does not look up connector adapters"

rg -q "connector->project" "$api_cpp" ||
  fail "multimodal run path does not project vision features through connector"

rg -q "copy_float_vector_to_multimodal_pointer" "$api_cpp" ||
  fail "multimodal run path does not return projected embeddings as multimodal_pointer"

rg -q "move_first_loaded_component\\(tmp_vision, \\*h, QDA_ROLE_VISION_ENCODER" "$api_cpp" ||
  fail "composition loader does not tag the vision component role"

rg -q "move_first_loaded_component\\(tmp_llm, \\*h, QDA_ROLE_TEXT_LLM" "$api_cpp" ||
  fail "composition loader does not tag the LLM component role"

rg -q "run_image\\(const WSTR prompt, multimodal_pointer image" "$vision_h" ||
  fail "Lfm2VlVisionTransformer.run_image() declaration is missing"

rg -q "Lfm2VlVisionTransformer::run_image" "$vision_cpp" ||
  fail "Lfm2VlVisionTransformer.run_image() implementation is missing"

if rg -q "built without ENABLE_QNN" "$api_cpp"; then
  fail "CPU multimodal run path is still gated behind ENABLE_QNN"
fi

if [[ ! -f "$api_lib" ]]; then
  fail "x86 API library is missing; run ./build.sh --target=api first"
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

export LD_LIBRARY_PATH="$root_dir/nntrainer/builddir_x86/nntrainer:$root_dir/nntrainer/builddir_x86/api/ccapi:$root_dir/builddir_x86/src:$root_dir/builddir_x86/api:${LD_LIBRARY_PATH:-}"
python3 - "$api_lib" "$tmpdir" <<'PY'
import ctypes
import sys

api_lib, model_base = sys.argv[1], sys.argv[2]
lib = ctypes.CDLL(api_lib)

load = lib.loadMultimodalCompositionJson
load.argtypes = [
    ctypes.c_char_p,
    ctypes.c_int,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_void_p),
]
load.restype = ctypes.c_int

destroy = lib.destroyModelHandle
destroy.argtypes = [ctypes.c_void_p]
destroy.restype = ctypes.c_int

composition = b"""{
  "id": "lfm2-siglip",
  "llm": {"model_id": "lfm2-siglip-llm", "backend": "CPU"},
  "vision": {"model_id": "siglip-lfm2-vision", "backend": "CPU"},
  "connector": {"model_id": "lfm2-siglip-connector", "backend": "CPU"}
}"""

handle = ctypes.c_void_p()
rc = load(composition, 0, None, model_base.encode("utf-8"), ctypes.byref(handle))
if handle.value:
    destroy(handle)

CAUSAL_LM_ERROR_INVALID_PARAMETER = 1
CAUSAL_LM_ERROR_UNSUPPORTED = 6
if rc == CAUSAL_LM_ERROR_UNSUPPORTED:
    raise SystemExit("FAIL: valid lfm2-siglip CPU composition still returns UNSUPPORTED")
if rc == CAUSAL_LM_ERROR_INVALID_PARAMETER:
    raise SystemExit("FAIL: valid lfm2-siglip CPU composition failed validation")

print(f"PASS: lfm2-siglip composition reaches real load path (rc={rc})")
PY

echo "PASS: LFM2+SigLIP composition load contract is present"
