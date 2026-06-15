# Multimodal Composition Architecture Direction

Date: 2026-06-11

이 문서는 Quick.AI와 nntrainer의 multimodal 처리 구조를 앞으로 어떤 방향으로
정리할지 논의하기 위한 아키텍처 메모다. 구현 계획이 아니라, 여러 모델 조합과
backend 조합을 장기적으로 관리하기 위한 구조적 방향을 정리한다.

## 배경

현재 Quick.AI에는 multimodal 경로가 두 가지 성격으로 존재한다.

| 경로 | 현재 소유권 | 특징 |
|---|---|---|
| QNN multimodal path | Quick.AI C API handle이 vision encoder와 LLM pointer를 들고 실행 | 외부 API 계층에서 두 모델의 관계와 실행 순서를 알고 있다. |
| LFM-VL path | nntrainer 모델 구현 안에서 vision, connector, LLM을 소유 | 외부에서는 하나의 모델처럼 보이지만 조합별 composite model class가 늘어날 수 있다. |

우리가 원하는 방향은 다음에 가깝다.

- LLM은 CPU/NPU 등 여러 backend로 교체 가능해야 한다.
- vision encoder도 CPU/NPU 등 여러 backend로 교체 가능해야 한다.
- connector 또는 projector도 조합별로 명시적으로 관리되어야 한다.
- 외부 API는 component pointer나 실행 순서를 몰라야 한다.
- 앱 개발자는 가능한 한 하나의 multimodal model handle만 다뤄야 한다.

## 현재 구조의 문제

### 1. Composition 소유권이 API 계층에 새고 있다

QNN 경로에서는 Quick.AI C API가 여러 sub-model pointer를 들고, convention으로
`models[0] = vision`, `models[1] = LLM` 같은 순서를 해석한다. 이 방식은 초기
bring-up에는 빠르지만, 조합이 많아질수록 API 계층이 model runtime의 내부 구조를
너무 많이 알게 된다.

외부 API가 다음 책임을 갖기 시작하면 경계가 불안정해진다.

- 어떤 component가 vision encoder인지 판단
- 어떤 component가 text LLM인지 판단
- vision output을 LLM embedding space에 맞추는 방법 결정
- image token 위치에 embedding을 splice하는 정책 관리
- backend별 tensor ownership과 dtype 변환 처리

이 책임들은 본질적으로 model runtime에 가까우며, nntrainer 쪽에 있어야 한다.

### 2. Fat composite model은 사용성은 좋지만 조합 확장성이 약하다

LFM-VL처럼 하나의 class가 `vision + connector + LLM`을 모두 들고 있으면 외부 API는
단순해진다. 그러나 `LLM x vision encoder x connector x backend` 조합이 늘어나면
각 조합마다 새 composite class 또는 새 loader branch가 필요해질 수 있다.

이 방식은 특정 제품 모델을 빠르게 안정화하는 데 좋지만, "여러 LLM과 여러 vision
encoder를 자유롭게 조합한다"는 목표에는 적합하지 않다.

### 3. Preprocessing 정책이 Android wrapper에 쌓일 위험이 있다

현재 native multimodal path는 Android에서 image를 decode하고, vision model id에
따라 processor를 선택해 float tensor를 만든다. 단기적으로는 현실적인 구조지만,
vision encoder가 늘어나면 AAR wrapper에 model-specific if/else가 계속 추가될 수
있다.

장기적으로는 vision descriptor가 요구하는 preprocessing spec을 노출하고, Android
wrapper는 그 spec을 실행하는 방향이 더 좋다.

## 아키텍처 선택지

### Option A. Quick.AI가 composition을 계속 소유한다

Quick.AI C API handle이 여러 component를 들고 실행 순서를 관리한다.

장점:

- 현재 QNN 구조와 가깝다.
- 기존 public C API를 크게 바꾸지 않고 확장하기 쉽다.
- Android/JNI에서 composition JSON을 다루기 쉽다.

단점:

- API 계층이 model runtime 내부 책임을 계속 갖는다.
- slot convention 또는 role lookup이 Quick.AI에 남는다.
- nntrainer 내부 composite model과 Quick.AI external composition이 계속 다른
  추상화로 존재한다.

이 방식은 bridge로는 유용하지만 최종 구조로 두기에는 계층이 높다.

### Option B. 모든 multimodal 조합을 fat composite model로 만든다

각 조합을 하나의 nntrainer model class로 구현한다.

장점:

- 외부 API는 가장 단순하다.
- 하나의 모델처럼 load/run/cancel/metrics를 처리할 수 있다.
- model-specific 최적화를 넣기 쉽다.

단점:

- 조합 수가 늘어날수록 class와 loader가 늘어난다.
- connector와 fusion policy를 재사용하기 어렵다.
- CPU/NPU component 교체를 일반화하기 어렵다.

특정 fully packaged model에는 적합하지만, pluggable composition의 기본 구조로는
부담이 크다.

### Option C. nntrainer에 generic multimodal composition runtime을 둔다

Quick.AI는 catalog와 load request를 전달하고, nntrainer가 component ownership,
execution, fusion을 담당한다.

장점:

- 외부 API는 하나의 model handle만 본다.
- QNN path와 LFM-VL path를 같은 개념으로 설명할 수 있다.
- component 교체, connector 재사용, backend 선택을 한 구조로 다룰 수 있다.
- model runtime 책임이 nntrainer 내부에 모인다.

단점:

- 초기 설계 비용이 가장 크다.
- tensor contract, connector contract, preprocessing metadata를 명확히 정의해야 한다.
- 기존 QNN slot-based path와 LFM-VL fat model path를 단계적으로 수렴시켜야 한다.

권장 방향은 Option C다.

## 권장 책임 분리

최종 구조에서는 Quick.AI와 nntrainer의 책임을 다음처럼 나눈다.

```text
Quick.AI / C API / Android
  - model catalog
  - composition id
  - component/backend selection
  - model path/native lib path
  - public handle lifecycle
  - JSON/Kotlin/JNI request boundary

nntrainer CausalLM
  - component ownership
  - vision execution
  - connector projection
  - text embedding lookup
  - image/text embedding fusion
  - LLM generation
  - streamer/cancel/metrics/KV-cache integration
```

Quick.AI는 control plane이고, nntrainer는 execution plane이다.

## Target Model

nntrainer 안에 `ComposedMultimodalModel` 또는 이에 준하는 runtime 개념을 둔다.
이 object는 외부에서는 하나의 `Transformer`처럼 보이지만, 내부적으로 role 기반
component를 가진다.

필수 role은 다음과 같다.

| Role | 책임 |
|---|---|
| `TEXT_LLM` | token embedding lookup, embedding prefill generation, text decoding |
| `VISION_ENCODER` | image tensor를 vision feature 또는 image embedding으로 변환 |
| `CONNECTOR` | vision output을 LLM embedding space로 project |
| `FUSION_POLICY` | image embedding을 text embedding sequence에 삽입 |
| `PREPROCESS_SPEC` | image decode/resize/normalize/layout 요구사항을 기술 |
| `COMPOSITION` | 검증된 component tuple과 default backend 조합 |

`CONNECTOR`가 없는 조합은 identity connector로 표현한다. 이렇게 하면 connector가
있는 모델과 없는 모델을 같은 pipeline에서 다룰 수 있다.

## Execution Flow

권장 실행 흐름은 다음과 같다.

```text
runMultimodal(handle, messages/images)
  -> composition resolves TEXT_LLM / VISION_ENCODER / CONNECTOR
  -> image preprocessing according to PREPROCESS_SPEC
  -> vision_encoder.encode(image_tensor)
  -> connector.project(vision_features)
  -> llm.lookupEmbedding(text token ids)
  -> fusion_policy.splice(text embeddings, image embeddings)
  -> llm.runWithEmbeddings(prefill embeddings)
  -> stream tokens through existing streamer
```

이 흐름은 backend와 무관해야 한다. CPU vision, NPU vision, CPU LLM, NPU LLM은 같은
role interface를 구현하고, runtime은 role contract만 보고 실행해야 한다.

## Tensor Contract

현재 `void * + size` 형태의 `multimodal_pointer`는 장기적으로 부족하다. 최소한
다음 정보를 담는 명시적 tensor contract가 필요하다.

```text
EmbeddingTensor
  - data pointer
  - byte size
  - dtype
  - shape
  - token count
  - embedding dimension
  - quant scale/offset
  - layout
  - ownership policy
```

이 정보가 있어야 다음 문제가 명확해진다.

- vision output이 LLM embedding dimension과 맞는가?
- connector가 필요한가?
- FP32, FP16, quantized embedding을 어떻게 전달하는가?
- CPU memory와 NPU buffer ownership은 누가 갖는가?
- multi-image/video frame batch를 어떤 layout으로 표현하는가?

초기 구현에서는 단순 struct로 시작하고, backend-specific memory handle은 optional
field로 뒤에 추가하는 편이 좋다.

## Compatibility Graph

임의의 LLM과 vision encoder를 아무렇게나 붙이면 안 된다. catalog는 가능한 조합을
명시적으로 제한해야 한다.

필요한 compatibility 조건:

- LLM이 `runWithEmbeddings`를 지원하는가?
- LLM embedding dtype과 bytes per token을 알 수 있는가?
- vision output dimension이 connector input과 맞는가?
- connector output dimension이 LLM hidden size와 맞는가?
- image token id 또는 placeholder policy가 정의되어 있는가?
- preprocessing spec이 vision encoder와 일치하는가?
- 선택한 backend 조합이 runtime에서 동시에 지원되는가?

따라서 catalog는 단순 model list가 아니라 compatibility graph를 제공해야 한다.

```json
{
  "id": "lfm2-jepa",
  "role": "composition",
  "components": {
    "llm": ["lfm2-jepa-llm"],
    "vision": ["jepa-qnn-vision"],
    "connector": ["lfm2-jepa-connector"]
  },
  "default_backends": {
    "llm": "CPU",
    "vision": "NPU",
    "connector": "CPU"
  }
}
```

Advanced caller는 backend override를 줄 수 있지만, 기본 UI와 일반 API는 검증된
composition id 중심으로 동작하는 것이 좋다.

## Public API Shape

외부 API는 component 구조를 숨긴다. 가장 단순한 형태는 다음이다.

```c
loadModelHandleByName(CAUSAL_LM_BACKEND_AUTO, "lfm2-jepa", quant, ...);
```

backend override가 필요하면 composition JSON을 받는 별도 advanced API를 둔다.

```json
{
  "composition_id": "lfm2-jepa",
  "components": {
    "llm": {
      "id": "lfm2-jepa-llm",
      "backend": "CPU"
    },
    "vision": {
      "id": "jepa-qnn-vision",
      "backend": "NPU"
    },
    "connector": {
      "id": "lfm2-jepa-connector",
      "backend": "CPU"
    }
  }
}
```

중요한 점은 이 API가 component pointer를 노출하지 않는다는 것이다. load가 끝나면
caller는 여전히 하나의 `CausalLmHandle`만 가진다.

## Android/AAR 방향

Android public API도 하나의 engine abstraction을 유지한다.

일반 사용자는 다음만 선택한다.

- family 또는 composition id
- runtime
- backend 또는 default backend
- model path

고급 UI에서는 LLM backend, vision backend, connector backend를 보여줄 수 있다.
하지만 실행 시에는 항상 하나의 `QuickDotAI` engine으로 load/run한다.

Image preprocessing은 단계적으로 다음 방향으로 이동한다.

1. 단기: 현재처럼 vision model id별 `NativeImageProcessor` 선택을 유지한다.
2. 중기: catalog descriptor에 `preprocess_spec`을 추가한다.
3. 장기: Android wrapper는 hard-coded processor 선택 대신 spec-driven processor를
   사용한다.

## Migration Strategy

### Phase 1. 용어와 metadata 정리

현재 문서와 코드에서 사용하는 용어를 통일한다.

- component
- role
- composition
- connector
- fusion policy
- preprocess spec
- embedding tensor

이 단계에서는 큰 동작 변경 없이 catalog와 문서의 의미를 맞춘다.

### Phase 2. Role 기반 descriptor 확장

`ModelDescriptor`에 role, embedding metadata, compatibility metadata,
preprocessing metadata를 추가한다. 기존 descriptor와 ABI 호환이 필요한 경우 catalog
JSON을 확장하고 C struct 변경은 최소화한다.

Android `ModelCatalog`는 이 metadata를 읽되, 기존 단일 모델 path는 그대로 유지한다.

### Phase 3. nntrainer composition runtime 도입

nntrainer CausalLM 안에 role 기반 component container를 둔다.

초기에는 다음만 지원해도 충분하다.

- one vision encoder
- optional connector
- one text LLM
- one image placeholder policy
- single-image path

multi-image/video는 같은 abstraction 위에서 다음 단계로 확장한다.

### Phase 4. QNN slot-based path를 composer로 이동

Quick.AI C API가 직접 `models[0]`, `models[1]`를 해석하던 부분을 줄이고, nntrainer
composer object를 하나의 model처럼 로드한다.

이 단계의 목표는 public API 동작을 유지하면서 내부 소유권을 이동하는 것이다.

### Phase 5. LFM-VL fat model을 preset composition으로 수렴

LFM-VL의 기존 구현은 유지하되, 내부 구조를 composer의 concrete preset으로 표현할 수
있게 한다.

완전히 generic composer로 옮기는 것이 어렵다면, 최소한 catalog와 public API에서는
다른 composition과 같은 형태로 보이게 한다.

### Phase 6. Spec-driven preprocessing

vision encoder descriptor에서 resize, normalize, layout, patching 정책을 노출한다.
Android wrapper는 processor class name이 아니라 spec을 보고 tensor를 만든다.

## Non-goals

이 문서가 당장 요구하지 않는 것:

- 모든 LLM과 모든 vision encoder의 자유 조합
- 모든 backend 사이의 zero-copy tensor 전달
- 기존 `runMultimodalHandle*` public API 제거
- LiteRT-LM 내부 preprocessing 구조 변경
- OpenAI tool calling 또는 XGrammar routing 개편

목표는 먼저 ownership과 interface boundary를 정리하는 것이다.

## 주요 리스크

### Tensor ownership

CPU pointer와 NPU buffer를 같은 API로 다루려면 ownership policy가 명확해야 한다.
초기에는 CPU memory 중심으로 두고, backend buffer는 optional extension으로 두는 것이
현실적이다.

### Quantized embedding

LLM이 quantized embedding을 소비하는 경우 vision/connector output의 scale/offset
정합성이 중요하다. 이 정보는 ad-hoc setter보다 tensor metadata로 전달되는 편이
좋다.

### Chat template와 image placeholder

image token id와 placeholder 문자열은 model별로 다르다. fusion policy는 tokenizer와
chat template 결과를 함께 고려해야 한다.

### API compatibility

기존 Android 앱과 C API caller가 깨지지 않아야 한다. 따라서 public method 이름은
유지하고 내부 routing만 새 composer로 보내는 방식이 안전하다.

## 제안 결론

최종 방향은 다음 한 문장으로 정리할 수 있다.

> Quick.AI는 multimodal composition을 선택하고, nntrainer는 multimodal composition을 실행한다.

QNN의 현재 pointer-pair 방식은 bring-up bridge로 유지할 수 있지만, 최종 소유권은
nntrainer runtime으로 이동하는 것이 좋다. LFM-VL의 fat model 방식은 사용성 측면의
좋은 기준이지만, 조합 확장성을 위해 generic composition runtime 위의 preset으로
수렴시키는 것이 좋다.

외부 API는 계속 하나의 model handle만 노출한다. 내부에서는 role 기반 component,
명시적 tensor contract, compatibility graph, spec-driven preprocessing으로 정리한다.
