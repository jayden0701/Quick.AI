#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

api_h="$root_dir/api/quick_dot_ai_api.h"
android_api_h="$root_dir/Android/QuickDotAI/src/main/cpp/include/quick_dot_ai_api.h"
api_cpp="$root_dir/api/quick_dot_ai_api.cpp"
jni_cpp="$root_dir/Android/QuickDotAI/src/main/cpp/quickai_jni.cpp"
native_kt="$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt"

rg -q "loadMultimodalCompositionJson\\(" "$api_h" ||
  fail "C API declaration for loadMultimodalCompositionJson is missing"

rg -q "loadMultimodalCompositionJson\\(" "$android_api_h" ||
  fail "Android JNI bundled C API header lacks loadMultimodalCompositionJson"

rg -q "ErrorCode loadMultimodalCompositionJson\\(" "$api_cpp" ||
  fail "C API implementation for loadMultimodalCompositionJson is missing"

rg -q "composition_json" "$api_cpp" ||
  fail "composition_json parameter is not parsed by the C API implementation"

for field in \
  "llm" \
  "vision" \
  "connector" \
  "model_id" \
  "backend"; do
  rg -q "\"$field\"" "$api_cpp" ||
    fail "composition JSON field '$field' is not handled"
done

rg -q "descriptor_has_role" "$api_cpp" ||
  fail "composition loader does not validate descriptor roles"

rg -q "descriptor_allows_backend" "$api_cpp" ||
  fail "composition loader does not validate component backends"

rg -q "descriptor_compatible_with" "$api_cpp" ||
  fail "composition loader does not validate component compatibility"

rg -q "Java_com_example_quickdotai_NativeCausalLm_loadMultimodalCompositionJsonNative" \
  "$jni_cpp" ||
  fail "JNI entry point for loadMultimodalCompositionJsonNative is missing"

rg -q "external fun loadMultimodalCompositionJsonNative\\(" "$native_kt" ||
  fail "Kotlin external declaration for loadMultimodalCompositionJsonNative is missing"

echo "PASS: multimodal composition API contract is present"
