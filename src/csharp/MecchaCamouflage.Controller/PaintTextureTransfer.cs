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
