#pragma once

#include "sdk.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Layout of RuntimePaintableComponent::MulticastPackedPaintBatchToOthers.
//
// The game does not expose a reflected struct for this RPC: the UFunction takes
// TArray<uint8> PackedData plus int32 StrokeCount, and the byte array is built
// by native code.  The header below is the part that can be validated without
// re-implementing the game's delta/grouped stroke payload:
//
//   offset  size  field
//   0x00    1     mode (1..3 = delta stroke stream, 4 = grouped stroke stream)
//   0x01    16    FGuid ReplicationSourceId shared by the batch
//   0x11    4     uint32 declared stroke count (must equal the RPC StrokeCount)
//   0x15    2     uint16 group count (mode 4 only)
//   0x17    ...   payload / group stream (mode 4)
//   0x15    ...   payload / stroke stream (modes 1..3)
//
// Each decoded element is a CompactPaintStroke (0x40 bytes).  The game's own
// decoder (RuntimePaintableComponent native code) expands it through the
// CompactPaintStroke -> PaintStroke converter, which reconstructs the UV from
// the skeletal triangle anchor and keeps the per-stroke Radius at offset 0x0C.
// The bridge captures those converted FPaintStrokes; it never re-quantises the
// packed bytes itself.
namespace packed_paint_batch
{
    constexpr std::size_t HeaderSize = 0x15;
    constexpr std::size_t GroupedHeaderSize = 0x17;
    constexpr std::uint8_t ModeDelta = 1;
    constexpr std::uint8_t ModeDelta2 = 2;
    constexpr std::uint8_t ModeDelta3 = 3;
    constexpr std::uint8_t ModeGrouped = 4;

    struct Header
    {
        bool ok{false};
        std::string failure{};
        std::uint8_t mode{0};
        sdk::FGuid replication_source_id{};
        std::uint32_t declared_count{0};
        std::uint32_t rpc_stroke_count{0};
        bool count_matches_rpc{false};
        std::uint16_t group_count{0};
        std::size_t payload_offset{0};
        std::size_t byte_count{0};
    };

    inline auto read_u16(const std::uint8_t* data) -> std::uint16_t
    {
        return static_cast<std::uint16_t>(data[0]) |
               static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
    }

    inline auto read_u32(const std::uint8_t* data) -> std::uint32_t
    {
        return static_cast<std::uint32_t>(data[0]) |
               (static_cast<std::uint32_t>(data[1]) << 8) |
               (static_cast<std::uint32_t>(data[2]) << 16) |
               (static_cast<std::uint32_t>(data[3]) << 24);
    }

    inline auto parse_header(const std::uint8_t* data,
                             std::size_t size,
                             std::uint32_t rpc_stroke_count) -> Header
    {
        Header header{};
        header.byte_count = size;
        header.rpc_stroke_count = rpc_stroke_count;
        if (!data || size < HeaderSize)
        {
            header.failure = "packed_header_too_short";
            return header;
        }
        header.mode = data[0];
        if (header.mode < ModeDelta || header.mode > ModeGrouped)
        {
            header.failure = "packed_mode_unsupported";
            return header;
        }
        header.replication_source_id.A = read_u32(data + 0x01);
        header.replication_source_id.B = read_u32(data + 0x05);
        header.replication_source_id.C = read_u32(data + 0x09);
        header.replication_source_id.D = read_u32(data + 0x0d);
        header.declared_count = read_u32(data + 0x11);
        header.count_matches_rpc =
            rpc_stroke_count == 0 || header.declared_count == rpc_stroke_count;
        header.payload_offset = HeaderSize;
        if (header.mode == ModeGrouped)
        {
            if (size < GroupedHeaderSize)
            {
                header.failure = "packed_grouped_header_too_short";
                return header;
            }
            header.group_count = read_u16(data + 0x15);
            header.payload_offset = GroupedHeaderSize;
        }
        header.ok = true;
        return header;
    }
}
