# Android Multimodal Image Preprocessing

Date: 2026-06-09

이 문서는 Android 측에서 multimodal image가 들어왔을 때 이미지가 어디에서
decode되고, 어떤 processor가 선택되며, resize/normalization 후 어떤 형태로 native
엔진에 전달되는지 정리한다.

범위는 Android/Kotlin 계층이다. native vision encoder 내부의 추가 처리나
LiteRT-LM 엔진 내부 구현은 이 문서에서 다루지 않는다.

## Short Answer

Android에는 두 multimodal 경로가 있다.

| 경로 | Android에서 resize/normalize를 직접 하는가? | 설명 |
|---|---:|---|
| `NativeQuickDotAI` | Yes | encoded image를 `Bitmap`으로 decode한 뒤 vision model id에 맞는 `NativeImageProcessor`가 `FloatArray` pixel tensor를 만든다. |
| `LiteRTLm` | No | `PromptPart.ImageFile/ImageBytes`를 LiteRT-LM `Content.ImageFile/ImageBytes`로 넘긴다. 실제 image preprocessing은 LiteRT-LM 내부 책임이다. |

`NativeQuickDotAI`의 processor 선택은 다음과 같다.

| Vision model id | Processor | Resize | Normalize |
|---|---|---|---|
| `siglip-lfm2-vision` | `SigLipNaFlexImageProcessor` | `256x256` square | `(x / 255 - 0.5) / 0.5` |
| `jepa-qnn-vision`, `vjepa-qnn` | `JepaImageProcessor` | `256x256` square | ImageNet mean/std |
| 그 외 native multimodal | `LlavaNextImageProcessor` | `512x512` base + selected grid patches | `(x / 255 - 0.5) / 0.5` |

## End-to-End Flow

```text
SampleTestAPP / caller
  -> PromptPart.ImageFile or PromptPart.ImageBytes
  -> NativeQuickDotAI.prepareMultimodalInput()
  -> BitmapFactory.decodeFile() or decodeByteArray()
  -> NativeImageProcessor.preprocessNative(Bitmap)
  -> NativeCausalLm.MultimodalInput(pixelValues, numPatches, original size)
  -> JNI jfloatArray -> float*
  -> quick_dot_ai_api multimodal run function
```

참고 코드:

- `Android/QuickDotAI/src/main/java/com/example/quickdotai/Types.kt`
- `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt`
- `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeCausalLm.kt`
- `Android/QuickDotAI/src/main/cpp/quickai_jni.cpp`

## Input Types

public API의 multimodal image input은 `PromptPart`로 표현된다.

| Type | 기대하는 데이터 | Android 처리 |
|---|---|---|
| `PromptPart.ImageFile` | device에서 읽을 수 있는 absolute file path | `BitmapFactory.decodeFile()`로 decode |
| `PromptPart.ImageBytes` | JPEG/PNG 등 encoded image file bytes | `BitmapFactory.decodeByteArray()`로 decode |
| `PromptPart.PreprocessedPixels` | 이미 전처리된 float pixel tensor | Android image processor를 우회하고 그대로 전달 |

`ImageBytes`는 decoded pixel buffer가 아니다. `Types.kt`의 주석도 raw file
contents, 즉 encoded image bytes를 요구한다.

SampleTestAPP의 picker 경로도 같은 구조다. picker URI에서 `openInputStream()`으로
bytes를 읽어 `selectedImageBytesList`에 저장하고, 실행 시
`PromptPart.ImageBytes`로 붙인다. 이 단계에서는 resize하지 않는다.

OpenAI-style JSON에서 `image_url` / `input_image`는 샘플 앱 내부 marker 역할만
한다. 실제 image bytes는 선택된 이미지 목록을 마지막 user message 앞쪽에
`PromptPart.ImageBytes`로 attach한다.

## Processor Selection

`NativeQuickDotAI.load()`는 vision이 켜진 경우 `processorForVisionModel()`로
processor를 선택한다.

```kotlin
private fun processorForVisionModel(visionModelId: String): NativeImageProcessor =
    when (visionModelId) {
        "siglip-lfm2-vision" -> SigLipNaFlexImageProcessor()
        "jepa-qnn-vision", ModelIds.VJEPA_QNN -> JepaImageProcessor()
        else -> LlavaNextNativeImageProcessor(appContext)
    }
```

`LoadModelRequest.visionModelId`가 있으면 그 값을 쓰고, 없으면 `modelId`를
fallback으로 사용한다. `visionBackend`나 `visionModelId`가 모두 없으면
`imageProcessor`는 null이므로 native multimodal 실행은 unsupported로 처리된다.

## NativeQuickDotAI Decode and Batching

`prepareMultimodalInput()`은 먼저 image part를 모은다.

1. `PreprocessedPixels`가 있으면 즉시 `NativeCausalLm.MultimodalInput`으로 감싸서
   반환한다. 이 경우 decode, resize, normalize를 하지 않는다.
2. image가 1개면 `preprocessSingleImage()`가 file/bytes를 `Bitmap`으로 decode한 뒤
   선택된 processor에 넘긴다.
3. image가 여러 개면 각 image를 개별 decode/preprocess하고, 모든
   `pixelValues`를 하나의 `FloatArray`로 concatenate한다.

multi-image 결과에는 다음 metadata가 같이 들어간다.

| Field | 의미 |
|---|---|
| `numImages` | 전처리에 성공한 image 개수 |
| `numPatches` | 전체 image patch 수 합계 |
| `patchesPerImage` | image별 patch 수 |
| `originalHeights`, `originalWidths` | image별 원본 크기 |

현재 decode 경로에는 `BitmapFactory.Options.inSampleSize` downsampling이나 EXIF
orientation correction이 보이지 않는다. 파일 또는 bytes를 그대로 decode한 뒤
processor resize 단계로 넘어간다.

## SigLipNaFlexImageProcessor

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/SigLipNaFlexImageProcessor.kt`

목적: `siglip-lfm2-vision`용 Android-side preprocessing.

처리 순서:

1. `Bitmap.createScaledBitmap(image, 256, 256, true)`로 정사각형 resize한다.
   원본 aspect ratio를 유지하지 않고 `256x256`에 맞춘다.
2. `256 * 256` pixels를 읽는다.
3. output `FloatArray` 크기는 `256 * 256 * 3`이다.
4. layout은 planar CHW다.
   - R plane: `out[i]`
   - G plane: `out[pixelCount + i]`
   - B plane: `out[2 * pixelCount + i]`
5. normalization은 채널 공통으로 `(x / 255 - 0.5) / 0.5`다.
6. patch size는 `16`, patch 수는 `(256 / 16) * (256 / 16) = 256`이다.

## JepaImageProcessor

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/JepaImageProcessor.kt`

목적: `jepa-qnn-vision` / `vjepa-qnn`용 Android-side preprocessing.

처리 순서:

1. `Bitmap.createScaledBitmap(image, 256, 256, true)`로 정사각형 resize한다.
   원본 aspect ratio를 유지하지 않고 `256x256`에 맞춘다.
2. output `FloatArray` 크기는 `256 * 256 * 3`이다.
3. layout은 planar CHW다.
4. normalization은 ImageNet mean/std를 사용한다.

```text
mean = [0.485, 0.456, 0.406]
std  = [0.229, 0.224, 0.225]
```

5. patch size는 `16`, patch 수는 `256`이다.

## LlavaNextImageProcessor

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/LlavaNextImageProcessor.kt`

목적: legacy/native multimodal fallback preprocessing.

기본 설정:

| 설정 | 값 |
|---|---|
| `cropSize` | `512` |
| `imageMean` | `[0.5, 0.5, 0.5]` |
| `imageStd` | `[0.5, 0.5, 0.5]` |
| `patchMergeType` | `"nopad"` |

처리 순서:

1. base image를 `512x512`로 resize한다.
2. 원본 height/width에 대해 `imageGridPinpoints` 중 best resolution을 선택한다.
3. high-resolution patching용 image를 selected grid resolution으로 resize한다.
4. grid image를 `512x512` patch들로 나눈다.
5. 최종 patch list는 `baseImage + highResPatches`다.
6. 각 patch를 normalize해서 하나의 `FloatArray`에 쓴다.

resize 구현은 Android 기본 scaler가 아니라 `PillowBilinearResizer`를 쓴다.
`PillowBilinearResizer`는 Pillow 스타일 bilinear resize를 맞추기 위해 22-bit
fixed point weight를 사용하며 horizontal pass와 vertical pass를 분리한다.

주의할 점:

- 기본 `patchMergeType`이 `"nopad"`라서 high-res patching image는 selected grid
  size로 직접 resize된다.
- 코드 주석에는 CHW라고 되어 있지만, 현재 `normalize()` 구현은
  `floatValues[offset + i * 3 + channel]`에 기록한다. 즉 LLaVA-NeXT fallback
  경로의 flattened layout은 코드상 RGB interleaved에 가깝다.
- `LlavaNextImageProcessor.loadBitmapFromUri(..., resize = true)`와
  `resizeBitmapIfTooLarge()` helper가 있지만, 현재 `NativeQuickDotAI`의
  multimodal path에서는 사용되지 않는다.

## JNI Boundary

Android processor 결과는 `NativeCausalLm.MultimodalInput`으로 전달된다.

핵심 field:

| Field | 설명 |
|---|---|
| `pixelValues` | 전처리된 float pixel tensor |
| `numPatches` | patch 수 |
| `originalHeight`, `originalWidth` | 원본 image 크기 |
| `numImages`, `patchesPerImage`, `originalHeights`, `originalWidths` | multi-image용 metadata |

JNI에서는 `jfloatArray`를 `GetFloatArrayElements()`로 받아 `float*`로 C API에
전달한다. 이 경계에서 추가 decode, resize, normalize는 하지 않는다.

관련 native 호출:

- single image prompt:
  `runMultimodalHandleStreamingNative()`
- single image OpenAI messages:
  `runMultimodalHandleWithMessagesStreamingNative()`
- multi-image prompt:
  `runMultimodalMultiImageStreamingNative()`
- multi-image OpenAI messages:
  `runMultimodalMultiImageWithMessagesStreamingNative()`

## LiteRT-LM Path

파일:
`Android/QuickDotAI/src/main/java/com/example/quickdotai/LiteRTLm.kt`

`LiteRTLm`은 Android 코드에서 직접 image tensor를 만들지 않는다.

`toLiteRtContents()` / `toLiteRtContentsFromMessages()`는 다음 변환만 수행한다.

| QuickDotAI type | LiteRT-LM type |
|---|---|
| `PromptPart.Text` | `Content.Text` |
| `PromptPart.ImageFile` | `Content.ImageFile` |
| `PromptPart.ImageBytes` | `Content.ImageBytes` |

`PromptPart.PreprocessedPixels`는 LiteRT-LM 경로에서 지원하지 않는다. 코드도
`UnsupportedOperationException`을 던지고, V-JEPA 같은 externally preprocessed
multi-image inference는 `NativeQuickDotAI`를 사용하라고 안내한다.

따라서 LiteRT-LM multimodal resize/normalization 정책은 이 repository의 Android
Kotlin 코드가 아니라 LiteRT-LM engine 내부 구현을 확인해야 한다.

## Quick Checks

이미지 전처리 동작을 바꿀 때 우선 확인할 파일:

| 목적 | 파일 |
|---|---|
| public input type 확인 | `Android/QuickDotAI/src/main/java/com/example/quickdotai/Types.kt` |
| native processor 선택/ batching | `Android/QuickDotAI/src/main/java/com/example/quickdotai/NativeQuickDotAI.kt` |
| SigLIP/LFM2 preprocessing | `Android/QuickDotAI/src/main/java/com/example/quickdotai/SigLipNaFlexImageProcessor.kt` |
| JEPA/QNN preprocessing | `Android/QuickDotAI/src/main/java/com/example/quickdotai/JepaImageProcessor.kt` |
| LLaVA-NeXT fallback preprocessing | `Android/QuickDotAI/src/main/java/com/example/quickdotai/LlavaNextImageProcessor.kt` |
| bilinear resize 구현 | `Android/QuickDotAI/src/main/java/com/example/quickdotai/PilloBilinearResizer.kt` |
| JNI 전달 | `Android/QuickDotAI/src/main/cpp/quickai_jni.cpp` |
| sample picker/image attach | `Android/SampleTestAPP/src/main/java/com/example/sampletestapp/MainActivity.kt` |
