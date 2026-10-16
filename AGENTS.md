# Forge Engine

A C++17 / DirectX 12 3D engine with a Dear ImGui editor, a C# scripting API, and a
CMake build.

## Layout

```
ForgeEngine/
  src/Engine/      Core renderer, scene, camera (Engine.cpp, Engine.h)
  src/Editor/      Dear ImGui editor UI and export (EditorUI.cpp)
  src/Scripting/   Native side of the C# scripting bridge (ScriptRuntime.cpp)
  Scripting/
    ForgeEngine.API/   The C# API the game code compiles against (RuntimeAPI.cs)
    ForgeScriptHost/   Out-of-process host that runs game scripts
  assets/shaders/Shaders.hlsl   ALL shader entry points live here, in one file
  Project/, Projects/MyGame/    Sample project and its scripts
```

## Building

Windows only. The CMake configure fails on purpose elsewhere:

```
cmake -S ForgeEngine -B out/build -G Ninja
```

The build requires MSVC and the Windows SDK. Third-party dependencies (Dear ImGui,
ImGuizmo, Assimp) are fetched by `FetchContent` on first configure, so the first
build needs network access.

There is no automated test suite. Validate changes as described below.

## Validating shader changes

`Shaders.hlsl` is a single file holding every entry point, and CMake copies it next
to the executable at build time. It is the only copy; there is no second shader
directory to keep in sync.

Entry points: `VSMain`, `PSPBR`, `VSShadow`, `VSSkyGrid`, `PSSkyGrid`, `PSOutline`,
`PSAxis`, `VSPostProcess`, `PSPostProcess`.

On Linux you can syntax-check and SPIR-V compile every entry point without the
Windows toolchain by using glslang. It is not part of the repo; fetch the release
build once:

```sh
mkdir -p /workspace/tools && cd /workspace/tools
curl -sL -o g.tar.gz https://github.com/KhronosGroup/glslang/releases/download/16.6.0/glslang-16.6.0-linux-x86_64-release.tar.gz
tar xzf g.tar.gz
```

Then validate each entry point (`-D` enables the DXC-style shader model defines):

```sh
SH=ForgeEngine/assets/shaders/Shaders.hlsl
for e in VSMain PSPBR VSShadow VSSkyGrid PSSkyGrid PSOutline PSAxis VSPostProcess PSPostProcess; do
  case $e in VS*) s=vert;; *) s=frag;; esac
  ./bin/glslangValidator -D -e $e -S $s -V $SH >/dev/null 2>&1 && echo "OK  $e" || echo "FAIL $e"
done
```

Note that this catches syntax and type errors only. It cannot catch DX12 pipeline
state mistakes, which is where the depth bug below came from.

## Sky and cloud rendering

The sky, grid, clouds and celestial bodies are all drawn by `PSSkyGrid` in a single
full-screen pass, driven by `gSkyParameters` (`FrameData` in `Engine.h`, which must
stay byte-compatible with the HLSL struct):

```
x = cloud coverage (0..1)   y = cloud altitude   z = wind speed   w = grid overlay
```

Default is `{0.35, 0.20, 1.0, 1.0}`.

Cloud noise notes, all learned by measurement:

- Noise is gradient (Perlin-style) noise, not value noise. Value noise stores a
  random value at every lattice corner, so the cell borders survive interpolation
  and read as squares. `Hash2x`/`Hash33` return unit-circle hashes to keep the
  gradient field isotropic.
- Clouds are sampled as 3D noise on the view sphere (`CloudFBM3D`), not from a
  flat-plane UV. The flat projection has a pole at the zenith where the UV barely
  changes, so one noise cell covered most of the upper dome and was visible as a
  grid; it also produced an azimuth seam.
- Octave lacunarity is a uniform 2.02. Mixing 1.97 and 2.09 stretched each octave
  along one axis and made the squares worse.
- `CloudFBM`/`CloudFBM3D` are normalised to 0..1. Measured, `broad*0.78 + detail*0.22`
  spans about 0.34..0.66, so the coverage remap `(x - 0.43) / 0.20` spreads it back
  over 0..1. Changing the noise constants invalidates that remap.
- The moon's craters come from tangent-plane disc coordinates, because `moonDot`
  only spans ~0.016 radians across the disc and driving detail from it renders a
  flat blob. The tangent basis must fall back to +Z when the moon is at the zenith,
  where `cross(moonDir, +Y)` collapses and `normalize()` returns NaN.

To check a sky change without running the engine, render the field in Python with
the same constants and measure it. Useful metrics: the fraction of near-white
pixels, the top-third mean RGB, the local standard deviation (cloud structure), and
the ratio of axis-aligned to diagonal low-frequency FFT energy, which is the
signature of a visible lattice. A correct cumulus field sits near 4; the old
value-noise field measured 91.

## Rendering order and depth

`DrawScenePass` runs the sky/grid pass first, then `DrawObjects`. Because it runs
first, the sky pipeline must not write depth: it uses `DepthWriteMask ZERO`, and
`PSSkyGrid` must not declare `SV_Depth`. If it does write depth while running with
`DepthFunc ALWAYS`, it stamps the far plane across the whole viewport and every
object below the grid plane disappears. Objects above the plane still draw, so this
failure is easy to misread as a modelling problem.

## C# scripting

Physics is implemented in managed C# in `RuntimeAPI.cs`, not in the native layer,
and the host runs out of process and talks to the engine over a text pipe. The
collision model is sphere-only: `Rigidbody.Simulate` integrates gravity, drag and
forces, and the ground plane is handled by clamping against `collider.effectiveRadius`.
There is no native physics library and no general collision solver.

`Scene` format versioning is handled by the `v8`/`v9` markers in the loader; keep
older markers loadable when extending the format.

## Working environment notes

This environment has no `ssh` by default and the `GITHUB_TOKEN` is read-only. Pushes
need a deploy key at `~/.ssh/openhands-forge-engine`. Note that `/tmp` is cleared
between sessions, so keep any tooling you install under `/workspace`.
