#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

descriptors="$root_dir/api/model_descriptors_public.cpp"
catalog="$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/ModelCatalog.kt"
api_cpp="$root_dir/api/quick_dot_ai_api.cpp"

for id in \
  "lfm2-siglip-llm" \
  "siglip-lfm2-vision" \
  "lfm2-siglip-connector" \
  "lfm2-siglip" \
  "lfm2-jepa-llm" \
  "jepa-qnn-vision" \
  "lfm2-jepa-connector" \
  "lfm2-jepa"; do
  rg -q "\"$id\"" "$descriptors" ||
    fail "descriptor '$id' is missing"
done

rg -q "descriptor_has_role" "$api_cpp" ||
  fail "descriptor_has_role helper is missing"

rg -q "descriptor_allows_backend" "$api_cpp" ||
  fail "descriptor_allows_backend helper is missing"

rg -q "descriptor_compatible_with" "$api_cpp" ||
  fail "descriptor_compatible_with helper is missing"

rg -q "fun compositions\\(" "$catalog" ||
  fail "Android ModelCatalog.compositions() helper is missing"

rg -q "fun visionOptionsForLlm\\(" "$catalog" ||
  fail "Android ModelCatalog.visionOptionsForLlm() helper is missing"

rg -q "fun connectorFor\\(" "$catalog" ||
  fail "Android ModelCatalog.connectorFor() helper is missing"

echo "PASS: multimodal composition contract is present"
