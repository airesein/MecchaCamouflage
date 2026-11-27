<p align="center">
  <img
    src="docs/assets/meccha-camouflage-readme-banner-v151-1600w.jpg"
    alt="Meccha Camouflage 演示"
    width="900"
  />
</p>

<p align="center">
  <a href="LICENSE.txt">
    <img
      src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg"
      alt="许可证：GPL-3.0-or-later"
    />
  </a>
</p>

<p align="center">
  <a href="README.md">English</a> |
  <a href="README.zh-CN.md">简体中文</a> |
  <a href="README.zh-TW.md">繁體中文</a> |
  <a href="README.ja.md">日本語</a> |
  <a href="README.ko.md">한국어</a>
</p>

<h1>
  <img
    src="resources/app-icons/icon.png"
    alt="Meccha Camouflage 图标"
    width="36"
  />
  Meccha Camouflage
</h1>

一款适用于 MECCHA CHAMELEON 的 Windows 桌面应用。

本项目是基于
[acentrist/MecchaCamouflage](https://github.com/acentrist/MecchaCamouflage)
开发的非官方修改版，与上游项目无隶属关系，也未获得其认可。

## 功能

- **涂装**：使用自定义颜色和材质涂抹玩家角色。
- **图像涂装**：把导入的图片涂装到玩家角色上。
- **ESP**：在游戏中显示玩家位置与信息。

## 下载

从
<a href="https://github.com/airesein/MecchaCamouflage/releases/latest">GitHub Releases</a>
下载最新的 <code>meccha-camouflage.exe</code>。

## 使用

1. 启动 MECCHA CHAMELEON。
2. 启动 `meccha-camouflage.exe`。

日志写入位置：

```text
%LOCALAPPDATA%\MecchaCamouflage\versions\<version>\logs\
```

如果 Windows 弹出提示，请在启动时允许 UAC，以添加以下 Microsoft Defender
排除项：

```text
%LOCALAPPDATA%\MecchaCamouflage\
```

## 开发

```bash
git clone --recurse-submodules https://github.com/airesein/MecchaCamouflage.git
cd MecchaCamouflage
make run
```

## 安全

请遵循 [SECURITY.md](SECURITY.md) 中的漏洞披露流程。

## 许可证

本项目基于
[GNU 通用公共许可证 v3.0 或更高版本](LICENSE.txt) 开源。

[GPL-3.0-or-later](LICENSE.txt) © Acentrist 及贡献者。
