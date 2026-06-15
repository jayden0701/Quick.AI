# OpenAI JSON / MESSAGES_API / Tool Calling 경로 정리 필요

## 배경

현재 Quick.AI에는 OpenAI-compatible 입력을 처리하는 경로가 여러 개 있습니다.

- `runModelHandleWithMessagesStreaming()`
- `runModelHandleWithJsonStreaming()`
- `runModelHandleWithTool()`
- TestAPP의 OpenAI Run 경로
- TestAPP의 Tool API JSON 직접 호출 경로
- CLI의 `--json-smoke`, `--tool-json`

기능 자체는 동작하지만, 지금 구조는 API 선택, capability 판단, JSON parsing, tool/response_format 처리 위치가 여러 곳에 흩어져 있습니다. 그 결과 `MESSAGES_API` 모델, full OpenAI JSON 모델, XGrammar tool calling 모델을 다룰 때 “어떤 입력이 어느 API로 전달되는지”를 코드만 보고 즉시 이해하기 어렵습니다.

이번 issue는 당장 동작을 바꾸자는 요청이 아니라, 추후 리팩터링을 위한 문제 정리와 변경 방향 제안입니다.

## 현재 문제

### 1. API 라우팅이 TestAPP UI 로직에 섞여 있음

현재 SampleTestAPP의 OpenAI Run 경로는 모델 capability에 따라 직접 분기합니다.

```kotlin
if (imgBytesList.isNotEmpty()) {
    e.runMultimodalHandleWithMessagesStreaming(messages, sink)
} else if (Capability.MESSAGES_API in descriptor.capabilities) {
    e.runModelHandleWithMessagesStreaming(messages, sink)
} else {
    e.runModelHandleWithJsonStreaming(jsonText, sink)
}
```

이 구조에서는 UI가 다음 책임을 동시에 갖습니다.

- JSON 문자열 보관
- JSON을 messages로 변환
- 이미지 attachment 삽입
- 모델 capability 해석
- 어떤 backend API를 호출할지 결정
- `tools`, `functions`, `response_format`이 보존되는지 여부를 암묵적으로 결정

UI 입장에서는 “OpenAI request를 실행한다”가 목적이어야 하는데, 현재는 backend routing 정책까지 알고 있습니다.

### 2. `MESSAGES_API` 의미가 불명확함

현재 `MESSAGES_API`는 “messages-based API를 써야 한다”는 의미로 쓰이고 있습니다. 하지만 이 플래그가 실제로 의미하는 바가 상황마다 다릅니다.

- 어떤 모델은 full OpenAI JSON을 받을 수 없어서 messages만 받아야 함
- 어떤 모델은 native engine이지만 message template 경로가 더 적합함
- 어떤 모델은 LiteRT라서 `runModelHandleWithTool()` 자체가 unsupported
- 어떤 모델은 native + `MESSAGES_API`지만 `runModelHandleWithTool()` 직접 호출은 가능함

예를 들어 `gemma4-e2b-qnn`은 native 모델이므로 `runModelHandleWithTool()` 직접 호출은 가능합니다. 하지만 TestAPP의 일반 OpenAI Run 경로에서는 `MESSAGES_API` 분기 때문에 `runModelHandleWithMessagesStreaming()`으로 빠지고, 이때 OpenAI JSON의 `tools`나 `response_format`은 보존되지 않습니다.

즉 사용자가 보는 현상은 다음처럼 혼란스럽습니다.

```json
{
  "messages": [
    {"role": "user", "content": "Return JSON"}
  ],
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "result",
      "schema": {
        "type": "object",
        "properties": {
          "status": {"type": "string"}
        },
        "required": ["status"]
      }
    }
  }
}
```

- full JSON streaming 경로에서는 `response_format`이 native에서 XGrammar로 변환됨
- `MESSAGES_API` 경로에서는 `messages`만 뽑히고 `response_format`은 사라짐
- Tool API JSON 경로에서는 `runModelHandleWithTool()`을 직접 호출하므로 hard constraint 적용 가능

동작은 설명할 수 있지만, API 사용자가 예측하기 어렵습니다.

### 3. JSON parsing이 여러 곳에 분산되어 있음

현재 parsing 책임이 여러 위치에 나뉘어 있습니다.

- TestAPP `parseOpenAIMessages()`
- TestAPP `ToolApiJsonRequest.parse()`
- native `runModelHandleWithJsonStreaming()` 내부의 `response_format` parsing
- CLI `--tool-json` parsing
- chat template renderer 내부의 OpenAI JSON handling

이 때문에 같은 OpenAI-compatible JSON이라도 호출 경로에 따라 지원 필드와 에러 처리 방식이 달라집니다.

예:

- `messages` parsing 실패는 TestAPP에서 처리
- `response_format` malformed error는 native에서 처리
- `tool_schema` object/string/null 허용 여부는 TestAPP와 CLI가 각각 판단
- `tools`/`functions`는 chat template 쪽으로 넘겨짐
- `response_format`은 native에서 제거한 뒤 XGrammar로 사용됨

결과적으로 “Quick.AI가 지원하는 OpenAI request schema”가 한 곳에 정의되어 있지 않습니다.

### 4. `tools`와 XGrammar tool calling의 개념이 섞여 보임

현재 코드상으로는 다음 두 개념이 다릅니다.

- OpenAI JSON `tools` / `functions`
  - chat template에 tool metadata를 넣어 모델에게 보여주는 용도
  - 출력 형식 강제는 하지 않음

- XGrammar `runModelHandleWithTool()`
  - JSON Schema를 grammar로 compile
  - decoding 중 invalid token을 mask
  - 출력 형식을 강제

하지만 TestAPP UI에서는 OpenAI 탭 안에 `Tool API JSON`도 같이 존재합니다. 사용자는 “OpenAI tools”와 “XGrammar tool”의 차이를 이해하지 못하면 어느 입력란을 써야 하는지 헷갈릴 수 있습니다.

### 5. 같은 입력을 CLI, TestAPP, native에서 서로 다르게 표현함

현재 hard-constrained tool 호출은 대략 이런 형태를 씁니다.

```json
{
  "prompt": "Return the exact tool JSON.",
  "tool_name": "android_tool_api_direct_enum",
  "tool_schema": {
    "type": "object",
    "properties": {
      "query": {"type": "string", "enum": ["android aar testing"]},
      "count": {"type": "integer", "enum": [3]}
    },
    "required": ["query", "count"]
  }
}
```

반면 OpenAI `response_format`은 다음 형태입니다.

```json
{
  "messages": [
    {"role": "user", "content": "Return the exact tool JSON."}
  ],
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "android_tool_api_direct_enum",
      "schema": {
        "type": "object",
        "properties": {
          "query": {"type": "string", "enum": ["android aar testing"]},
          "count": {"type": "integer", "enum": [3]}
        },
        "required": ["query", "count"]
      }
    }
  }
}
```

두 입력은 내부적으로 모두 XGrammar를 사용할 수 있지만, 진입점과 parsing 위치가 다릅니다. 장기적으로는 같은 normalized request로 해석될 수 있어야 합니다.

## 개선 목표

### 목표 1. OpenAI-compatible request parsing을 한 계층으로 모으기

Android/TestAPP/native/CLI가 각각 JSON을 해석하지 않고, 공통 request model을 정의합니다.

예상 형태:

```kotlin
data class QuickAiRequest(
    val messages: List<QuickAiChatMessage>,
    val tools: List<OpenAiToolSpec> = emptyList(),
    val responseFormat: ResponseFormat? = null,
    val directTool: DirectToolRequest? = null,
    val attachments: List<RequestAttachment> = emptyList(),
    val rawOptions: Map<String, JsonElement> = emptyMap(),
)
```

native 쪽도 같은 개념을 C++ struct 또는 internal normalized JSON으로 가질 수 있습니다.

```cpp
struct NormalizedOpenAiRequest {
  std::vector<CausalLMChatMessage> messages;
  std::optional<ResponseFormatSpec> response_format;
  std::vector<ToolSpec> tools;
  std::optional<DirectToolSpec> direct_tool;
};
```

중요한 점은 “파싱”과 “실행 라우팅”을 분리하는 것입니다.

### 목표 2. 실행 라우터를 별도 계층으로 분리하기

UI가 capability를 직접 보고 API를 고르지 않게 합니다.

예상 API:

```kotlin
engine.runOpenAiRequest(
    requestJson = jsonText,
    attachments = selectedImages,
    sink = sink
)
```

내부 라우터가 다음을 결정합니다.

- 이 모델이 full JSON streaming을 지원하는가?
- messages-only 경로를 써야 하는가?
- `response_format` hard constraint를 지원하는가?
- OpenAI `tools`를 prompt에 넣을 수 있는가?
- direct XGrammar tool request인가?
- LiteRT 엔진이라면 어떤 기능을 unsupported로 반환해야 하는가?

UI는 라우팅 결과와 에러만 보여줍니다.

### 목표 3. capability를 더 명확하게 쪼개기

현재 `MESSAGES_API` 하나로는 의미가 너무 넓습니다.

예상 capability:

```kotlin
enum class Capability {
    STREAMING,
    OPENAI_MESSAGES_INPUT,
    OPENAI_FULL_JSON_INPUT,
    OPENAI_TOOLS_PROMPTING,
    STRUCTURED_OUTPUT_XGRAMMAR,
    DIRECT_TOOL_XGRAMMAR,
    MULTIMODAL,
    MULTI_IMAGE,
    EMBEDDING,
}
```

예:

| 모델 | capability 예시 |
|---|---|
| `qwen3-0.6b` | `OPENAI_FULL_JSON_INPUT`, `STRUCTURED_OUTPUT_XGRAMMAR`, `DIRECT_TOOL_XGRAMMAR` |
| `function-gemma` | `OPENAI_FULL_JSON_INPUT`, `OPENAI_TOOLS_PROMPTING`, `DIRECT_TOOL_XGRAMMAR` |
| `gemma4-e2b-qnn` | `OPENAI_MESSAGES_INPUT`, `DIRECT_TOOL_XGRAMMAR` 가능 여부를 명시 |
| `gemma4` LiteRT | `OPENAI_MESSAGES_INPUT`, `MULTIMODAL`, direct XGrammar unsupported |

이렇게 되면 `MESSAGES_API`가 “full JSON을 버려도 된다”는 의미처럼 쓰이는 문제를 줄일 수 있습니다.

### 목표 4. `response_format`과 direct tool schema를 같은 structured-output 계층으로 다루기

`response_format`과 `runModelHandleWithTool()`은 진입점은 다르지만 둘 다 “출력 형식 제약”이라는 공통 목적이 있습니다.

공통 내부 표현:

```kotlin
sealed class StructuredOutputSpec {
    data object Text : StructuredOutputSpec()
    data object JsonObject : StructuredOutputSpec()
    data class JsonSchema(
        val name: String?,
        val schema: JsonObject,
        val strict: Boolean,
    ) : StructuredOutputSpec()
}
```

그리고 실행 시점에 엔진별로 지원 경로를 선택합니다.

- native full JSON streaming + `response_format`
  - streaming 가능
  - XGrammar grammar attach

- native direct tool
  - non-streaming
  - XGrammar grammar attach

- LiteRT
  - 현재 unsupported
  - 향후 LiteRT constrained decoding 지원 여부에 따라 확장

### 목표 5. 에러 메시지를 사용자 관점으로 통일하기

현재는 어떤 경로에서 실패했는지에 따라 에러가 다르게 보입니다.

개선 예:

```text
This model accepts messages-only input. The request includes response_format,
but this execution path cannot preserve it. Use a model with OPENAI_FULL_JSON_INPUT
or run direct XGrammar tool output if supported.
```

한국어 TestAPP 메시지 예:

```text
선택한 모델은 messages-only 경로를 사용하므로 response_format을 적용할 수 없습니다.
XGrammar 직접 호출을 지원하는 native 모델이면 Tool API JSON을 사용하세요.
```

이런 메시지가 있으면 사용자는 “왜 JSON schema가 무시됐는지”를 바로 이해할 수 있습니다.

## 제안하는 변경 방향

### 1단계. 현행 동작을 문서화하고 테스트로 고정

먼저 리팩터링 전에 현재 동작을 regression test로 고정합니다.

테스트 항목:

- full OpenAI JSON request에서 `response_format`이 native로 전달되고 XGrammar가 적용되는지
- `MESSAGES_API` 라우팅 시 `response_format`과 `tools`가 보존되지 않는 현재 동작
- `Tool API JSON`이 `runModelHandleWithTool()`을 직접 호출하는 동작
- LiteRT 모델에서 direct tool call이 `UNSUPPORTED`를 반환하는 동작
- malformed `response_format` 에러
- malformed direct `tool_schema` 에러

이 단계에서는 동작을 바꾸지 않습니다.

### 2단계. Android 공통 parser 도입

`SampleTestAPP`에 흩어진 parsing을 AAR 또는 shared Android module 쪽으로 이동합니다.

예상 파일:

```text
Android/QuickDotAI/src/main/java/com/example/quickdotai/openai/OpenAiRequestParser.kt
Android/QuickDotAI/src/main/java/com/example/quickdotai/openai/OpenAiRequestModels.kt
Android/QuickDotAI/src/main/java/com/example/quickdotai/openai/StructuredOutputSpec.kt
```

TestAPP는 parser를 직접 구현하지 않고 호출만 합니다.

```kotlin
val request = OpenAiRequestParser.parse(jsonText, attachments)
engine.runOpenAiRequest(request, sink)
```

`ToolApiJsonRequest.kt`도 장기적으로는 `DirectToolRequest` parser로 통합합니다.

### 3단계. 라우팅 정책을 engine facade로 이동

현재 TestAPP의 capability 분기를 `QuickDotAI` facade 내부로 옮깁니다.

예상 API:

```kotlin
fun runOpenAiRequest(
    request: QuickAiRequest,
    sink: StreamSink
): BackendResult<Unit>
```

라우터 예:

```kotlin
when {
    request.directTool != null && supports(DIRECT_TOOL_XGRAMMAR) ->
        runModelHandleWithTool(...)

    request.responseFormat != null && supports(OPENAI_FULL_JSON_INPUT) ->
        runModelHandleWithJsonStreaming(request.toJson(), sink)

    request.responseFormat != null && !supports(STRUCTURED_OUTPUT_XGRAMMAR) ->
        unsupported("response_format is not supported by this model path")

    supports(OPENAI_MESSAGES_INPUT) ->
        runModelHandleWithMessagesStreaming(request.messages, sink)

    supports(OPENAI_FULL_JSON_INPUT) ->
        runModelHandleWithJsonStreaming(request.toJson(), sink)

    else ->
        unsupported("No compatible OpenAI request path")
}
```

이렇게 하면 TestAPP는 “실행”만 요청하고, 어떤 backend API를 쓸지는 library가 책임집니다.

### 4단계. native C++ parser 정리

현재 `runModelHandleWithJsonStreaming()` 내부에서 직접 `response_format`을 파싱합니다. 이 로직을 helper로 분리합니다.

예상 파일:

```text
api/openai_request_parser.h
api/openai_request_parser.cpp
```

예상 함수:

```cpp
Expected<OpenAiRequest, ErrorCode> parse_openai_request(const json &request);
Expected<ResponseFormatSpec, ErrorCode> parse_response_format(const json &value);
json build_template_request_without_native_only_fields(const OpenAiRequest &request);
```

`runModelHandleWithJsonStreaming()`은 다음 정도만 담당합니다.

1. JSON parse
2. normalized request 생성
3. chat template 적용
4. 필요 시 XGrammar attach
5. streaming 실행

### 5단계. CLI도 같은 request model을 사용

CLI의 `--json-smoke`와 `--tool-json`도 공통 parser를 사용하도록 맞춥니다.

장기적으로는 다음처럼 하나의 CLI 입력으로 합칠 수 있습니다.

```bash
quick_dot_ai_test --openai-json qwen3-0.6b /sdcard/Download/aistudio-mobile/models '<json>' W4A32
```

direct tool은 별도 shortcut으로 유지하되, 내부적으로는 같은 normalized request로 변환합니다.

```bash
quick_dot_ai_test --tool-json qwen3-0.6b /sdcard/Download/aistudio-mobile/models '<json>' W4A32
```

## 기대 결과

리팩터링 후에는 다음 질문에 코드상으로 명확히 답할 수 있어야 합니다.

1. 이 모델은 OpenAI full JSON을 보존하는가?
2. 이 모델은 messages-only 입력만 받는가?
3. `tools`는 prompt hint로 들어가는가, hard constraint로 쓰이는가?
4. `response_format`은 적용되는가, 아니면 unsupported 에러가 나는가?
5. direct XGrammar tool 호출이 가능한가?
6. TestAPP, CLI, AAR에서 같은 JSON이 같은 방식으로 해석되는가?

## Non-goals

이 issue에서 바로 해결하지 않을 범위:

- LiteRT-LM에 XGrammar constrained decoding을 새로 구현
- 모든 모델의 chat template 품질 개선
- OpenAI API 전체 호환성 보장
- tool execution loop 구현
- function call 결과를 다시 model에 넣는 agent loop 구현

이번 issue의 목적은 API 경로와 parsing 책임을 정리하는 것입니다.

## 예시: 개선 후 사용자 경험

### 예시 1. `qwen3-0.6b` + `response_format`

입력:

```json
{
  "messages": [
    {"role": "user", "content": "Return one result."}
  ],
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "result",
      "schema": {
        "type": "object",
        "properties": {
          "status": {"type": "string"}
        },
        "required": ["status"]
      }
    }
  }
}
```

라우팅:

```text
OpenAI full JSON -> response_format parsed -> XGrammar attached -> streaming output
```

### 예시 2. `gemma4-e2b-qnn` + `response_format`

현재는 messages-only 경로로 빠지면서 `response_format`이 사라질 수 있습니다.

개선 후에는 둘 중 하나로 명확하게 처리합니다.

```text
Option A: model supports structured output -> XGrammar path 사용
Option B: model does not support structured output -> 명시적 UNSUPPORTED 반환
```

사용자에게는 “무시됨”이 아니라 “지원되지 않음”으로 보여야 합니다.

### 예시 3. direct tool JSON

입력:

```json
{
  "prompt": "Return the exact tool JSON.",
  "tool_name": "android_tool_api_direct_enum",
  "tool_schema": {
    "type": "object",
    "properties": {
      "query": {"type": "string", "enum": ["android aar testing"]},
      "count": {"type": "integer", "enum": [3]}
    },
    "required": ["query", "count"]
  }
}
```

라우팅:

```text
DirectToolRequest -> supports DIRECT_TOOL_XGRAMMAR 확인 -> runModelHandleWithTool()
```

LiteRT 모델이면:

```text
UNSUPPORTED: this engine does not support direct XGrammar tool output
```

## Acceptance Criteria

- OpenAI request parsing 책임이 TestAPP에서 제거되거나 최소화된다.
- `messages`, `tools`, `functions`, `response_format`, direct tool schema 처리 정책이 한 곳에서 설명되고 테스트된다.
- TestAPP는 capability별 backend API를 직접 고르지 않는다.
- `MESSAGES_API`가 full JSON 필드를 조용히 버리는 경로가 사라지거나, 최소한 명시적 warning/error가 제공된다.
- `response_format`이 적용되는 경로와 unsupported 경로가 테스트로 구분된다.
- CLI, TestAPP, AAR 문서의 예제가 같은 개념 모델을 사용한다.

## 관련 코드 위치

- `Android/SampleTestAPP/src/main/java/com/example/sampletestapp/MainActivity.kt`
  - OpenAI Run 라우팅
  - `parseOpenAIMessages()`
  - Tool API JSON 호출

- `Android/SampleTestAPP/src/main/java/com/example/sampletestapp/ToolApiJsonRequest.kt`
  - direct tool JSON parser

- `Android/QuickDotAI/src/main/java/com/example/quickdotai/QuickDotAI.kt`
  - public engine interface

- `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`
  - native engine implementation

- `Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLm.kt`
  - LiteRT messages API implementation

- `api/quick_dot_ai_api.cpp`
  - `runModelHandleWithMessagesStreaming()`
  - `runModelHandleWithJsonStreaming()`
  - `runModelHandleWithTool()`
  - native `response_format` parsing

- `api-app/test_api.cpp`
  - CLI JSON smoke / tool JSON parsing

## 제안 라벨

- `refactor`
- `android`
- `openai-api`
- `tool-calling`
- `xgrammar`
- `good first design issue`는 아님
