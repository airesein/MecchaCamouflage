# MecchaCamouflage 交接文档

最后更新：2026-11-28  
当前功能 HEAD：`9d5a4a7 fix(image): replay reused paint via image paint mesh samples`  
交接文档提交：`3805858 docs: add project handoff`  
当前测试版：`v1.7.5-test.11`  
稳定版：`v1.7.4`  
当前工作区：干净（本文提交前）

## 1. 仓库与环境

- 上游：<https://github.com/acentrist/MecchaCamouflage>
- 你的 fork（CI/发布）：<https://github.com/airesein/MecchaCamouflage>
- 游戏：MECCHA CHAMELEON，UE 5.6.1，build 2026-11-16
- 游戏路径：`E:\Program Files\steam\steamapps\common\MECCHA CHAMELEON\Chameleon\Binaries\Win64\PenguinHotel-Win64-Shipping.exe`
- 工作目录：`E:\项目\变色龙\MecchaCamouflage`
- 运行时：UE4SS v4.0.0（Win64 `dwmapi.dll` + `ue4ss\`）；映射 `Win64\ue4ss\Chameleon-5.6.1-0+UE5-30afe5ff.usmap`，并复制到 `.build\Mappings.usmap`
- .NET：`.build\tools\dotnet`（10.0.404）；构建时需把该目录加入 `PATH`
- 子模块：`third_party/minhook`（原生构建必需）；`third_party/CUE4Parse` 已初始化；`third_party/UnrealMappingsDumper` 可不初始化
- CI 只 checkout `minhook`，不依赖 `UnrealMappingsDumper`

## 2. 当前状态

- 六体型已支持：`round / cube / fukuyoka / mecho / octopus / porigon`。
- `.mcpaint` 单文件（ZIP）导出/导入；Reuse Paint 有 **Start / Stop / Preview / Stop Preview**；路径框有内容即可用；底部 GitHub 链接已改为 fork。
- 最新 test.11 的 Reuse `Start`：使用 **Image Paint 的 mesh 表面采样点**，按采样 UV 读取保存的 raw UV 纹理，再走 `PaintAtUVWithBrush`。
- `Preview`：本地纹理导入并保存快照；`Stop Preview`：恢复快照。
- 已知问题：
  - Reuse 回放颜色比 Preview **暗**（raw 纹理又被 sRGB→linear 一次）。
  - Reuse 回放 **较慢**（逐笔走游戏队列）。
  - 用户目标：**导出游戏中画的，下次一模一样，同时别人可见**；当前 `.mcpaint` 只有最终纹理，没有笔画历史，所以不是 1:1。

## 3. 目录与关键文件

- 原生：
  - `src/native/bridge/bridge.cpp`：桥、导出/导入、Image Paint 4 面、`replay_paint_texture`、`PaintAtUVWithBrush`、mesh 采样。
  - `src/native/include/sdk.hpp`：`FPaintStroke` / `FPaintChannelData` / 字段偏移。
- C# Controller：
  - `src/csharp/MecchaCamouflage.Controller/HostSession.cs`：导出/导入、Reuse、Preview、Stop、进度。
  - `src/csharp/MecchaCamouflage.Controller/PaintTextureTransfer.cs`：`.mcpaint` ZIP、manifest 归一化。
- Core：
  - `src/csharp/MecchaCamouflage.Core/Models.cs`：六体型、`ImagePaintSettings`。
  - `src/csharp/MecchaCamouflage.Core/Localization/Strings.json`：16 种语言。
- WebHost：
  - `src/csharp/MecchaCamouflage.WebHost/MainForm.cs`：命令分发、Reuse 命令。
  - `src/csharp/MecchaCamouflage.WebHost/web/app.js` / `index.html` / `styles.css`：Reuse 按钮、路径启用、GitHub 链接。
- 测试：`src/csharp/MecchaCamouflage.Tests/Program.cs`（186 个测试）。
- 脚本：`scripts/build.ps1`、`scripts/refresh-image-reference-profile.ps1`（六体型 capture）、`scripts/mesh.ps1`。
- 资源：`resources/mesh-profiles/*.mesh-profile-v2.json`、`*.image-profile-v2.json`；`exported-models/*.obj`。
- 文档：`docs/runtime-*.md`、`docs/release-checklist.md`、`docs/repository-layout.md`。
- CI/发布：`.github/workflows/ci.yml`、`.github/workflows/release.yml`。

## 4. `.mcpaint` 与 Reuse 行为

- `.mcpaint` = ZIP：
  - `manifest.json`
  - `albedo.png`
  - `pbr.png`
  - `albedo.rgba`
  - `pbr.rgba`
- 导出目录：`%LOCALAPPDATA%\MecchaCamouflage\versions\<version>\paint-exports\`
- manifest 中的纹理名在打包/解包时归一化为固定名，修复过 “texture files are missing”。
- Reuse `Start`（test.11）：mesh 采样 raw UV 纹理 → channel 7 strokes → `PaintAtUVWithBrush`；不经过 4 面图。
- Reuse `Preview`：本地 `import_paint_texture`（带快照）；`Stop Preview` 恢复。
- 旧 `.json` 导出仍可手动导入。
- 纹理导入是 **本地**（Preview/Unpreview）；生产同步只走 `PaintAtUVWithBrush`，由游戏自己复制。

## 5. 运行契约（build 2026-11-16）

- GUObjectArray RVA：`0xB2A9620`
- GNames RVA：`0xB555CC0`
- GMalloc RVA：`0xB5FB5D8`
- GObjects 签名：`48 8D 05 ?? ?? ?? ?? 4C 8B C9 48 89 01 B8 FF FF`
- GNames 签名：`48 8D 05 ?? ?? ?? ?? E9 89 00 00 00`
- `PaintAtUVWithBrush` 参数：`Uv, ChannelData, BrushSettings, Channel`；生产只走该 RPC。
- AMRE 通道 7：`R=Metallic`、`G=Roughness`、`B=Emissive`。
- raw 纹理 UV 索引：`cx = round(u * (size - 1))`，`cy = round(v * (size - 1))`（**不要** `1-v`）。
- Image Paint 4 面画布：`1024x512`；raw UV 纹理：`1024x1024`。
- `ImportChannelFromBytes` 只允许 Preview/Unpreview，不能作为生产回放。
- 游戏单次 paint 请求上限 900 秒；Reuse 进度复用现有 job progress。

## 6. 当前已知问题与用户需求

1. **颜色变暗**
   - 原因：Reuse 回放对 raw 纹理执行了 `appearance_srgb_to_linear`；Preview 直接导入 raw，所以 Preview 正确、回放偏暗。
   - 建议：raw 纹理回放 **不要再做 sRGB→linear**（直接用 raw 值）；只让 4 面 Image Paint 路径做转换。

2. **速度**
   - mesh 采样 + 游戏笔刷队列，1024 纹理/密集样本时较慢。
   - 可选：`Apply Exact` 本地导入（瞬间）；降低采样密度；安全地合并相同笔画（不要用会跳色留白的方式，之前已因此回退）。

3. **精确 + 同步**
   - 当前 `.mcpaint` 只存最终纹理，没有笔画历史；回放是纹理重采样，不是 1:1。
   - 用户要求：导出游戏中画的，下次一模一样，同时别人可见。
   - 方案：
     - **Apply Exact**：本地导入 raw 纹理；瞬间、精确；但仅本机，别人看不到。
     - **导出/记录笔画**：读取游戏的 recorded stroke queue（`GetRecordedStrokeCount` / `RuntimePaintReplicationManager`），或 hook `ProcessEvent` 捕获 `PaintAtUVWithBrush` 参数；把 strokes 写入 `.mcpaint`；Reuse 原样回放。
     - 需确认游戏是否保留完整历史；可能只保留待同步的笔画。若只保留 pending：旧画只能纹理回放/本地 Apply Exact；新画可在绘制时记录。

## 7. 建议下一步（按优先级）

1. **颜色修正**：Reuse raw 纹理回放去掉 sRGB→linear；用 Preview 对照验证。
2. **Apply Exact**：增加按钮，直接本地导入 `.mcpaint`（瞬间、精确、无同步）；`Paint for Others` 保留同步回放。
3. **导出游戏 recorded strokes（无需录制）**：先做 `export_recorded_strokes`，读取游戏队列/组件；若可用，写入 `.mcpaint` 并原样回放；若为空，记录原因并回退。
4. 若游戏不保留历史：在绘制时 hook `PaintAtUVWithBrush` 记录 strokes（可选录制开关），写入 `.mcpaint`；新画即可精确同步回放。
5. 速度优化：在保证覆盖的前提下降低采样/合并相同笔画；不要用会跳色留白的方式。
6. 发布 `test.12`；用同体型/UV、开小号验证其他玩家可见。

## 8. 构建、测试、发布

```powershell
# 构建（必须把便携 dotnet 加进 PATH，否则 build.ps1 找不到 dotnet）
$env:PATH = (Resolve-Path '.build\tools\dotnet').Path + ';' + $env:PATH
powershell -NoProfile -ExecutionPolicy Bypass -File '.\scripts\build.ps1' -Configuration Release

# 测试
& .build\tools\dotnet\dotnet.exe run --project 'src\csharp\MecchaCamouflage.Tests\MecchaCamouflage.Tests.csproj' -c Release
```

发布：

```powershell
git push fork main
git tag -a v1.7.5-test.N -m "v1.7.5-test.N"
git push fork v1.7.5-test.N
```

- CI：**手动触发**（Actions → CI → Run workflow，`workflow_dispatch`）；不再随每次 push 自动构建。
- Release：push `v*-test*` tag 触发 `.github/workflows/release.yml`，生成 `meccha-camouflage-v<tag>.exe`。
- 构建产物：`.build\bin\meccha-camouflage.exe`。
- 发布页：`https://github.com/airesein/MecchaCamouflage/releases`
- 最新测试版：`v1.7.5-test.11`（直接 UV/mesh 采样回放）；稳定版：`v1.7.4`。

## 9. 注意事项

- Reuse 回放要求 **同一体型/UV**；换体型会错位。
- 纹理导入是本地；只有 `PaintAtUVWithBrush` 回放会被游戏同步。
- Reuse 的 PBR 按 packed PBR 解读：`R=Metallic, G=Roughness, B=Emissive`。
- 旧 `.mcpaint` 只有纹理，没有笔画；要 1:1 必须新增 strokes 导出/记录。
- `ColorCompressionTolerance` 默认 0；Image Paint 仍使用；Reuse 已移除按颜色跳过（会留白）。
- `.mcpaint` 只在本机 `%LOCALAPPDATA%`，不会自动上传；分享需手动发文件。
- 第三方注入工具本身有账号/封禁风险。
- 推送代理曾使用 `127.0.0.1:10808`；若失败可切换网络或直连。
- 构建/测试时 `dotnet` 必须在 PATH；CI 内已处理。
- 发布 tag 必须是 `-test` 才会走 prerelease。

## 10. 最近提交

```text
3805858 docs: add project handoff
9d5a4a7 fix(image): replay reused paint via image paint mesh samples
fa24207 fix(image): never skip replay texels by previous color
39df642 feat(image): rework reuse paint controls and replay speed
d3fb69d fix(image): replay paint texels regardless of alpha
d5faaba feat(image): replay reused paint as direct UV brush strokes
60f712e fix(image): normalize mcpaint manifest entry names
249adb6 feat(image): add mecho octopus porigon reference poses
64bcd8e test(image): accept six body type refresh set
16e5279 feat(image): add .mcpaint container and six body types
```

## 11. 交接结论

- 现在可用的：六体型、`.mcpaint` 导出/导入、Reuse 四按钮、Preview 精确本地显示、mesh 采样同步回放。
- 还差：raw 纹理回放颜色修正、Apply Exact 一键本地精确应用、以及“从游戏导出/记录笔画并原样回放”的 1:1 同步。
- 下一步从第 7 节第 1、2 条开始；第 3 条需要先确认游戏 recorded stroke queue 是否保留完整历史。
