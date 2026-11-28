using System.IO.Compression;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace MecchaCamouflage.Controller;

public sealed record PaintTextureExportResult(
    bool Success,
    string Message,
    string Directory,
    string ManifestPath,
    string AlbedoPng,
    string PbrPng,
    int TextureSize,
    string StrokesJson = "",
    int StrokeCount = 0,
    string StrokeSource = "");

public sealed record PaintTextureImportResult(
    bool Success,
    string Message,
    string Stage);

public sealed record PaintTextureReplayResult(
    bool Success,
    string Message,
    string Stage,
    int TextureSize = 0,
    int StrokeCount = 0,
    bool UsedRecordedStrokes = false);

public sealed record PaintTextureReuseResult(
    bool Success,
    string Message,
    string Stage,
    bool LocalImportSucceeded,
    bool ReplaySucceeded,
    int TextureSize = 0,
    int StrokeCount = 0,
    bool UsedRecordedStrokes = false);

public sealed record PaintTextureInspectResult(
    bool Success,
    string Message,
    int TextureSize = 0,
    int StrokeCount = 0,
    string StrokeSource = "",
    bool HasRecordedStrokes = false);

public sealed record PaintStrokeSample(
    double U,
    double V,
    int Channel,
    double Radius,
    double Hardness,
    double Opacity,
    double Spacing,
    int Falloff,
    int BlendMode,
    double Rotation,
    string BrushTexturePath,
    double AlbedoR,
    double AlbedoG,
    double AlbedoB,
    double AlbedoA,
    double Metallic,
    double Roughness,
    double Height,
    double Emissive,
    int ApplyMode);

public sealed record PaintStrokeFileSummary(
    string Schema,
    int Count,
    string Source,
    long Dropped);

public sealed record PaintStrokeRecordingStatus(
    bool Success,
    string Message,
    bool Active,
    int StrokeCount,
    string Source,
    long Dropped,
    string RecordingSource = "",
    int CapturedPaintAtUVWithBrush = 0,
    int CapturedPaintAtScreenPosition = 0,
    int CapturedPaintAtWorldPosition = 0,
    int CapturedPaintAtUV = 0,
    int CapturedStrokeBatches = 0,
    int CapturedServerStrokeBatches = 0,
    int CapturedNativeStrokes = 0,
    int CapturedRecordedQueueStrokes = 0,
    int PaintEventsObserved = 0,
    int PaintEventsCaptured = 0,
    int PaintEventsIgnoredReceiver = 0,
    int PaintEventsLayoutUnavailable = 0,
    bool GlobalProcessEventHook = false,
    string ZeroCountReason = "",
    int DiscoveryPaintCalls = 0,
    int DiscoveryPaintIgnoredReceiver = 0,
    string DiscoverySummary = "");

/// <summary>
/// Portable representation of the game's recorded PaintAtUVWithBrush
/// parameters. The native bridge writes the same schema; this type exists so
/// the host can validate archives and tests can round-trip the raw values
/// without introducing any sRGB, resampling, or compression step.
/// </summary>
public static class PaintStrokeFile
{
    public const string Schema = "meccha_paint_strokes_v1";
    public const string FileName = "strokes.json";

    public static string Serialize(
        IReadOnlyList<PaintStrokeSample> strokes,
        string source,
        long dropped = 0)
    {
        var strokeArray = new JsonArray();
        foreach (var stroke in strokes)
        {
            strokeArray.Add(new JsonObject
            {
                ["u"] = stroke.U,
                ["v"] = stroke.V,
                ["channel"] = stroke.Channel,
                ["radius"] = stroke.Radius,
                ["hardness"] = stroke.Hardness,
                ["opacity"] = stroke.Opacity,
                ["spacing"] = stroke.Spacing,
                ["falloff"] = stroke.Falloff,
                ["blend_mode"] = stroke.BlendMode,
                ["rotation"] = stroke.Rotation,
                ["brush_texture_ptr"] = "0x0",
                ["brush_texture_path"] = stroke.BrushTexturePath,
                ["albedo_r"] = stroke.AlbedoR,
                ["albedo_g"] = stroke.AlbedoG,
                ["albedo_b"] = stroke.AlbedoB,
                ["albedo_a"] = stroke.AlbedoA,
                ["metallic"] = stroke.Metallic,
                ["roughness"] = stroke.Roughness,
                ["height"] = stroke.Height,
                ["emissive"] = stroke.Emissive,
                ["apply_mode"] = stroke.ApplyMode
            });
        }
        var root = new JsonObject
        {
            ["schema"] = Schema,
            ["source"] = string.IsNullOrWhiteSpace(source) ? "host" : source,
            ["count"] = strokes.Count,
            ["dropped"] = dropped,
            ["brush_texture_pointer_is_process_local"] = true,
            ["strokes"] = strokeArray
        };
        return root.ToJsonString();
    }

    public static bool TryParse(
        string json,
        out IReadOnlyList<PaintStrokeSample> strokes,
        out PaintStrokeFileSummary summary,
        out string message)
    {
        strokes = [];
        summary = new PaintStrokeFileSummary("", 0, "", 0);
        message = "";
        if (string.IsNullOrWhiteSpace(json))
        {
            message = "The recorded strokes file is empty.";
            return false;
        }
        try
        {
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object ||
                !root.TryGetProperty("schema", out var schemaElement) ||
                schemaElement.ValueKind != JsonValueKind.String ||
                !string.Equals(schemaElement.GetString(), Schema, StringComparison.Ordinal))
            {
                message = "The recorded strokes file has an unsupported schema.";
                return false;
            }
            if (!root.TryGetProperty("strokes", out var strokesElement) ||
                strokesElement.ValueKind != JsonValueKind.Array)
            {
                message = "The recorded strokes file has no strokes array.";
                return false;
            }
            var source = root.TryGetProperty("source", out var sourceElement) &&
                         sourceElement.ValueKind == JsonValueKind.String
                ? sourceElement.GetString() ?? ""
                : "";
            var count = root.TryGetProperty("count", out var countElement) &&
                        countElement.TryGetInt32(out var declared)
                ? declared
                : strokesElement.GetArrayLength();
            var dropped = root.TryGetProperty("dropped", out var droppedElement) &&
                          droppedElement.TryGetInt64(out var droppedValue)
                ? droppedValue
                : 0L;
            var parsed = new List<PaintStrokeSample>(strokesElement.GetArrayLength());
            foreach (var stroke in strokesElement.EnumerateArray())
            {
                if (stroke.ValueKind != JsonValueKind.Object)
                {
                    message = "The recorded strokes file contains a malformed stroke.";
                    return false;
                }
                parsed.Add(new PaintStrokeSample(
                    Number(stroke, "u"),
                    Number(stroke, "v"),
                    Integer(stroke, "channel"),
                    Number(stroke, "radius"),
                    Number(stroke, "hardness", 1.0),
                    Number(stroke, "opacity", 1.0),
                    Number(stroke, "spacing", 0.25),
                    Integer(stroke, "falloff", 2),
                    Integer(stroke, "blend_mode"),
                    Number(stroke, "rotation"),
                    Text(stroke, "brush_texture_path"),
                    Number(stroke, "albedo_r", 1.0),
                    Number(stroke, "albedo_g", 1.0),
                    Number(stroke, "albedo_b", 1.0),
                    Number(stroke, "albedo_a", 1.0),
                    Number(stroke, "metallic"),
                    Number(stroke, "roughness", 0.65),
                    Number(stroke, "height"),
                    Number(stroke, "emissive"),
                    Integer(stroke, "apply_mode")));
            }
            strokes = parsed;
            summary = new PaintStrokeFileSummary(Schema, count, source, dropped);
            return true;
        }
        catch (JsonException ex)
        {
            message = "The recorded strokes file is not valid JSON: " + ex.Message;
            return false;
        }
    }

    public static bool TryReadSummary(string path, out PaintStrokeFileSummary summary, out string message)
    {
        summary = new PaintStrokeFileSummary("", 0, "", 0);
        message = "";
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            var schema = Text(root, "schema");
            var count = root.TryGetProperty("count", out var countElement) &&
                        countElement.TryGetInt32(out var declared)
                ? declared
                : 0;
            var source = Text(root, "source");
            var dropped = root.TryGetProperty("dropped", out var droppedElement) &&
                          droppedElement.TryGetInt64(out var droppedValue)
                ? droppedValue
                : 0L;
            summary = new PaintStrokeFileSummary(schema, count, source, dropped);
            return true;
        }
        catch (Exception ex) when (ex is IOException or JsonException or UnauthorizedAccessException)
        {
            message = "The recorded strokes file could not be read: " + ex.Message;
            return false;
        }
    }

    private static double Number(JsonElement element, string name, double fallback = 0.0) =>
        element.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.Number
            ? value.GetDouble()
            : fallback;

    private static int Integer(JsonElement element, string name, int fallback = 0) =>
        element.TryGetProperty(name, out var value) && value.TryGetInt32(out var number)
            ? number
            : fallback;

    private static string Text(JsonElement element, string name) =>
        element.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? ""
            : "";
}

public static class PaintTextureArchive
{
    public const string Extension = ".mcpaint";

    public static void Write(
        string path,
        string manifestPath,
        string albedoRgba,
        string pbrRgba,
        string albedoPng,
        string pbrPng,
        string? strokesJson = null)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var temporary = full + ".tmp-" + Guid.NewGuid().ToString("N");
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (var archive = new ZipArchive(stream, ZipArchiveMode.Create))
            {
                AddText(archive, "manifest.json", NormalizeManifest(File.ReadAllText(manifestPath)));
                Add(archive, "albedo.png", albedoPng);
                Add(archive, "pbr.png", pbrPng);
                Add(archive, "albedo.rgba", albedoRgba);
                Add(archive, "pbr.rgba", pbrRgba);
                if (!string.IsNullOrWhiteSpace(strokesJson) && File.Exists(strokesJson))
                    Add(archive, PaintStrokeFile.FileName, strokesJson);
            }
            File.Move(temporary, full, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    public static void Extract(string path, string targetDirectory)
    {
        Directory.CreateDirectory(targetDirectory);
        ZipFile.ExtractToDirectory(path, targetDirectory);
        var manifestPath = Path.Combine(targetDirectory, "manifest.json");
        if (File.Exists(manifestPath))
            File.WriteAllText(manifestPath, NormalizeManifest(File.ReadAllText(manifestPath)));
    }

    private static string NormalizeManifest(string json)
    {
        var manifest = JsonNode.Parse(json) as JsonObject
            ?? throw new InvalidDataException("The paint export manifest is not a JSON object.");
        manifest["albedo_rgba"] = "albedo.rgba";
        manifest["pbr_rgba"] = "pbr.rgba";
        manifest["albedo_png"] = "albedo.png";
        manifest["pbr_png"] = "pbr.png";
        return manifest.ToJsonString();
    }

    private static void Add(ZipArchive archive, string name, string source)
    {
        var entry = archive.CreateEntry(name, CompressionLevel.Optimal);
        using var input = File.OpenRead(source);
        using var output = entry.Open();
        input.CopyTo(output);
    }

    private static void AddText(ZipArchive archive, string name, string content)
    {
        var entry = archive.CreateEntry(name, CompressionLevel.Optimal);
        using var output = entry.Open();
        using var writer = new StreamWriter(output, new UTF8Encoding(false));
        writer.Write(content);
    }
}
