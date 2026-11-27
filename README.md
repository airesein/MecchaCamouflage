<p align="center">
  <img
    src="docs/assets/meccha-camouflage-readme-banner-v151-1600w.jpg"
    alt="Meccha Camouflage demo"
    width="900"
  />
</p>

<p align="center">
  <a href="LICENSE.txt">
    <img
      src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg"
      alt="License: GPL-3.0-or-later"
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
    alt="Meccha Camouflage icon"
    width="36"
  />
  Meccha Camouflage
</h1>

A Windows desktop app for MECCHA CHAMELEON.

This project is an unofficial, modified build based on
[acentrist/MecchaCamouflage](https://github.com/acentrist/MecchaCamouflage).
It is not affiliated with or endorsed by the upstream project.

## Features

- **Paint**: Paint a player character with custom colors and materials.
- **Image Paint**: Paint imported images onto a player character.
- **ESP**: Show player locations and information in game.

## Download

Download the latest <code>meccha-camouflage.exe</code> from
<a href="https://github.com/airesein/MecchaCamouflage/releases/latest">GitHub Releases</a>.

## Usage

1. Start MECCHA CHAMELEON.
2. Start `meccha-camouflage.exe`.

Logs are written under:

```text
%LOCALAPPDATA%\MecchaCamouflage\versions\<version>\logs\
```

If Windows asks, approve the UAC prompt at startup to add this Microsoft
Defender exclusion:

```text
%LOCALAPPDATA%\MecchaCamouflage\
```

## Development

```bash
git clone --recurse-submodules https://github.com/airesein/MecchaCamouflage.git
cd MecchaCamouflage
make run
```

## Security

Follow the disclosure process in [SECURITY.md](SECURITY.md).

## License

This project is open source under the
[GNU General Public License v3.0 or later](LICENSE.txt).

[GPL-3.0-or-later](LICENSE.txt) © Acentrist and contributors.
