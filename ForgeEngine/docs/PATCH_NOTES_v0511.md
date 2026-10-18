# ForgeEngine v0.5.11 — Bullet physics

This release replaces the script-side physics approximation with a real Bullet
solver (through BulletSharp) running inside the C# scripting API.

## Physics

The `Rigidbody` component no longer integrates velocity by itself. Bodies are now
owned by a `DiscreteDynamicsWorld`, so collisions, stacking, friction and
restitution behave like a real physics engine instead of a per-object fallback.

An implicit ground plane still exists at `y = 0`, so objects come to rest even
when a scene does not declare a floor. Scenes that worked before keep working.

Existing components keep their signatures, so current game scripts and the
`MyGame` sample compile unchanged:

- `Rigidbody`, `Collider`, `SphereCollider`, `BoxCollider`, `CapsuleCollider`
- `OnCollisionEnter`, `OnCollisionStay`, `OnCollisionExit`
- `OnTriggerEnter`, `OnTriggerStay`, `OnTriggerExit`
- `Physics.Raycast`, `Physics.OverlapSphere`

New API on `Rigidbody`:

| Member | Purpose |
| --- | --- |
| `angularVelocity` | Read or write spin, in degrees per second |
| `angularDrag` | Angular damping |
| `AddForce(force, ForceMode)` | Force, Acceleration, Impulse or VelocityChange |
| `AddForceAtPosition` | Off-centre force, producing torque |
| `AddTorque` | Rotational force |
| `Sleep` / `WakeUp` / `IsSleeping` | Control body deactivation |
| `freezePositionX/Y/Z`, `freezeRotationX/Y/Z` | Lock individual axes |

New API on `Collider`:

| Member | Purpose |
| --- | --- |
| `friction` | Surface friction, defaults to `Physics.defaultFriction` |
| `bounciness` | Restitution, defaults to `Physics.defaultRestitution` |

New API on `Physics`:

| Member | Purpose |
| --- | --- |
| `defaultFriction`, `defaultRestitution` | Solver defaults |
| `sleepThreshold` | Velocity below which a body may sleep |
| `RaycastAll` | Every collider along a ray, nearest first |
| `IgnoreCollision` | Stop two colliders from reporting contacts |
| `ResetWorld` | Drop all bodies, for restarting a scene |

## Editor and scripting round trip

The scripting host rebuilds the managed scene from native state every frame, so
each fixed step reconciles in both directions. A per-body shadow value records
what the solver last published; the native engine echoes that value back, so any
difference is a real editor drag or script teleport and is pushed into Bullet.
Dragging an object in the Inspector therefore moves its body, and a body falling
in play mode moves the object.

## Fixes

- `Quaternion.eulerAngles` used the wrong denominator in its Z term, so it was not
  the inverse of `Quaternion.Euler`. Rotating an object and reading the value back
  could return a different angle. It now round-trips exactly.
- The `MyGame` script project referenced `ForgeEngine.API` one directory too high,
  so it failed to build standalone.
- BulletSharp's build targets only rename the native library to `libbulletc.dll`,
  which meant the Linux loader never found `libbulletc.so`. The API project now
  copies the correct native binary, and that flows into every project that
  references it, including the game's script project.

## Building

The C# side now restores BulletSharp from NuGet on first build. This needs network
access once; after that the package is cached.

```powershell
# Script host
dotnet build ForgeEngine/Scripting/ForgeScriptHost/ForgeScriptHost.csproj

# A game's scripts
dotnet build ForgeEngine/Projects/MyGame/Scripts/ForgeGameScripts.csproj
```

The C++ editor is built with CMake as before; see `BUILD_AND_TROUBLESHOOTING.md`.
