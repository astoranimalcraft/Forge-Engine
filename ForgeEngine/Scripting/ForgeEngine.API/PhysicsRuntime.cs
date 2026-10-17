using BulletSharp;
using BV3 = BulletSharp.Math.Vector3;
using BQ = BulletSharp.Math.Quaternion;
using BM = BulletSharp.Math.Matrix;

namespace ForgeEngine;

/// <summary>
/// Physics world backed by Bullet (through BulletSharp).
///
/// The scripting host rebuilds the managed scene from the native engine every
/// frame (<see cref="Scene.SyncObjects"/>), so the native position wins for
/// anything the editor or a script moved. Each fixed step therefore reconciles in
/// both directions: a native position that differs from the value the solver last
/// produced counts as an external move and is pushed into Bullet, and everything
/// else is published from the solver back into the scene.
///
/// The public surface (Rigidbody, Collider, OnCollision*, OnTrigger*, Raycast,
/// OverlapSphere) is unchanged, so existing game scripts keep working.
/// </summary>
public static class Physics
{
    /// <summary>World gravity. Assigning it updates the live Bullet world.</summary>
    public static Vector3 gravity
    {
        get => _gravity;
        set
        {
            _gravity = value;
            if (_world is not null) _world.Gravity = ToBullet(value);
        }
    }
    private static Vector3 _gravity = new(0, -9.81f, 0);

    /// <summary>Friction applied to bodies whose collider does not override it.</summary>
    public static float defaultFriction { get; set; } = 0.5f;

    /// <summary>Bounciness applied to bodies whose collider does not override it.</summary>
    public static float defaultRestitution { get; set; } = 0.05f;

    /// <summary>Angular velocity threshold below which a body is allowed to sleep.</summary>
    public static float sleepThreshold { get; set; } = 0.05f;

    /// <summary>Number of constraint solver iterations per step. More iterations give steadier stacks at a higher cost.</summary>
    public static int solverIterations
    {
        get => _solverIterations;
        set
        {
            _solverIterations = Math.Clamp(value, 1, 60);
            if (_world is not null) _world.SolverInfo.NumIterations = _solverIterations;
        }
    }
    private static int _solverIterations = 10;

    /// <summary>Maximum number of fixed substeps a single step may run. 1 disables substepping.</summary>
    public static int maxSubSteps { get; set; } = 4;

    /// <summary>Fixed step used for substepping, in seconds.</summary>
    public static float fixedTimeStep { get; set; } = 1f / 60f;

    /// <summary>Maximum travel per substep, in metres. Smaller values stop fast objects tunnelling through walls.</summary>
    public static float maxSubStepTravel { get; set; } = 2f;

    /// <summary>Enables continuous collision detection for every dynamic body, which stops fast objects passing through thin geometry.</summary>
    public static bool continuousCollisionDetection { get; set; }

    /// <summary>Gap kept between shapes, in metres. A slightly larger margin is cheaper and more stable than a tight one.</summary>
    public static float collisionMargin { get; set; } = 0.02f;

    private static DefaultCollisionConfiguration? _configuration;
    private static CollisionDispatcher? _dispatcher;
    private static DbvtBroadphase? _broadphase;
    private static SequentialImpulseConstraintSolver? _solver;
    private static DiscreteDynamicsWorld? _world;
    private static GhostPairCallback? _ghostPairCallback;

    private static readonly Dictionary<int, BodyEntry> Bodies = new();
    private static readonly Dictionary<long, (Collider A, Collider B)> CollisionContacts = new();
    private static readonly Dictionary<long, (Collider A, Collider B)> TriggerContacts = new();
    private static readonly HashSet<long> IgnoredPairs = new();

    private sealed class BodyEntry
    {
        public RigidBody Body = null!;
        public CollisionShape Shape = null!;
        public DefaultMotionState Motion = null!;
        public GhostObject? Ghost;
        public Vector3 Shadow;
        public bool Dynamic;
        public bool Trigger;
        public int LastType = -1;
        public Vector3 LastSize;
        public Vector3 LastScale;
        public Vector3 LastCenter;
        public Vector3 LastRotation;
        public bool LastTrigger;
        public bool LastKinematic;
        public float LastMass;
        public bool LastUseGravity;
    }

    private static long PairKey(int a, int b) => a < b ? ((long)a << 32) | (uint)b : ((long)b << 32) | (uint)a;

    internal static BV3 ToBullet(Vector3 v) => new(v.x, v.y, v.z);
    internal static Vector3 FromBullet(BV3 v) => new(v.X, v.Y, v.Z);

    private static float Wrap360(float value) { float v = value % 360f; return v < 0f ? v + 360f : v; }

    /// <summary>Builds a rotation quaternion from Euler degrees using the engine's XYZ order.</summary>
    private static BQ EulerToQuaternion(Vector3 degrees)
    {
        var q = Quaternion.Euler(degrees);
        return new BQ(q.x, q.y, q.z, q.w);
    }

    /// <summary>Extracts Euler degrees from a quaternion; matches Quaternion.eulerAngles.</summary>
    private static Vector3 QuaternionToEuler(BQ q)
    {
        float len = MathF.Sqrt(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W);
        if (len > 1e-6f) { q.X /= len; q.Y /= len; q.Z /= len; q.W /= len; }
        float sinY = Math.Clamp(2f * (q.Y * q.W - q.X * q.Z), -1f, 1f);
        float yDeg = MathF.Asin(sinY) * Mathf.Rad2Deg;
        float xDeg = MathF.Atan2(2f * (q.X * q.W + q.Y * q.Z), 1f - 2f * (q.X * q.X + q.Y * q.Y)) * Mathf.Rad2Deg;
        float zDeg = MathF.Atan2(2f * (q.Z * q.W + q.X * q.Y), 1f - 2f * (q.Y * q.Y + q.Z * q.Z)) * Mathf.Rad2Deg;
        return new Vector3(Wrap360(xDeg), Wrap360(yDeg), Wrap360(zDeg));
    }

    private static BM ComposeTransform(Vector3 position, Vector3 rotationDegrees)
        => BM.RotationQuaternion(EulerToQuaternion(rotationDegrees)) * BM.Translation(ToBullet(position));

    private static void EnsureWorld()
    {
        if (_world is not null) return;
        _configuration = new DefaultCollisionConfiguration();
        _dispatcher = new CollisionDispatcher(_configuration);
        _broadphase = new DbvtBroadphase();
        _solver = new SequentialImpulseConstraintSolver();
        _world = new DiscreteDynamicsWorld(_dispatcher, _broadphase, _solver, _configuration);
        _world.Gravity = ToBullet(_gravity);
        _world.SolverInfo.NumIterations = _solverIterations;
        // Without this, ghost objects never populate their overlap list.
        _ghostPairCallback = new GhostPairCallback();
        _broadphase.OverlappingPairCache!.SetInternalGhostPairCallback(_ghostPairCallback);

        // Implicit ground at y=0 so objects still come to rest without the scene
        // having to declare a floor, matching the previous custom solver.
        var groundShape = new StaticPlaneShape(new BV3(0, 1, 0), 0f);
        var groundInfo = new RigidBodyConstructionInfo(0f, new DefaultMotionState(BM.Identity), groundShape);
        _world.AddRigidBody(new RigidBody(groundInfo));
    }

    /// <summary>Drops every body and recreates the world on the next step.</summary>
    public static void ResetWorld()
    {
        if (_world is null) return;
        foreach (var entry in Bodies.Values) DisposeEntry(entry);
        Bodies.Clear();
        CollisionContacts.Clear();
        TriggerContacts.Clear();
        IgnoredPairs.Clear();
        _world.Dispose();
        _world = null;
        _dispatcher = null;
        _broadphase = null;
        _solver = null;
        _configuration = null;
        _ghostPairCallback = null;
    }

    private static void DisposeEntry(BodyEntry entry)
    {
        if (_world is null) return;
        if (entry.Ghost is not null) _world.RemoveCollisionObject(entry.Ghost);
        else _world.RemoveRigidBody(entry.Body);
        entry.Body.Dispose();
        entry.Shape.Dispose();
    }

    private static CollisionShape CreateShape(int type, Vector3 size, Vector3 scale, Vector3 center)
    {
        var shape = CreatePrimitiveShape(type, size, scale);

        // A collision shape always sits on the body origin. To offset it, Bullet wants the
        // shape inside a compound with a child transform; setting a transform on the body
        // would move the whole object instead.
        if (center.x == 0f && center.y == 0f && center.z == 0f) return shape;

        var compound = new CompoundShape();
        var childTransform = BM.Translation(center.x * scale.x, center.y * scale.y, center.z * scale.z);
        compound.AddChildShape(childTransform, shape);
        return compound;
    }

    private static CollisionShape CreatePrimitiveShape(int type, Vector3 size, Vector3 scale)
    {
        float sx = MathF.Abs(size.x * scale.x), sy = MathF.Abs(size.y * scale.y), sz = MathF.Abs(size.z * scale.z);
        switch (type)
        {
            case 1:
            {
                float radius = MathF.Max(0.01f, 0.5f * MathF.Max(sx, MathF.Max(sy, sz)));
                return new SphereShape(radius);
            }
            case 2:
            {
                float radius = MathF.Max(0.01f, 0.5f * MathF.Max(sx, sz));
                float cylinder = MathF.Max(0.01f, sy - 2f * radius);
                return new CapsuleShape(radius, cylinder);
            }
            default:
                return new BoxShape(new BV3(MathF.Max(0.005f, sx * 0.5f), MathF.Max(0.005f, sy * 0.5f), MathF.Max(0.005f, sz * 0.5f)));
        }
    }

    /// <summary>Local-space half extent of the shape, used for the world bounds accessor.</summary>
    internal static Vector3 LocalHalfExtent(int type, Vector3 size, Vector3 scale)
    {
        float sx = MathF.Abs(size.x * scale.x), sy = MathF.Abs(size.y * scale.y), sz = MathF.Abs(size.z * scale.z);
        return type switch
        {
            1 => new Vector3(0.5f * MathF.Max(sx, MathF.Max(sy, sz)), 0.5f * MathF.Max(sx, MathF.Max(sy, sz)), 0.5f * MathF.Max(sx, MathF.Max(sy, sz))),
            2 => new Vector3(0.5f * MathF.Max(sx, sz), 0.5f * sy, 0.5f * MathF.Max(sx, sz)),
            _ => new Vector3(0.5f * sx, 0.5f * sy, 0.5f * sz)
        };
    }

    /// <summary>Reconciles the Bullet bodies with the current scene state.</summary>
    private static void SyncBodies()
    {
        var live = new HashSet<int>();
        foreach (var go in Scene.gameObjects)
        {
            var state = go.State;
            bool hasCollider = state.HasCollider && go.GetComponent<Collider>() is not null;
            bool hasBody = state.HasRigidbody && go.GetComponent<Rigidbody>() is not null;
            if (!hasCollider && !hasBody) continue;

            live.Add(state.Id);
            bool trigger = state.IsTrigger;
            bool kinematic = hasBody && state.IsKinematic;
            bool dynamic = hasBody && !state.IsKinematic;

            if (!Bodies.TryGetValue(state.Id, out var entry))
            {
                entry = CreateEntry(go, trigger, kinematic, dynamic);
                if (entry is null) continue;
                Bodies[state.Id] = entry;
            }
            else if (entry.LastType != state.ColliderType || entry.LastSize != state.ColliderSize ||
                     entry.LastScale != state.Scale || entry.LastTrigger != trigger ||
                     entry.LastCenter != state.ColliderCenter ||
                     entry.LastKinematic != kinematic)
            {
                // Structure changed (shape, size, scale, trigger or kinematic flag):
                // rebuild the body, carrying the transform and velocity across.
                var previousPosition = entry.Shadow;
                var previousVelocity = entry.Body.LinearVelocity;
                var previousAngular = entry.Body.AngularVelocity;
                DisposeEntry(entry);
                var rebuilt = CreateEntry(go, trigger, kinematic, dynamic, previousPosition, previousVelocity, previousAngular);
                if (rebuilt is null) { Bodies.Remove(state.Id); continue; }
                entry = rebuilt;
                Bodies[state.Id] = entry;
            }

            ApplyBodyProperties(entry, go, dynamic);
        }

        foreach (int stale in Bodies.Keys.Where(id => !live.Contains(id)).ToArray())
        {
            DisposeEntry(Bodies[stale]);
            Bodies.Remove(stale);
        }
    }

    private static BodyEntry? CreateEntry(GameObject go, bool trigger, bool kinematic, bool dynamic,
        Vector3? positionOverride = null, BV3? velocityOverride = null, BV3? angularOverride = null)
    {
        var state = go.State;
        var position = positionOverride ?? state.Position;
        var shape = CreateShape(state.ColliderType, state.ColliderSize, state.Scale, state.ColliderCenter);
        var transform = ComposeTransform(position, state.Rotation);
        var motion = new DefaultMotionState(transform);
        float mass = dynamic ? MathF.Max(0.01f, state.Mass) : 0f;
        var inertia = mass > 0f ? shape.CalculateLocalInertia(mass) : BV3.Zero;

        RigidBody body;
        try
        {
            body = new RigidBody(new RigidBodyConstructionInfo(mass, motion, shape, inertia));
        }
        catch (Exception ex)
        {
            Debug.LogError($"Physics: failed to create a body for '{state.Name}': {ex.Message}");
            shape.Dispose();
            return null;
        }

        var entry = new BodyEntry
        {
            Body = body,
            Shape = shape,
            Motion = motion,
            Shadow = position,
            Dynamic = dynamic,
            Trigger = trigger,
            LastType = state.ColliderType,
            LastSize = state.ColliderSize,
            LastScale = state.Scale,
            LastCenter = state.ColliderCenter,
            LastRotation = state.Rotation,
            LastTrigger = trigger,
            LastKinematic = kinematic,
            LastMass = state.Mass,
            LastUseGravity = state.UseGravity
        };

        body.UserObject = go.GetComponent<Collider>() ?? (object)go;

        if (trigger)
        {
            var ghost = new PairCachingGhostObject { CollisionShape = shape, WorldTransform = transform };
            ghost.CollisionFlags = CollisionFlags.NoContactResponse;
            entry.Ghost = ghost;
            _world!.AddCollisionObject(ghost, CollisionFilterGroups.SensorTrigger, CollisionFilterGroups.AllFilter);
        }
        else
        {
            if (kinematic) body.CollisionFlags |= CollisionFlags.KinematicObject;
            _world!.AddRigidBody(body);
        }

        if (velocityOverride.HasValue) body.LinearVelocity = velocityOverride.Value;
        if (angularOverride.HasValue) body.AngularVelocity = angularOverride.Value;

        // Per-body gravity must be assigned after the body joins the world, otherwise
        // AddRigidBody overwrites it with the world gravity.
        body.Gravity = state.UseGravity ? ToBullet(_gravity) : BV3.Zero;
        return entry;
    }

    private static void ApplyBodyProperties(BodyEntry entry, GameObject go, bool dynamic)
    {
        var state = go.State;
        var collider = go.GetComponent<Collider>();
        float mass = dynamic ? MathF.Max(0.01f, state.Mass) : 0f;

        if (MathF.Abs(mass - entry.LastMass) > 1e-6f)
        {
            entry.Body.SetMassProps(mass, mass > 0f ? entry.Shape.CalculateLocalInertia(mass) : BV3.Zero);
            entry.Body.UpdateInertiaTensor();
            entry.LastMass = mass;
        }

        if (entry.LastUseGravity != state.UseGravity)
        {
            entry.Body.Gravity = state.UseGravity ? ToBullet(_gravity) : BV3.Zero;
            entry.LastUseGravity = state.UseGravity;
        }

        float friction = collider?.friction ?? defaultFriction;
        float restitution = collider?.bounciness ?? defaultRestitution;
        entry.Body.Friction = friction;
        entry.Body.Restitution = restitution;

        var rb = go.GetComponent<Rigidbody>();
        if (rb is not null)
        {
            entry.Body.SetDamping(MathF.Max(0f, rb.drag), MathF.Max(0f, rb.angularDrag));
            // CCD sweeps the shape along its motion; without it a fast body can pass a wall
            // between two steps. Only meaningful for dynamic bodies.
            bool ccd = (rb.continuousCollisionDetection || continuousCollisionDetection) && dynamic;
            entry.Body.CcdMotionThreshold = ccd ? MathF.Max(0.001f, rb.continuousCollisionDetectionThreshold) : 0f;
            entry.Body.CcdSweptSphereRadius = ccd ? MathF.Max(0.01f, rb.collisionMargin) : 0f;
            entry.Shape.Margin = MathF.Max(0.001f, rb.collisionMargin);
        }
        else
        {
            entry.Shape.Margin = MathF.Max(0.001f, collisionMargin);
        }

        entry.LastSize = state.ColliderSize;
        entry.LastScale = state.Scale;
        entry.LastCenter = state.ColliderCenter;
        entry.LastType = state.ColliderType;
        entry.LastTrigger = state.IsTrigger;
    }

    /// <summary>
    /// Pushes positions the native side changed into Bullet. The shadow value makes
    /// this exact: it holds whatever the solver last published (or what was last
    /// pushed in), and the native engine echoes that value straight back, so any
    /// difference is a genuine editor drag or script teleport.
    /// </summary>
    private static void PushExternalMoves()
    {
        foreach (var go in Scene.gameObjects)
        {
            if (!Bodies.TryGetValue(go.State.Id, out var entry)) continue;

            var native = go.State.Position;
            var nativeRotation = go.State.Rotation;

            // The native side echoes back whatever the solver last published, so any difference
            // is a genuine editor drag, gizmo rotation or script teleport. Position and rotation
            // are checked separately: a rotate-only edit must still reach the body.
            bool moved = Vector3.Distance(native, entry.Shadow) >= 1e-4f;
            bool rotated = Vector3.Distance(nativeRotation, entry.LastRotation) >= 1e-3f;
            if (!moved && !rotated) continue;

            var transform = ComposeTransform(native, nativeRotation);
            if (entry.Trigger) entry.Ghost!.WorldTransform = transform;
            entry.Motion.WorldTransform = transform;
            entry.Body.WorldTransform = transform;
            entry.Body.InterpolationWorldTransform = transform;
            entry.Body.Activate(true);
            entry.Shadow = native;
            entry.LastRotation = nativeRotation;
        }
    }

    private static void PublishTransforms()
    {
        foreach (var go in Scene.gameObjects)
        {
            if (!Bodies.TryGetValue(go.State.Id, out var entry)) continue;
            if (!entry.Dynamic || entry.Trigger) continue;

            var transform = entry.Body.WorldTransform;
            var position = FromBullet(transform.Origin);
            go.State.Position = position;
            if (transform.Decompose(out BV3 _, out BQ rotation, out BV3 _))
                go.State.Rotation = QuaternionToEuler(rotation);
            entry.Shadow = position;
            // Keep the rotation shadow current so the next PushExternalMoves only reacts to
            // genuine external edits, not to the solver's own output echoing back.
            entry.LastRotation = go.State.Rotation;
        }
    }

    private static void DispatchContacts()
    {
        var liveCollision = new HashSet<long>();
        var liveTrigger = new HashSet<long>();
        var dispatcher = _world!.Dispatcher;

        for (int m = 0; m < dispatcher.NumManifolds; ++m)
        {
            var manifold = dispatcher.GetManifoldByIndexInternal(m);
            if (manifold.NumContacts == 0) continue;

            var colliderA = manifold.Body0?.UserObject as Collider;
            var colliderB = manifold.Body1?.UserObject as Collider;
            if (colliderA is null || colliderB is null) continue;
            if (colliderA.isTrigger || colliderB.isTrigger) continue;

            var goA = colliderA.gameObject;
            var goB = colliderB.gameObject;
            long key = PairKey(goA.State.Id, goB.State.Id);
            if (IgnoredPairs.Contains(key)) continue;
            liveCollision.Add(key);

            var contacts = new ContactPoint[manifold.NumContacts];
            for (int c = 0; c < manifold.NumContacts; ++c)
            {
                var point = manifold.GetContactPoint(c);
                contacts[c] = new ContactPoint
                {
                    point = FromBullet(point.PositionWorldOnA),
                    normal = FromBullet(point.NormalWorldOnB)
                };
            }

            Vector3 velocityA = VelocityOf(goA), velocityB = VelocityOf(goB);
            Vector3 relativeVelocity = velocityB - velocityA;
            float impulse = contacts.Length > 0 ? manifold.GetContactPoint(0).AppliedImpulse : 0f;
            Vector3 impulseVector = contacts.Length > 0 ? contacts[0].normal * impulse : Vector3.zero;

            bool entered = !CollisionContacts.ContainsKey(key);
            CollisionContacts[key] = (colliderA, colliderB);
            DispatchCollision(goA, goB, colliderB, relativeVelocity, impulseVector, contacts, entered);

            var flipped = new ContactPoint[contacts.Length];
            for (int c = 0; c < contacts.Length; ++c)
                flipped[c] = new ContactPoint { point = contacts[c].point, normal = -contacts[c].normal };
            DispatchCollision(goB, goA, colliderA, -relativeVelocity, -impulseVector, flipped, entered);
        }

        // Triggers come from the ghost overlap lists, since ghosts produce no contacts.
        foreach (var go in Scene.gameObjects)
        {
            if (!Bodies.TryGetValue(go.State.Id, out var entry) || entry.Ghost is null) continue;
            var self = go.GetComponent<Collider>();
            if (self is null) continue;

            for (int i = 0; i < entry.Ghost.NumOverlappingObjects; ++i)
            {
                var other = entry.Ghost.GetOverlappingObject(i)?.UserObject as Collider;
                if (other is null || ReferenceEquals(other, self)) continue;
                long key = PairKey(go.State.Id, other.gameObject.State.Id);
                if (IgnoredPairs.Contains(key)) continue;
                liveTrigger.Add(key);
                bool entered = !TriggerContacts.ContainsKey(key);
                TriggerContacts[key] = (self, other);
                DispatchTrigger(go, other, entered);
                DispatchTrigger(other.gameObject, self, entered);
            }
        }

        CloseOldContacts(CollisionContacts, liveCollision, false);
        CloseOldContacts(TriggerContacts, liveTrigger, true);
    }

    /// <summary>Runs one fixed physics step. Called from the host's fixed loop.</summary>
    public static void SimulateRuntimeStep(float fixedDeltaTime)
    {
        EnsureWorld();
        SyncBodies();
        PushExternalMoves();

        // Forces and constraints must be applied before the step: StepSimulation clears the
        // accumulated force of every body once it has integrated.
        foreach (var go in Scene.gameObjects)
        {
            if (!go.activeSelf) continue;
            foreach (var force in go.GetComponents<ConstantForce>()) force.Simulate(fixedDeltaTime);
            foreach (var body in go.GetComponents<Rigidbody>()) body.Simulate(fixedDeltaTime);
        }

        // Substepping keeps fast objects from skipping through thin geometry. The native
        // step length is capped so a long frame cannot let a body jump a wall.
        int subSteps = Math.Clamp(maxSubSteps, 1, 16);
        float step = MathF.Max(fixedTimeStep, 1e-4f);
        if (subSteps > 1 && fixedDeltaTime > step * 1.5f) subSteps = (int)MathF.Min(subSteps, MathF.Ceiling(fixedDeltaTime / step) + 1);
        _world!.StepSimulation(fixedDeltaTime, subSteps, step);

        PublishTransforms();
        DispatchContacts();

        foreach (var go in Scene.gameObjects)
        {
            if (!go.activeSelf) continue;
            foreach (var auto in go.GetComponents<AutoDestroy>()) auto.Tick(fixedDeltaTime);
            foreach (var aim in go.GetComponents<LookAtTarget>()) aim.Apply();
            foreach (var spin in go.GetComponents<Rotator>()) spin.Tick(fixedDeltaTime);
            foreach (var hover in go.GetComponents<Oscillator>()) hover.Tick(fixedDeltaTime);
            foreach (var kill in go.GetComponents<KillBelowY>()) kill.Tick(fixedDeltaTime);
            foreach (var pulse in go.GetComponents<MaterialPulse>()) pulse.Tick(fixedDeltaTime);
        }
    }

    // ---- accessors used by Rigidbody ----

    internal static bool TryGetBody(int id, out RigidBody body)
    {
        if (Bodies.TryGetValue(id, out var entry)) { body = entry.Body; return true; }
        body = null!;
        return false;
    }

    internal static Vector3 ReadVelocity(int id, Vector3 fallback)
        => TryGetBody(id, out var body) ? FromBullet(body.LinearVelocity) : fallback;

    internal static void WriteVelocity(int id, Vector3 value)
    {
        if (!TryGetBody(id, out var body)) return;
        body.LinearVelocity = ToBullet(value);
        body.Activate(true);
    }

    internal static Vector3 ReadAngularVelocity(int id, Vector3 fallback)
        => TryGetBody(id, out var body) ? FromBullet(body.AngularVelocity) : fallback;

    internal static void WriteAngularVelocity(int id, Vector3 value)
    {
        if (!TryGetBody(id, out var body)) return;
        body.AngularVelocity = ToBullet(value);
        body.Activate(true);
    }

    internal static void ApplyForce(int id, Vector3 force)
    {
        if (TryGetBody(id, out var body)) { body.ApplyCentralForce(ToBullet(force)); body.Activate(true); }
    }

    internal static void ApplyForceAtPosition(int id, Vector3 force, Vector3 worldPosition)
    {
        if (TryGetBody(id, out var body))
        {
            body.ApplyForce(ToBullet(force), ToBullet(worldPosition));
            body.Activate(true);
        }
    }

    internal static void ApplyImpulse(int id, Vector3 impulse)
    {
        if (TryGetBody(id, out var body)) { body.ApplyCentralImpulse(ToBullet(impulse)); body.Activate(true); }
    }

    internal static void ApplyTorque(int id, Vector3 torque)
    {
        if (TryGetBody(id, out var body)) { body.ApplyTorque(ToBullet(torque)); body.Activate(true); }
    }

    internal static void SetConstraints(int id, bool freezeX, bool freezeY, bool freezeZ,
        bool freezeRotX, bool freezeRotY, bool freezeRotZ)
    {
        if (!TryGetBody(id, out var body)) return;

        // The Bullet factors only scale applied force, torque and gravity; a velocity that a
        // script assigns directly is not masked by them. Unity's freeze does stop that motion,
        // so mask the velocity components too. This runs every frame before the step, which is
        // what keeps the axis pinned.
        body.LinearFactor = new BV3(freezeX ? 0f : 1f, freezeY ? 0f : 1f, freezeZ ? 0f : 1f);
        body.AngularFactor = new BV3(freezeRotX ? 0f : 1f, freezeRotY ? 0f : 1f, freezeRotZ ? 0f : 1f);

        var linear = body.LinearVelocity;
        var angular = body.AngularVelocity;
        body.LinearVelocity = new BV3(freezeX ? 0f : linear.X, freezeY ? 0f : linear.Y, freezeZ ? 0f : linear.Z);
        body.AngularVelocity = new BV3(freezeRotX ? 0f : angular.X, freezeRotY ? 0f : angular.Y, freezeRotZ ? 0f : angular.Z);
    }

    /// <summary>Caps how fast a body may spin, in degrees per second.</summary>
    internal static void ClampAngularVelocity(int id, float maxDegreesPerSecond)
    {
        if (!TryGetBody(id, out var body)) return;
        var angular = body.AngularVelocity;
        float radiansPerSecond = maxDegreesPerSecond * MathF.PI / 180f;
        float speed = angular.Length;
        if (speed > radiansPerSecond && speed > 1e-6f)
            body.AngularVelocity = angular * (radiansPerSecond / speed);
    }

    internal static void SetInterpolation(int id, bool enable)
    {
        if (!TryGetBody(id, out var body)) return;
        body.InterpolationWorldTransform = body.WorldTransform;
        if (!enable) body.InterpolationLinearVelocity = BV3.Zero;
    }

    internal static void Sleep(int id) { if (TryGetBody(id, out var body)) body.ActivationState = ActivationState.IslandSleeping; }
    internal static void WakeUp(int id) { if (TryGetBody(id, out var body)) body.Activate(true); }
    internal static bool IsSleeping(int id) => TryGetBody(id, out var body) && !body.IsActive;

    // ---- queries ----

    public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hit, float maxDistance = 1000f)
        => Raycast(new Ray(origin, direction), out hit, maxDistance);

    public static bool Raycast(Ray ray, out RaycastHit hit, float maxDistance = 1000f)
    {
        EnsureWorld();
        hit = default;
        var from = ToBullet(ray.origin);
        var to = ToBullet(ray.origin + ray.direction * MathF.Max(0f, maxDistance));
        using var callback = new ClosestRayResultCallback(ref from, ref to);
        _world!.RayTestRef(ref from, ref to, callback);
        if (!callback.HasHit) return false;

        var collider = callback.CollisionObject?.UserObject as Collider;
        var point = FromBullet(callback.HitPointWorld);
        hit = new RaycastHit
        {
            point = point,
            normal = FromBullet(callback.HitNormalWorld),
            distance = Vector3.Distance(ray.origin, point),
            collider = collider
        };
        return true;
    }

    /// <summary>Every collider a ray passes through, nearest first.</summary>
    public static RaycastHit[] RaycastAll(Vector3 origin, Vector3 direction, float maxDistance = 1000f)
    {
        EnsureWorld();
        var from = ToBullet(origin);
        var to = ToBullet(origin + direction.normalized * MathF.Max(0f, maxDistance));
        using var callback = new AllHitsRayResultCallback(from, to);
        _world!.RayTestRef(ref from, ref to, callback);

        var results = new List<RaycastHit>();
        for (int i = 0; i < callback.CollisionObjects.Count; ++i)
        {
            var point = FromBullet(callback.HitPointWorld[i]);
            results.Add(new RaycastHit
            {
                point = point,
                normal = FromBullet(callback.HitNormalWorld[i]),
                distance = Vector3.Distance(origin, point),
                collider = callback.CollisionObjects[i]?.UserObject as Collider
            });
        }
        return results.OrderBy(h => h.distance).ToArray();
    }

    public static Collider[] OverlapSphere(Vector3 center, float radius)
    {
        var results = new List<Collider>();
        foreach (var go in Scene.gameObjects)
        {
            var collider = go.GetComponent<Collider>();
            if (collider is null || !collider.enabled) continue;
            if (Vector3.Distance(center, go.transform.position) <= radius + collider.effectiveRadius)
                results.Add(collider);
        }
        return results.ToArray();
    }

    /// <summary>Stops two colliders from reporting contacts with each other.</summary>
    public static void IgnoreCollision(Collider a, Collider b, bool ignore = true)
    {
        long key = PairKey(a.gameObject.State.Id, b.gameObject.State.Id);
        if (ignore)
        {
            IgnoredPairs.Add(key);
            if (Bodies.TryGetValue(a.gameObject.State.Id, out var entryA) &&
                Bodies.TryGetValue(b.gameObject.State.Id, out var entryB))
            {
                entryA.Body.SetIgnoreCollisionCheck(entryB.Body, true);
            }
        }
        else
        {
            IgnoredPairs.Remove(key);
            if (Bodies.TryGetValue(a.gameObject.State.Id, out var entryA) &&
                Bodies.TryGetValue(b.gameObject.State.Id, out var entryB))
            {
                entryA.Body.SetIgnoreCollisionCheck(entryB.Body, false);
            }
        }
    }

    // ---- callbacks ----

    private static Vector3 VelocityOf(GameObject go)
    {
        var rb = go.GetComponent<Rigidbody>();
        return rb is { enabled: true, isKinematic: false } ? rb.velocity : Vector3.zero;
    }

    private static void DispatchCollision(GameObject target, GameObject other, Collider otherCollider,
        Vector3 relativeVelocity, Vector3 impulse, ContactPoint[] contacts, bool entered)
    {
        var collision = new Collision
        {
            gameObject = other,
            collider = otherCollider,
            relativeVelocity = relativeVelocity,
            impulse = impulse,
            contacts = contacts
        };
        foreach (var script in target.GetComponents<MonoBehaviour>())
        {
            if (!script.enabled || !script.gameObject.activeSelf) continue;
            if (entered) script.OnCollisionEnter(collision);
            else script.OnCollisionStay(collision);
        }
    }

    private static void DispatchTrigger(GameObject target, Collider other, bool entered)
    {
        foreach (var script in target.GetComponents<MonoBehaviour>())
        {
            if (!script.enabled || !script.gameObject.activeSelf) continue;
            if (entered) script.OnTriggerEnter(other);
            else script.OnTriggerStay(other);
        }
    }

    private static void CloseOldContacts(Dictionary<long, (Collider A, Collider B)> active, HashSet<long> live, bool trigger)
    {
        if (active.Count == 0) return;
        foreach (var (key, pair) in active.ToArray())
        {
            if (live.Contains(key)) continue;
            active.Remove(key);
            if (pair.A?.gameObject is not { } goA || pair.B?.gameObject is not { } goB) continue;
            if (Scene.FindById(goA.State.Id) is null || Scene.FindById(goB.State.Id) is null) continue;
            if (trigger)
            {
                DispatchTriggerExit(goA, pair.B);
                DispatchTriggerExit(goB, pair.A);
            }
            else
            {
                var contacts = new[] { new ContactPoint { point = goA.transform.position, normal = Vector3.zero } };
                DispatchCollision(goA, goB, pair.B, Vector3.zero, Vector3.zero, contacts, false);
                DispatchCollision(goB, goA, pair.A, Vector3.zero, Vector3.zero, contacts, false);
            }
        }
    }

    private static void DispatchTriggerExit(GameObject target, Collider other)
    {
        foreach (var script in target.GetComponents<MonoBehaviour>())
        {
            if (!script.enabled || !script.gameObject.activeSelf) continue;
            script.OnTriggerExit(other);
        }
    }
}
