#include "VideoEncoder.hpp"

#include <cstring>
#include <stdexcept>

namespace camstream {

VideoEncoder::VideoEncoder(int cam_id, int width, int height, int fps,
                           int bitrate_kbps)
    : cam_id_(cam_id), width_(width), height_(height), fps_(fps)
{
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) throw std::runtime_error("libx264 encoder not found – install libx264");

    ctx_ = avcodec_alloc_context3(codec);
    if (!ctx_) throw std::runtime_error("Failed to allocate AVCodecContext");

    ctx_->bit_rate     = bitrate_kbps * 1000;
    ctx_->width        = width;
    ctx_->height       = height;
    ctx_->time_base    = { 1, fps };
    ctx_->framerate    = { fps, 1 };
    ctx_->gop_size     = fps * 2;   // key-frame every ~2 seconds
    ctx_->max_b_frames = 0;         // no B-frames → lower latency
    ctx_->pix_fmt      = AV_PIX_FMT_YUV420P;
    // Store SPS/PPS in extradata instead of inline in every keyframe
    ctx_->flags       |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // libx264 options for real-time streaming
    av_opt_set(ctx_->priv_data, "preset", "ultrafast",    0);
    av_opt_set(ctx_->priv_data, "tune",   "zerolatency",  0);

    if (avcodec_open2(ctx_, codec, nullptr) < 0)
        throw std::runtime_error("Failed to open AVCodecContext");

    // Cache the extradata (SPS/PPS) so the server can send it to clients
    if (ctx_->extradata && ctx_->extradata_size > 0) {
        extradata_.assign(ctx_->extradata,
                          ctx_->extradata + ctx_->extradata_size);
    }

    // Allocate reusable frame and packet
    frame_ = av_frame_alloc();
    if (!frame_) throw std::runtime_error("Failed to alloc AVFrame");
    frame_->format = AV_PIX_FMT_YUV420P;
    frame_->width  = width;
    frame_->height = height;
    if (av_frame_get_buffer(frame_, 0) < 0)
        throw std::runtime_error("Failed to alloc frame buffer");

    pkt_ = av_packet_alloc();
    if (!pkt_) throw std::runtime_error("Failed to alloc AVPacket");

    // BGR24 → YUV420P colour-space converter
    sws_ = sws_getContext(width, height, AV_PIX_FMT_BGR24,
                          width, height, AV_PIX_FMT_YUV420P,
                          SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_) throw std::runtime_error("Failed to create SwsContext");
}

VideoEncoder::~VideoEncoder()
{
    if (sws_)   sws_freeContext(sws_);
    if (frame_) av_frame_free(&frame_);
    if (pkt_)   av_packet_free(&pkt_);
    if (ctx_)   avcodec_free_context(&ctx_);
}

std::vector<EncodedPacket> VideoEncoder::encode(const cv::Mat& bgr, int64_t pts_ms)
{
    // Convert BGR (OpenCV native) to YUV420P (H.264 native)
    const uint8_t* src_data[1]    = { bgr.data };
    const int      src_stride[1]  = { static_cast<int>(bgr.step) };
    sws_scale(sws_, src_data, src_stride, 0, height_,
              frame_->data, frame_->linesize);

    frame_->pts = frame_pts_++;

    if (avcodec_send_frame(ctx_, frame_) < 0) return {};
    return drain();
}

std::vector<EncodedPacket> VideoEncoder::flush()
{
    avcodec_send_frame(ctx_, nullptr);  // nullptr signals EOF
    return drain();
}

std::vector<EncodedPacket> VideoEncoder::drain()
{
    std::vector<EncodedPacket> out;
    while (true) {
        const int ret = avcodec_receive_packet(ctx_, pkt_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) break;

        const int64_t pts_ms  = pkt_->pts * 1000 / fps_;
        const bool    is_key  = (pkt_->flags & AV_PKT_FLAG_KEY) != 0;

        out.push_back({
            cam_id_,
            std::vector<uint8_t>(pkt_->data, pkt_->data + pkt_->size),
            pts_ms,
            is_key
        });
        av_packet_unref(pkt_);
    }
    return out;
}

} // namespace camstream
