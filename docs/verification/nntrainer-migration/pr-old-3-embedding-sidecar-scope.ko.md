# PR-OLD-3 Embedding Sidecar Scope Report

## Scope

- PR/work unit: PR-OLD-3 후보 범위 확정
- Source commits consulted:
  - OLD `76dc25a6`: `quantized_lut_path`, `output_quant_scale`, `output_quant_offset`, `ufixed8` manifest 기반 4-bit embedding LUT
  - OLD `e0a68c75`: `.json` manifest와 raw `UINT16` sidecar embedding 이중 경로
  - OLD `b64ac377`: signed 4-bit `sfixed4` rowwise scale
  - OLD `e9692a60`: 위 변경들의 merge 결과
- Files expected:
  - `Applications/CausalLM/layers/embedding_layer.h`
  - `Applications/CausalLM/layers/embedding_layer.cpp`
  - focused `EmbeddingLayer` unit test target

## Main Gap

`main`에는 generic `UINT4`/`QINT4` tensor class와 safetensors `U4`/`I4`
dtype mapping이 이미 존재한다. 따라서 PR-OLD-3의 핵심은 tensor dtype
이식이 아니라, CausalLM `EmbeddingLayer`가 OLD의 sidecar embedding LUT를
읽고 실행하는 경로를 현재 main 구조에 맞게 추가하는 것이다.

현재 main의 CausalLM embedding은 managed weight 기반 `FP32`/`FP16`/
`Q4_0`/`Q6_K` 흐름에 머물러 있다. OLD의 Android QNN/Gemma4 계열에서
필요했던 별도 embedding sidecar 파일 경로는 main에 없다.

## Recommended PR Boundary

PR-OLD-3는 아래 항목만 포함한다.

- `quantized_lut_path=<path>` layer property.
- non-`.json` 경로: raw `UINT16` sidecar embedding 파일을 token row 단위로
  memcpy하여 출력.
- `.json` manifest 경로: packed unsigned 4-bit `ufixed8` LUT와 tensorwise
  scale/offset dequant.
- `.json` manifest 경로: packed signed 4-bit `sfixed4` LUT와 per-row scale
  dequant.
- `output_quant_scale` / `output_quant_offset` 기반 `UINT16` output
  requantization.
- sidecar mode에서는 token-id input dtype을 OLD와 같이 `FP32`로 강제하는지
  검증 후 반영.

## Explicitly Out Of Scope

- tokenizer cache, BPE, WordPiece, tokenizer_loader.
- OLD의 debug `std::cout` dump.
- current main `EmbeddingLayer` wholesale replacement.
- global `Name` lowercasing 정책 변경. QNN graph name exact-match 문제가
  별도 재현될 때만 분리 PR로 다룬다.
- generic `UINT4`/`QINT4` tensor dtype 이식. main에 이미 존재하므로 skip
  대상으로 본다.

## Tests To Write First

- raw `UINT16` fixture: 2 vocab rows x 4 dims, token IDs `[1, 0]`, exact output.
- `ufixed8` manifest fixture: packed nibbles plus scale/offset, `FP32` output과
  `UINT16` requant output 검증.
- `sfixed4` manifest fixture: `0x7`/`0x8` nibble로 sign extension `7`/`-8`
  및 rowwise scale 검증.
- negative cases:
  - invalid `out_dim`
  - row scale count mismatch
  - unsupported manifest datatype

## Affected Models

- `/home/jrock/Quick.AI/gemma4_it_x86/nntr_config.json`은
  `embedding_dtype: "Q6_K"`와 managed weight model
  `nntr_gemma4_q40_embdq6k.bin`을 사용하므로 이 PR의 sidecar path를 직접
  검증하지 않는다. x86 smoke test는 regression 확인용이다.
- Android QNN Gemma4 계열 model 중 `embedding_file_name`/`ple_file_name`에
  4-bit sidecar `.bin` 또는 `.json` manifest를 쓰는 구성이 실제 영향권이다.

## Current Decision

PR-OLD-3는 skip하지 않는다. 다만 "UINT4/signed dtype 지원"이라는 넓은
제목 대신 "CausalLM EmbeddingLayer sidecar LUT support"로 좁혀 진행한다.
