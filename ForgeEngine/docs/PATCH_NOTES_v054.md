# Forge Engine v0.5.4 — Shadow, Culling, Picking, Camera and PBR Fixes

## Changes

- Raised the directional shadow map to 4096 × 4096 and reduced excessive rasterizer depth bias to preserve contact detail while the 5×5 PCF filter reduces jagged edges.
- Mesh creation now calculates an actual local-space bounding sphere from CPU vertices. Frustum culling transforms the mesh bounds center through the object and view matrices and tests against normalized-equivalent side planes. Selected objects and terrain stay uncullable so editor interaction remains stable.
- Mouse picking unprojects both clip-space near and far points using the exact inverse view-projection matrix and samples the pixel center. Picking uses two-sided Möller–Trumbore tests against retained CPU mesh geometry; deformable terrain uses its actual current triangles.
- Selected objects use a more visible expanded-shell outline pass.
- Entering Camera View immediately applies the selected camera transform; loading a new scene exits the previous scene's camera view to avoid reusing a stale object index. The Camera Preview panel now displays a copy of the final post-processed Scene frame rather than a static FOV sketch (use Look Through Camera to make this the selected camera's actual view).
- Material support includes UV tiling and offset, albedo, tangent-space normal maps, packed metallic/roughness maps (glTF convention: G roughness, B metallic), and AO maps (R channel). A dedicated anisotropic wrap sampler makes tiling repeat correctly while the post-process sampler stays clamped.
- Previous compile fixes retained: C++ method renamed to `SelectSceneObject` to avoid hiding Win32 GDI `SelectObject`, `template` is not used as an identifier, `HFONT` selection uses `HGDIOBJ`, folder naming uses `snprintf`, and `LoadScene` declares `std::error_code ec` for filesystem error overloads.

## Validation

Static checks were performed for changed source structure, scene serialization XML/JSON support files, and archive integrity. This environment does not provide MSVC + Windows SDK + DirectX 12 or the .NET SDK, so a native Windows build and runtime verification are still required.
