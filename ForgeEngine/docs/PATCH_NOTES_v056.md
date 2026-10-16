# Forge Engine v0.5.6 — Sky, Camera Viewport, Project Panel Fixes

## Fixed

### C# scripting compilation error (ScriptHost)
- `Application.isPlaying` had an `internal` setter that the separate `ForgeScriptHost`
  assembly could not assign, failing the build with `CS0200` twice. The setter is now
  `public` and `dotnet build` of `ForgeEngine.API`, `ForgeScriptHost` and
  `ForgeGameScripts` passes with 0 errors.
- Protocol parsing bug: the `OBJ|...` line carries 26 fields (the last three are
  `hasCollider`, `hasAudioSource` and `hasCamera`), but the host accepted lines with
  only 23 fields and silently defaulted those three flags to `false`. Scripts never
  saw colliders, audio sources or cameras attached in the editor. The host now
  requires the full 26-field line.

### Project Files panel flickering
- `ImGui::PushID(const char*)` keeps the pointer, it does not copy the string.
  Three call sites passed a temporary from `path.wstring().c_str()` that is destroyed
  at the end of the statement, so every row used a dangling ID seed. Item IDs changed
  between frames, which collapsed and re-expanded the folder tree and re-selected the
  asset table: the visible "blinking" of the Project explorer.
  The folder tree, asset grid and project-hub cards now keep the seed string alive
  for the full `PushID`/`PopID` scope.

## New

### Recreated procedural sky (UE5-style)
- `ProceduralSky()` in `Shaders.hlsl` is rebuilt around a three-band blue gradient
  (deep azure zenith -> cobalt mid -> pale cyan horizon) with a dim blue haze below
  the horizon, replacing the previous washed-out near-white dome.
- Two procedural cloud layers: a domain-warped cumulus deck with eroded soft edges,
  sun-facing silver lining and blue-grey shaded undersides, plus an anisotropic
  cirrus veil streaked across the upper sky. Both layers drift with time.
- Twilight band, sun disc, Mie halo, stars and the light glow lobes are preserved and
  tint to match the new palette.
- New live controls in Lighting > "Procedural Sky & Clouds": cloud coverage,
  cloud altitude and wind speed. They travel in the new `skyParameters` row of the
  frame constant buffer (`FrameConstants` and `FrameData` stay in lockstep).

### Camera viewport on selection
- Selecting a Camera object (hierarchy, click in Scene, or Create > Camera) now
  switches the Scene viewport to that camera immediately, Unreal-style. Selecting a
  non-camera object returns to the free editor camera. "Exit Camera View" in the
  Camera Preview panel still detaches manually.

### New features
- PNG screenshot of the game viewport (File > Save Screenshot, or F12). The back
  buffer is copied before the editor overlay is drawn, encoded with WIC and saved to
  `<Project>/Assets/Misc/Screenshots/forge_YYYYMMDD_HHMMSS.png`. The result path is
  logged to the Console.
- Stats overlay window (Window > Stats Overlay, or F3): smoothed frame time/FPS,
  object and renderer counts, triangle count, viewport size, active camera mode and
  in-scene clock.
- Hierarchy search box: filters Scene objects by name, keeping parents of matching
  children visible.

## Validation
- `dotnet build`: 0 errors for all three C# projects.
- `fxc /Tps_5_0 PSSkyGrid|PSPBR|PSPostProcess`: shaders compile cleanly.
- Full MSVC/DirectX 12 editor build via CMake (see build output in `out/build/vs`).
