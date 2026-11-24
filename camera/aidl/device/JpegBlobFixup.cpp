/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "CamDevSession-impl"

#include "JpegBlobFixup.h"

#include <aidl/android/hardware/camera/device/CameraBlob.h>
#include <aidl/android/hardware/camera/device/CameraBlobId.h>
#include <hardware/gralloc.h>
#include <inttypes.h>
#include <log/log.h>
#include <string.h>
#include <ui/GraphicBufferMapper.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using ::aidl::android::hardware::camera::device::CameraBlob;
using ::aidl::android::hardware::camera::device::CameraBlobId;

/*
 * Legacy HALs end JPEG buffers with a camera3_jpeg_blob (16-bit id, padding, size), while
 * AIDL clients expect a CameraBlob (32-bit id, size). Translate it, like cameraserver does
 * for HIDL providers, so readers can find the JPEG size instead of using the whole buffer.
 */
void fixUpLegacyJpegBlobHeader(const camera3_stream_buffer_t& buf) {
    const camera3_stream_t* stream = buf.stream;
    if (stream == nullptr || buf.buffer == nullptr || buf.status != CAMERA3_BUFFER_STATUS_OK ||
        stream->format != HAL_PIXEL_FORMAT_BLOB ||
        (stream->data_space != HAL_DATASPACE_V0_JFIF &&
         stream->data_space != HAL_DATASPACE_JFIF)) {
        return;
    }

    if (buf.release_fence != -1) {
        ALOGW("%s: JPEG buffer still in use by the HAL, skipping", __FUNCTION__);
        return;
    }

    buffer_handle_t handle = *buf.buffer;
    uint64_t width = 0;
    auto& mapper = GraphicBufferMapper::get();
    if (mapper.getWidth(handle, &width) != OK || width > INT32_MAX ||
        width < sizeof(camera3_jpeg_blob_t) || width < sizeof(CameraBlob)) {
        ALOGE("%s: invalid JPEG buffer width %" PRIu64, __FUNCTION__, width);
        return;
    }

    void* mapped = nullptr;
    if (mapper.lock(handle, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_RARELY,
                    Rect(static_cast<int32_t>(width), 1), &mapped) != OK ||
        mapped == nullptr) {
        ALOGE("%s: failed to lock JPEG buffer", __FUNCTION__);
        return;
    }

    uint8_t* base = static_cast<uint8_t*>(mapped);
    camera3_jpeg_blob_t legacy;
    memcpy(&legacy, base + width - sizeof(legacy), sizeof(legacy));
    if (legacy.jpeg_blob_id == CAMERA3_JPEG_BLOB_ID) {
        if (legacy.jpeg_size <= width - sizeof(CameraBlob)) {
            CameraBlob blob = {.blobId = CameraBlobId::JPEG,
                               .blobSizeBytes = static_cast<int32_t>(legacy.jpeg_size)};
            memcpy(base + width - sizeof(blob), &blob, sizeof(blob));
        } else {
            ALOGE("%s: JPEG size %u overlaps the blob header (buffer %" PRIu64 ")",
                  __FUNCTION__, legacy.jpeg_size, width);
        }
    }

    mapper.unlock(handle);
}

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
