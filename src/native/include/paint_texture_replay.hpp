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

    // Keep the first live mesh sample as the center of a merged stroke. Its
    // disk contains the original disks, including those on thin UV islands.
    template <typename Sample>
    std::vector<float> plan_brush_radii(const Texture& texture,
                                        const std::vector<Sample>& samples,
                                        float base_radius)
    {
        struct Tile
        {
            std::size_t first;
            double farthest_squared{0.0};
            std::size_t count{1};
            float merged_radius{0.0f};
        };

        const double tile_width = 2.0 * base_radius;
        const auto tile_key = [tile_width](const Sample& sample) {
            const auto x = static_cast<std::uint64_t>(sample.u / tile_width);
            const auto y = static_cast<std::uint64_t>(sample.v / tile_width);
            return (y << 32) | x;
        };
        const auto valid_uv = [](const Sample& sample) {
            return sample.u >= 0.0 && sample.u <= 1.0 &&
                   sample.v >= 0.0 && sample.v <= 1.0;
        };

        std::unordered_map<std::uint64_t, Tile> tiles;
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const auto& sample = samples[i];
            if (!valid_uv(sample))
            {
                continue;
            }
            auto [it, inserted] = tiles.try_emplace(tile_key(sample), Tile{i});
            if (!inserted)
            {
                auto& tile = it->second;
                const auto& first = samples[tile.first];
                const double du = sample.u - first.u;
                const double dv = sample.v - first.v;
                tile.farthest_squared = std::max(tile.farthest_squared, du * du + dv * dv);
                ++tile.count;
            }
        }

        const double raster_margin = 0.5 / texture.size;
        for (auto& entry : tiles)
        {
            auto& tile = entry.second;
            if (tile.count < 2)
            {
                continue;
            }
            const auto& first = samples[tile.first];
            const auto radius = static_cast<float>(
                base_radius + std::sqrt(tile.farthest_squared) + raster_margin);
            // Check both the enlarged footprint and the reach of neighboring
            // original strokes so the merge cannot cross a color/material edge.
            if (texture.is_uniform(first.u, first.v, radius + base_radius))
            {
                tile.merged_radius = radius;
            }
        }

        std::vector<float> radii(samples.size(), base_radius);
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            if (!valid_uv(samples[i]))
            {
                continue;
            }
            const auto& tile = tiles.find(tile_key(samples[i]))->second;
            if (tile.merged_radius > 0.0f)
            {
                radii[i] = i == tile.first ? tile.merged_radius : 0.0f;
            }
        }
        return radii;
    }
}
