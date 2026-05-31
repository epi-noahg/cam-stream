#pragma once

#include <cstdint>
#include <vector>

#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace camstream {

/// Decodes H.264 NAL data to BGR OpenCV frames using libavcodec.
///
/// Call init() once with the SPS/PPS extradata from the server's Init packet,
/// then call decode() for every Video packet payload.
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    // Non-copyable, non-movable
    VideoDecoder(const VideoDecoder&)            = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    /// Initialise the decoder with codec parameters from the server.
    /// @param width         Frame width in pixels.
    /// @param height        Frame height in pixels.
    /// @param extradata     SPS/PPS bytes (may be null if empty).
    /// @param extradata_sz  Length of extradata in bytes.
    /// @returns true on success.
    bool init(int width, int height,
              const uint8_t* extradata, int extradata_sz);

    /// Decode one encoded packet; returns the BGR frame or an empty Mat
    /// if the codec needs more input or an error occurred.
    cv::Mat decode(const uint8_t* data, int size);

    bool isInitialized() const noexcept { return initialized_; }
    int  width()         const noexcept { return width_;  }
    int  height()        const noexcept { return height_; }

private:
    bool            initialized_{false};
    int             width_{0}, height_{0};

    AVCodecContext* ctx_   {nullptr};
    AVFrame*        frame_ {nullptr};
    AVPacket*       pkt_   {nullptr};
    SwsContext*     sws_   {nullptr};
};

} // namespace camstream
