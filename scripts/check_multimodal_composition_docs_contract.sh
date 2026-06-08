#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

orientation="$root_dir/docs/RepositoryOrientation.md"
guides="$root_dir/docs/Guides.md"
api_readme="$root_dir/api/README.md"
android_readme="$root_dir/Android/QuickDotAI/README.md"
android_arch="$root_dir/Android/Architecture.md"
plan="$root_dir/docs/superpowers/plans/2026-06-08-multimodal-composition.md"

rg -q "TEXT_LLM.*VISION_ENCODER.*CONNECTOR.*COMPOSITION" "$api_readme" ||
  fail "C API docs do not describe model roles"
rg -q "loadMultimodalCompositionJson" "$api_readme" ||
  fail "C API docs do not document loadMultimodalCompositionJson"
rg -q "lfm2-siglip" "$api_readme" ||
  fail "C API docs do not list lfm2-siglip composition"
rg -q "lfm2-jepa" "$api_readme" ||
  fail "C API docs do not list lfm2-jepa composition"
rg -q "pair-specific weights" "$api_readme" ||
  fail "C API docs do not explain pair-specific weights"

rg -q "compositionId = \"lfm2-jepa\"" "$android_readme" ||
  fail "Android README does not show LFM2+JEPA LoadModelRequest"
rg -q "llmModelId = \"lfm2-jepa-llm\"" "$android_readme" ||
  fail "Android README does not show LFM2+JEPA LLM component"
rg -q "visionModelId = \"jepa-qnn-vision\"" "$android_readme" ||
  fail "Android README does not show LFM2+JEPA vision component"
rg -q "connectorModelId = \"lfm2-jepa-connector\"" "$android_readme" ||
  fail "Android README does not show LFM2+JEPA connector component"
rg -q "SigLipNaFlexImageProcessor.*JepaImageProcessor" "$android_readme" ||
  fail "Android README does not document processor selection"

rg -q "descriptor-driven multimodal composition" "$orientation" ||
  fail "Repository orientation does not mention composition architecture"
rg -q "Multimodal Composition" "$guides" ||
  fail "Guides hub does not link composition docs"
rg -q "composition fields" "$android_arch" ||
  fail "Android architecture does not describe composition fields"
rg -q "loadMultimodalCompositionJsonNative" "$android_arch" ||
  fail "Android architecture does not describe JNI composition load"

rg -q "Task 10.*Documentation and End-to-End Verification" "$plan" ||
  fail "Plan Task 10 section is missing"
rg -q "Final verification passed" "$plan" ||
  fail "Plan Task 10 does not record final verification"

echo "PASS: multimodal composition documentation contract is present"
