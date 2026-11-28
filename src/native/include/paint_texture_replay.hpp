#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace paint_texture_replay
{
    // The bridge validates both square RGBA buffers before making this view.
    struct Texture
    {
        int size;
        const std::vector<std::uint8_t>& albedo;
        const std::vector<std::uint8_t>& pbr;

        std::size_t offset(int x, int y) const
        {
            return (static_cast<std::size_t>(y) * size + x) * 4;
        }

        std::array<double, 6> sample(double u, double v) const
        {
            const double x = std::clamp(u, 0.0, 1.0) * (size - 1);
            const double y = std::clamp(v, 0.0, 1.0) * (size - 1);
            const int x0 = static_cast<int>(x);
            const int y0 = static_cast<int>(y);
            const int x1 = std::min(x0 + 1, size - 1);
            const int y1 = std::min(y0 + 1, size - 1);
            const auto top_left = offset(x0, y0);
            const auto top_right = offset(x1, y0);
            const auto bottom_left = offset(x0, y1);
            const auto bottom_right = offset(x1, y1);
            const double tx = x - x0;
            const double ty = y - y0;
            const auto interpolate = [&](const auto& bytes, std::size_t channel) {
                const double top = bytes[top_left + channel] * (1.0 - tx) +
                                   bytes[top_right + channel] * tx;
                const double bottom = bytes[bottom_left + channel] * (1.0 - tx) +
                                      bytes[bottom_right + channel] * tx;
                return (top * (1.0 - ty) + bottom * ty) / 255.0;
            };
            return {interpolate(albedo, 0), interpolate(albedo, 1), interpolate(albedo, 2),
                    interpolate(pbr, 0), interpolate(pbr, 1), interpolate(pbr, 2)};
        }

        bool is_uniform(double u, double v, double reach) const
        {
            if (u - reach < 0.0 || v - reach < 0.0 ||
                u + reach > 1.0 || v + reach > 1.0)
            {
                return false;
            }
            const double max_coord = size - 1;
            const int left = static_cast<int>(std::floor((u - reach) * max_coord));
            const int top = static_cast<int>(std::floor((v - reach) * max_coord));
            const int right = static_cast<int>(std::ceil((u + reach) * max_coord));
            const int bottom = static_cast<int>(std::ceil((v + reach) * max_coord));
            const auto first = offset(left, top);
            for (int y = top; y <= bottom; ++y)
            {
                for (int x = left; x <= right; ++x)
                {
                    const auto pixel = offset(x, y);
                    if (albedo[pixel] != albedo[first] ||
                        albedo[pixel + 1] != albedo[first + 1] ||
                        albedo[pixel + 2] != albedo[first + 2] ||
                        pbr[pixel] != pbr[first] ||
                        pbr[pixel + 1] != pbr[first + 1] ||
                        pbr[pixel + 2] != pbr[first + 2])
                    {
                        return false;
                    }
                }
            }
            return true;
        }
    };

    // Try wide tiles first, then subdivide the samples that could not be
    // merged. A merged stroke stays centered on a live mesh sample and its
    // disk contains the fine strokes it replaces.
    template <typename Sample>
    std::vector<float> plan_brush_radii(const Texture& texture,
                                       const std::vector<Sample>& samples,
                                       float detail_radius,
                                       double max_radius)
    {
        struct Tile
        {
            std::size_t representative;
            double center_distance_squared;
            double min_u;
            double max_u;
            double min_v;
            double max_v;
            std::size_t count{1};
            float merged_radius{0.0f};
        };

        const auto valid_uv = [](const Sample& sample) {
            return sample.u >= 0.0 && sample.u <= 1.0 &&
                   sample.v >= 0.0 && sample.v <= 1.0;
        };
        const double raster_margin = 0.5 / texture.size;
        std::vector<float> radii(samples.size(), detail_radius);
        for (double tile_width = max_radius;
             tile_width >= 2.0 * detail_radius;
             tile_width *= 0.5)
        {
            const auto tile_key = [tile_width](const Sample& sample) {
                const auto x = static_cast<std::uint64_t>(sample.u / tile_width);
                const auto y = static_cast<std::uint64_t>(sample.v / tile_width);
                return (y << 32) | x;
            };
            std::unordered_map<std::uint64_t, Tile> tiles;
            for (std::size_t i = 0; i < samples.size(); ++i)
            {
                const auto& sample = samples[i];
                if (radii[i] != detail_radius || !valid_uv(sample))
                {
                    continue;
                }
                const auto key = tile_key(sample);
                const double center_u = (static_cast<std::uint32_t>(key) + 0.5) * tile_width;
                const double center_v = ((key >> 32) + 0.5) * tile_width;
                const double du = sample.u - center_u;
                const double dv = sample.v - center_v;
                const double distance_squared = du * du + dv * dv;
                auto [it, inserted] = tiles.try_emplace(key, Tile{
                    i, distance_squared, sample.u, sample.u, sample.v, sample.v});
                if (!inserted)
                {
                    auto& tile = it->second;
                    ++tile.count;
                    tile.min_u = std::min(tile.min_u, sample.u);
                    tile.max_u = std::max(tile.max_u, sample.u);
                    tile.min_v = std::min(tile.min_v, sample.v);
                    tile.max_v = std::max(tile.max_v, sample.v);
                    if (distance_squared < tile.center_distance_squared)
                    {
                        tile.representative = i;
                        tile.center_distance_squared = distance_squared;
                    }
                }
            }

            for (auto& entry : tiles)
            {
                auto& tile = entry.second;
                if (tile.count < 2)
                {
                    continue;
                }
                const auto& center = samples[tile.representative];
                const double du = std::max(center.u - tile.min_u, tile.max_u - center.u);
                const double dv = std::max(center.v - tile.min_v, tile.max_v - center.v);
                const auto radius = static_cast<float>(
                    detail_radius + std::hypot(du, dv) + raster_margin);
                // Include the reach of fine strokes around the enlarged brush.
                if (radius <= max_radius &&
                    texture.is_uniform(center.u, center.v, radius + detail_radius))
                {
                    tile.merged_radius = radius;
                }
            }

            for (std::size_t i = 0; i < samples.size(); ++i)
            {
                if (radii[i] != detail_radius || !valid_uv(samples[i]))
                {
                    continue;
                }
                const auto& tile = tiles.find(tile_key(samples[i]))->second;
                if (tile.merged_radius > 0.0f)
                {
                    radii[i] = i == tile.representative ? tile.merged_radius : 0.0f;
                }
            }
        }
        return radii;
    }
}
