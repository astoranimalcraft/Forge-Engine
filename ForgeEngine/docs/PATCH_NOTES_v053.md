# DX12 Forge Engine v0.5.3 — Editor, project workflow and rendering

## Rendering and sky

- Procedural sky uses a blue zenith-to-horizon gradient, time-of-day tint, an antialiased sun disc, a restrained atmospheric halo and animated, layered procedural cloud noise.
- Sky colors are authored for the HDR scene target and passed through the post-processing tone mapper. If a monitor or project shader configuration still appears clipped, adjust `Exposure` in `Window → Post Processing`.
- The post-processing pass reads a typeless depth buffer through an `R32_FLOAT` SRV and provides a compact screen-space AO approximation, bloom, camera-motion blur, chromatic aberration, sharpen, saturation, exposure, vignette and ACES-style tone mapping.
- AO and bloom are screen-space approximations, not full multi-pass/temporal versions of SSAO/HBAO or Unreal Lumen. Motion blur uses a camera-motion vector rather than a per-object temporal velocity buffer.

## Startup and project hub

- The engine shows a native dark loading splash while DirectX 12 devices, descriptors, render targets, shaders and editor infrastructure are initialized.
- The Project Hub offers New Project, Open Project and recent project cards with a generated illustration and creation date.
- Opening a project displays a second progress splash, creates the standard `Assets`, `Config`, `Scripts`, `Plugins` and `Presets` folders, restores `Assets/Scenes/Default.forgeScene` where present, reloads saved object data and model asset paths, and uses `Config/EditorLayout.ini` for dock state.
- The first-time project scripts use `Scripts/ForgeGameScripts.csproj`. The build system rewrites its `ForgeEngine.API` `ProjectReference` to the actual engine checkout path before running `dotnet build`, so projects outside the engine directory can compile without a stale relative reference.

## Editor interaction

- The window uses a frameless client area with a custom ImGui caption containing minimize, maximize/restore and close buttons; its blank area can drag the window, and the resize frame remains native.
- Project Files includes a recursive folder tree, New Folder/New Subfolder and Rename for files/folders.
- Scene Hierarchy is a recursive tree with drag-and-drop parenting, parent/unparent context actions, duplicate/delete actions and cycle checks. Parent relationships are stored in the scene format. **At this version, parenting organizes the hierarchy but does not yet apply inherited parent transforms to child world transforms.**
- Scene format v5 saves parent indices, model asset paths, script components and terrain height values, while the loader still accepts scene versions 1–4.

## Build and validation

Required: Windows 10/11 x64, Visual Studio/MSVC with the Windows SDK, CMake 3.21+, Git, Ninja Multi-Config, and the .NET 8 SDK for managed scripts. Use the `windows-ninja` CMake preset and then `cmake --build --preset debug`.

This package was reviewed with static source checks in a non-Windows environment. It was **not** compiled with MSVC/Windows SDK, D3DCompile, or the .NET SDK in this session. A Windows build and runtime check are still required before considering these modifications verified.
