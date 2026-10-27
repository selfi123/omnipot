# PotPlayer Native Linux Feature-Parity Matrix

Status legend:
- `DISCOVERED`: Feature identified in PotPlayer reference (via binary inspection, menus, skins, language files, or documentation).
- `PLANNED`: Feature specified and scheduled in architecture/milestones.
- `IMPLEMENTED`: Feature code written and integrated into the native Linux player.
- `TESTED`: Feature verified and tested against test media or automated test suite.
- `BLOCKED`: Feature temporarily impeded by platform or legal dependency constraints (documented with technical limitation).

---

## Category A: Core Playback

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| CORE-01 | Audio & Video Formats | MP4, MKV, AVI, WEBM, FLV, TS, MP3, FLAC, AAC, OGG, OPUS, WAV | libmpv / FFmpeg demuxers and decoders | `PLANNED` | Validated against test sample `Finding.Emily.2026.1080p.mp4` |
| CORE-02 | Play / Pause | Spacebar or click toggles play/pause with OSD indicator | `mpv_command_async("cycle", "pause")` + Qt widget sync | `PLANNED` | Milestone 1 Foundation |
| CORE-03 | Stop & Close Media | Stop button stops playback and unloads media container | `mpv_command_async("stop")` + clear video surface | `PLANNED` | Milestone 1 Foundation |
| CORE-04 | Time Seeking (Absolute & Relative) | Left/Right (5s), Ctrl+Left/Right (30s), Shift (1m), Ctrl+Alt (5m) | `mpv_command_async("seek", ...)` with exact and keyframe modes | `PLANNED` | Milestone 1 Foundation |
| CORE-05 | Frame Stepping | 'F' steps one frame forward, 'D' steps one frame backward | `mpv_command("frame-step")` and `mpv_command("frame-back-step")` | `PLANNED` | Milestone 1 Foundation |
| CORE-06 | Previous / Next File in Playlist | PageUp / PageDown or dedicated toolbar buttons | Playlist controller navigation + auto next on EOF | `PLANNED` | Milestone 1 Foundation |
| CORE-07 | Playback Speed Adjustment | 'C' increases (+0.1x), 'X' decreases (-0.1x), 'Z' resets to 1.0x | `mpv_set_property("speed", ...)` (0.2x to 12.0x) + audio pitch correction | `PLANNED` | Milestone 2 |
| CORE-08 | Fullscreen Mode | 'Enter' or double-click video canvas toggles borderless fullscreen | `QMainWindow::showFullScreen()` / `showNormal()` | `PLANNED` | Milestone 1 Foundation |
| CORE-09 | Aspect Ratio & Video Scaling | Keeps original ratio, 4:3, 16:9, 16:10, 2.35:1, custom stretch | `video-aspect-override` property in libmpv | `PLANNED` | Milestone 2 |
| CORE-10 | Video Rotation & Flipping | 90°, 180°, 270° rotation, horizontal and vertical flip | `video-rotate`, `video-hflip`, `video-vflip` properties | `PLANNED` | Milestone 3 |
| CORE-11 | A-B Repeat Looping | '[' sets point A, ']' sets point B, '\' clears loop | `ab-loop-a` and `ab-loop-b` properties | `PLANNED` | Milestone 3 |
| CORE-12 | Playback History & Resume | Remembers playback position per file upon reopening | Local SQLite / JSON state storage in `~/.config/potplayer-linux/history.json` | `PLANNED` | Milestone 2 |
| CORE-13 | Drag-and-Drop Playback | Dragging media files or URLs onto the player window opens them | Qt `dragEnterEvent` and `dropEvent` handling | `PLANNED` | Milestone 2 |
| CORE-14 | Command-Line File & URL Open | `potplayer [file/url]` parses CLI arguments and starts playback | `QCommandLineParser` with `CmdLine64.txt` compatibility | `PLANNED` | Milestone 1 Foundation |
| CORE-15 | Multiple Instances vs Single Instance | Settings switch between single instance or allowing multiple instances | `QLocalServer` / `QLocalSocket` IPC for single-instance IPC | `PLANNED` | Milestone 2 |
| CORE-16 | Error Recovery | Gracefully handles corrupted files, missing codecs, or EOF | `MPV_EVENT_END_FILE` error checking and OSD alerting | `PLANNED` | Milestone 1 Foundation |

---

## Category B: Video Rendering & Processing

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| VID-01 | Hardware Decoding | Supports DXVA2, D3D11, NVDEC, Intel QuickSync | `hwdec=auto-safe` (VAAPI on Intel/AMD, NVDEC on NVIDIA) | `PLANNED` | Milestone 3 |
| VID-02 | GPU Renderer Selection | Direct3D 9, 11, 12, OpenGL, Vulkan | `vo=gpu` / `vo=gpu-next` over OpenGL/Vulkan Qt render contexts | `PLANNED` | Milestone 3 |
| VID-03 | Video Scaling Algorithms | Bilinear, Bicubic, Lanczos, Spline36 | `scale=spline36`, `cscale=spline36`, `dscale=mitchell` | `PLANNED` | Milestone 3 |
| VID-04 | Color & ProcAmp Adjustments | Brightness, Contrast, Saturation, Hue sliders in Control Box | `brightness`, `contrast`, `saturation`, `hue`, `gamma` properties | `PLANNED` | Milestone 3 |
| VID-05 | Deinterlacing | Auto / Yadif / BwDif deinterlacing | `deinterlace=yes/no` with `bwdif` filter | `PLANNED` | Milestone 3 |
| VID-06 | Pixel Shaders & Post-processing | 34 HLSL shaders in `PxShader/` (Adaptive Sharpen, Contour, etc.) | Custom GLSL / mpv user shader pipeline (`glsl-shaders`) | `PLANNED` | Milestone 4 |
| VID-07 | Frame Capture (Screenshot) | Ctrl+E saves screenshot, Ctrl+C copies current frame to clipboard | `screenshot-raw` / `screenshot-to-file` + `QClipboard::setImage` | `PLANNED` | Milestone 4 |
| VID-08 | Playback Info & OSD Stats | Tab or Ctrl+F1 displays detailed real-time stream information | Custom OSD overlay reading codec, resolution, FPS, bitrate, drops | `PLANNED` | Milestone 2 |
| VID-09 | Multi-Monitor & DPI Scaling | Remembers monitor position and respects HiDPI scale factors | Qt 6 automatic Wayland/X11 high-DPI scaling | `PLANNED` | Milestone 1 Foundation |

---

## Category C: Audio

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| AUD-01 | Audio Track Selection | Switch between multiple embedded audio streams | `aid` property cycle and enumeration | `PLANNED` | Milestone 2 |
| AUD-02 | Audio Device Selection | Select audio output device (speakers, HDMI, headphones) | `audio-device` enumeration via PipeWire / PulseAudio | `PLANNED` | Milestone 3 |
| AUD-03 | Volume & Mute Control | Volume slider (0-100%, boost up to 200%), 'M' toggles mute | `volume` and `mute` properties in libmpv | `PLANNED` | Milestone 1 Foundation |
| AUD-04 | Audio Delay Synchronization | Shift+< / Shift+> shifts audio delay in 50ms steps | `audio-delay` property adjustment | `PLANNED` | Milestone 3 |
| AUD-05 | Channel Downmixing | 2.0 Stereo, 5.1 Surround, 7.1 Surround downmixing | `audio-channels` configuration | `PLANNED` | Milestone 3 |
| AUD-06 | 10-Band Equalizer | Equalizer panel in Control Box (F6) with presets | `af=equalizer=...` filter chain in libmpv | `PLANNED` | Milestone 3 |
| AUD-07 | Audio Normalization & ReplayGain | Normalizes audio loudness spikes, supports album/track gain | `replaygain` and `loudnorm` audio filters | `PLANNED` | Milestone 3 |
| AUD-08 | External Audio Track Loading | `/aud="file"` or context menu loads external audio file | `audio-add` command in libmpv | `PLANNED` | Milestone 2 |
| AUD-09 | Sound Spectrum Visualization | Built-in sound spectrum visualizer during audio-only playback | Qt QPainter spectrum visualizer from audio FFT data | `PLANNED` | Milestone 4 |

---

## Category D: Subtitles

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| SUB-01 | Embedded & External Subtitles | SRT, ASS, SSA, VTT, SUB, PGS, VobSub formats | libass integration via libmpv | `PLANNED` | Milestone 2 |
| SUB-02 | Subtitle Track Selection | 'L' cycles subtitle tracks or select from Subtitle menu | `sid` property cycle and enumeration | `PLANNED` | Milestone 2 |
| SUB-03 | Subtitle Auto-Discovery | Auto-loads subtitle files with matching basename | `sub-auto=fuzzy` in libmpv | `PLANNED` | Milestone 2 |
| SUB-04 | Subtitle Delay Synchronization | '<' and '>' adjusts subtitle delay in 0.5s steps | `sub-delay` property in libmpv | `PLANNED` | Milestone 2 |
| SUB-05 | Font, Size & Styling | Adjust font family, font size, bold, border, shadow | `sub-font`, `sub-font-size`, `sub-border-size`, `sub-color` | `PLANNED` | Milestone 3 |
| SUB-06 | Subtitle Vertical Positioning | Alt+Up / Alt+Down moves subtitles up/down the screen | `sub-pos` property in libmpv | `PLANNED` | Milestone 3 |
| SUB-07 | ASS/SSA Override vs Raw Style | Toggle between author ASS styles and user default styles | `sub-ass-override` property in libmpv | `PLANNED` | Milestone 3 |
| SUB-08 | Online Subtitle Search | OpenSubtitles, SubDL, Podnapisi plugins | Native C++ REST client for OpenSubtitles API / SubDL | `PLANNED` | Milestone 4 |

---

## Category E: Playlists & Media Library

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| PL-01 | Add, Remove, Clear Items | Interactive playlist UI for managing playback items | `QAbstractListModel` backing playlist drawer | `PLANNED` | Milestone 2 |
| PL-02 | Daum Playlist (`.dpl`) Support | Load and save native PotPlayer `.dpl` playlist format | Dedicated `DplPlaylistParser` and serializer | `PLANNED` | Milestone 2 |
| PL-03 | M3U / M3U8 Playlist Support | Load and save standard M3U/M3U8 playlists | Custom M3U parser and serializer | `PLANNED` | Milestone 2 |
| PL-04 | Shuffle & Repeat Modes | Repeat None, Repeat Track, Repeat All, Shuffle playback | Playlist queue manager with loop modes | `PLANNED` | Milestone 2 |
| PL-05 | Recent Media & Resume History | Recents menu and automatic position restoration | SQLite / JSON cache storing file URIs and timestamps | `PLANNED` | Milestone 2 |
| PL-06 | Directory & Recursive Import | Adding a folder recursively parses all media files | Recursive directory scanner with media file filters | `PLANNED` | Milestone 2 |
| PL-07 | Playlist Filtering & Search | Filter bar at top of playlist panel for instant search | `QSortFilterProxyModel` connected to search edit | `PLANNED` | Milestone 2 |
| PL-08 | Playlist Persistence | Remembers active playlist across application restarts | Persists `default.dpl` in application config folder | `PLANNED` | Milestone 2 |

---

## Category F: Streaming & External Sources

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| STR-01 | HTTP / HTTPS Media Streams | Directly opens URLs pointing to audio/video files | libmpv network streaming / ffmpeg protocols | `PLANNED` | Milestone 3 |
| STR-02 | HLS & DASH Streaming | Plays `.m3u8` HLS streams and `.mpd` DASH manifests | libmpv built-in demuxers for HLS/DASH | `PLANNED` | Milestone 3 |
| STR-03 | Custom HTTP Headers & User-Agent | CLI switches `/user_agent=`, `/referer=`, `/headers=` | `user-agent`, `referrer`, `http-header-fields` properties | `PLANNED` | Milestone 3 |
| STR-04 | Streaming Stdin Pipe (`-`) | `cat file | potplayer -` streams video from stdin | `mpv_set_option_string("input-ipc-server", ...)` / `fd://0` | `PLANNED` | Milestone 3 |
| STR-05 | YouTube & Video Site Playback | Built-in AngelScript YouTube parser | Native `yt-dlp` hook integration | `PLANNED` | Milestone 4 |

---

## Category G: Chapters, Bookmarks & Navigation

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| NAV-01 | Chapter Navigation | Next/previous chapter via shortcuts and Chapter menu | `chapter` property and `chapter-list` enumeration | `PLANNED` | Milestone 3 |
| NAV-02 | Bookmarks System | 'P' adds bookmark, bookmark list in playlist drawer | JSON bookmark database mapped to file hash/path | `PLANNED` | Milestone 3 |
| NAV-03 | Embedded Container Metadata | Displays Title, Artist, Album, Year, Track info | `metadata` dictionary inspection via libmpv | `PLANNED` | Milestone 2 |
| NAV-04 | DVD / ISO Playback | Open DVD disc / ISO folder | libmpv `dvd://` and `dvdnav://` protocols | `BLOCKED` | Dependent on optional `libdvdnav` on Fedora |

---

## Category H: Capture & Media Utilities

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| CAP-01 | Still Frame Capture | Screenshot to PNG/JPG/WebP with configurable filename | `screenshot-to-file` command | `PLANNED` | Milestone 4 |
| CAP-02 | Consecutive Image Sequences | Capture N frames at specified intervals | Automated timer-based screenshot series | `PLANNED` | Milestone 4 |
| CAP-03 | Clipboard Frame Copy | Ctrl+C copies current frame to system clipboard | `screenshot-raw` -> `QImage` -> `QClipboard` | `PLANNED` | Milestone 4 |
| CAP-04 | Audio Stream Extraction | Extract audio track to MP3, AAC, FLAC | FFmpeg CLI or libavformat transcode pipe | `PLANNED` | Milestone 4 |

---

## Category I: Input, Controls & Desktop Integration

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| INP-01 | Complete Keyboard Shortcuts | Space, Enter, Arrows, M, Z/X/C, D/F, Tab, F5, F6, F8 | Qt `keyPressEvent` action dispatch system | `PLANNED` | Milestone 1 Foundation |
| INP-02 | Mouse Click & Wheel Controls | Left click play/pause, Wheel volume +/- 2%, Double click fullscreen | Qt `mousePressEvent` and `wheelEvent` handlers | `PLANNED` | Milestone 1 Foundation |
| INP-03 | Context Menu Hierarchy | Comprehensive right-click menu matching PotPlayer | `QMenu` constructed from hierarchical action tree | `PLANNED` | Milestone 1 Foundation |
| INP-04 | Interactive Seek Bar | Hover time tooltip, scrubbing, click-to-seek, chapter marks | Custom `QSlider` subclass (`TimeBar`) | `PLANNED` | Milestone 1 Foundation |
| INP-05 | Control Box (Quick Settings) | F6 floating panel for Video, Audio, Subtitle, Playback, Color | Dedicated `ControlBoxDialog` with synchronized controls | `PLANNED` | Milestone 2 |
| INP-06 | Wayland & GNOME Integration | Desktop file, window title updates, system dark theme | `.desktop` file, MPRIS D-Bus integration, Wayland support | `PLANNED` | Milestone 2 |

---

## Category J: Configuration & Extensibility

| Feature ID | Feature Name | PotPlayer Reference Behavior | Native Linux Implementation Strategy | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| CFG-01 | Persistent Configuration | Saves all settings in INI format across sessions | `QSettings` INI format at `~/.config/potplayer-linux/config.ini` | `PLANNED` | Milestone 2 |
| CFG-02 | Comprehensive Preferences (F5) | Multi-page tree preferences dialog | `PreferencesDialog` with tree navigation | `PLANNED` | Milestone 3 |
| CFG-03 | Reset to Default Settings | One-click button to reset all options to defaults | Config file re-initialization | `PLANNED` | Milestone 3 |
| CFG-04 | Export / Import Presets | Export `.ini` settings files and load them | INI file import/export utility | `PLANNED` | Milestone 3 |
| CFG-05 | Logging & Diagnostics | Log window showing mpv/ffmpeg diagnostics and errors | `mpv_request_log_messages` -> Diagnostics dialog | `PLANNED` | Milestone 3 |
