#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace paint_texture_replay
{
    enum class Strategy { Adaptive, Contour, Dominant };

    constexpr Strategy strategy_from_name(std::string_view name)
    {
        if (name == "contour") return Strategy::Contour;
        if (name == "dominant") return Strategy::Dominant;
        return Strategy::Adaptive;
    }

    constexpr const char* strategy_name(Strategy strategy)
    {
        switch (strategy)
        {
            case Strategy::Contour: return "contour";
            case Strategy::Dominant: return "dominant";
            default: return "adaptive";
        }
    }

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

        bool same_material(std::size_t first, std::size_t second) const
        {
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                if (albedo[first + channel] != albedo[second + channel] ||
                    pbr[first + channel] != pbr[second + channel])
                {
                    return false;
                }
            }
            return true;
        }

        std::size_t nearest_texel(double u, double v) const
        {
            return offset(static_cast<int>(std::lround(u * (size - 1))),
                          static_cast<int>(std::lround(v * (size - 1)))) / 4;
        }

        std::uint64_t material_key(std::size_t pixel) const
        {
            const auto first = pixel * 4;
            std::uint64_t key = 0;
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                key = (key << 8) | albedo[first + channel];
            }
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                key = (key << 8) | pbr[first + channel];
            }
            return key;
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
                    if (!same_material(pixel, first))
                    {
                        return false;
                    }
                }
            }
            return true;
        }
    };

    inline std::array<double, 6> material_channels(std::uint64_t key)
    {
        std::array<double, 6> channels{};
        for (auto channel = channels.rbegin(); channel != channels.rend(); ++channel)
        {
            *channel = static_cast<double>(key & 0xff) / 255.0;
            key >>= 8;
        }
        return channels;
    }

    template <typename Sample>
    bool valid_uv(const Sample& sample)
    {
        return sample.u >= 0.0 && sample.u <= 1.0 &&
               sample.v >= 0.0 && sample.v <= 1.0;
    }

    // Try wide tiles first, then subdivide the samples that could not be
    // merged. A merged stroke stays centered on a live mesh sample and its
    // disk contains the fine strokes it replaces.
    template <typename Sample>
    std::vector<float> plan_brush_radii(const Texture& texture,
                                       const std::vector<Sample>& samples,
                                       float detail_radius,
                                       double max_radius,
                                       const std::vector<bool>& covered = {})
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

        const double raster_margin = 0.5 / texture.size;
        std::vector<float> radii(samples.size(), detail_radius);
        for (std::size_t i = 0; i < covered.size(); ++i)
        {
            if (covered[i]) radii[i] = 0.0f;
        }
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

    // Conservative distance from a texel to any color, material, or UV edge.
    // The two step costs underestimate Euclidean distance in every direction.
    inline std::vector<std::uint16_t> texture_edge_distances(const Texture& texture)
    {
        const int size = texture.size;
        std::vector<std::uint16_t> distances(static_cast<std::size_t>(size) * size,
                                             std::numeric_limits<std::uint16_t>::max());
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const auto pixel = texture.offset(x, y) / 4;
                if (x == 0 || y == 0 || x == size - 1 || y == size - 1)
                {
                    distances[pixel] = 0;
                }
                if (x > 0 && !texture.same_material(pixel * 4, (pixel - 1) * 4))
                {
                    distances[pixel] = distances[pixel - 1] = 0;
                }
                if (y > 0 && !texture.same_material(pixel * 4, (pixel - size) * 4))
                {
                    distances[pixel] = distances[pixel - size] = 0;
                }
            }
        }
        if (size < 3) return distances;

        for (const int direction : {1, -1})
        {
            const int start = direction == 1 ? 1 : size - 2;
            const int end = direction == 1 ? size - 1 : 0;
            for (int y = start; y != end; y += direction)
            {
                const auto row = static_cast<std::size_t>(y) * size;
                const auto previous_row = static_cast<std::size_t>(y - direction) * size;
                for (int x = start; x != end; x += direction)
                {
                    auto& distance = distances[row + x];
                    if (distance == 0) continue;
                    distance = static_cast<std::uint16_t>(std::min({
                        int(distance), distances[row + (x - direction)] + 92,
                        distances[previous_row + x] + 92,
                        distances[previous_row + (x - 1)] + 130,
                        distances[previous_row + (x + 1)] + 130}));
                }
            }
        }
        return distances;
    }

    inline double edge_clearance_uv(std::uint16_t distance, int texture_size)
    {
        // Allow for the center's subtexel offset and bilinear sampling.
        return std::max(0.0, distance / 100.0 - 2.0) / std::max(1, texture_size - 1);
    }

    inline std::uint64_t cell_key(int x, int y)
    {
        return (static_cast<std::uint64_t>(y) << 32) | static_cast<std::uint32_t>(x);
    }

    template <typename Sample>
    auto bucket_samples(const std::vector<Sample>& samples, double cell_width)
        -> std::unordered_map<std::uint64_t, std::vector<std::size_t>>
    {
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> buckets;
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const auto& sample = samples[i];
            if (!valid_uv(sample)) continue;
            buckets[cell_key(static_cast<int>(sample.u / cell_width),
                             static_cast<int>(sample.v / cell_width))].push_back(i);
        }
        return buckets;
    }

    template <typename Sample>
    std::vector<float> plan_contour_radii(const Texture& texture,
                                          const std::vector<Sample>& samples,
                                          float detail_radius,
                                          double max_radius,
                                          const std::vector<std::uint16_t>& distances)
    {
        struct Candidate { std::size_t index; float radius; };
        std::vector<Candidate> candidates;
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const auto& sample = samples[i];
            if (!valid_uv(sample)) continue;
            const auto pixel = texture.nearest_texel(sample.u, sample.v);
            const auto radius = static_cast<float>(std::min(max_radius,
                edge_clearance_uv(distances[pixel], texture.size) - detail_radius));
            if (radius >= 1.5 * detail_radius) candidates.push_back({i, radius});
        }
        std::vector<float> radii(samples.size(), detail_radius);
        if (candidates.empty()) return radii;

        std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
            return left.radius == right.radius ? left.index < right.index : left.radius > right.radius;
        });
        const double cell_width = 2.0 * detail_radius;
        const auto buckets = bucket_samples(samples, cell_width);
        const double raster_margin = 0.5 / texture.size;
        std::vector<std::size_t> covered;
        for (const auto& candidate : candidates)
        {
            if (radii[candidate.index] != detail_radius) continue;
            const auto& center = samples[candidate.index];
            const double reach = candidate.radius - detail_radius - raster_margin;
            if (reach <= 0.0) continue;
            const int left = static_cast<int>((center.u - reach) / cell_width);
            const int right = static_cast<int>((center.u + reach) / cell_width);
            const int top = static_cast<int>((center.v - reach) / cell_width);
            const int bottom = static_cast<int>((center.v + reach) / cell_width);
            covered.clear();
            for (int y = top; y <= bottom; ++y)
            {
                for (int x = left; x <= right; ++x)
                {
                    const auto bucket = buckets.find(cell_key(x, y));
                    if (bucket == buckets.end()) continue;
                    for (const auto index : bucket->second)
                    {
                        if (radii[index] != detail_radius) continue;
                        const double du = samples[index].u - center.u;
                        const double dv = samples[index].v - center.v;
                        if (du * du + dv * dv <= reach * reach) covered.push_back(index);
                    }
                }
            }
            if (covered.size() < 2) continue;
            for (const auto index : covered) radii[index] = 0.0f;
            radii[candidate.index] = candidate.radius;
        }
        return radii;
    }

    struct BaseStroke
    {
        std::size_t sample_index;
        float radius;
        std::uint64_t material;
    };

    struct ReplayPlan
    {
        Strategy strategy;
        std::vector<float> radii;
        std::vector<BaseStroke> base_strokes{};

        std::size_t stroke_count() const
        {
            return base_strokes.size() + static_cast<std::size_t>(
                std::count_if(radii.begin(), radii.end(), [](float radius) { return radius > 0.0f; }));
        }
    };

    template <typename Sample>
    ReplayPlan plan_dominant_replay(const Texture& texture,
                                    const std::vector<Sample>& samples,
                                    float detail_radius,
                                    double max_radius,
                                    const std::vector<std::uint16_t>& distances)
    {
        ReplayPlan plan{Strategy::Dominant, {}};
        const double cell_width = 2.0 * max_radius;
        const auto buckets = bucket_samples(samples, cell_width);
        const double raster_margin = 0.5 / texture.size;
        std::vector<bool> covered(samples.size(), false);
        for (const auto& [key, members] : buckets)
        {
            if (members.size() < 3) continue;
            const double left = static_cast<std::uint32_t>(key) * cell_width;
            const double top = (key >> 32) * cell_width;
            const double right = std::min(1.0, left + cell_width);
            const double bottom = std::min(1.0, top + cell_width);
            const auto distance_to_edge = [&](const auto& sample) {
                return std::min({sample.u - left, right - sample.u,
                                 sample.v - top, bottom - sample.v});
            };
            const auto anchor = *std::max_element(members.begin(), members.end(),
                [&](auto a, auto b) { return distance_to_edge(samples[a]) < distance_to_edge(samples[b]); });
            const auto& center = samples[anchor];
            const auto radius = static_cast<float>(std::min(max_radius, distance_to_edge(center) - raster_margin));
            if (radius < 3.0 * detail_radius) continue;

            std::unordered_map<std::uint64_t, std::size_t> histogram;
            std::vector<std::pair<std::size_t, std::size_t>> affected;
            bool crosses_islands = false;
            for (const auto index : members)
            {
                const auto& sample = samples[index];
                const double du = sample.u - center.u;
                const double dv = sample.v - center.v;
                const double outer = radius + detail_radius;
                if (du * du + dv * dv > outer * outer) continue;
                if (sample.uv_island != center.uv_island)
                {
                    crosses_islands = true;
                    break;
                }
                if (du * du + dv * dv > radius * radius) continue;
                const auto pixel = texture.nearest_texel(sample.u, sample.v);
                ++histogram[texture.material_key(pixel)];
                affected.emplace_back(index, pixel);
            }
            if (crosses_islands || affected.size() < 3) continue;

            const auto dominant = std::max_element(histogram.begin(), histogram.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            if (dominant->second * 4 < affected.size() * 3) continue;

            std::vector<std::size_t> resolved;
            const double inner = radius - detail_radius - raster_margin;
            for (const auto& [index, pixel] : affected)
            {
                const double du = samples[index].u - center.u;
                const double dv = samples[index].v - center.v;
                if (du * du + dv * dv <= inner * inner &&
                    edge_clearance_uv(distances[pixel], texture.size) >= 2.0 * detail_radius &&
                    texture.material_key(pixel) == dominant->first)
                {
                    resolved.push_back(index);
                }
            }
            if (resolved.size() < 2) continue;
            for (const auto index : resolved) covered[index] = true;
            plan.base_strokes.push_back({anchor, radius, dominant->first});
        }
        std::sort(plan.base_strokes.begin(), plan.base_strokes.end(),
            [](const auto& a, const auto& b) { return a.sample_index < b.sample_index; });
        plan.radii = plan_brush_radii(texture, samples, detail_radius, max_radius, covered);
        return plan;
    }

    template <typename Sample>
    ReplayPlan plan_replay(const Texture& texture,
                           const std::vector<Sample>& samples,
                           float detail_radius,
                           double max_radius,
                           Strategy strategy)
    {
        ReplayPlan adaptive{Strategy::Adaptive,
            plan_brush_radii(texture, samples, detail_radius, max_radius)};
        if (strategy == Strategy::Adaptive) return adaptive;

        const auto distances = texture_edge_distances(texture);
        auto candidate = strategy == Strategy::Contour
            ? ReplayPlan{Strategy::Contour,
                plan_contour_radii(texture, samples, detail_radius, max_radius, distances)}
            : plan_dominant_replay(texture, samples, detail_radius, max_radius, distances);
        if (candidate.stroke_count() < adaptive.stroke_count()) return candidate;
        return adaptive;
    }
}
