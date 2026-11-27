<p align="center">
  <img
    src="docs/assets/meccha-camouflage-readme-banner-v151-1600w.jpg"
    alt="Meccha Camouflage 데모"
    width="900"
  />
</p>

<p align="center">
  <a href="LICENSE.txt">
    <img
      src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg"
      alt="라이선스: GPL-3.0-or-later"
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
    alt="Meccha Camouflage 아이콘"
    width="36"
  />
  Meccha Camouflage
</h1>

MECCHA CHAMELEON용 Windows 데스크톱 앱입니다.

이 프로젝트는
[acentrist/MecchaCamouflage](https://github.com/acentrist/MecchaCamouflage)
를 기반으로 한 비공식 수정 빌드입니다. 업스트림 프로젝트와 제휴하거나 승인받은 것이 아닙니다.

## 기능

- **페인트**: 사용자 지정 색상과 재질로 플레이어 캐릭터를 칠합니다.
- **이미지 페인트**: 가져온 이미지를 플레이어 캐릭터에 칠합니다.
- **ESP**: 게임 내에서 플레이어 위치와 정보를 표시합니다.

## 다운로드

<a href="https://github.com/airesein/MecchaCamouflage/releases/latest">GitHub Releases</a>
에서 최신 <code>meccha-camouflage.exe</code>를 다운로드하세요.

## 사용법

1. MECCHA CHAMELEON을 시작합니다.
2. `meccha-camouflage.exe`를 시작합니다.

로그는 다음 위치에 기록됩니다:

```text
%LOCALAPPDATA%\MecchaCamouflage\versions\<version>\logs\
```

Windows에서 요청하면 시작 시 UAC를 승인하여 다음 Microsoft Defender
제외 항목을 추가하세요:

```text
%LOCALAPPDATA%\MecchaCamouflage\
```

## 개발

```bash
git clone --recurse-submodules https://github.com/airesein/MecchaCamouflage.git
cd MecchaCamouflage
make run
```

## 보안

취약점 신고는 [SECURITY.md](SECURITY.md)의 절차를 따르세요.

## 라이선스

이 프로젝트는
[GNU General Public License v3.0 이상](LICENSE.txt)에 따라 오픈 소스로 공개됩니다.

[GPL-3.0-or-later](LICENSE.txt) © Acentrist 및 기여자.
