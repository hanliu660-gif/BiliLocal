# BiliLocal — modernized 64-bit fork

A modernized, Windows x64 fork of [BiliLocal 0.4.2](https://github.com/AncientLysine/BiliLocal)
(GPL-3.0, last updated 2016): a local video player that overlays **danmaku**
(bullet comments) on recorded livestreams. Built for the daily loop of playing
server-side stream recordings (`*.flv` / `*.mp4`) together with the same-name
danmaku XML (`*.xml`) that recorder tools drop next to the video.

现代化改造的 64 位 Windows 分支：给本地录播挂载弹幕的播放器。

> **维护者必读**：[docs/HANDOVER.md](docs/HANDOVER.md) 是交接手册——本机目录布局、构建/部署
> 流程、VLC 运行库升级注意事项、修过的坑（含根因与证据）、验证方法与回滚方式都在里面。
> 改代码前先读它。

## What changed vs upstream

- **64-bit build** (Qt 6.8 LTS / MSVC 2022), and a long list of upstream-era
  bugs fixed (config persistence, UTF-8 BOM danmaku parsing, crash-on-quit,
  fullscreen glitches — see the commit history, every commit documents the
  root cause and its evidence)
- **VLC 3.0.24 (FFmpeg 8.1.2) + Direct3D11 hardware decode, direct output**: video is rendered by
  libvlc itself into a native child window (`--avcodec-hw=d3d11va`,
  `--vout=direct3d11`); danmaku are painted by a separate transparent OpenGL
  overlay that follows the main window. Playback CPU dropped from ~210% to
  ~11% on the dev machine
- **Modern UI**: a single auto-hiding bottom bar (play/pause, time, seek
  strip, danmaku toggle, speed, volume, open, settings, fullscreen, playlist);
  the playlist panel deliberately never steals keyboard focus
- **Player conveniences**: click-to-pause / double-click fullscreen on the video
  area, 0–9 percentage jump, ±10 s / ±60 s skips, wheel volume,
  Ctrl+wheel danmaku opacity, mute memory, hold-right-button 3× speed,
  resume-where-you-left-off plus auto-resume of the last unfinished media on a
  bare launch, light/dark/system theme, single instance with argument
  forwarding
- **Online features removed** — the 2013–2016 danmaku servers are long dead;
  only local danmaku files load now
- **Single-platform consolidation**: the vmem (CPU-copy) rendering line, the
  QtMultimedia / dummy player backends, the Raster and Quick2 outputs, the
  linux / macx / android configs, the plugin interface and the danmaku
  network stack were removed. Everything is preserved in git history; tag
  `v0.5.0-vmem` marks the last pre-rewrite vmem state
- Proper i18n (English sources + complete zh_CN / zh_TW) and over 6,000
  lines of dead code stripped

## Danmaku

- Formats (auto-detected): Bilibili XML (incl. UTF-8 BOM), AcFun JSON,
  Niconico, AcFunLocalizer, ASS
- Auto-association: open `video.flv`, a `video.xml` / `video.json` next to it
  is mounted automatically
- Collision-free layout computed on worker threads; per-screen and repeat
  limits; shielding by type / sender / regex; per-source delay calibration
  with a visual density track editor

## Usage

- Launch the executable and drop `*.flv` / `*.mp4` onto the window (or pass
  paths on the command line); a same-name `*.xml` / `*.json` next to the
  video is mounted as its danmaku track automatically
- Move the mouse to reveal the single bottom bar; it hides after 2 s idle.
  Left to right: play/pause, time (h:mm:ss past one hour), seek strip
  (click-to-jump, draggable), danmaku on/off, speed cycler (1.0 → 3.0),
  volume, and four icon buttons (open, settings, fullscreen, playlist)
- The playlist panel never steals keyboard focus
- Hover the video to freeze the danmaku under the cursor (it resumes when
  you move off); right-click it for the full text (word-wrapped,
  selectable) plus a read-only block with the masked username and stable
  user hash (uid itself is anonymized by Bilibili; the hash still
  identifies the person within a recording, so shield-sender works),
  sent time, video position, same-sender count, and copy
- Shortcuts: click video = pause/resume (250 ms disambiguation against
  double-click, `/Interface/Sensitive` toggles it); wheel = volume;
  Ctrl+wheel = danmaku opacity; 0–9 = percentage jump; Shift+←/→ = ±60 s;
  ←/→ = ±10 s; ↑/↓ = volume; M = mute; T = always-on-top; F / F11 /
  double-click = fullscreen; Esc = leave fullscreen; Ctrl+←/→ = danmaku
  delay; hold right button = 3× speed (release restores; a short right
  click opens the menu)
- Resume: closing remembers the position; reopening jumps back when more
  than 10 s was watched and the end was not reached (`Player/Resume=false`
  disables). A bare launch auto-plays the most recent unfinished media
  (`Player/AutoResume`); the open dialogs share one last-directory memory
  (`Interface/OpenDir`)

## Build (Windows x64)

Toolchain used by this fork:

- Qt 6.8.3 (msvc2022_64) — the Core5Compat module is required (legacy
  GBK/encoding danmaku files go through its QTextCodec)
- MSVC 2022 (v143) — the CMake Visual Studio generator locates the
  toolchain on its own
- VLC 3.0.24 — SDK (headers + MSVC import libs) at build time, full runtime
  (libvlc, libvlccore, plugins) at run time

```bat
cmake -G "Visual Studio 17 2022" -A x64 -B build-cmake6 -S . -DQt6_DIR=<qt6>\lib\cmake\Qt6
cmake --build build-cmake6 --config Release -j8
```

Sources with Chinese string literals must stay UTF-8 with BOM. Runtime needs
Qt6 Core/Gui/Widgets/Network/Concurrent/OpenGL/OpenGLWidgets/Core5Compat, the
VLC runtime, the VC runtime (vcruntime140 etc.), and `locale/<lang>/bililocal.qm`
for translations — the BOM/i18n housekeeping workflow lives in
[docs/HANDOVER.md](docs/HANDOVER.md).

## Tests

Five QtTest targets (41 cases; parse 9, config 7, shield 9, load 12,
list 4) build and run through CTest — the load/parse suites caught the
upstream ASS color bug and several regressions of our own:

```bat
ctest --test-dir build-cmake6 -C Release
```

## Lineage

- Upstream: `AncientLysine/BiliLocal` @ `9d1cb05` (0.4.2, 2016)
- `v0.5.0-vmem` — rollback point of the original vmem rendering line, before
  the VLC 3 / d3d11 direct-output rewrite
- `v0.6.0-qt5-final` — last Qt 5 / MinGW state before the Qt 6.8 LTS /
  MSVC migration

## License

GPL-3.0, see [COPYING](COPYING). All credit for the original player goes to
Lysine (AncientLysine). This fork's changes are documented commit by commit.
