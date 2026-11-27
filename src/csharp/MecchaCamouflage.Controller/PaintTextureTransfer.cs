using System.IO.Compression;

namespace MecchaCamouflage.Controller;

public sealed record PaintTextureExportResult(
    bool Success,
    string Message,
    string Directory,
    string ManifestPath,
    string AlbedoPng,
    string PbrPng,
    int TextureSize);

public sealed record PaintTextureImportResult(
    bool Success,
    string Message,
    string Stage);

public static class PaintTextureArchive
{
    public const string Extension = ".mcpaint";

    public static void Write(
        string path,
        string manifestPath,
        string albedoRgba,
        string pbrRgba,
        string albedoPng,
        string pbrPng)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var temporary = full + ".tmp-" + Guid.NewGuid().ToString("N");
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (var archive = new ZipArchive(stream, ZipArchiveMode.Create))
            {
                Add(archive, "manifest.json", manifestPath);
                Add(archive, "albedo.png", albedoPng);
                Add(archive, "pbr.png", pbrPng);
                Add(archive, "albedo.rgba", albedoRgba);
                Add(archive, "pbr.rgba", pbrRgba);
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
    }

    private static void Add(ZipArchive archive, string name, string source)
    {
        var entry = archive.CreateEntry(name, CompressionLevel.Optimal);
        using var input = File.OpenRead(source);
        using var output = entry.Open();
        input.CopyTo(output);
    }
}
