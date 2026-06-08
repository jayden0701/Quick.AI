# Quick.AI Repository Orientation

이 문서는 레포를 처음 다시 볼 때 필요한 현재 구조, 실제 앱 경로, 추론 엔진 경로,
빌드 진입점을 빠르게 확인하기 위한 요약이다. `docs/superpowers` 아래의
plans/specs 문서는 이미 완료된 작업 기록으로 보고, 현재 작업 지시로 해석하지 않는다.

## 현재 핵심 구조

Quick.AI는 on-device LLM stack이다. 최상위 프로젝트가 Android 앱/AAR, native
C API, Quick.AI model extensions, QNN integration을 묶고 있고, 실제 추론 엔진의
기반은 `nntrainer` 하위 레포다.

```text
.
├── Android/
│   ├── QuickDotAI/       # Android AAR module
│   └── SampleTestAPP/    # 실제 테스트/설치 대상 sample app
├── api/                  # public C API: libquick_dot_ai_api.so
├── qnn/                  # QNN context/plugin support
├── src/                  # Quick.AI native models, descriptors, resources
├── nntrainer/            # nntrainer 기반 추론 엔진
├── xgrammar/             # structured generation support
├── build.sh              # native unified build
├── apk-build-install.sh  # Android native build + APK install workflow
└── apk_install_android.sh
```

`git status --short` 기준으로 현재 최상위 작업트리는 `nntrainer`가 modified로
표시된다. 이 문서는 레포 파악만 했고 해당 변경은 건드리지 않았다.

## Android 앱 경로

실제로 테스트/배포하는 Android 쪽은 `Android/` 아래에 있다.

`Android/settings.gradle.kts`에는 현재 두 모듈만 포함되어 있다.

```text
:QuickDotAI
:SampleTestAPP
```

`Android/QuickDotAI`는 앱이 사용하는 AAR 모듈이다. 주요 파일은 다음과 같다.

| Path | 역할 |
|---|---|
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/QuickDotAI.kt` | public Kotlin interface |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/Types.kt` | request/response DTO, enum, error, metrics |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/ModelCatalog.kt` | model descriptor catalog and selection |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt` | native backend wrapper |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt` | low-level JNI declarations |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/SigLipNaFlexImageProcessor.kt` | SigLIP/LFM2 native preprocessing |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/JepaImageProcessor.kt` | JEPA/QNN native preprocessing |
| `Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLm.kt` | LiteRT-LM backend wrapper |
| `Android/QuickDotAI/src/main/cpp/quickai_jni.cpp` | Kotlin/JNI bridge |
| `Android/QuickDotAI/src/main/cpp/CMakeLists.txt` | builds `libquickai_jni.so` and links native libs |
| `Android/QuickDotAI/prebuilt_libs/` | APK packaging용 prebuilt `.so` 위치 |

`Android/SampleTestAPP`는 현재 실제 실행 가능한 앱이다. 핵심 구현은
`Android/SampleTestAPP/src/main/java/com/example/sampletestapp/MainActivity.kt`에
있고, `:QuickDotAI`를 직접 링크해서 테스트한다.

기존 문서에 언급되는 REST server, foreground service, launcher/client split은
현재 Gradle build에 들어간 구현체가 아니라 계획된 계층이다.

## Native 추론 경로

Android native backend의 큰 흐름은 다음과 같다.

```text
SampleTestAPP
  -> QuickDotAI Kotlin API
  -> NativeQuickDotAI / NativeCausalLm
  -> libquickai_jni.so
  -> libquick_dot_ai_api.so
  -> Quick.AI model descriptors/configs
  -> nntrainer CausalLM runtime
```

`api/quick_dot_ai_api.h`와 `api/quick_dot_ai_api.cpp`가 public C API의 중심이다.
새 코드에서는 legacy enum보다 string model id 기반의 handle API를 우선 사용한다.

중요 API 흐름:

| API | 용도 |
|---|---|
| `getModelCatalogJson()` | native descriptor registry를 JSON catalog로 노출 |
| `loadModelHandleByName()` | string model id로 native model handle load |
| `runModelHandleWithMessagesStreaming()` | OpenAI-style messages streaming |
| `runModelHandleWithJsonStreaming()` | OpenAI JSON request streaming |
| `runMultimodalHandle*()` | multimodal-capable model path |
| `loadMultimodalCompositionJson()` | descriptor-driven multimodal composition load |
| `cancelModelHandle()` | cooperative cancellation |
| `destroyModelHandle()` | handle release |

`ModelCatalog.kt`는 JNI를 통해 native catalog를 읽고, Kotlin 계층에서 LiteRT
descriptor를 병합한다. Android UI는 family/runtime/backend 3축으로 descriptor를
선택한 뒤 `QuickDotAI.createEngine(context, descriptor)`로 engine을 만든다.

Descriptor-driven multimodal composition is the current path for freely pairing
component models. Catalog descriptors carry a role (`TEXT_LLM`,
`VISION_ENCODER`, `CONNECTOR`, or `COMPOSITION`), an optional embedding
dimension, and `compatible_with` metadata. The C API validates composition JSON
such as `lfm2-siglip` or `lfm2-jepa` before loading the role-tagged components
into one handle. Android exposes the same path through `LoadModelRequest`
composition fields and `NativeCausalLm.loadMultimodalCompositionJsonNative()`.

## nntrainer 관계

`nntrainer`는 Android가 이용하는 실제 추론 엔진 기반이다. Quick.AI는
`nntrainer/Applications/CausalLM`의 CausalLM runtime과 factory registration을
활용하면서, Quick.AI-specific model과 descriptor는 최상위 `src/`와 `api/`에 둔다.

모델 추가/수정 시 보통 확인할 위치:

| Path | 역할 |
|---|---|
| `src/models/` | Quick.AI model implementations |
| `src/model_descriptors_*.cpp` 또는 관련 descriptor TU | model catalog registration |
| `src/res/<model>/` | config/generation/nntr config resources |
| `api/model_descriptor.h` | descriptor struct/API |
| `api/model_config.cpp` | descriptor to config resolution |
| `nntrainer/Applications/CausalLM/` | nntrainer CausalLM base runtime |

QNN 관련 모델은 `src/models/qnn/`와 `qnn/`을 같이 봐야 한다.

## QNN 경로

QNN은 Android build에서 `--enable-qnn`으로 켜진다. 관련 위치는 다음과 같다.

| Path | 역할 |
|---|---|
| `qnn/README.md` | QNN context guide |
| `qnn/qnn_context.cpp` | QNN context library entry |
| `qnn/jni/qnn/` | QNN SDK wrapper/header support |
| `src/models/qnn/` | QNN model implementation support |
| `src/models/qnn/gemma4-e2b-qnn/` | Gemma4 E2B QNN model |
| `src/res/gemma4-e2b-qnn/` | QNN model config resources |

QNN model ids such as `gemma4-e2b-qnn`, `vjepa-qnn` are Android/QNN-enabled
build에서 catalog에 나타나는 구조다.

## Build and Install

Android 전체 workflow는 사용자가 알려준 대로 `./apk-build-install.sh`가 진입점이다.

스크립트 흐름:

1. `NDK_ROOT`를 확인하고 `ANDROID_NDK`로 export한다.
2. `./build.sh --platform=android --enable-qnn --clean`으로 native libraries를 빌드한다.
3. `./apk_install_android.sh`로 Android packaging용 native libraries를 준비한다.
4. `./install_libs/*.so`를 `Android/QuickDotAI/prebuilt_libs/`로 복사한다.
5. `Android/gradlew ":SampleTestAPP:installDebug"`를 실행한다.

실행 전 필요한 환경:

```bash
export NDK_ROOT=/path/to/android-ndk
./apk-build-install.sh
```

`build.sh`는 x86과 Android를 모두 지원한다.

```bash
./build.sh
./build.sh --platform=android
./build.sh --platform=android --enable-qnn
./build.sh --target=src,api
./build.sh --clean
```

Android 실기기 테스트는 APK install 후 `SampleTestAPP`를 기준으로 진행한다.

## 문서 읽는 순서

새 작업 전에 빠르게 볼 문서는 다음 순서가 효율적이다.

1. `README.md`: 프로젝트 전체 목적, supported models, build options.
2. `docs/Architecture.md`: native architecture, C API, descriptor registry.
3. `Android/Architecture.md`: 현재 Android module 상태와 packaging 흐름.
4. `Android/QuickDotAI/README.md`: Kotlin AAR API.
5. `api/README.md`: C API reference.
6. `qnn/README.md`: QNN 작업이 있을 때만 추가 확인.
7. `docs/ChatAndOpenAIUsage.md`, `docs/XGrammarReference.md`: chat/tool/JSON streaming 작업 때 확인.

`docs/superpowers` 아래의 specs/plans는 과거 작업 산출물 성격으로 보고, 현재
작업 요구사항은 사용자의 최신 지시를 우선한다.

## 작업 시작 전 체크포인트

코드를 수정하기 전에 보통 다음을 확인하면 된다.

```bash
git status --short
git diff --stat
```

Android/API 작업이면 다음 파일부터 보면 된다.

```text
Android/SampleTestAPP/src/main/java/com/example/sampletestapp/MainActivity.kt
Android/QuickDotAI/src/main/java/com/example/quickdotai/
Android/QuickDotAI/src/main/cpp/quickai_jni.cpp
api/quick_dot_ai_api.h
api/quick_dot_ai_api.cpp
```

Native model/QNN 작업이면 다음 파일부터 보면 된다.

```text
src/models/
src/res/
api/model_config.cpp
api/model_descriptors_public.cpp
qnn/
nntrainer/Applications/CausalLM/
```
