/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <hardware/camera3.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

// Translates a legacy camera3_jpeg_blob trailer into an AIDL CameraBlob.
void fixUpLegacyJpegBlobHeader(const camera3_stream_buffer_t& buf);

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
