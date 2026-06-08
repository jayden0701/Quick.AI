// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2026 Samsung Electronics Co., Ltd. All Rights Reserved.
 */
package com.example.quickdotai

import android.graphics.Bitmap
import android.graphics.Color

/**
 * SigLIP/LFM2 preprocessing MVP: fixed 256x256 square resize, CHW FP32,
 * normalized with mean/std 0.5.
 */
class SigLipNaFlexImageProcessor : NativeImageProcessor {
    companion object {
        const val IMAGE_SIZE = 256
        const val PATCH_SIZE = 16
        private const val MEAN = 0.5f
        private const val STD = 0.5f
    }

    override fun preprocessNative(image: Bitmap): NativeCausalLm.MultimodalInput {
        val resized = Bitmap.createScaledBitmap(image, IMAGE_SIZE, IMAGE_SIZE, true)
        val pixelCount = IMAGE_SIZE * IMAGE_SIZE
        val pixels = IntArray(pixelCount)
        val out = FloatArray(pixelCount * 3)

        resized.getPixels(pixels, 0, IMAGE_SIZE, 0, 0, IMAGE_SIZE, IMAGE_SIZE)
        for (i in 0 until pixelCount) {
            val pixel = pixels[i]
            out[i] = ((Color.red(pixel) / 255.0f) - MEAN) / STD
            out[pixelCount + i] = ((Color.green(pixel) / 255.0f) - MEAN) / STD
            out[2 * pixelCount + i] = ((Color.blue(pixel) / 255.0f) - MEAN) / STD
        }

        val patchesPerImage = (IMAGE_SIZE / PATCH_SIZE) * (IMAGE_SIZE / PATCH_SIZE)
        return NativeCausalLm.MultimodalInput(
            pixelValues = out,
            numPatches = patchesPerImage,
            originalHeight = image.height,
            originalWidth = image.width
        )
    }
}
