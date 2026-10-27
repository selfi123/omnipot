# Reverse Engineering & Behavioral Analysis of PotPlayer

## 1. Reference Binary Identity & Environment

The behavioral reference application is the 64-bit Windows release of Kakao PotPlayer installed under Wine on Fedora 44 Workstation (x86_64).

- **Reference Installation Directory**: `/home/ni8stalker/.wine/drive_c/Program Files/DAUM/PotPlayer`
- **Main Executable**: `PotPlayerMini64.exe`
- **Full Player Executable**: `PotPlayer64.exe`
- **Runtime Environment**: Wine Staging 11.0 on Fedora 44 (Workstation Edition), GNOME, Wayland/XWayland, Linux kernel 6.x/7.x.

### Reference Binary Hashes (SHA-256)

| Binary File | Size (Bytes) | SHA-256 Digest | Role / Purpose |
| :--- | :--- | :--- | :--- |
| `PotPlayerMini64.exe` | 2,515,984 | `6f0f63bee95d6715e0656f8b0bcbcce0cc3b27f03f012c8b8c4c34557a2e1cb7` | Primary lightweight playback GUI executable |
| `PotPlayer64.exe` | 2,551,824 | `87826627635c5db59d9cae2516e611ee352415ac419a6049ca4ccd0338f97cf0` | Full player executable (with broadcast/live integration) |
| `PotPlayer64.dll` | 35,302,432 | `4375fbe42132b1e5c36495aa1ae570e6a9d15dfa6acffe90beb5a6929fe99d9f` | Core player engine, filter manager, UI framework |
| `ffcodec64.dll` | 38,105,088 | `33af2edefdc92fced584b4f9ad411a18498eaa8ddbf6975b0156a39e44b199cc` | Internal FFmpeg-based video and audio codec bundle |
| `ATextOut64.dll` | 2,505,744 | `de32908cf5370035944ce8ed7a8c39a399766dc3bd46d544988c66c7b600133a` | Subtitle rendering and rasterization engine |
| `MediaDB64.dll` | 6,686,736 | `e0230b9dc717ab0dc4d35e5ae30778eb189cec012b97998aeba3a0b34cd9ed17` | Media database and metadata caching (MediaInfo.sdb) |
| `KakaoUtil64.dll` | 3,289,616 | `716b867b05fcd67551c4136b5cc15a9a345bdcfb52784799c8cf6c002ef8369f` | Kakao network utilities and updater integration |
| `PotIcons64.dll` | 2,523,152 | `94cf9b2f2e4f0cb6bc0e201eb46a02af050df2829c1a4047e11b8e5fa6c26d0c` | Shell filetype icon resource library |
| `PotScreenSaver64.scr`| 2,591,776 | `ac5f93619298f1bc81488f1b5c43a85edd1c10dee62248a7db693991f3c9a91d` | Screensaver module |
| `DesktopHook64.dll` | 97,808 | `401dbb44405af94728a0f302f1b5e500ebb2ea2ac3540a52efa4563b5b9ea805` | Global keyboard/mouse hook for hotkeys |
| `CaptureUWP64.dll` | 142,864 | `1fcfefd038d15312171b8ed92415138ef2b2ebce4f8961d40abdfef4c90bc66f` | Windows Graphics Capture / UWP screen capture |
| `DaumCrashHandler64.dll`| 189,456 | `ed5ca1e99615c579562f8fcdf89563478573230e59f51ba5fdc8b70c150c47a1` | Crash dump generation and reporting |
| `GameCaptureHook64.dll`| 435,216 | `72f5494925adf61307a8dc25c8264682ac66cde7f93d245fcefc8a7dfa339c43` | DirectX/Vulkan game capture injection hook |
| `WebView2Loader64.dll`| 165,968 | `462b36fd1be6ca9f7563466a89e57c41ef4a4def3e0a84fa885d203aea4a3aaf` | Microsoft Edge WebView2 loader for embedded browser |

---

## 2. Static Analysis & Ghidra / REA Findings

Analysis of `PotPlayerMini64.exe` was conducted using the integrated **REA MCP server** paired with **Ghidra 12.2** on **OpenJDK 25**.

### Binary Layout & Protection
- **Image Base**: `0x140000000`
- **Identified Segments**:
  - `Headers` (1,024 bytes)
  - `SECTION.0` through `SECTION.6` (Code and data stubs)
  - `.idata` (4,096 bytes)
  - `.rsrc` (155,648 bytes)
  - `.themida` (3,481,600 bytes)
  - `.boot` (2,269,184 bytes)
- **Observations & Caveats**:
  - The executable is packed with **Themida / WinLicense** anti-reverse-engineering software.
  - The static procedure count is reported as only 7 entry points, and the defined string inventory is only 58 items.
  - The real application logic is packed and decrypted into memory at runtime, or delegated to `PotPlayer64.dll` (35.3 MB).
  - Attempting to statically unpack or defeat Themida is neither necessary nor productive for our objective. We utilize **black-box behavioral observation, shipped resource extraction, protocol analysis, and configuration inspection** to fully reverse engineer the player's workflows.

---

## 3. Shipped Component & Module Architecture

Investigation of the files shipped with the PotPlayer installation reveals its functional decomposition:

### A. Demuxing & Decoding Subsystem
- `ffcodec64.dll` (38.1 MB) and `Module/FFmpeg63/FFmpeg64.dll` (20.1 MB):
  PotPlayer relies heavily on an internal build of **FFmpeg** for demuxing media containers and software decoding video and audio codecs.
- `Module/Bass64/`:
  The BASS audio library (`bass.dll`) along with specialized audio add-ons:
  - `bass_flac.dll`: FLAC decoding
  - `bass_ape.dll`: Monkey's Audio (APE) decoding
  - `bass_mpc.dll`: Musepack decoding
  - `bass_wv.dll`: WavPack decoding
  - `bass_tta.dll`: True Audio decoding
  - `bass_ofr.dll` and `OptimFROG.dll`: OptimFROG decoding
- `libmfxsw64.dll` and `QuickSync64.dll`:
  Intel Media SDK (QuickSync) hardware acceleration hooks.
- `Module/OpenCodec`:
  External DirectShow filter registration mechanism for additional proprietary codecs (e.g. CoreAVC, LAV Filters).

### B. Subtitle Subsystem
- `ATextOut64.dll`:
  PotPlayer's native text and subtitle rasterization engine. Supports GDI, FreeType, DirectWrite, and libass.
- `Module/Whisper`:
  Whisper AI speech-to-text integration for automatic subtitle generation from audio tracks.
- `Extension/Subtitle`:
  AngelScript (`.as`) scripts for online subtitle searching and translation:
  - Search: OpenSubtitles (`OpenSubtitle.as`), Podnapisi (`podnapisi.as`), SubDL (`SubDL.as`), Titlovi (`titlovi.as`), Ysubs (`ysubs.as`).
  - Translate: Google Translate (`google.as`), Bing Translator (`bing.as`).

### C. Pixel Shaders (`PxShader/`)
PotPlayer ships 34 HLSL pixel shaders that apply real-time post-processing to video surfaces:
- Sharpening: `Adaptive sharpen.txt`, `Adaptive sharpen(Contrast).txt`, `EdgeSharpen v1_1.txt`, `SharpenComplex2.txt`, `Sharpen_5x5.txt`, `Unsharp mask.txt`.
- Color/Levels: `BT601BT709.txt`, `Procamp.txt`, `Levels.txt`, `Remap_16_235.txt`, `GrayScale.txt`, `Invert.txt`.
- Geometry & Correction: `HorzFlip.txt`, `VertFlip.txt`, `Deinterlace (blend).txt`, `LCD angle correction.txt`, `Sphere.txt`.
- Effects: `Contour.txt`, `Denoise.txt`, `Emboss.txt`, `NightVision.txt`, `Spotlight.txt`, `Wave.txt`.

### D. Skin & UI Architecture (`Skins/Default.dsf`)
The skin package `Default.dsf` is a ZIP archive containing:
- `VideoSkin.xml`, `AudioSkin.xml`, `ChildSkin.xml`: XML layout tree describing window frames, anchors, margins, and control hierarchies.
- `BottomVideo.txt`: Control specification for playback buttons, seek bar, time display, codec badges (HW/SW, HDR, Video Codec FourCC, Audio Codec, Channels, Bitrate), volume slider, and auxiliary panel toggles.
- `TopCommon.txt` / `Title.txt`: Title bar, minimize/maximize/close buttons, system menu icon, window pinning (always on top).
- `Popup.txt`: On-screen control overlays and popup menus.
- `Main.png`, `Control.png`, `Slider.png`, `ScrollBar.bmp`: Atlas graphics for UI states (normal, hover, pushed, disabled).

### E. Scripting & Extension System (`Extension/api.txt`)
PotPlayer integrates an AngelScript engine allowing custom Lua/AngelScript scripts to extend the player:
- `HostGetAPIVersion()`, `HostGetVersion()`, `HostOpenConsole()`, `HostPrintUTF8()`.
- Media parsing: `MediaPlayParse - YouTube.as` parses YouTube URLs, extracts formats/streams, and feeds direct streaming links to the demuxer.
- URL lists: `Extension/Media/UrlList/`.

---

## 4. Behavioral Reverse Engineering

### Command-Line Arguments (`CmdLine64.txt`)
PotPlayer supports extensive command line parameters:
- Media opening: `PotPlayerMini64.exe "path/to/media"` (files, directories, URLs, wildcards `*`, `?`).
- Standard Input: `-` reads media stream from stdin (e.g. piped from yt-dlp).
- Playback control:
  - `/seek=hh:mm:ss.ms` or `/seek=seconds`: seek on start.
  - `/sub="subfile"`: load external subtitle.
  - `/aud="audiofile"`: load external audio track.
  - `/volume="num"`: set initial volume.
  - `/autoplay`: autoplay on launch.
- Playlist manipulation:
  - `/add`: append to playlist.
  - `/insert`: insert after currently playing track.
  - `/sort`: sort playlist alphabetically.
  - `/randomize`: shuffle playlist.
- Instance control:
  - `/new`: force new instance.
  - `/current`: reuse active instance.
- Streaming headers:
  - `/user_agent="ua"`, `/referer="ref"`, `/headers="hdrs"`.
- Dialogs:
  - `/filedlg`, `/urldlg`, `/folderdlg`, `/simple`.

### Playlist Storage (`.dpl` Daum Playlist Format)
Investigated at:
`/home/ni8stalker/.wine/drive_c/users/ni8stalker/AppData/Roaming/PotPlayerMini64/Playlist/PotPlayerMini64.dpl`

Format specification:
```ini
DAUMPLAYLIST
playname=<current_playing_file_path>
playtime=<playback_position_in_ms>
topindex=0
saveplaypos=0
<index>*file*<filepath_or_url>
<index>*title*<optional_custom_title>
<index>*duration2*<duration_in_ms>
<index>*start*<start_offset_in_ms>
```

### Keyboard Shortcuts & Workflow Reference

| Shortcut | Action | PotPlayer Reference Behavior |
| :--- | :--- | :--- |
| `Space` | Play / Pause | Toggles playback state; displays OSD play/pause glyph |
| `Enter` | Fullscreen | Toggles windowed / borderless fullscreen |
| `Left` / `Right` | Seek 5 seconds | Backward / Forward 5s seek with time OSD |
| `Ctrl + Left` / `Right` | Seek 30 seconds | Backward / Forward 30s seek |
| `Shift + Left` / `Right`| Seek 1 minute | Backward / Forward 60s seek |
| `Ctrl + Alt + Left/Right`| Seek 5 minutes | Backward / Forward 300s seek |
| `Up` / `Down` | Volume +/- 2% | Adjusts volume with percentage OSD |
| `M` | Mute | Toggles audio mute |
| `Z` | Speed normal | Resets playback rate to 1.0x |
| `X` | Speed down | Decreases speed by 0.1x (minimum 0.2x) |
| `C` | Speed up | Increases speed by 0.1x (maximum 12.0x) |
| `D` | Previous frame | Single frame back-step |
| `F` | Next frame | Single frame step forward |
| `[` | Repeat Point A | Sets A-B repeat start point |
| `]` | Repeat Point B | Sets A-B repeat end point and activates loop |
| `\` | Repeat Clear | Clears A-B repeat loop |
| `Ctrl + F1` or `Tab` | Playback OSD Stats | Shows detailed stream info, decoder, renderer, FPS, bitrate |
| `F5` | Preferences | Opens comprehensive configuration dialog |
| `F6` | Control Box | Opens quick controls (Video, Audio, Subtitle, Playback, Color) |
| `F8` | Playlist Drawer | Toggles docked right-hand playlist panel |
| `Ctrl + O` | Open File | File open dialog |
| `Ctrl + U` | Open URL | Network stream / URL open dialog |
| `Ctrl + C` | Frame Capture | Copies current video frame to system clipboard |
| `Ctrl + E` | Save Frame | Saves screenshot of current frame to disk |

---

## 5. Technical Limitations & Native Target Strategy

1. **DirectShow / Media Foundation Non-portability**:
   - The reference Windows PotPlayer uses DirectShow filter graphs (`EVR`, `VMR9`, `MadVR`) and Windows Media Foundation.
   - On Linux, these APIs do not exist. Attempting to replicate DirectShow on Linux leads to brittle, unmaintainable architecture.
   - **Resolution**: Use **libmpv** as the high-performance playback engine. libmpv provides a unified C API wrapping FFmpeg demuxers, hardware video decoders (VAAPI, NVDEC, Vulkan), libass subtitle rendering, and custom shader pipelines.
2. **Skin Engine Compatibility**:
   - PotPlayer's `.dsf` skins rely on Windows GDI/DirectX drawing with Win32 window styles (`WS_POPUP`, `SetWindowRgn`).
   - On Linux GNOME/Wayland, window decorations and compositing are managed by Wayland subsurfaces or X11 protocols.
   - **Resolution**: Implement native Qt 6 Widgets styled with a sleek, dark PotPlayer aesthetic, providing custom controls for the timeline, codec badges, control box, and playlist drawer.
