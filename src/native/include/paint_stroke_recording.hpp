#pragma once

#include "sdk.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

// Self-contained serialization for recorded PaintAtUVWithBrush strokes.
//
// The recorded stroke list is the exact value-parameter payload the game
// receives: UV, channel data, brush settings, and the target channel.  The
// on-disk schema is intentionally plain JSON so that the C# host can validate
// the archive without duplicating any colour/brush math.  Nothing here applies
// sRGB conversion, texture resampling, or lossy compression.
namespace paint_stroke_recording
{
    constexpr const char* Schema = "meccha_paint_strokes_v1";
    constexpr const char* FileName = "strokes.json";
    constexpr const char* NativeSource = "native_process_event_hook";
    constexpr std::size_t MaxStrokes = 4'000'000;
    constexpr std::size_t MaxRecordsBytes = 256 * 1024 * 1024;

    struct ParsedStrokes
    {
        bool ok{false};
        std::string failure{};
        std::string source{};
        std::vector<sdk::FPaintStroke> strokes{};
        int declared_count{-1};
        int count{-1};
        std::uint64_t dropped{0};
    };

    inline auto append_number(std::string& out, double value) -> void
    {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "%.17g", value);
        out += buffer;
    }

    inline auto append_float(std::string& out, float value) -> void
    {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
        out += buffer;
    }

    inline auto append_hex_pointer(std::string& out, std::uintptr_t value) -> void
    {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "0x%llX", static_cast<unsigned long long>(value));
        out += buffer;
    }

    inline auto escape_json(const std::string& text) -> std::string
    {
        std::string out{};
        out.reserve(text.size() + 8);
        for (const char ch : text)
        {
            const auto value = static_cast<unsigned char>(ch);
            switch (ch)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (value < 0x20)
                {
                    char buffer[8]{};
                    std::snprintf(buffer, sizeof(buffer), "\\u%04X", value);
                    out += buffer;
                }
                else
                {
                    out.push_back(ch);
                }
                break;
            }
        }
        return out;
    }

    using BrushTexturePathResolver = std::function<std::string(std::uintptr_t)>;
    using BrushTextureLookup = std::function<std::uintptr_t(const std::string&)>;

    inline auto serialize(const std::vector<sdk::FPaintStroke>& strokes,
                          const std::string& source,
                          std::uint64_t dropped,
                          const BrushTexturePathResolver& brush_path) -> std::string
    {
        std::string out{};
        out.reserve(strokes.size() * 320U + 256U);
        out += "{\"schema\":\"";
        out += Schema;
        out += "\",\"source\":\"";
        out += escape_json(source.empty() ? std::string(NativeSource) : source);
        out += "\",\"count\":";
        out += std::to_string(strokes.size());
        out += ",\"dropped\":";
        out += std::to_string(dropped);
        out += ",\"brush_texture_pointer_is_process_local\":true";
        out += ",\"strokes\":[";
        bool first = true;
        for (const auto& stroke : strokes)
        {
            if (!first)
            {
                out.push_back(',');
            }
            first = false;
            const auto brush_texture =
                reinterpret_cast<std::uintptr_t>(stroke.BrushSettings.BrushTexture);
            const std::string texture_path =
                brush_texture != 0 && brush_path ? brush_path(brush_texture) : std::string{};
            out += "{\"u\":";
            append_number(out, stroke.Uv.X);
            out += ",\"v\":";
            append_number(out, stroke.Uv.Y);
            out += ",\"channel\":";
            out += std::to_string(static_cast<int>(stroke.TargetChannel));
            out += ",\"radius\":";
            append_float(out, stroke.BrushSettings.Radius);
            out += ",\"hardness\":";
            append_float(out, stroke.BrushSettings.Hardness);
            out += ",\"opacity\":";
            append_float(out, stroke.BrushSettings.Opacity);
            out += ",\"spacing\":";
            append_float(out, stroke.BrushSettings.Spacing);
            out += ",\"falloff\":";
            out += std::to_string(static_cast<int>(stroke.BrushSettings.Falloff));
            out += ",\"blend_mode\":";
            out += std::to_string(static_cast<int>(stroke.BrushSettings.BlendMode));
            out += ",\"rotation\":";
            append_float(out, stroke.BrushSettings.Rotation);
            out += ",\"brush_texture_ptr\":\"";
            append_hex_pointer(out, brush_texture);
            out += "\",\"brush_texture_path\":\"";
            out += escape_json(texture_path);
            out += "\",\"albedo_r\":";
            append_float(out, stroke.ChannelData.AlbedoColor.R);
            out += ",\"albedo_g\":";
            append_float(out, stroke.ChannelData.AlbedoColor.G);
            out += ",\"albedo_b\":";
            append_float(out, stroke.ChannelData.AlbedoColor.B);
            out += ",\"albedo_a\":";
            append_float(out, stroke.ChannelData.AlbedoColor.A);
            out += ",\"metallic\":";
            append_float(out, stroke.ChannelData.Metallic);
            out += ",\"roughness\":";
            append_float(out, stroke.ChannelData.Roughness);
            out += ",\"height\":";
            append_float(out, stroke.ChannelData.Height);
            out += ",\"emissive\":";
            append_float(out, stroke.ChannelData.EmissiveColor);
            out += ",\"apply_mode\":";
            out += std::to_string(static_cast<int>(stroke.ChannelData.ApplyMode));
            out.push_back('}');
        }
        out += "]}";
        return out;
    }

    namespace detail
    {
        inline auto find_key(const std::string& text, const std::string& key) -> std::size_t
        {
            const std::string needle = std::string("\"") + key + "\":";
            return text.find(needle);
        }

        inline auto matching_bracket(const std::string& text,
                                     std::size_t open,
                                     char open_ch,
                                     char close_ch) -> std::size_t
        {
            bool in_string = false;
            bool escaped = false;
            int depth = 0;
            for (std::size_t index = open; index < text.size(); ++index)
            {
                const char ch = text[index];
                if (in_string)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (ch == '\\')
                    {
                        escaped = true;
                    }
                    else if (ch == '"')
                    {
                        in_string = false;
                    }
                    continue;
                }
                if (ch == '"')
                {
                    in_string = true;
                    continue;
                }
                if (ch == open_ch)
                {
                    ++depth;
                }
                else if (ch == close_ch)
                {
                    --depth;
                    if (depth == 0)
                    {
                        return index;
                    }
                }
            }
            return std::string::npos;
        }

        inline auto number_field(const std::string& text, const std::string& key, double fallback) -> double
        {
            const std::string needle = std::string("\"") + key + "\":";
            const auto start = text.find(needle);
            if (start == std::string::npos)
            {
                return fallback;
            }
            const char* begin = text.c_str() + start + needle.size();
            char* end = nullptr;
            const double value = std::strtod(begin, &end);
            return end == begin || !std::isfinite(value) ? fallback : value;
        }

        inline auto int_field(const std::string& text,
                              const std::string& key,
                              int fallback,
                              int minimum,
                              int maximum) -> int
        {
            const auto value = number_field(text, key, static_cast<double>(fallback));
            return std::clamp(static_cast<int>(value), minimum, maximum);
        }

        inline auto append_utf8(std::string& target, std::uint32_t codepoint) -> void
        {
            if (codepoint <= 0x7F)
            {
                target.push_back(static_cast<char>(codepoint));
            }
            else if (codepoint <= 0x7FF)
            {
                target.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                target.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint <= 0xFFFF)
            {
                target.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                target.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                target.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else
            {
                target.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                target.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                target.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                target.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }

        inline auto string_field(const std::string& text,
                                 const std::string& key,
                                 const std::string& fallback) -> std::string
        {
            const std::string needle = std::string("\"") + key + "\":";
            const auto start = text.find(needle);
            if (start == std::string::npos)
            {
                return fallback;
            }
            auto pos = text.find('"', start + needle.size());
            if (pos == std::string::npos)
            {
                return fallback;
            }
            ++pos;
            std::string out{};
            bool escaped = false;
            for (; pos < text.size(); ++pos)
            {
                const char ch = text[pos];
                if (escaped)
                {
                    switch (ch)
                    {
                    case '"':
                        out.push_back('"');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    case '/':
                        out.push_back('/');
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u':
                    {
                        if (pos + 4 >= text.size())
                        {
                            return fallback;
                        }
                        std::uint32_t codepoint = 0;
                        bool valid = true;
                        for (std::size_t offset = 1; offset <= 4; ++offset)
                        {
                            const char digit = text[pos + offset];
                            int value = -1;
                            if (digit >= '0' && digit <= '9')
                            {
                                value = digit - '0';
                            }
                            else if (digit >= 'a' && digit <= 'f')
                            {
                                value = digit - 'a' + 10;
                            }
                            else if (digit >= 'A' && digit <= 'F')
                            {
                                value = digit - 'A' + 10;
                            }
                            if (value < 0)
                            {
                                valid = false;
                                break;
                            }
                            codepoint = (codepoint << 4) | static_cast<std::uint32_t>(value);
                        }
                        if (!valid)
                        {
                            return fallback;
                        }
                        append_utf8(out, codepoint);
                        pos += 4;
                        break;
                    }
                    default:
                        out.push_back(ch);
                        break;
                    }
                    escaped = false;
                    continue;
                }
                if (ch == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (ch == '"')
                {
                    return out;
                }
                out.push_back(ch);
            }
            return fallback;
        }

        template <typename Fn>
        inline auto for_each_stroke_object(const std::string& text,
                                           std::size_t begin,
                                           std::size_t end,
                                           Fn&& fn) -> void
        {
            for (std::size_t pos = begin; pos < end;)
            {
                const auto open = text.find('{', pos);
                if (open == std::string::npos || open >= end)
                {
                    break;
                }
                const auto close = matching_bracket(text, open, '{', '}');
                if (close == std::string::npos || close >= end)
                {
                    break;
                }
                fn(text.substr(open, close - open + 1));
                pos = close + 1;
            }
        }
    }

    inline auto parse(const std::string& json,
                      const BrushTextureLookup& brush_lookup,
                      std::uintptr_t fallback_brush_texture) -> ParsedStrokes
    {
        ParsedStrokes result{};
        if (json.empty())
        {
            result.failure = "strokes_file_empty";
            return result;
        }
        if (json.size() > MaxRecordsBytes)
        {
            result.failure = "strokes_file_too_large";
            return result;
        }
        const auto schema_start = detail::find_key(json, "schema");
        if (schema_start != std::string::npos)
        {
            const auto schema = detail::string_field(json, "schema", "");
            if (schema != Schema)
            {
                result.failure = "strokes_schema_unsupported:" + schema;
                return result;
            }
        }
        result.source = detail::string_field(json, "source", NativeSource);
        result.declared_count = detail::int_field(json, "count", -1, -1, 100'000'000);
        const auto dropped = detail::number_field(json, "dropped", 0.0);
        result.dropped = dropped > 0.0 ? static_cast<std::uint64_t>(dropped) : 0ULL;

        const std::string needle = "\"strokes\":";
        const auto key_pos = json.find(needle);
        if (key_pos == std::string::npos)
        {
            result.failure = "strokes_array_missing";
            return result;
        }
        const auto open = json.find('[', key_pos + needle.size());
        if (open == std::string::npos)
        {
            result.failure = "strokes_array_missing";
            return result;
        }
        const auto close = detail::matching_bracket(json, open, '[', ']');
        if (close == std::string::npos || close <= open)
        {
            result.failure = "strokes_array_invalid";
            return result;
        }

        result.strokes.reserve(
            result.declared_count > 0
                ? static_cast<std::size_t>(std::min(result.declared_count, 100'000))
                : 1024U);
        detail::for_each_stroke_object(json, open + 1, close, [&](const std::string& object_text) {
            if (result.strokes.size() >= MaxStrokes)
            {
                return;
            }
            sdk::FPaintStroke stroke{};
            stroke.Uv.X = detail::number_field(object_text, "u", 0.0);
            stroke.Uv.Y = detail::number_field(object_text, "v", 0.0);
            stroke.TargetChannel = static_cast<sdk::EPaintChannel>(
                detail::int_field(object_text, "channel", 0, 0,
                                  static_cast<int>(sdk::EPaintChannel::Max) - 1));
            stroke.BrushSettings.Radius =
                static_cast<float>(detail::number_field(object_text, "radius", 0.02));
            stroke.BrushSettings.Hardness =
                static_cast<float>(detail::number_field(object_text, "hardness", 1.0));
            stroke.BrushSettings.Opacity =
                static_cast<float>(detail::number_field(object_text, "opacity", 1.0));
            stroke.BrushSettings.Spacing =
                static_cast<float>(detail::number_field(object_text, "spacing", 0.25));
            stroke.BrushSettings.Falloff = static_cast<sdk::EBrushFalloff>(
                detail::int_field(object_text, "falloff", 2, 0,
                                  static_cast<int>(sdk::EBrushFalloff::Max) - 1));
            stroke.BrushSettings.BlendMode = static_cast<sdk::EPaintBlendMode>(
                detail::int_field(object_text, "blend_mode", 0, 0,
                                  static_cast<int>(sdk::EPaintBlendMode::Max) - 1));
            stroke.BrushSettings.Rotation =
                static_cast<float>(detail::number_field(object_text, "rotation", 0.0));
            const auto texture_path = detail::string_field(object_text, "brush_texture_path", "");
            std::uintptr_t brush_texture = 0;
            if (!texture_path.empty() && brush_lookup)
            {
                brush_texture = brush_lookup(texture_path);
            }
            if (brush_texture == 0)
            {
                brush_texture = fallback_brush_texture;
            }
            stroke.BrushSettings.BrushTexture = reinterpret_cast<void*>(brush_texture);
            stroke.ChannelData.AlbedoColor.R =
                static_cast<float>(detail::number_field(object_text, "albedo_r", 1.0));
            stroke.ChannelData.AlbedoColor.G =
                static_cast<float>(detail::number_field(object_text, "albedo_g", 1.0));
            stroke.ChannelData.AlbedoColor.B =
                static_cast<float>(detail::number_field(object_text, "albedo_b", 1.0));
            stroke.ChannelData.AlbedoColor.A =
                static_cast<float>(detail::number_field(object_text, "albedo_a", 1.0));
            stroke.ChannelData.Metallic =
                static_cast<float>(detail::number_field(object_text, "metallic", 0.0));
            stroke.ChannelData.Roughness =
                static_cast<float>(detail::number_field(object_text, "roughness", 0.65));
            stroke.ChannelData.Height =
                static_cast<float>(detail::number_field(object_text, "height", 0.0));
            stroke.ChannelData.EmissiveColor =
                static_cast<float>(detail::number_field(object_text, "emissive", 0.0));
            stroke.ChannelData.ApplyMode = static_cast<sdk::EPaintChannelApplyMode>(
                detail::int_field(object_text, "apply_mode", 0, 0,
                                  static_cast<int>(sdk::EPaintChannelApplyMode::Max) - 1));
            stroke.bHasWorldPosition = false;
            stroke.bHasLocalPosition = false;
            stroke.bHasSkeletalTriangleAnchor = false;
            result.strokes.push_back(stroke);
        });

        result.count = static_cast<int>(result.strokes.size());
        result.ok = !result.strokes.empty();
        if (!result.ok)
        {
            result.failure = "strokes_empty";
        }
        return result;
    }
}
