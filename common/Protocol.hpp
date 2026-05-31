#pragma once

#include <cstdint>

/// camstream – shared wire protocol between server and client.
///
/// Every message on the TCP stream is a PacketHeader followed by
/// exactly data_size bytes of payload.  All multi-byte integers are
/// in network byte order (big-endian).
namespace camstream {

// ── Constants ────────────────────────────────────────────────────────────────

constexpr uint32_t PACKET_MAGIC = 0xCAFEBABE;
constexpr int      MAX_CAMERAS  = 3;
constexpr uint16_t DEFAULT_PORT = 8554;

// ── Packet types ─────────────────────────────────────────────────────────────

enum class PacketType : uint8_t {
    /// Sent once per camera right after a client connects.
    /// Payload layout: [ InitPayloadHeader ] [ raw H.264 extradata (SPS/PPS) ]
    Init  = 0x01,

    /// One encoded H.264 video frame.
    /// Payload: raw NAL data (AVCC format, extradata prepended by encoder).
    Video = 0x02,
};

// ── Wire structures ───────────────────────────────────────────────────────────

#pragma pack(push, 1)

/// Fixed-size header that precedes every packet on the wire.
struct PacketHeader {
    uint32_t magic;      ///< Must equal PACKET_MAGIC (network byte order)
    uint8_t  type;       ///< PacketType
    uint8_t  cam_id;     ///< Camera index in [0, MAX_CAMERAS)
    uint8_t  flags;      ///< Bit 0: key-frame (Video packets only)
    uint8_t  reserved;   ///< Padding – always 0
    uint32_t pts_ms;     ///< Presentation timestamp in ms (Video packets only, network byte order)
    uint32_t data_size;  ///< Payload size in bytes (network byte order)
};

/// Leading fields of an Init packet's payload (followed by raw extradata).
struct InitPayloadHeader {
    uint32_t width;   ///< Frame width  in pixels (network byte order)
    uint32_t height;  ///< Frame height in pixels (network byte order)
    uint32_t fps;     ///< Capture frame-rate     (network byte order)
};

#pragma pack(pop)

} // namespace camstream
