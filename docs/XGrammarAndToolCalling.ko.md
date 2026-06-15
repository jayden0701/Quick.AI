# XGrammar와 Tool Calling 사용 정리

이 문서는 현재 Quick.AI 코드 기준으로 XGrammar와 tool 호출을 어떻게 부르고 사용할 수 있는지 정리한 한글 설명서입니다.

핵심은 `tool`이라는 말이 코드 안에서 두 가지 의미로 쓰인다는 점입니다.

| 구분 | 호출 API | 노출 범위 | 실제 동작 |
|---|---|---|---|
| OpenAI JSON `tools` / `functions` | `runModelHandleWithJsonStreaming()` | Android / JNI / Native C API | tool 정보를 chat template에 넣어 모델에게 보여줍니다. 출력 형식은 강제하지 않습니다. |
| XGrammar tool schema | `runModelHandleWithTool()` | Android / JNI / Native C API | JSON Schema 기반 grammar를 붙여 decoding 중 invalid token을 막습니다. |
| OpenAI JSON `response_format` | `runModelHandleWithJsonStreaming()` | Android / JNI / Native C API | OpenAI-compatible request 안의 JSON schema를 XGrammar grammar로 변환해 출력 형식을 강제합니다. |

## 1. OpenAI JSON tools 경로

Android 앱이나 AAR 사용자 입장에서는 이 경로가 OpenAI-style tool metadata 전달의 기본 사용법입니다.

Kotlin에서는 `QuickDotAI.runModelHandleWithJsonStreaming()`을 호출합니다.

```kotlin
engine.runModelHandleWithJsonStreaming(jsonRequest, sink)
```

입력 JSON은 OpenAI 스타일의 `messages`와 `tools`를 포함할 수 있습니다.

```json
{
  "messages": [
    {"role": "system", "content": "You can call tools when they are useful."},
    {"role": "user", "content": "01012345678 번호로 상담 예약 확인 문자를 보내줘."}
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "send_sms",
        "description": "Send a text message to a phone number.",
        "parameters": {
          "type": "object",
          "properties": {
            "phone_number": {"type": "string"},
            "message": {"type": "string"}
          },
          "required": ["phone_number", "message"]
        }
      }
    }
  ]
}
```

호출 흐름은 다음과 같습니다.

1. Android `NativeQuickDotAI.runModelHandleWithJsonStreaming()`
2. JNI `NativeCausalLm_runModelHandleWithJsonStreamingNative()`
3. Native C API `runModelHandleWithJsonStreaming()`
4. `ChatTemplate::apply(request)`
5. minja chat-template renderer가 `messages`, `tools`, `functions`를 prompt로 변환
6. 변환된 prompt로 일반 streaming inference 실행

관련 코드:

- `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`
- `Android/QuickDotAI/src/main/cpp/quickai_jni.cpp`
- `api/quick_dot_ai_api.cpp`
- `nntrainer/Applications/CausalLM/chat_template.cpp`

주의할 점은 이 경로가 실제 외부 함수를 실행하지 않는다는 것입니다. 모델이 `send_sms` 같은 tool call 형태의 텍스트를 생성하면, 앱 또는 상위 레이어가 그 결과를 파싱하고 실제 함수를 실행해야 합니다.

또한 OpenAI JSON `tools`는 hard constraint가 아닙니다. 모델에게 tool schema를 보여주는 역할을 하며, 생성 결과가 반드시 schema-valid JSON이 되도록 강제하지 않습니다.

OpenAI-compatible structured output이 필요하면 같은 JSON streaming API에 `response_format`을 추가합니다. 이 경우 `response_format`은 chat template로 넘기지 않고 native에서 XGrammar schema로 변환합니다.

```json
{
  "messages": [
    {"role": "system", "content": "Return only JSON."},
    {"role": "user", "content": "Make a three item test checklist."}
  ],
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "test_checklist",
      "strict": true,
      "schema": {
        "type": "object",
        "properties": {
          "title": {"type": "string"},
          "items": {
            "type": "array",
            "items": {"type": "string"}
          }
        },
        "required": ["title", "items"]
      }
    }
  }
}
```

현재 지원하는 `response_format.type`은 다음과 같습니다.

| type | 동작 |
|---|---|
| `text` | 기존 일반 streaming 생성 |
| `json_object` | `{ "type": "object" }` schema로 JSON object 출력을 강제 |
| `json_schema` | `response_format.json_schema.schema`를 XGrammar JSON Schema로 사용 |

## 2. XGrammar hard-constrained output 경로

XGrammar 경로는 모델 출력 자체가 JSON Schema를 따르도록 token sampling 단계에서 제약을 거는 방식입니다.

Native C API는 다음 함수를 제공합니다.

```c
ErrorCode runModelHandleWithTool(CausalLmHandle handle,
                                 const char *inputTextPrompt,
                                 const char **outputText,
                                 const char *tool_name,
                                 const char *tool_schema);
```

Android AAR에서는 같은 기능을 Kotlin API로 호출할 수 있습니다.

```kotlin
val schema = """
{
  "type": "object",
  "properties": {
    "query": {"type": "string"},
    "count": {"type": "integer"}
  },
  "required": ["query"]
}
""".trimIndent()

val result = engine.runModelHandleWithTool(
    prompt = "Search recent AI news.",
    toolName = "web_search",
    toolSchema = schema
)
```

파라미터 의미는 다음과 같습니다.

| 파라미터 | 의미 |
|---|---|
| `handle` | `loadModelHandle()`로 로드한 모델 handle |
| `inputTextPrompt` | 모델에 넣을 prompt |
| `outputText` | 생성 결과를 받을 포인터 |
| `tool_name` | 등록된 grammar/tool 이름 |
| `tool_schema` | 동적 등록에 사용할 JSON Schema 문자열 |

### 2.1 Toolset.json으로 미리 등록하기

모델 디렉터리에 `Toolset.json`을 넣으면 모델 로드 시 자동으로 grammar가 precompile됩니다.

```text
model/
  tokenizer.json
  config.json
  nntr_config.json
  Toolset.json
```

`Toolset.json` 예:

```json
{
  "set_alarm": {
    "type": "object",
    "properties": {
      "time": {"type": "string", "pattern": "^\\d{2}:\\d{2}$"},
      "label": {"type": "string"}
    },
    "required": ["time"]
  }
}
```

이후 Native C API에서 schema 없이 tool 이름만 지정할 수 있습니다.

```c
const char *output = NULL;

ErrorCode err = runModelHandleWithTool(
    handle,
    "Create an alarm for 07:00.",
    &output,
    "set_alarm",
    NULL);
```

### 2.2 동적 schema로 등록하기

`Toolset.json`에 없는 tool이라도 첫 호출에서 `tool_schema`를 넘기면 `XGrammarManager::registerTool()`로 등록됩니다.

```c
const char *schema =
    "{"
    "\"type\":\"object\","
    "\"properties\":{"
    "  \"query\":{\"type\":\"string\"},"
    "  \"count\":{\"type\":\"integer\"}"
    "},"
    "\"required\":[\"query\"]"
    "}";

const char *output = NULL;

ErrorCode err = runModelHandleWithTool(
    handle,
    "Search recent AI news.",
    &output,
    "web_search",
    schema);
```

여기서 `tool_schema`는 OpenAI tool object 전체가 아니라 JSON Schema입니다. OpenAI function tool 기준으로 보면 `function.parameters`에 해당하는 부분을 넘겨야 합니다.

### 2.3 SampleTestAPP에서 Tool JSON으로 직접 호출하기

`SampleTestAPP`의 OpenAI 탭에는 `TOOL API JSON` 입력 영역이 있습니다.
이 영역은 OpenAI JSON `tools`와 다릅니다. `Run Tool JSON` 버튼은 아래
형태의 JSON을 파싱한 뒤 항상 `runModelHandleWithTool()`을 직접 호출합니다.

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

CLI에서도 같은 경로를 테스트할 수 있습니다.

```bash
export ANDROID_SERIAL=R3CW202SCPB
adb shell 'cd /data/local/tmp/Quick.AI &&
  export LD_LIBRARY_PATH=/data/local/tmp/Quick.AI:$LD_LIBRARY_PATH &&
  export NNTR_NUM_THREADS=7 &&
  ./quick_dot_ai_test --tool-json qwen3-0.6b \
    /sdcard/Download/aistudio-mobile/models \
    "{\"prompt\":\"Return the exact tool JSON.\",\"tool_name\":\"android_tool_api_direct_enum\",\"tool_schema\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"enum\":[\"android aar testing\"]},\"count\":{\"type\":\"integer\",\"enum\":[3]}},\"required\":[\"query\",\"count\"]}}" \
    W4A32'
```

검증용 schema는 `enum`처럼 좁은 제약을 두는 것이 좋습니다. XGrammar는
구조적으로 invalid token을 막지만, 필드 값의 의미나 길이는 schema와 prompt가
제한해야 합니다.

## 3. XGrammar 내부 동작

모델 로드 시 `api/quick_dot_ai_api.cpp`에서 XGrammar manager를 초기화합니다.
multi-model handle에서는 vision encoder가 아니라 실제 text-generation 모델의
tokenizer 기준으로 초기화합니다.

```cpp
size_t model_index = text_generation_model_index(h);
auto *tokenizer = h.models[model_index]->getTokenizer();
unsigned int vocab_size = h.models[model_index]->getVocabSize();
causallm::XGrammarManager::Instance().initialize(tokenizer, vocab_size);
```

그 뒤 모델 디렉터리에 `Toolset.json`이 있으면 자동으로 로드합니다.

```cpp
std::string default_toolset_path = abs_model_dir + "/Toolset.json";
if (check_file_exists(default_toolset_path)) {
  loadToolset(default_toolset_path.c_str(), tokenizer, vocab_size);
}
```

`XGrammarManager`는 다음 상태를 보관합니다.

| 구성요소 | 역할 |
|---|---|
| `TokenizerInfo` | 모델 vocabulary를 XGrammar가 이해하는 형태로 보관 |
| `GrammarCompiler` | JSON Schema, EBNF, regex를 compiled grammar로 변환 |
| `compiled_grammars_` | `tool_name -> XGrammar` 캐시 |

`XGrammar` 객체는 compiled grammar와 matcher를 들고 있습니다. Sampling 중에는 matcher가 다음 token bitmask를 만들고, invalid token의 logit을 막습니다.

QNN 경로에서는 `Quick_Dot_AI_QNN::sample()`에서 다음 순서로 동작합니다.

1. `xgrammar_->applyGrammarMask(...)`로 invalid token logit masking
2. 일반 sampler 호출
3. sampled token을 `GrammarMatcher::AcceptToken(token)`에 반영
4. 다음 token bitmask를 다시 채움

CPU `CausalLM::generate()` 경로도 같은 순서로 grammar mask를 적용합니다. 따라서 `qwen3-0.6b` 같은 CPU transformer 모델에서도 `runModelHandleWithTool()`과 OpenAI `response_format`의 hard constraint가 적용됩니다.

관련 코드:

- `src/xgrammar/xgrammar_manager.h`
- `src/xgrammar/xgrammar_manager.cpp`
- `src/xgrammar/xgrammar_wrapper.h`
- `src/xgrammar/xgrammar_wrapper.cpp`
- `src/models/qnn/quick_dot_ai_qnn.cpp`
- `nntrainer/Applications/CausalLM/models/causal_lm.cpp`

## 4. Cache 동작

`Toolset.json`을 compile하는 비용이 있을 수 있어서 sidecar cache를 사용합니다.

```text
Toolset.json
Toolset.json.cache
```

동작 순서:

1. `Toolset.json.cache`가 있으면 먼저 cache에서 grammar를 로드합니다.
2. cache가 없거나 불완전하면 `Toolset.json`에서 schema를 읽어 compile합니다.
3. compile에 성공하면 새 cache를 저장합니다.

개발 중 schema를 수정했다면 stale cache를 피하기 위해 `Toolset.json.cache`를 삭제하고 다시 로드하는 것이 안전합니다.

## 5. 현재 코드 기준 제한사항

현재 구현에서 중요한 제한은 다음과 같습니다.

1. OpenAI JSON `tools/functions`는 schema 강제가 아닙니다.
   - chat template에 tool 정보를 넣어 모델이 tool-call 형태를 생성하도록 유도합니다.
   - 실제 tool 실행, 결과 파싱, 후속 tool response message 구성은 앱 또는 호출자가 처리해야 합니다.

2. `runModelHandleWithTool()`은 non-streaming C API이므로 AAR API도 complete string을 반환합니다.
   - Streaming hard-constraint 출력이 필요하면 `runModelHandleWithJsonStreaming()`의 `response_format` 경로를 사용합니다.

3. `MESSAGES_API` 모델은 OpenAI 탭의 일반 Run 경로에서 `messages`만 전달합니다.
   - 이 경로는 JSON 전체의 `tools`나 `response_format`을 보존하지 않습니다.
   - hard-constrained tool schema 테스트는 `TOOL API JSON` 입력이나 CLI `--tool-json`을 사용합니다.

4. `XGrammarManager`는 singleton입니다.
   - 여러 모델 handle을 동시에 로드하거나 다른 tokenizer 모델을 번갈아 쓰는 경우, manager 상태가 마지막 초기화 모델 기준으로 바뀔 수 있습니다.

## 6. 언제 어떤 경로를 써야 하나

| 목표 | 추천 경로 |
|---|---|
| Android 앱에서 tool metadata를 모델에게 전달 | `runModelHandleWithJsonStreaming()` + OpenAI JSON `tools` |
| 모델이 tool call 형태로 답하도록 유도 | OpenAI JSON `tools/functions` |
| 생성 텍스트가 반드시 JSON Schema를 따르게 만들기 | AAR/native `runModelHandleWithTool()` 또는 JSON streaming `response_format` |
| 실제 외부 함수 실행까지 자동화 | 현재 코드에는 별도 executor가 없으므로 앱/상위 레이어에서 직접 구현 |

정리하면, Quick.AI의 OpenAI `tools/functions`는 “tool 정보를 prompt에 넣는 방식”이고, XGrammar는 “출력 형식을 token 단위로 강제하는 방식”입니다. schema-valid output이 필요하면 AAR/native `runModelHandleWithTool()` 또는 OpenAI-compatible `response_format`을 사용해야 합니다.
