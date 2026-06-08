#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

api_cpp="$root_dir/api/quick_dot_ai_api.cpp"
connector_h="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_connector.h"
connector_cpp="$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl/lfm2_vl_connector.cpp"

rg -q "struct ConnectorAdapter" "$api_cpp" ||
  fail "ConnectorAdapter interface is missing"

rg -q "output_embedding_dim\\(\\) const" "$api_cpp" ||
  fail "ConnectorAdapter.output_embedding_dim() is missing"

rg -q "project\\(const std::vector<float> &vision_features" "$api_cpp" ||
  fail "ConnectorAdapter.project() is missing"

rg -q "class Lfm2ConnectorAdapter" "$api_cpp" ||
  fail "Lfm2ConnectorAdapter wrapper is missing"

rg -q "std::vector<std::unique_ptr<ConnectorAdapter>> connectors;" "$api_cpp" ||
  fail "CausalLmModel.connectors storage is missing"

rg -q "connector->loadWeights" "$api_cpp" ||
  fail "LFM connector adapter does not load connector weights"

rg -q "std::vector<float> project\\(" "$connector_h" ||
  fail "Lfm2VlConnector.project() declaration is missing"

rg -q "Lfm2VlConnector::project" "$connector_cpp" ||
  fail "Lfm2VlConnector.project() implementation is missing"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
fixture="$tmpdir/connector_project_shape.cpp"
cat >"$fixture" <<'CPP'
#include "lfm2_vl_connector.h"

#include <fstream>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2)
    return 10;

  {
    std::ofstream weights(argv[1], std::ios::binary);
    std::vector<float> values(31, 0.0f);
    weights.write(reinterpret_cast<const char *>(values.data()),
                  static_cast<std::streamsize>(values.size() *
                                               sizeof(float)));
  }

  causallm::Lfm2VlConnector connector(/*in_features=*/4, /*hidden_size=*/3,
                                      /*out_features=*/2);
  connector.loadWeights(argv[1]);

  const std::vector<float> features{1.0f, 2.0f, 3.0f, 4.0f};
  const auto projected =
    connector.project(features, /*n_patches=*/4, /*embed_dim=*/1,
                      /*patch_h=*/2, /*patch_w=*/2);
  return projected.size() == 2 ? 0 : 20;
}
CPP

c++ -std=c++17 \
  -I"$root_dir/nntrainer/Applications/CausalLM/models/lfm2/lfm2-vl" \
  "$fixture" "$connector_cpp" -o "$tmpdir/connector_project_shape"
"$tmpdir/connector_project_shape" "$tmpdir/connector_weights.bin" ||
  fail "Lfm2VlConnector.project() did not return the expected shape"

echo "PASS: multimodal connector contract is present"
