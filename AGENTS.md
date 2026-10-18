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

`Scene` format versioning is handled by the version marker in the loader (currently
`FORGEENGINE_SCENE 11`; the loader still accepts the legacy `DX12FORGE_SCENE`
signature and versions 1..11). Keep older versions loadable when extending the format:
each version's extra fields are appended in order and read behind a `if (version >= N)`
guard, so a new version must only append.

### Building and testing the C# side on Linux

`ForgeEngine.API` and `ForgeScriptHost` are plain `net8.0` and have no Windows
dependency, so they build and run on Linux even though the engine itself does not.
This is the only part of the project that can be executed here, and it is worth
using: physics and component logic can be tested for real instead of by inspection.

The SDK is not on the system path. It was installed to `/workspace/dotnet`:

```sh
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1   # the image has no libicu and apt cannot provide it
/workspace/dotnet/dotnet build ForgeEngine/Scripting/ForgeEngine.API
/workspace/dotnet/dotnet build ForgeEngine/Scripting/ForgeScriptHost
```

`DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` is required. Without it the CLI aborts with
"Couldn't find a valid ICU package" before doing anything.

### The pipe protocol

The engine writes one `FRAME|...` line per tick, then one `OBJ|...` line per object,
then `END`; `SET|...` lines carry transforms back from scripts. New per-object fields
are appended to `OBJ` and guarded by length checks (`p.Length >= 26`, `>= 37`), so
adding a field is backward compatible as long as the reader keeps its bounds check.
`OBJ` field order must stay in sync between `ScriptRuntime.cpp` (writer) and
`Program.cs` (reader).

Three files must be edited together whenever a per-object field is added, and their
token indices must agree exactly:

- `ScriptRuntime.cpp` — the `OBJ` writer and the `SET` parser (native side)
- `Program.cs` — the `OBJ` parser and the `SET` writer (C# host)
- `ScriptRuntime.h` / `RuntimeAPI.cs` — the two `ForgeScriptObjectState` /
  `RuntimeObjectData` field sets

`OBJ` and `SET` do **not** share an index base. `OBJ` starts its payload at `p[3]`
(`p[0]` is the literal `OBJ`, `p[1]` the id, `p[2]` the Base64 name); `SET` starts at
`p[2]` and also omits `hasMeshRenderer`. The simplest way to keep them straight is to
count the emit arguments directly rather than trusting a hand-written table, e.g. split
the `Emit("SET", ...)` argument list on top-level commas. As of the v11 component
block: `OBJ` has 109 tokens (`p[0]`..`p[108]`, components at `p[65]`..`p[108]`) and
`SET` has 103 (`p[0]`..`p[102]`, components at `p[59]`..`p[102]`).

`EditorUI.cpp` mirrors every field in both directions (`state.*` ↔ `object.*`) and
serialises them to the scene file; a field added to only one direction will silently
reset on the next script tick or scene save.

### Cloud noise: avoid axis-aligned FBM

`CloudFBM3D` in `Shaders.hlsl` multiplies the domain by an orthonormal matrix
(`kCloudOctaveRotation`) between octaves. Without it, every octave's gradient-noise
lattice shares the same axis directions, so the lattice spacing reinforces and the sky
reads as a square grid. Do not remove the rotation, and keep the matrix orthonormal
(determinant `+1`) so it rotates the lattice without stretching the noise.

Measured on the axis-line share of spectral energy for the 5-octave field used by the
cumulus deck (40x40 samples, step 0.06, Hann window): 42.1% without the rotation,
18.2% with it. To reproduce, port `GradientNoise3D`/`CloudFBM3D`/`Hash33` to Python and
compute the separable 2D DFT, then compare `sum(|F|^2)` on the `k==0 or l==0` lines
against the total.

### Component API surface

`RuntimeAPI.cs` exposes one class per component, all deriving from `Component`. The
built-in set is `Transform`, `MeshRenderer`, `Collider` (+`Box/Sphere/Capsule`),
`Rigidbody`, `Camera`, `Light`, `AudioSource`, `AudioListener`, `CharacterController`,
`ParticleSystem` and `Animator`. Adding a component means touching the same five
places every time:

1. `Engine.h` — the `GameObject` fields (a `hasX` flag plus its data)
2. `EditorUI.cpp` — the Inspector panel, the `+ Add Component` menu entry, and both
   directions of the state marshal
3. `RuntimeAPI.cs` — the `RuntimeObjectData` fields, the `Component` subclass, and the
   `Clone`/`CopyFrom` field lists
4. `ScriptRuntime.h` / `RuntimeAPI.cs` + `ScriptRuntime.cpp` / `Program.cs` — the pipe
   protocol, per the protocol section above
5. `EditorUI.cpp` — the scene writer/reader, behind a new version guard

`GameObject.AddComponent<T>()`/`GetComponent<T>()` work off a component list, and
`GameObject.RefreshNativeComponents()` syncs the native `hasX` flags from the C# state.
Components that need per-frame work (`ParticleSystem.Simulate`, `Animator.Simulate`)
are ticked from `ProcessFrame` in `Program.cs` **after** the script `Update` calls, so a
script can still drive them in the same frame.

### Physics: the Bullet solver

`PhysicsRuntime.cs` owns a BulletSharp `DiscreteDynamicsWorld` with an implicit ground
plane at `y = 0`, so bodies come to rest even when a scene declares no floor.
`Physics.SimulateRuntimeStep(fixedDeltaTime)` is the single entry point, called from
the `FixedUpdate` loop in `Program.cs`. Each step runs, in order: external-move
reconciliation, per-body gravity, `stepSimulation`, then publish-back and callback
dispatch (`OnCollisionEnter/Stay/Exit`, `OnTriggerEnter/...`, Unity-style). The
script-driven components (`ConstantForce`, `Rotator`, `Oscillator`, ...) tick around it.

Sync works in both directions because the host rebuilds the managed scene from native
state every frame. Each body keeps a `Shadow` position holding whatever the solver last
published; the native side echoes that value straight back, so any difference is a real
editor drag or script teleport and is pushed into the body. Do not re-add a
"dynamic bodies are owned by the solver" guard: it silently drops editor drags, which is
exactly what a user sees as "I moved it and nothing happened".

Ordering rules that are easy to get wrong, all verified against the real library:

- Set per-body `Gravity` **after** `AddRigidBody`; the constructor's construction info
  overwrites it.
- Kinematic bodies need a `KinematicCharacterController`-free setup: drive them through
  `MotionState.WorldTransform` and also set `InterpolationWorldTransform`, otherwise the
  push does not register.
- `GhostObject` overlap lists stay empty until a `GhostPairCallback` is registered on the
  broadphase.
- `Scene.SyncObjects` must run before `Scene.Find`/`AddComponent` in tests; components
  added to an object the scene has not seen are dropped.
- Every component that applies force or constraints must be ticked from
  `SimulateRuntimeStep` **before** `StepSimulation`. `Rigidbody.Simulate` was written but
  never called, which silently made `AddForce` and every freeze flag dead code. If you add
  a component that mutates a body, wire its tick in there too.
- `LinearFactor`/`AngularFactor` only scale applied force, torque and gravity. They do
  **not** mask a velocity a script assigns directly, so `SetConstraints` also zeroes the
  masked velocity components; otherwise a "frozen" body still spins.

BulletSharp 0.11.1 is `net40`, so it raises `NU1701` on net8.0. Its `.targets` only
renames the native library to `libbulletc.dll`, so on Linux the loader never finds
`libbulletc.so`. `ForgeEngine.API.csproj` therefore copies
`build/net40-client/libbulletc-linux-x64.so` (Windows: `-windows-x64.dll`) as a
`Content` item; content items flow through `GetCopyToOutputDirectoryItems`, so the
script host and every game project get it for free. The `RigidBody` constructors take a
single `RigidBodyConstructionInfo`, not `(mass, motionState, shape, inertia)`.

BulletSharp 0.11.1 dates from 2016 and is effectively unmaintained. Its managed API
differs from current BulletSharp docs, so confirm signatures by reflection before
writing against it.

### CharacterController: sweeps, not a solver body

`CharacterController.Move` is kinematic: it sweeps a `CapsuleShape` through the world
and resolves the result itself, so it never becomes a rigid body. The pieces are
`Physics.SweepCapsule` (one convex sweep) and `MoveWithSlide` / `TryStepUp` /
`TryFindGround` in `RuntimeAPI.cs`.

Things that were wrong at first and are easy to reintroduce:

- The sweep's travel is `ClosestHitFraction * |motion|`. The property is
  `ClosestHitFraction`, not `HitFraction`, and it is a **fraction** of the motion. Using
  the distance to the contact point instead adds the capsule's own radius and parks the
  character short of (or inside) every wall.
- The character's own collider must be skipped or the sweep starts already touching it.
  `CharacterSweepCallback.NeedsCollision` compares `UserObject` against the collider passed
  in. A `CharacterController` with no `Collider` component on the same object is also
  valid; `Move` only passes a collider when one exists.
- Do **not** filter "walkable" hits out inside the sweep by comparing the contact normal
  to `slopeLimit`. At a box edge Bullet reports the sphere-versus-edge normal, which is
  diagonal (~22 degrees on a right-angled step), so a slope-angle filter either swallows
  step edges (walking through them) or refuses to climb slopes. The decision has to come
  from the surface the character would stand on: `TryStepUp` re-probes with `TryFindGround`
  and accepts only if that surface is within the slope limit.
- Walkability is judged per-hit in `MoveWithSlide` only to decide whether a wall may hide a
  step. Step-up must be tried *only* for a non-walkable hit: a walkable hit already gets
  climbed by sliding, and testing step-up first launches the character up a slope in
  `stepOffset` chunks.
- Sliding along a wall must use the **horizontal** part of the hit normal. Projecting the
  remaining motion onto the full normal lets a step's rounded edge lift the character,
  which reads as the capsule climbing a wall.
- The ground probe casts down `halfHeight + stepOffset + skinWidth` from the capsule
  centre and rejects surfaces steeper than `slopeLimit`. `halfHeight` is `height * 0.5`,
  not `radius`; using `radius` sinks the character by half its height.
- `Move` takes a per-frame **displacement** and divides by `Time.deltaTime` to recover a
  velocity, so it must be called from `Update` with `speed * Time.deltaTime`, not from
  `FixedUpdate`. Gravity is integrated inside `Move`; `Jump()` only adds the impulse.

Test it with a harness that mirrors the host's frame order (native echo -> `SyncObjects`
-> fixed physics steps -> `Update`) and injects input through `Input.BeginFrame`. Note
that `RuntimeObjectData.Rotation` is in **degrees** while `Transform.rotation` is in
radians, and that `InputMap` only resolves `GetButton("Jump")`/`GetButton("Run")` when
`Config/Inputs.json` is reachable, so a test outside the project tree needs a copy of it.

## Known C# API gaps

These are declared but not wired, so they silently do nothing. Check them before
building a feature on top:

- `Input.GetAxisRaw("Mouse ScrollWheel")` always returns 0. The native input state has no
  scroll field, so the value never reaches the host. `Inputs.json` ships the axis anyway.
- There is no `Cursor` class, so a script cannot lock or hide the pointer. Play mode does
  hide the cursor while the right mouse button is held.
- There is no parent/child API in C#. `parentIndex` exists on the native object, is carried
  through the pipe protocol and the editor draws the hierarchy, but `Transform.localPosition`
  is currently the same value as `position` and `lossyScale` returns the local scale, so a
  child cannot be offset from or follow its parent through scripts.
- `Time.fixedDeltaTime` is a hard-coded 0.02f; `Rigidbody.drag` and `angularDrag` are plain
  auto-properties that never reach Bullet.
- `Input.GetAxis`/`GetAxisRaw` return 0 for any axis that is not in `Config/Inputs.json`,
  except for the legacy `Horizontal`, `Vertical`, `Mouse X` and `Mouse Y` fallbacks.

## Working environment notes

`ssh` is not installed in a fresh container, and the `GITHUB_TOKEN` is read-only for
this repo, so pushes go through the deploy key at `/workspace/.ssh/forge-engine-deploy`
(wired up via `core.sshCommand`). The container may recreate `/workspace/.ssh` with
group-writable permissions, which makes OpenSSH reject the key; run
`chmod 700 /workspace/.ssh && chmod 600 /workspace/.ssh/forge-engine-deploy` before
pushing. Install the client with `sudo apt-get update && sudo apt-get install -y
openssh-client` (the user is in the `sudo` group, and `apt-get update` is required
first or the package has "no installation candidate"). Note that `/tmp` is cleared
between sessions, so keep any tooling you install under `/workspace`.

The .NET SDK is not on `PATH`; use `/workspace/dotnet/dotnet`. Set
`DOTNET_CLI_TELEMETRY_OPTOUT=1` and `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` to keep
builds quiet and offline.
