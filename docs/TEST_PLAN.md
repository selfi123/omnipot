# PotPlayer Native Linux Test Plan & Verification Procedures

This document outlines automated and manual verification procedures for validating feature parity between the Native Linux player and the reference Kakao PotPlayer.

---

## 1. Automated Testing Strategy (`tests/`)

Automated tests are built with **Qt Test** (`QTest`) and execute automatically via `ctest`.

### Test Suites

1. **`test_playback_engine`**:
   - Tests `PlaybackEngine` initialization and shutdown without resource leaks.
   - Tests state transitions: `Idle` -> `Opening` -> `Playing` -> `Paused` -> `Stopped`.
   - Tests seek command calculation and boundary clamp logic (negative seek, seek past duration).
   - Tests volume clamping (0 to 100%, boost up to 200%) and mute toggle.
   - Tests speed multipliers (0.2x to 12.0x).

2. **`test_playlist`**:
   - Tests Daum Playlist (`.dpl`) parser against reference `.dpl` files.
   - Tests `.dpl` file serialization round-trip fidelity.
   - Tests M3U/M3U8 parser and UTF-8 handling.
   - Tests playlist navigation: `next()`, `previous()`, repeat modes (all, one, none), shuffle.

3. **`test_config`**:
   - Tests `QSettings` serialization to `config.ini`.
   - Tests default fallback behavior when configuration is missing.
   - Tests recent history persistence and eviction policy (max 30 items).

---

## 2. Manual Verification Workflows

### Manual Test Matrix

| Test ID | Procedure | Expected Reference Result | Native Linux Observed Result | Pass/Fail |
| :--- | :--- | :--- | :--- | :--- |
| TC-PLAY-01 | Launch player with `/home/ni8stalker/Downloads/Finding.Emily.2026.1080p.mp4` | Video renders immediately, audio plays in sync, duration displayed | Pending M1 build | - |
| TC-PLAY-02 | Press `Space` during playback | Playback pauses instantly; pressing again resumes | Pending M1 build | - |
| TC-SEEK-01 | Press `Right Arrow` | Video jumps forward by 5 seconds; time label and seek bar update | Pending M1 build | - |
| TC-SEEK-02 | Click directly on the timeline `TimeBar` | Playback immediately seeks to clicked position without audio glitch | Pending M1 build | - |
| TC-VOL-01 | Scroll mouse wheel up/down over video | Volume changes in 2% steps; volume slider updates | Pending M1 build | - |
| TC-FULL-01 | Press `Enter` or double-click video | Window transitions smoothly to borderless fullscreen; controls autohide | Pending M1 build | - |
| TC-SPEED-01| Press `C` twice then `X` | Speed becomes 1.2x then 1.1x with pitch compensation | Pending M2 build | - |
| TC-PL-01 | Drag `.dpl` file into playlist drawer | Playlist loads all items with accurate titles and durations | Pending M2 build | - |
| TC-CTRL-01 | Press `F6` | Control Box opens showing Playback, Video, Audio, Subtitle tabs | Pending M3 build | - |
| TC-OSD-01 | Press `Tab` or `Ctrl+F1` | Real-time stream statistics overlay appears on video canvas | Pending M2 build | - |

---

## 3. Regression & Edge Case Testing

1. **Nonexistent / Inaccessible File**:
   - Pass invalid path on command line.
   - Player must display an error OSD/dialog and return cleanly to idle state without crashing.
2. **Unsupported Codec / Corrupt Header**:
   - Provide file with random noise or corrupt container header.
   - libmpv must fire `MPV_EVENT_END_FILE` with error status; player must notify user and allow opening another file.
3. **Rapid Seeking**:
   - Scrub seek slider vigorously.
   - Playback engine must drop outdated seek events and seek smoothly to final position without freeze or audio desync.
4. **Window Resizing & HiDPI**:
   - Resize window rapidly from minimum size to 4K monitor.
   - Aspect ratio must be preserved, video surface must not flicker or exhibit artifacts.
