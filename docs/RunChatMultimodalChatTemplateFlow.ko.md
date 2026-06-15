# runChatMultimodalHandleStreaming Chat Template Flow

Date: 2026-06-09

이 문서는 `runChatMultimodalHandleStreaming()`에 chat template가 적용되지 않은
multimodal 입력이 들어왔을 때, Android wrapper부터 native C API까지 어느 단계에서
chat template가 적용되는지 정리한다.

## Short Answer

`NativeQuickDotAI.runChatMultimodalHandleStreaming()`에 들어온 `parts`는 raw prompt로
그대로 LLM에 전달되지 않는다. Android wrapper가 먼저 `parts`를 `USER`
`QuickAiChatMessage`로 감싸고, message 기반 multimodal API로 넘긴다.

message 기반 native API는 `apply_chat_template_messages()`를 호출해 model-local
chat template 또는 hardcoded fallback을 적용한다. 그 결과 문자열은 다시 raw
`runMultimodalHandleStreaming()`으로 전달된다.

단, raw `runMultimodalHandleStreaming()`은 전달받은 prompt가 이미 template 적용된
입력인지 marker substring으로 다시 판단한다. 현재 marker 목록에 없는 포맷은 한 번
더 chat template가 적용될 수 있다.

## End-to-End Flow

```text
NativeQuickDotAI.runChatMultimodalHandleStreaming(parts)
  -> QuickAiChatMessage(role = USER, parts = parts)
  -> NativeQuickDotAI.runMultimodalHandleWithMessagesStreaming(messages)
  -> JNI convertQuickAiChatMessage()
     - role: enum name -> lowercase string
     - content: PromptPart.Text만 공백으로 이어붙임
     - image: pixelValues로 별도 전달
  -> C API runMultimodalHandleWithMessagesStreaming()
  -> apply_chat_template_messages()
     - g_chat_template 있으면 minja template apply
     - 없으면 model_dir/tokenizer_config.json on-demand load
     - 실패하면 architecture별 hardcoded fallback
  -> runMultimodalHandleStreaming(formattedInput, pixelValues, ...)
  -> raw prompt가 이미 formatted인지 marker로 판정
  -> prepare_input_for_model()
     - input_already_formatted=true: 그대로 사용
     - input_already_formatted=false: apply_chat_template() 재호출
  -> execute_multimodal()
```

## Android Wrapper

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`

`runChatMultimodalHandleStreaming()`은 active chat session 존재만 확인하고, 받은
`parts`를 user message 하나로 만든다. 이후 직접 native raw multimodal API를
호출하지 않고 `runMultimodalHandleWithMessagesStreaming()`으로 우회한다.

```kotlin
override fun runChatMultimodalHandleStreaming(
    parts: List<PromptPart>,
    sink: StreamSink
): BackendResult<QuickAiChatResult> {
    if (activeSession == null) {
        val err = BackendResult.Err(
            QuickAiError.BAD_REQUEST,
            "No active chat session — call openChatSession() first"
        )
        sink.onError(err.error, err.message)
        return err
    }

    val messages = listOf(
        QuickAiChatMessage(role = QuickAiChatRole.USER, parts = parts)
    )
    return when (val r = runMultimodalHandleWithMessagesStreaming(messages, forwardingSink)) {
        is BackendResult.Ok -> { ... }
        is BackendResult.Err -> BackendResult.Err(r.error, r.message)
    }
}
```

message 기반 multimodal path에서는 image processor로 `pixelValues`를 만든 뒤 JNI
message API를 호출한다.

```kotlin
NativeCausalLm.runMultimodalHandleWithMessagesStreamingNative(
    handle = handle,
    messages = messages.toTypedArray(),
    addGenerationPrompt = true,
    pixelValues = multimodalInput.pixelValues,
    numPatches = multimodalInput.numPatches,
    originalHeight = multimodalInput.originalHeight,
    originalWidth = multimodalInput.originalWidth,
    listener = object : NativeCausalLm.NativeStreamListener {
        override fun onDelta(text: String) {
            sink.onDelta(text)
        }
    }
)
```

## JNI Message Conversion

파일:
`Android/QuickDotAI/src/main/cpp/quickai_jni.cpp`

JNI는 `QuickAiChatMessage`를 C API의 `CausalLMChatMessage`로 변환한다. role은
lowercase 문자열이 되고, `PromptPart.Text`만 content로 합쳐진다. image part는
content에 들어가지 않는다.

```cpp
// Call enum.name() and convert to lowercase for OpenAI API compatibility
jstring roleNameJ = (jstring)env->CallObjectMethod(roleEnum, nameMid);
if (roleNameJ != nullptr) {
  const char *roleName = env->GetStringUTFChars(roleNameJ, nullptr);
  outRole = roleName ? roleName : "";
  std::transform(outRole.begin(), outRole.end(), outRole.begin(), ::tolower);
  env->ReleaseStringUTFChars(roleNameJ, roleName);
  env->DeleteLocalRef(roleNameJ);
}
```

```cpp
// PromptPart.Text class
jclass textPartCls = env->FindClass("com/example/quickdotai/PromptPart$Text");
...
if (env->IsInstanceOf(partObj, textPartCls)) {
  jstring textJ = (jstring)env->GetObjectField(partObj, textFid);
  if (textJ != nullptr) {
    const char *text = env->GetStringUTFChars(textJ, nullptr);
    if (text != nullptr) {
      if (!content.empty())
        content += " ";
      content += text;
      env->ReleaseStringUTFChars(textJ, text);
    }
    env->DeleteLocalRef(textJ);
  }
}
```

변환된 message 배열과 `pixelValues`는 C API로 전달된다.

```cpp
ErrorCode ec = runMultimodalHandleWithMessagesStreaming(
  handle, msgs.data(), msgs.size(), addGenerationPrompt == JNI_TRUE, pixels,
  numPatches, originalHeight, originalWidth, &stream_trampoline, &ctx);
```

## First Template Application

파일:
`api/quick_dot_ai_api.cpp`

`runMultimodalHandleWithMessagesStreaming()`은 message 배열을 native
`ChatMessage`로 바꾼 뒤 `apply_chat_template_messages()`를 호출한다.

```cpp
auto chat_messages = convertMessages(messages, num_messages);
std::string arch = h.architectures.size() > *llm_index
                     ? h.architectures[*llm_index]
                     : std::string();
std::string model_dir = h.model_dirs.size() > *llm_index
                          ? h.model_dirs[*llm_index]
                          : std::string();
formattedInput = apply_chat_template_messages(
  arch, chat_messages, add_generation_prompt, model_dir);
```

`apply_chat_template_messages()`는 다음 순서로 template를 적용한다.

1. `g_chat_template`가 없고 `model_dir`가 있으면
   `model_dir/tokenizer_config.json`에서 on-demand load를 시도한다.
2. `g_chat_template`가 있으면 `messages`와 `add_generation_prompt`를 JSON request로
   만들어 `g_chat_template->apply(request)`를 호출한다.
3. template apply가 실패하거나 template가 없으면 architecture별 hardcoded fallback을
   사용한다.

```cpp
if (!g_chat_template && !model_dir.empty()) {
  std::string tc_path = model_dir + "/tokenizer_config.json";
  if (check_file_exists(tc_path)) {
    try {
      g_chat_template = causallm::ChatTemplate::Load(model_dir);
      ...
    } catch (const std::exception &e) {
      LOGE("[Warning] Failed to load chat template from %s: %s",
           model_dir.c_str(), e.what());
    }
  }
}
```

```cpp
if (g_chat_template) {
  nlohmann::json request;
  request["messages"] = nlohmann::json::array();
  for (const auto &msg : messages) {
    request["messages"].push_back(
      {{"role", msg.role}, {"content", msg.content}});
  }
  request["add_generation_prompt"] = add_generation_prompt;

  try {
    return g_chat_template->apply(request);
  } catch (const std::exception &e) {
    LOGE("Chat template apply failed: %s", e.what());
  }
}
```

fallback 예시는 다음과 같다.

```cpp
} else if (architecture == "Qwen2ForCausalLM" ||
           architecture == "Qwen3ForCausalLM" ||
           architecture == "Qwen3MoeForCausalLM" ||
           architecture == "Qwen3SlimMoeForCausalLM" ||
           architecture == "Qwen3CachedSlimMoeForCausalLM") {
  for (const auto &msg : messages) {
    result += "<|im_start|>" + msg.role + "\n" + msg.content + "<|im_end|>\n";
  }
  if (add_generation_prompt) {
    result += "<|im_start|>assistant\n";
  }
} else if (architecture == "Gemma4ForCausalLM" ||
           architecture == "Gemma4_E2B_QNN") {
  result += "<bos>";
  for (const auto &msg : messages) {
    std::string role = msg.role;
    if (role == "assistant") {
      role = "model";
    }
    result += "<|turn>" + role + "\n" + msg.content + "<turn|>\n";
  }
  if (add_generation_prompt) {
    result += "<|turn>model\n";
  }
}
```

이 시점의 `formattedInput`은 이미 chat template가 적용된 prompt다.

## Delegation To Raw Multimodal Path

message path는 formatted prompt를 만든 뒤 raw multimodal path로 위임한다.

```cpp
return runMultimodalHandleStreaming(handle, formattedInput.c_str(),
                                    pixelValues, numPatches, originalHeight,
                                    originalWidth, callback, user_data);
```

raw path는 image embedding을 만든 뒤, LLM에 넣을 text input을 준비한다.

```cpp
const std::string raw_input(prompt);
const bool input_already_formatted =
  raw_input.find("<|turn_start|>") != std::string::npos ||
  raw_input.find("<|im_start|>") != std::string::npos ||
  raw_input.find("<start_of_turn>") != std::string::npos;
std::string input = prepare_input_for_model(h, *llm_index, raw_input,
                                            input_already_formatted);

return execute_multimodal(h, llm, image_embeds, input, callback, user_data);
```

`prepare_input_for_model()`은 `input_already_formatted`가 true면 그대로 반환하고,
false면 raw string으로 보고 `apply_chat_template()`를 호출한다.

```cpp
static std::string prepare_input_for_model(CausalLmModel &h, size_t model_index,
                                           const std::string &input,
                                           bool input_already_formatted) {
  if (model_index >= h.architectures.size() || !g_use_chat_template) {
    return input;
  }

  const std::string &architecture = h.architectures[model_index];
  if (h.kv_len > 0) {
    const auto *cb = ModelCallbackRegistry::instance().lookup(architecture);
    if (cb && cb->incremental_prompt) {
      return cb->incremental_prompt(input);
    }
  }

  if (input_already_formatted) {
    return input;
  }

  return apply_chat_template(architecture, input);
}
```

## Raw Prompt Template Application

`apply_chat_template()`는 raw single-turn input을 user message로 감싸 template를
적용한다.

```cpp
if (g_chat_template) {
  nlohmann::json request;
  request["messages"] = nlohmann::json::array();
  request["messages"].push_back({{"role", "user"}, {"content", input}});
  request["add_generation_prompt"] = true;
  try {
    return g_chat_template->apply(request);
  } catch (const std::exception &e) {
    LOGE("Chat template apply failed: %s", e.what());
  }
}
```

fallback도 architecture별로 적용된다.

```cpp
if (architecture == "LlamaForCausalLM") {
  return "[INST] " + input + " [/INST]";
} else if (architecture == "Qwen2ForCausalLM" ||
           architecture == "Qwen3ForCausalLM" ||
           architecture == "Qwen3MoeForCausalLM" ||
           architecture == "Qwen3SlimMoeForCausalLM" ||
           architecture == "Qwen3CachedSlimMoeForCausalLM") {
  return "<|im_start|>user\n" + input + "<|im_end|>\n<|im_start|>assistant\n";
} else if (architecture == "Gemma3ForCausalLM") {
  return "<start_of_turn>user\n" + input +
         "<end_of_turn>\n<start_of_turn>model\n";
} else if (architecture == "Gemma4ForCausalLM" ||
           architecture == "Gemma4_E2B_QNN") {
  return "<bos><|turn>user\n" + input + "<turn|>\n<|turn>model\n";
}
```

## Current Behavior By Format

| Formatted prompt marker | raw path의 `input_already_formatted` | 결과 |
|---|---:|---|
| `<|im_start|>` | true | Qwen 계열 formatted prompt는 그대로 사용 |
| `<start_of_turn>` | true | Gemma3 formatted prompt는 그대로 사용 |
| `<|turn_start|>` | true | 해당 marker를 쓰는 template는 그대로 사용 |
| `[INST]` | false | Llama fallback prompt는 raw로 오인되어 재적용 가능 |
| `<bos><|turn>` | false | Gemma4 fallback prompt는 raw로 오인되어 재적용 가능 |

따라서 `runChatMultimodalHandleStreaming()`의 의도된 흐름은 message 단계에서 한 번
template를 적용하는 것이다. 하지만 raw multimodal path의 formatted marker 감지가
현재 세 가지 substring에만 의존하기 때문에, marker 목록에 없는 template output은
`prepare_input_for_model()`에서 두 번째 template 적용을 받을 수 있다.

## Android Template Application Scenarios

Android public API에서 chat template와 만나는 경로는 backend별로 다르다.

| Backend | Android API / helper | Android에서 하는 일 | Template 적용 위치 |
|---|---|---|---|
| `NativeQuickDotAI` | `runChatModelHandleStreaming(text)` | raw text를 `USER` message로 감싼다 | C API `apply_chat_template_messages()` |
| `NativeQuickDotAI` | `runChatMultimodalHandleStreaming(parts)` | `parts`를 `USER` multimodal message로 감싼다 | C API `apply_chat_template_messages()`, 이후 raw multimodal 재판정 |
| `NativeQuickDotAI` | `runModelHandleWithMessagesStreaming(messages)` | caller의 `QuickAiChatMessage[]`를 JNI로 넘긴다 | C API `apply_chat_template_messages()` |
| `NativeQuickDotAI` | `runModelHandleWithJsonStreaming(json)` | OpenAI JSON string을 JNI로 넘긴다 | C API `g_chat_template->apply(request)` |
| `NativeQuickDotAI` | `runMultimodalHandle*Streaming(parts)` | text/image를 분리해 raw multimodal JNI로 넘긴다 | C API `prepare_input_for_model()`에서 필요 시 적용 |
| `NativeCausalLm` low-level | `runModelHandleStreamingNative(prompt)` | raw prompt를 그대로 JNI로 넘긴다 | C API `prepare_input_for_model()`에서 필요 시 적용 |
| `LiteRTLm` | chat / multimodal session APIs | `QuickAiChatMessage`를 LiteRT-LM `Message`/`Contents`로 변환한다 | LiteRT-LM `Conversation` 내부 |
| `LiteRTLm` | `runModelHandleWithMessagesStreaming(messages)` | text-only prompt string을 만들어 `Conversation.sendMessage()` 호출 | LiteRT-LM `Conversation` 내부 또는 raw prompt 처리 |

### Native Text Chat Session

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`

`runChatModelHandleStreaming(text)`는 active session을 찾고
`NativeChatSession.runStreaming(text, sink)`로 위임한다.

```kotlin
override fun runChatModelHandleStreaming(
    text: String,
    sink: StreamSink
): BackendResult<QuickAiChatResult> {
    val session = activeSession
    if (session == null) {
        val err = BackendResult.Err(
            QuickAiError.BAD_REQUEST,
            "No active chat session — call openChatSession() first"
        )
        sink.onError(err.error, err.message)
        return err
    }
    return session.runStreaming(text, sink)
}
```

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeChatSession.kt`

`NativeChatSession`은 raw text를 `QuickAiChatMessage(USER, Text(text))`로 바꾼다.
따라서 Android caller가 chat template를 직접 적용하지 않아도 message 기반 native
API가 template를 적용한다.

```kotlin
// Convert raw text to message format for C++ chat template
val messages = listOf(
    QuickAiChatMessage(
        role = QuickAiChatRole.USER,
        parts = listOf(PromptPart.Text(text))
    )
)

val errorCode = NativeCausalLm.runModelHandleWithMessagesStreamingNative(
    handle,
    messages.toTypedArray(),
    true,
    object : NativeCausalLm.NativeStreamListener {
        override fun onDelta(text: String) {
            if (cancelRequested.get()) return
            accumulated.append(text)
            sink.onDelta(text)
        }
    }
)
```

결론: text chat session은 Android에서 raw text를 message로 정규화하고, native
`runModelHandleWithMessagesStreaming()`에서 `apply_chat_template_messages()`가
실행된다.

### Native OpenAI Messages API

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`

`runModelHandleWithMessagesStreaming(messages)`는 caller가 이미 만든 OpenAI-style
message list를 JNI로 그대로 넘긴다. Android는 template를 렌더링하지 않는다.

```kotlin
val errorCode = NativeCausalLm.runModelHandleWithMessagesStreamingNative(
    handle = handle,
    messages = messages.toTypedArray(),
    addGenerationPrompt = true,
    listener = object : NativeCausalLm.NativeStreamListener {
        override fun onDelta(text: String) {
            sink.onDelta(text)
        }
    }
)
```

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt`

JNI binding은 이 API가 C API의 message streaming entry로 바로 이어진다는 것을
명시한다.

```kotlin
external fun runModelHandleWithMessagesStreamingNative(
    handle: Long,
    messages: Array< QuickAiChatMessage>,
    addGenerationPrompt: Boolean,
    listener: NativeStreamListener
): Int
```

결론: messages API는 Android에서 role/parts DTO만 전달하고, template 렌더링은 C
API의 `apply_chat_template_messages()` 책임이다.

### Native OpenAI JSON API

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`

`runModelHandleWithJsonStreaming(jsonRequest)`는 OpenAI-compatible JSON string을 JNI로
넘긴다.

```kotlin
val errorCode = NativeCausalLm.runModelHandleWithJsonStreamingNative(
    handle = handle,
    jsonRequest = jsonRequest,
    listener = object : NativeCausalLm.NativeStreamListener {
        override fun onDelta(text: String) {
            sink.onDelta(text)
        }
    }
)
```

파일:
`Android/QuickDotAI/src/main/cpp/quickai_jni.cpp`

JNI는 JSON 문자열을 C API `runModelHandleWithJsonStreaming()`에 전달한다.

```cpp
const char *jsonRequest = env->GetStringUTFChars(jsonRequestJ, nullptr);
...
ErrorCode ec = runModelHandleWithJsonStreaming(handle, jsonRequest,
                                               &stream_trampoline, &ctx);
```

결론: JSON API는 Android에서 message 추출이나 template 렌더링을 하지 않는다.
C API가 JSON을 parse하고 `g_chat_template->apply(request)`를 호출한다. 이 경로는
model-local chat template가 필요하며, unavailable이면 unsupported로 실패할 수
있다.

### Native Raw Multimodal API

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`

`runMultimodalHandle()`과 `runMultimodalHandleStreaming()`은 `parts`에서 image를
preprocess하고 text만 `textPrompt`로 추출한다. 그 뒤 raw multimodal JNI API를
호출한다.

```kotlin
val multimodalInput = prepareMultimodalInput(parts, processor)
    ?: return BackendResult.Err(
        QuickAiError.INVALID_PARAMETER,
        "No valid image found in parts"
    )

val textPrompt = extractTextPrompt(parts)

val errorCode = NativeCausalLm.runMultimodalHandleStreamingNative(
    handle,
    textPrompt,
    multimodalInput.pixelValues,
    multimodalInput.numPatches,
    multimodalInput.originalHeight,
    multimodalInput.originalWidth,
    object : NativeCausalLm.NativeStreamListener {
        override fun onDelta(text: String) {
            accumulated.append(text)
        }
    }
)
```

streaming variant도 같은 구조다.

```kotlin
NativeCausalLm.runMultimodalHandleStreamingNative(
    handle,
    textPrompt,
    multimodalInput.pixelValues,
    multimodalInput.numPatches,
    multimodalInput.originalHeight,
    multimodalInput.originalWidth
) { delta ->
    sink.onDelta(delta)
}
```

결론: raw multimodal API는 Android에서 chat message로 감싸지 않는다. C API의
`runMultimodalHandleStreaming()`이 `textPrompt`에 formatted marker가 있는지 보고,
없으면 `prepare_input_for_model()`에서 raw single-turn template를 적용한다.

### Native Low-Level Raw Text JNI

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt`

low-level JNI에는 raw prompt를 받는 `runModelHandleStreamingNative()`가 있다.

```kotlin
external fun runModelHandleStreamingNative(
    handle: Long,
    prompt: String,
    listener: NativeStreamListener
): Int
```

파일:
`Android/QuickDotAI/src/main/cpp/quickai_jni.cpp`

JNI는 prompt를 그대로 C API에 넘긴다.

```cpp
const char *prompt = env->GetStringUTFChars(promptJ, nullptr);
...
ErrorCode ec =
  runModelHandleStreaming(handle, prompt, &stream_trampoline, &ctx);
```

결론: 이 경로는 Android에서 message 변환이 없다. native
`runModelHandleStreaming()`이 `input_already_formatted`와 `g_use_chat_template`를
보고 template 적용 여부를 결정한다.

### Native Multimodal Chat Session

`NativeQuickDotAI.runChatMultimodalHandleStreaming(parts)`는 앞에서 설명한 것처럼
`parts`를 `USER` message 하나로 감싼 뒤
`runMultimodalHandleWithMessagesStreaming(messages, sink)`를 호출한다.

```kotlin
val messages = listOf(
    QuickAiChatMessage(role = QuickAiChatRole.USER, parts = parts)
)
return when (val r = runMultimodalHandleWithMessagesStreaming(messages, forwardingSink)) {
    is BackendResult.Ok -> { ... }
    is BackendResult.Err -> BackendResult.Err(r.error, r.message)
}
```

`runMultimodalHandleWithMessagesStreaming()`은 image preprocessing을 한 뒤 message
JNI API를 호출한다.

```kotlin
NativeCausalLm.runMultimodalHandleWithMessagesStreamingNative(
    handle = handle,
    messages = messages.toTypedArray(),
    addGenerationPrompt = true,
    pixelValues = multimodalInput.pixelValues,
    numPatches = multimodalInput.numPatches,
    originalHeight = multimodalInput.originalHeight,
    originalWidth = multimodalInput.originalWidth,
    listener = object : NativeCausalLm.NativeStreamListener {
        override fun onDelta(text: String) {
            sink.onDelta(text)
        }
    }
)
```

결론: multimodal chat session은 message 기반 template 적용을 먼저 받은 뒤, raw
multimodal path의 formatted marker 재판정을 한 번 더 통과한다.

### LiteRT-LM OpenAI Messages API

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLm.kt`

`LiteRTLm.runModelHandleWithMessagesStreaming(messages)`는 native C API의
`apply_chat_template_messages()`를 사용하지 않는다. 현재 구현은 message role과 text
part를 단일 prompt string으로 합친 뒤 LiteRT-LM `Conversation.sendMessage(prompt)`에
전달한다.

```kotlin
val prompt = messages.joinToString("\n") { msg ->
    "${msg.role}: ${msg.parts.filterIsInstance<PromptPart.Text>().joinToString("") { it.text }}"
}

return try {
    val message = c.sendMessage(prompt)
    val text = message.toString()
    if (text.isNotEmpty()) sink.onDelta(text)
    sink.onDone()
    BackendResult.Ok(Unit)
} catch (t: Throwable) {
    val err = BackendResult.Err(QuickAiError.INFERENCE_FAILED, t.message)
    sink.onError(err.error, err.message)
    err
}
```

결론: LiteRT-LM messages API는 Android에서 lightweight text prompt를 조립하고
LiteRT-LM runtime에 맡긴다. Quick.AI native C API의 `g_chat_template`는 관여하지
않는다.

### LiteRT-LM Flat Multimodal API

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLm.kt`

`LiteRTLm.runMultimodalHandle()`은 Android에서 `PromptPart`를 LiteRT-LM `Contents`로
바꾼 뒤 `Conversation.sendMessage(contents)`를 호출한다.

```kotlin
val contents = try {
    toLiteRtContents(parts)
} catch (t: Throwable) {
    ...
}

val message = c.sendMessage(contents)
val output = message.toString()
```

streaming도 `sendMessageAsync(contents, callback)`으로 위임한다.

```kotlin
c.sendMessageAsync(contents, callback)
```

결론: `LiteRTLm` flat multimodal 경로에서는 Quick.AI Android wrapper가 Jinja/minja
chat template를 직접 적용하지 않는다. `Conversation`에 text/image `Contents`를
넘기고, LiteRT-LM runtime이 모델에 맞는 prompt 구성과 state 관리를 수행한다.

### LiteRT-LM Chat Session API

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLm.kt`

`runChatModelHandleStreaming(text)`와 `runChatMultimodalHandleStreaming(parts)`는
각각 `QuickAiChatMessage`를 만든 뒤 `LiteRTLmChatSession.runStreaming()`으로 넘긴다.

```kotlin
val messages = listOf(
    QuickAiChatMessage(role = QuickAiChatRole.USER, parts = listOf(PromptPart.Text(text)))
)
session.runStreaming(messages, sink)
```

```kotlin
val messages = listOf(
    QuickAiChatMessage(role = QuickAiChatRole.USER, parts = parts)
)
session.runStreaming(messages, sink)
```

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLmChatSession.kt`

session은 마지막 user turn에 image가 있으면 `Contents`, 없으면 plain text를
`Conversation`에 보낸다.

```kotlin
if (hasImages(prep.lastUser)) {
    val contents = toChatContents(prep.lastUser)
    c.sendMessageAsync(contents, callback, extraContext)
} else {
    val text = extractText(prep.lastUser)
    c.sendMessageAsync(text, callback, extraContext)
}
```

`chatRebuild()`나 prior turns가 있는 경우에는 seed messages를 LiteRT-LM
`ConversationConfig.initialMessages`로 넘겨 conversation을 다시 만든다.

```kotlin
val initial = try {
    messages.map { toLiteRtMessage(it) }
} catch (t: Throwable) {
    ...
}
conversation = createConversationFromConfig(engine, config, initial)
```

결론: LiteRT-LM chat session에서는 Android가 message/content 변환과 session seed
구성을 담당하지만, Quick.AI native C API의 `g_chat_template`나
`apply_chat_template_messages()`는 사용하지 않는다.

## Practical Implications

- chat template가 없는 raw `parts` 입력은 Android wrapper에서 `USER` message로
  변환되고, message 기반 C API에서 template가 적용된다.
- image는 chat template content에 직접 들어가지 않는다. template에는 text part만
  들어가고, image tensor는 `pixelValues`로 별도 전달된다.
- raw `runMultimodalHandleStreaming()`을 직접 호출하는 경우에는 raw prompt가 먼저
  vision encoder에 전달되고, 이후 LLM input 준비 단계에서 template 적용 여부가
  결정된다.
- double-apply를 피하려면 raw multimodal path의 `input_already_formatted` 판정을
  template 종류와 일치시키거나, message path에서 raw path로 넘길 때 명시적으로
  `input_already_formatted=true`를 전달할 수 있는 구조가 필요하다.
