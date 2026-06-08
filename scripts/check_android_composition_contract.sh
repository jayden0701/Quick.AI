#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

types_kt="$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/Types.kt"
native_kt="$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt"
native_jni_kt="$root_dir/Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt"
main_kt="$root_dir/Android/SampleTestAPP/src/main/java/com/example/sampletestapp/MainActivity.kt"

rg -q 'SerialName\("composition_id"\).*compositionId' "$types_kt" ||
  fail "LoadModelRequest.compositionId is missing"
rg -q 'SerialName\("llm_model_id"\).*llmModelId' "$types_kt" ||
  fail "LoadModelRequest.llmModelId is missing"
rg -q 'SerialName\("llm_backend"\).*llmBackend' "$types_kt" ||
  fail "LoadModelRequest.llmBackend is missing"
rg -q 'SerialName\("connector_model_id"\).*connectorModelId' "$types_kt" ||
  fail "LoadModelRequest.connectorModelId is missing"
rg -q 'SerialName\("connector_backend"\).*connectorBackend' "$types_kt" ||
  fail "LoadModelRequest.connectorBackend is missing"
rg -q 'compositionId.*llmModelId.*llmBackend.*visionModelId.*visionBackend.*connectorModelId.*connectorBackend.*quantization' "$types_kt" ||
  fail "LoadModelRequest.modelKey does not include composition components"

rg -q 'loadMultimodalCompositionJsonNative' "$native_jni_kt" ||
  fail "JNI declaration for composition load is missing"
rg -q 'buildMultimodalCompositionJson' "$native_kt" ||
  fail "NativeQuickDotAI composition JSON builder is missing"
rg -q 'req\.compositionId != null' "$native_kt" ||
  fail "NativeQuickDotAI.load does not route composition requests"
rg -q 'loadMultimodalCompositionJsonNative' "$native_kt" ||
  fail "NativeQuickDotAI.load does not call the composition JNI entry point"
rg -q 'processorForVisionModel' "$native_kt" ||
  fail "NativeQuickDotAI does not select image processor by vision model id"
rg -q 'SigLipNaFlexImageProcessor' "$native_kt" ||
  fail "SigLIP processor is not selected for SigLIP vision"
rg -q 'JepaImageProcessor' "$native_kt" ||
  fail "JEPA processor is not selected for JEPA vision"

rg -q 'selCompositionId' "$main_kt" ||
  fail "SampleTestAPP composition selection state is missing"
rg -q 'selLlmModelId' "$main_kt" ||
  fail "SampleTestAPP LLM component selection state is missing"
rg -q 'selVisionModelId' "$main_kt" ||
  fail "SampleTestAPP vision component selection state is missing"
rg -q 'selConnectorModelId' "$main_kt" ||
  fail "SampleTestAPP connector display state is missing"
rg -q 'llmOptionsForComposition' "$main_kt" ||
  fail "SampleTestAPP does not use catalog LLM options for compositions"
rg -q 'visionOptionsForLlm' "$main_kt" ||
  fail "SampleTestAPP does not use catalog vision options for selected LLM"
rg -q 'connectorFor' "$main_kt" ||
  fail "SampleTestAPP does not resolve connector from LLM/vision pair"
rg -q 'compositionId = selCompositionId' "$main_kt" ||
  fail "SampleTestAPP load request does not pass compositionId"
rg -q 'CONNECTOR' "$main_kt" ||
  fail "SampleTestAPP connector UI label is missing"

echo "PASS: Android composition contract is present"
