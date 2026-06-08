#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

rg -q "QDA_ROLE_TEXT_LLM" "$root_dir/api/model_descriptor.h" ||
  fail "ModelRole enum with QDA_ROLE_TEXT_LLM is missing"

rg -q "ModelRole role;" "$root_dir/api/model_descriptor.h" ||
  fail "ModelDescriptor.role is missing"

rg -q "unsigned int embedding_dim;" "$root_dir/api/model_descriptor.h" ||
  fail "ModelDescriptor.embedding_dim is missing"

rg -q "compatible_with;" "$root_dir/api/model_descriptor.h" ||
  fail "ModelDescriptor.compatible_with is missing"

rg -q '\\\"role\\\"' "$root_dir/api/quick_dot_ai_api.cpp" ||
  fail "getModelCatalogJson does not emit role"

rg -q '\\\"embedding_dim\\\"' "$root_dir/api/quick_dot_ai_api.cpp" ||
  fail "getModelCatalogJson does not emit embedding_dim"

rg -q '\\\"compatible_with\\\"' "$root_dir/api/quick_dot_ai_api.cpp" ||
  fail "getModelCatalogJson does not emit compatible_with"

rg -q "enum class ModelRole" \
  "$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/ModelCatalog.kt" ||
  fail "Android ModelRole enum is missing"

rg -q "VISION_ENCODER" \
  "$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/ModelCatalog.kt" ||
  fail "Android Capability.VISION_ENCODER decode support is missing"

echo "PASS: multimodal catalog contract is present"
