#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

api_cpp="$root_dir/api/quick_dot_ai_api.cpp"

rg -q "is_lfm2_jepa_mixed_backend_composition" "$api_cpp" ||
  fail "LFM2+JEPA mixed-backend composition matcher is missing"

rg -q "load_lfm2_jepa_composition_handle" "$api_cpp" ||
  fail "LFM2+JEPA composition loader helper is missing"

rg -q "jepa-qnn-vision" "$api_cpp" ||
  fail "LFM2+JEPA loader does not reference the JEPA QNN vision descriptor"

rg -q "vision.backend == CAUSAL_LM_BACKEND_NPU" "$api_cpp" ||
  fail "LFM2+JEPA loader does not require NPU vision backend"

rg -q "load_transformer_component\\(vision" "$api_cpp" ||
  fail "mixed-backend loader does not load the vision component separately"

rg -q "load_transformer_component\\(llm" "$api_cpp" ||
  fail "mixed-backend loader does not load the LLM component separately"

rg -q "load_lfm2_connector_adapter\\(connector.descriptor" "$api_cpp" ||
  fail "mixed-backend loader does not load the connector adapter"

rg -q "validate_multi_image_patch_layout" "$api_cpp" ||
  fail "multi-image patch layout validation helper is missing"

rg -q "max_original_dimension" "$api_cpp" ||
  fail "multi-image original-dimension aggregation helper is missing"

if rg -q "STUB|TODO: Implement multi-image|originalHeights\\[0\\]|originalWidths\\[0\\]" "$api_cpp"; then
  fail "multi-image path still delegates through the first image stub"
fi

echo "PASS: LFM2+JEPA mixed-backend contract is present"
