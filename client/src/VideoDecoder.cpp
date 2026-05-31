#include "VideoDecoder.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace camstream {

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder()
{
    if (sws_)   sws_freeContext(sws_);
    if (frame_) av_frame_free(&frame_);
    if (pkt_)   av_packet_free(&pkt_);
    if (ctx_)   avcodec_free_context(&ctx_);
}

bool VideoDecoder::init(int width, int height,
                        const uint8_t* extradata, int extradata_sz)
{
    width_  = width;
    height_ = height;

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "[VideoDecoder] H.264 decoder not found\n";
        return false;
    }

    ctx_ = avcodec_alloc_context3(codec);
    if (!ctx_) return false;

    // Feed the SPS/PPS data so the decoder is ready before the first frame
    if (extradata && extradata_sz > 0) {
        ctx_->extradata = static_cast<uint8_t*>(
            av_malloc(static_cast<size_t>(extradata_sz) + AV_INPUT_BUFFER_PADDING_SIZE));
        std::memcpy(ctx_->extradata, extradata, static_cast<size_t>(extradata_sz));
        ctx_->extradata_size = extradata_sz;
    }

    if (avcodec_open2(ctx_, codec, nullptr) < 0) {
        std::cerr << "[VideoDecoder] Failed to open H.264 decoder\n";
        return false;
    }

    frame_ = av_frame_alloc();
    pkt_   = av_packet_alloc();
    if (!frame_ || !pkt_) return false;

    // YUV420P → BGR24 converter (H.264 outputs YUV, OpenCV/imshow expects BGR)
    sws_ = sws_getContext(width, height, AV_PIX_FMT_YUV420P,
                          width, height, AV_PIX_FMT_BGR24,
                          SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_) return false;

    initialized_ = true;
    return true;
}

cv::Mat VideoDecoder::decode(const uint8_t* data, int size)
{
    if (!initialized_) return {};

    // Wrap the data pointer without copying – valid for the duration of this call
    pkt_->data = const_cast<uint8_t*>(data);
    pkt_->size = size;

    if (avcodec_send_packet(ctx_, pkt_) < 0) return {};
    if (avcodec_receive_frame(ctx_, frame_) < 0) return {};

    // Convert decoded YUV frame to BGR for OpenCV
    cv::Mat out(height_, width_, CV_8UC3);
    uint8_t*     dst[1]    = { out.data };
    const int    stride[1] = { static_cast<int>(out.step) };
    sws_scale(sws_, frame_->data, frame_->linesize, 0, height_, dst, stride);

    return out;
}

} // namespace camstream
