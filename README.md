# BiliLocal — modernized 64-bit fork

A modernized, Windows x64 fork of [BiliLocal 0.4.2](https://github.com/AncientLysine/BiliLocal)
(GPL-3.0, last updated 2016): a local video player that overlays **danmaku**
(bullet comments) on recorded livestreams. Built for the daily loop of playing
server-side stream recordings (`*.flv` / `*.mp4`) together with the same-name
danmaku XML (`*.xml`) that recorder tools drop next to the video.

现代化改造的 64 位 Windows 分支：给本地录播挂载弹幕的播放器。

## What changed vs upstream

- **64-bit build** (Qt 5.15.2 / MinGW 8.1.0), and a long list of upstream-era
  bugs fixed (config persistence, UTF-8 BOM danmaku parsing, crash-on-quit,
  fullscreen glitches — see the commit history, every commit documents the
  root cause and its evidence)
- **VLC 3 + Direct3D11 hardware decode, direct output**: video is rendered by
  libvlc itself into a native child window (`--avcodec-hw=d3d11va`,
  `--vout=direct3d11`); danmaku are painted by a separate transparent OpenGL
  overlay that follows the main window. Playback CPU dropped from ~210% to
  ~11% on the dev machine
- **Modern UI**: a single auto-hiding bottom bar (play/pause, time, seek
  strip, danmaku toggle, speed, volume, open, settings, fullscreen, playlist);
  the playlist panel deliberately never steals keyboard focus
- **Player conveniences**: 0–9 percentage jump, ±10 s / ±60 s skips, wheel
  volume, mute memory, hold-right-button 3× speed, resume-where-you-left-off,
  single instance with argument forwarding
- **Online features removed** — the 2013–2016 danmaku servers are long dead;
  only local danmaku files load now
- **Single-platform consolidation**: the vmem (CPU-copy) rendering line, the
  QtMultimedia / dummy player backends, the Raster and Quick2 outputs, and
  the linux / macx / android configs were removed. Everything is preserved in
  git history; tag `v0.5.0-vmem` marks the last pre-rewrite vmem state
- Proper i18n (English sources + complete zh_CN / zh_TW) and roughly 5,700
  lines of dead code stripped

## Danmaku

- Formats (auto-detected): Bilibili XML (incl. UTF-8 BOM), AcFun JSON,
  Niconico, AcFunLocalizer, ASS
- Auto-association: open `video.flv`, a `video.xml` / `video.json` next to it
  is mounted automatically
- Collision-free layout computed on worker threads; per-screen and repeat
  limits; shielding by type / sender / regex; per-source delay calibration
  with a visual density track editor

## Build (Windows x64)

Toolchain used by this fork:

- Qt 5.15.2 (mingw81_64)
- MinGW 8.1.0 (x86_64-w64-mingw32)
- VLC 3.0.21 — SDK (headers + import libs) at build time, full runtime
  (libvlc, libvlccore, plugins) at run time

```bat
set PATH=<mingw>\bin;<qt>\bin;%PATH%
qmake BiliLocal.pro THIRDPARTY=<vlc-sdk-root> CONFIG+=release
mingw32-make -j8 release
```

Sources with Chinese string literals must stay UTF-8 with BOM. Runtime needs
Qt5 Core/Gui/Widgets/Network/Concurrent, the VLC runtime, the VC runtime
(vcruntime140 etc.), and `locale/<lang>/bililocal.qm` for translations
(regenerate with `lupdate` / `lrelease` after changing UI strings).

## Lineage

- Upstream: `AncientLysine/BiliLocal` @ `9d1cb05` (0.4.2, 2016)
- `v0.5.0-vmem` — rollback point of the original vmem rendering line, before
  the VLC 3 / d3d11 direct-output rewrite

## License

GPL-3.0, see [COPYING](COPYING). All credit for the original player goes to
Lysine (AncientLysine). This fork's changes are documented commit by commit.
