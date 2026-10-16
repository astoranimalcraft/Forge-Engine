# Patch Notes v0.5.10 — Forge Engine rename, sky/PBR, inspector & C# API, Unity camera

## Renomeação DX12ForgeEngine → ForgeEngine
- C++ class `DX12ForgeEngine` → `ForgeEngine`, editor `DX12ForgeEditorUI` → `ForgeEditorUI`.
- CMake target/project and the binary are now `ForgeEngine`.
- Project registry file renamed `DX12Forge.project` → `ForgeEngine.project` (legacy marker still accepted on load).
- Window chrome, splash, About box and menus no longer read "DX12 FORGE".
- Scene/post-process signatures accept the legacy `DX12FORGE_*` tags for compatibility.

## Céu procedural (gradiente azul + nuvens volumétricas)
- Single continuous elevation ramp (zenith → mid → horizon) instead of two hard bands, so the
  gradient no longer collapses into a flat white/grey horizon under ACES + gamma.
- Peak HDR luminance kept near ~3 so tone mapping preserves the azure hue.
- Mie-style forward-scatter horizon glow added.
- Cumulus deck now estimates a normal from the density signal, giving bright tops and dark,
  self-shadowed bases like UE5's volumetric skies; slate under-lit term prevents flattening to white.
- Aerial-perspective fade blends the deck into the atmosphere with distance.
- Simulated preview confirms the sky is blue in every direction (zenith #547FCB, mid #6090CB,
  horizon #88B8DE) and all HLSL entry points compile with DXC.

## PBR & texturas
- Emissive texture slot (register `t7`) plus `gEmissive`/`gMaterialFlags` constants.
- Emission is added after lighting so glowing materials keep their hue in HDR and pick up bloom.
- Authored AO strength (`occlusionStrength`) now scales the ambient occlusion term instead of
  being ignored.
- Normal map, metallic-roughness and AO persist through the scene format (v8) and emissive (v9).
- Second pipeline state for two-sided materials (`CULL_MODE_NONE`).

## Inspector & API C#
- "Add Component" popup with a Rendering submenu; the Material/PBR section hides when an object
  has no material (per-object `hasMaterial`).
- Camera inspector: orthographic, ortho size, near/far, depth, background color, draw skybox,
  draw gizmos and a per-layer culling mask with checkboxes.
- C# `Material` class (color, baseColor, metallic, roughness, emissiveColor, emissiveIntensity,
  normalMapScale, doubleSided, `EnableEmission`/`DisableEmission`).
- C# `Camera` is State-backed with `orthographic`, `orthographicSize`, `depth`, `backgroundColor`,
  `drawSkybox`, `drawGizmos`, `cullingMask`, `aspect`, `main`, `allCameras`,
  `WorldToViewportPoint`, `ViewportPointToRay`, `ScreenPointToRay`, `WorldToScreenPoint`.
- New components: `Rotator`, `Oscillator`, `KillBelowY`, `MaterialPulse` (all driven from the host).
- Fixed a field-index bug in the native `SET` parser: camera values were read six tokens too late
  (the managed `SET` line omits `HasMeshRenderer`), so script-driven camera edits were ignored.

## Câmera estilo Unity
- Selecting a camera opens a Camera Preview panel fed by a copy of the real post-processed frame.
- "Look Through Camera" makes that camera drive the Scene viewport; the camera's own icon is
  skipped in its own view.

## Export estilo Unity
- Exported games ship their data next to the executable in a `<Product>_Data` folder.
- `CopyGameAssets` skips engine source, editor project files, build/intermediate folders and the
  scripting host tree, so the engine is never re-shipped inside the exported game.
