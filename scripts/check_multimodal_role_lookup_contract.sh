#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

api_cpp="$root_dir/api/quick_dot_ai_api.cpp"

rg -q "struct LoadedComponent" "$api_cpp" ||
  fail "LoadedComponent struct is missing"

rg -q "std::vector<LoadedComponent> components;" "$api_cpp" ||
  fail "CausalLmModel.components is missing"

rg -q "find_component\\(" "$api_cpp" ||
  fail "find_component role lookup helper is missing"

rg -q "find_text_llm\\(" "$api_cpp" ||
  fail "find_text_llm helper is missing"

rg -q "find_vision_encoder\\(" "$api_cpp" ||
  fail "find_vision_encoder helper is missing"

rg -q "QDA_ROLE_VISION_ENCODER" "$api_cpp" ||
  fail "vision component role is not assigned"

rg -q "QDA_ROLE_TEXT_LLM" "$api_cpp" ||
  fail "text LLM component role is not assigned"

! rg -q "causallm::Transformer \\*vision = h\\.models\\[0\\]\\.get\\(\\)" \
  "$api_cpp" ||
  fail "run_vision_encoder still assumes h.models[0] is the vision encoder"

! rg -q "causallm::Transformer \\*llm = h\\.models\\[1\\]\\.get\\(\\)" \
  "$api_cpp" ||
  fail "run_vision_encoder still assumes h.models[1] is the LLM"

! rg -q "execute_multimodal\\(h, h\\.models\\[1\\]\\.get\\(\\)" "$api_cpp" ||
  fail "multimodal execution still assumes h.models[1] is the LLM"

echo "PASS: multimodal role lookup contract is present"
