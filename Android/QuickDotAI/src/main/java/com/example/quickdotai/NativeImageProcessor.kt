// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2026 Samsung Electronics Co., Ltd. All Rights Reserved.
 */
package com.example.quickdotai

import android.graphics.Bitmap

/**
 * Converts a decoded Bitmap into the native multimodal pixel tensor shape.
 */
interface NativeImageProcessor {
    fun preprocessNative(image: Bitmap): NativeCausalLm.MultimodalInput
}
