#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace orbiter { namespace dds {
enum class Error { none, truncated, invalid_header, unsupported_format, dimension_limit, allocation_failed };
struct Image {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
struct Result {
    Error error = Error::none;
    Image image;
    explicit operator bool() const noexcept { return error == Error::none; }
};
// Legacy 2D DXT1/3/5 only; validates the complete declared mip chain,
// decodes level 0 to tightly packed row-major RGBA8 (top-left origin).
// RGB565 expands by bit replication, interpolation uses integer floor.
// Bytes retain source color encoding; no gamma conversion, premultiplication,
// orientation change, or GPU-dependent filtering is performed.
// Limits: each dimension <=16384, width*height <=16777216 (64 MiB output).
// Rejects DX10, DXT2/4, non-FourCC, cube/volume DDS; ignores pitch hints and
// permits trailing bytes. Count 0 means one level only when mip flag is absent.
// Errors always return an empty image. Input is borrowed only for this call.
// No DDS structs, unaligned loads, external dependencies, engine or GPU state.
Result decode(const std::uint8_t* data, std::size_t size);
} }
