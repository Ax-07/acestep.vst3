# ACE-Step VST3 Shell

A JUCE/CMake VST3 for generating and auditioning ACE-Step 1.5 audio inside a DAW.
The plugin connects to the **Python ACE-Step 1.5 REST API** (typically
`http://127.0.0.1:8001`) for generation, task polling, preview downloads, and
backend health. Keep the API server running while using the plugin.

The editor stores backend URL, prompt/lyrics, generation parameters, and result
selection in the DAW project. Generated audio is previewed in the plugin; it is
**not automatically inserted** as a timeline clip.

## Ableton workflow

1. Launch the Python ACE-Step 1.5 API server and open the VST3 in Ableton.
2. Set **Signal Path** to `http://127.0.0.1:8001`.
3. Generate audio and wait for a completed take in **Result Deck**.
4. **Press and drag** the **Drag WAV to Track** button onto an Ableton audio track.
   A simple click does not open anything; use the separate **Reveal** button
   to locate the file in Explorer/Finder.
5. If the native drag is rejected by your host, use **Reveal** in Preview Deck
   and drag the file from Explorer/Finder. This is a host-dependent fallback.

The label says WAV because the plugin requests WAV output. The downloaded file
keeps the audio format indicated by the backend.

## BPM sync and bar-length loops

In **Compose / Engine**, select **Sync BPM to DAW** to read Live's tempo and
time signature (via JUCE's host playhead). The host value appears under the
Lyrics field. The setting is opt-in so older sessions and standalone uses keep
working without a host BPM.

Choose **Length** in seconds (10 / 30 / 60 / 120) or select **4 / 8 / 16 /
32 bars (DAW BPM)**. Selecting bars automatically enables BPM sync.
The BPM and musical length are captured when **Generate** is pressed and
are sent to ACE-Step's Python `/release_task` API as `bpm`,
`time_signature` (for supported signatures), and `audio_duration`.

- Duration calculation: `bars * numerator * (4 / denominator) * 60 / BPM`.
  For example at **128 BPM in 4/4**, 4 / 8 / 16 / 32 bars mean
  **7.5 / 15 / 30 / 60 seconds**.
- ACE-Step requires **at least 10 seconds** of generation. For short
  loops, the plugin requests 10 seconds, then writes a separate WAV
  matching the selected number of bars exactly (at the source sample rate).
- In bar mode, the original full-length file **and** the exact-length
  `*-4bars.wav`, `*-8bars.wav`, etc. are kept in the exports folder.
  Preview and **Drag WAV to Track** use the bar-length file.
- A small 5 ms fade is applied at the start and end of a bar-length export
  to reduce clicks. **The model is not guaranteed to compose a seamless
  musical loop**: you may still need Warp, crossfades, or editing in Ableton.
- If the host does not provide tempo, start Ableton's transport. ACE-Step
  supports only BPM values in the 30-300 range. Unsupported time signatures
  still determine the WAV length but are not supplied as metadata to ACE-Step.

## Saved audio and export folder

Downloaded generations are saved permanently, by default, to:

- Windows: the user's **Music/ACE-Step/Exports** folder
- macOS/Linux: the equivalent user music folder

Files use unique timestamps and take numbers, so new generations do not
overwrite previous ones. The **Reveal** button opens the actual saved file.

To use a custom export directory, define the `ACESTEP_EXPORT_DIR` environment
variable **before starting your DAW**. For example, in PowerShell on Windows:

```powershell
[Environment]::SetEnvironmentVariable(
    "ACESTEP_EXPORT_DIR",
    "C:\AI\ACE-Step-Exports",
    "User"
)
```

Restart Ableton after changing the variable. The folder will be created on the
first successful download. If it cannot be created or written, the plugin
reports an error rather than silently writing to a temporary location.

## Build

Requirements: CMake 3.22+, a C++20 compiler, and network access to fetch JUCE
during first CMake configuration.

```bash
cmake -S plugins/acestep_vst3 -B build-vst3
cmake --build build-vst3 --config Release
```

On Windows use a supported Visual Studio generator/toolchain; on macOS use
Xcode or Ninja. The output bundle is under the `acestep_vst3_artefacts`
directory inside your build tree. Copy the **entire** `.vst3` bundle into your
DAW's VST3 plugin folder after closing the DAW.

## Validation

- Plugin opens and the configured API server reports healthy.
- Generating a take downloads playable audio to the exports folder.
- Click **Drag WAV to Track** -> no action; use **Reveal** to open the saved file.
- Press-and-drag **Drag WAV to Track** -> a native file drag starts.
- Drop the file onto a DAW audio track; if rejected, use Reveal + OS drag.
- Save/reopen the DAW project and verify plugin state is restored.

Native drag/drop may differ across DAWs and operating systems. The plugin does
not use private Ableton APIs to create arrangement clips programmatically.
