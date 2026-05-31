#pragma once

#include <cstdint>
#include <vector>

#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace camstream {

/// One encoded output packet produced by VideoEncoder.
struct EncodedPacket {
    int                  cam_id;
    std::vector<uint8_t> data;
    int64_t              pts_ms;
    bool                 is_keyframe;
};

/// Encodes raw BGR frames to H.264 using libx264 through libavcodec.
///
/// Encoding runs synchronously – call encode() once per captured frame and
/// collect the returned packets (the codec may buffer a few frames before
/// producing output).  Call flush() once when stopping to drain the buffer.
class VideoEncoder {
public:
    /// @param cam_id       Forwarded into every EncodedPacket.
    /// @param width        Frame width in pixels.
    /// @param height       Frame height in pixels.
    /// @param fps          Frame rate; drives time-base and GOP size.
    /// @param bitrate_kbps Target average bitrate in kbit/s.
    VideoEncoder(int cam_id, int width, int height, int fps,
                 int bitrate_kbps = 500);
    ~VideoEncoder();

    // Non-copyable, non-movable
    VideoEncoder(const VideoEncoder&)            = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;

    /// Encode one BGR frame; returns zero or more output packets.
    std::vector<EncodedPacket> encode(const cv::Mat& bgr, int64_t pts_ms);

    /// Flush any frames buffered inside the encoder; call once when stopping.
    std::vector<EncodedPacket> flush();

    /// H.264 global header (SPS/PPS); valid after construction.
    const std::vector<uint8_t>& extradata() const noexcept { return extradata_; }

    int width()  const noexcept { return width_;  }
    int height() const noexcept { return height_; }
    int fps()    const noexcept { return fps_;    }

private:
    /// Drain all packets currently available from the codec.
    std::vector<EncodedPacket> drain();

    int cam_id_, width_, height_, fps_;

    AVCodecContext* ctx_   {nullptr};
    AVFrame*        frame_ {nullptr};
    AVPacket*       pkt_   {nullptr};
    SwsContext*     sws_   {nullptr};

    int64_t frame_pts_{0};

    std::vector<uint8_t> extradata_;
};

} // namespace camstream
