# DX12 Forge Engine v0.5.1 — viewport, sky, camera and terrain fixes

## Changes

- The native DirectX 12 axis gizmo is now rendered in the scene pass in addition to the ImGuizmo screen-space handles.
- The daytime sky uses a stronger blue zenith and blue/cyan horizon, with restrained solar disc/Mie glow and reduced bloom contribution to avoid washing the full sky toward white.
- Terrain is excluded from the center-only frustum culling test. A large height field no longer disappears simply because its center moves behind or below the camera.
- Terrain sculpting now ray-tests the actual deformed terrain triangles instead of intersecting a flat `Y = terrain.position.y` plane. This makes sculpting follow hills and slopes.
- Terrain selection uses a terrain-sized local bound rather than the default cube bound.
- Selecting a Camera displays its world-space FOV frustum as viewport lines.
- The Camera Preview panel has **Look Through Camera**, which aligns the editor camera to the selected camera transform and activates its FOV for the scene view.
- Post-process bloom has been restrained so the sky and clouds retain contrast.

## Verification checklist on Windows

1. Configure/build with the repository's `windows-ninja` CMake preset from a Visual Studio Developer PowerShell.
2. Select a cube, sphere and imported model. Verify the colored gizmo and ImGuizmo handles appear; test Move/Rotate/Scale.
3. Create a Terrain, select it, and sculpt across flat ground and slopes with Raise/Lower/Smooth/Flatten.
4. Walk/strafe until the terrain's center passes behind the camera; visible parts of the terrain should remain rendered.
5. Create a Camera, adjust FOV, confirm the blue frustum lines change, then click **Look Through Camera** and compare the viewport to the camera's position/orientation.
6. Check sky at daytime and sunset; if your monitor still shows a white sky, capture the viewport and `docs/ASSERTION_DIAGNOSTICS.md` output because GPU/driver/HDR display configuration can affect the result.

## Notes

The camera preview uses the actual editor view aligned to the camera transform; this revision does not introduce a separate off-screen render target per camera. The camera frustum lines are a viewport visualization, not a rendered camera image embedded in the panel.

The project was source-patched in this environment but not compiled: this environment has no MSVC/Windows SDK/DirectX 12 toolchain. Please run the Windows build and report any compiler diagnostics if a follow-up patch is needed.

## v0.5.2 — ImGuizmo matrix/layout correction

- Removed the legacy native DirectX 12 gizmo draw call. Transform handles are exclusively ImGuizmo; no native gizmo fallback is drawn.
- Corrected matrix handoff between DirectXMath and ImGuizmo. DirectXMath row-vector matrices are stored row-major; that same flat storage is the transposed matrix representation ImGuizmo expects when it interprets the values as column-major. The previous explicit transposes moved translation out of the expected slots and caused the gizmo to be offset, distorted, or difficult to manipulate.
- Edited matrices are read back directly into `XMFLOAT4X4` before decomposition, preserving the DirectXMath world-matrix convention.
- This patch has not been compiled in a Windows/DirectX 12 toolchain in this environment; build and test Move/Rotate/Scale on an object away from the world origin.
