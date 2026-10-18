using System.Collections.Concurrent;
using System.Globalization;
using System.Reflection;

namespace ForgeEngine;

/// <summary>A 2D vector with Unity-style lowercase fields.</summary>
public struct Vector2
{
    public float x;
    public float y;
    public Vector2(float x, float y) { this.x = x; this.y = y; }
    public float magnitude => MathF.Sqrt(x * x + y * y);
    public Vector2 normalized => magnitude > 1e-6f ? this / magnitude : zero;
    public static Vector2 zero => new(0, 0);
    public static Vector2 one => new(1, 1);
    public static Vector2 right => new(1, 0);
    public static Vector2 up => new(0, 1);
    public static float Dot(Vector2 a, Vector2 b) => a.x*b.x + a.y*b.y;
    public static float Distance(Vector2 a, Vector2 b) => (a-b).magnitude;
    public static Vector2 Lerp(Vector2 a, Vector2 b, float t) => a + (b-a)*Mathf.Clamp01(t);
    public static Vector2 Min(Vector2 a, Vector2 b) => new(MathF.Min(a.x,b.x), MathF.Min(a.y,b.y));
    public static Vector2 Max(Vector2 a, Vector2 b) => new(MathF.Max(a.x,b.x), MathF.Max(a.y,b.y));
    public static Vector2 operator +(Vector2 a, Vector2 b) => new(a.x + b.x, a.y + b.y);
    public static Vector2 operator -(Vector2 a, Vector2 b) => new(a.x - b.x, a.y - b.y);
    public static Vector2 operator *(Vector2 a, float b) => new(a.x * b, a.y * b);
    public static Vector2 operator /(Vector2 a, float b) => MathF.Abs(b) < 1e-6f ? zero : new(a.x / b, a.y / b);
    public override string ToString() => $"({x:0.###}, {y:0.###})";
}

/// <summary>A 3D vector. Transform rotations are expressed as Euler degrees.</summary>
public struct Vector3
{
    public float x;
    public float y;
    public float z;
    public Vector3(float x, float y, float z) { this.x = x; this.y = y; this.z = z; }
    public float magnitude => MathF.Sqrt(x * x + y * y + z * z);
    public float sqrMagnitude => x * x + y * y + z * z;
    public Vector3 normalized => magnitude > 1e-6f ? this / magnitude : zero;
    public static Vector3 zero => new(0, 0, 0);
    public static Vector3 one => new(1, 1, 1);
    public static Vector3 up => new(0, 1, 0);
    public static Vector3 down => new(0, -1, 0);
    public static Vector3 right => new(1, 0, 0);
    public static Vector3 left => new(-1, 0, 0);
    public static Vector3 forward => new(0, 0, 1);
    public static Vector3 back => new(0, 0, -1);
    public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.x + b.x, a.y + b.y, a.z + b.z);
    public static Vector3 operator -(Vector3 a, Vector3 b) => new(a.x - b.x, a.y - b.y, a.z - b.z);
    public static Vector3 operator -(Vector3 a) => new(-a.x, -a.y, -a.z);
    public static Vector3 operator *(Vector3 a, float b) => new(a.x * b, a.y * b, a.z * b);
    public static Vector3 operator *(float b, Vector3 a) => a * b;
    public static Vector3 operator /(Vector3 a, float b) => MathF.Abs(b) < 1e-6f ? zero : new(a.x / b, a.y / b, a.z / b);
    public static bool operator ==(Vector3 a, Vector3 b) => MathF.Abs(a.x-b.x)<1e-6f && MathF.Abs(a.y-b.y)<1e-6f && MathF.Abs(a.z-b.z)<1e-6f;
    public static bool operator !=(Vector3 a, Vector3 b) => !(a == b);
    public static float Dot(Vector3 a, Vector3 b) => a.x*b.x + a.y*b.y + a.z*b.z;
    public static Vector3 Cross(Vector3 a, Vector3 b) => new(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
    public static float Distance(Vector3 a, Vector3 b) => (a-b).magnitude;
    public static float Angle(Vector3 from, Vector3 to)
    {
        float denominator = MathF.Sqrt(from.sqrMagnitude * to.sqrMagnitude);
        if (denominator < 1e-6f) return 0;
        return MathF.Acos(Mathf.Clamp(Dot(from, to) / denominator, -1, 1)) * Mathf.Rad2Deg;
    }
    public static Vector3 Lerp(Vector3 a, Vector3 b, float t) => a + (b-a)*Mathf.Clamp01(t);
    public static Vector3 MoveTowards(Vector3 current, Vector3 target, float maxDistanceDelta)
    {
        Vector3 delta = target-current; float distance = delta.magnitude;
        return distance <= maxDistanceDelta || distance < 1e-6f ? target : current + delta/distance*maxDistanceDelta;
    }
    public static Vector3 ClampMagnitude(Vector3 value, float maxLength)
        => value.sqrMagnitude > maxLength*maxLength ? value.normalized*maxLength : value;
    public static Vector3 Project(Vector3 vector, Vector3 onNormal)
        => onNormal.sqrMagnitude < 1e-6f ? zero : onNormal * (Dot(vector, onNormal) / onNormal.sqrMagnitude);
    public static Vector3 Reflect(Vector3 inDirection, Vector3 inNormal)
        => inDirection - 2f * Dot(inDirection, inNormal) * inNormal;
    public static Vector3 Min(Vector3 a, Vector3 b) => new(MathF.Min(a.x,b.x), MathF.Min(a.y,b.y), MathF.Min(a.z,b.z));
    public static Vector3 Max(Vector3 a, Vector3 b) => new(MathF.Max(a.x,b.x), MathF.Max(a.y,b.y), MathF.Max(a.z,b.z));
    public static Vector3 SmoothDamp(Vector3 current, Vector3 target, ref Vector3 currentVelocity, float smoothTime, float maxSpeed = float.PositiveInfinity)
    {
        float deltaTime = Time.deltaTime;
        smoothTime = MathF.Max(0.0001f, smoothTime);
        float omega = 2f / smoothTime;
        float x = omega * deltaTime;
        float exp = 1f / (1f + x + 0.48f*x*x + 0.235f*x*x*x);
        Vector3 change = current - target;
        float maxChange = maxSpeed * smoothTime;
        change = ClampMagnitude(change, maxChange);
        Vector3 temp = (currentVelocity + omega * change) * deltaTime;
        currentVelocity = (currentVelocity - omega * temp) * exp;
        return target + (change + temp) * exp;
    }
    public static Vector3 RotateTowards(Vector3 current, Vector3 target, float maxRadiansDelta)
    {
        float angle = Angle(current, target);
        if (angle <= maxRadiansDelta * Mathf.Rad2Deg) return target;
        return Lerp(current, target, maxRadiansDelta * Mathf.Rad2Deg / angle);
    }
    public override bool Equals(object? obj) => obj is Vector3 v && this == v;
    public override int GetHashCode() => HashCode.Combine(x,y,z);
    public override string ToString() => $"({x:0.###}, {y:0.###}, {z:0.###})";
}

/// <summary>Four-component vector useful for homogeneous coordinates and custom math.</summary>
public struct Vector4
{
    public float x, y, z, w;
    public Vector4(float x, float y, float z, float w) { this.x=x; this.y=y; this.z=z; this.w=w; }
    public static Vector4 zero => new(0,0,0,0);
    public static Vector4 one => new(1,1,1,1);
    public static Vector4 operator +(Vector4 a, Vector4 b) => new(a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w);
    public static Vector4 operator -(Vector4 a, Vector4 b) => new(a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w);
    public static Vector4 operator *(Vector4 a, float b) => new(a.x*b,a.y*b,a.z*b,a.w*b);
    public override string ToString() => $"({x:0.###}, {y:0.###}, {z:0.###}, {w:0.###})";
}

/// <summary>RGBA color with normalized 0..1 channels.</summary>
public struct Color
{
    public float r, g, b, a;
    public Color(float r, float g, float b, float a = 1f) { this.r=Mathf.Clamp01(r); this.g=Mathf.Clamp01(g); this.b=Mathf.Clamp01(b); this.a=Mathf.Clamp01(a); }
    public static Color white => new(1,1,1,1);
    public static Color black => new(0,0,0,1);
    public static Color red => new(1,0,0,1);
    public static Color green => new(0,1,0,1);
    public static Color blue => new(0,0,1,1);
    public static Color clear => new(0,0,0,0);
    public static Color gray => new(0.5f,0.5f,0.5f,1);
    public static Color cyan => new(0,1,1,1);
    public static Color magenta => new(1,0,1,1);
    public static Color yellow => new(1,0.92f,0.016f,1);
    public static Color orange => new(1,0.5f,0.15f,1);
    public static Color operator *(Color c, float k) => new(c.r*k, c.g*k, c.b*k, c.a*k);
    public static Color operator +(Color a, Color b) => new(a.r+b.r, a.g+b.g, a.b+b.b, a.a+b.a);
    public float grayscale => 0.2126f*r + 0.7152f*g + 0.0722f*b;
    public static Color Lerp(Color a, Color b, float t) => new(Mathf.Lerp(a.r,b.r,t),Mathf.Lerp(a.g,b.g,t),Mathf.Lerp(a.b,b.b,t),Mathf.Lerp(a.a,b.a,t));
    public override string ToString() => $"RGBA({r:0.###}, {g:0.###}, {b:0.###}, {a:0.###})";
}

public enum Space { World, Self }
public enum LightType { None = 0, Directional = 1, Point = 2, Spot = 3 }

/// <summary>Rotation quaternion. Euler angles use the same X-then-Y-then-Z order as Transform.rotation.</summary>
public struct Quaternion
{
    public float x, y, z, w;
    public Quaternion(float x, float y, float z, float w) { this.x=x; this.y=y; this.z=z; this.w=w; }
    public static Quaternion identity => new(0, 0, 0, 1);
    public float magnitude => MathF.Sqrt(x*x + y*y + z*z + w*w);
    public Quaternion normalized { get { float m = magnitude; return m > 1e-6f ? new(x/m, y/m, z/m, w/m) : identity; } }

    public static Quaternion AngleAxis(float degrees, Vector3 axis)
    {
        Vector3 unit = axis.normalized;
        if (unit.sqrMagnitude < 1e-8f) return identity;
        float rad = degrees * Mathf.Deg2Rad * 0.5f;
        float s = MathF.Sin(rad);
        return new Quaternion(unit.x*s, unit.y*s, unit.z*s, MathF.Cos(rad));
    }

    /// <summary>Multiplies two rotations; b is applied first, then a (Unity convention).</summary>
    public static Quaternion operator *(Quaternion a, Quaternion b) => new(
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z);

    public static Quaternion Euler(float xDegrees, float yDegrees, float zDegrees)
        => mul(AngleAxis(zDegrees, Vector3.forward), mul(AngleAxis(yDegrees, Vector3.up), AngleAxis(xDegrees, Vector3.right)));
    public static Quaternion Euler(Vector3 eulerDegrees) => Euler(eulerDegrees.x, eulerDegrees.y, eulerDegrees.z);
    public static Quaternion LookRotation(Vector3 forward)
    {
        Vector3 unit = forward.normalized;
        if (unit.sqrMagnitude < 1e-8f) return identity;
        float pitch = -MathF.Asin(Mathf.Clamp(unit.y, -1, 1)) * Mathf.Rad2Deg;
        float yaw = MathF.Atan2(unit.x, unit.z) * Mathf.Rad2Deg;
        return Euler(pitch, yaw, 0);
    }

    public Vector3 eulerAngles
    {
        get
        {
            Quaternion q = normalized;
            float sinY = Mathf.Clamp(2f*(q.y*q.w - q.x*q.z), -1f, 1f);
            float yDeg = MathF.Asin(sinY) * Mathf.Rad2Deg;
            float xDeg = MathF.Atan2(2f*(q.x*q.w + q.y*q.z), 1f - 2f*(q.x*q.x + q.y*q.y)) * Mathf.Rad2Deg;
            float zDeg = MathF.Atan2(2f*(q.z*q.w + q.x*q.y), 1f - 2f*(q.y*q.y + q.z*q.z)) * Mathf.Rad2Deg;
            // Normalize into [0, 360) so values round-trip through Transform.rotation predictably.
            return new Vector3(Wrap360(xDeg), Wrap360(yDeg), Wrap360(zDeg));
        }
        set => this = Euler(value);
    }

    public static float Dot(Quaternion a, Quaternion b) => a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    public static Quaternion Lerp(Quaternion a, Quaternion b, float t)
    {
        t = Mathf.Clamp01(t);
        if (Dot(a, b) < 0f) b = new(-b.x, -b.y, -b.z, -b.w);
        return new Quaternion(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t).normalized;
    }
    public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
    {
        t = Mathf.Clamp01(t);
        float dot = Dot(a, b);
        if (dot < 0f) { b = new(-b.x, -b.y, -b.z, -b.w); dot = -dot; }
        if (dot > 0.9995f) return Lerp(a, b, t);
        float theta0 = MathF.Acos(dot);
        float theta = theta0 * t;
        float sinTheta = MathF.Sin(theta);
        float sinTheta0 = MathF.Sin(theta0);
        float s0 = MathF.Cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;
        return new Quaternion(s0*a.x+s1*b.x, s0*a.y+s1*b.y, s0*a.z+s1*b.z, s0*a.w+s1*b.w).normalized;
    }
    public Quaternion inverse => new(-x, -y, -z, w);
    public static Quaternion Inverse(Quaternion q) => q.inverse;
    public static Quaternion RotateTowards(Quaternion current, Quaternion target, float maxDegreesDelta)
    {
        float angle = Angle(current, target);
        return angle <= maxDegreesDelta ? target : Slerp(current, target, maxDegreesDelta / angle);
    }
    public static float Angle(Quaternion a, Quaternion b)
    {
        float dot = MathF.Abs(Dot(a, b));
        return MathF.Acos(Mathf.Clamp(dot, -1f, 1f)) * 2f * Mathf.Rad2Deg;
    }
    public static Vector3 operator *(Quaternion q, Vector3 v)
    {
        // Rotate a vector by the quaternion (matches Transform.TransformDirection for the same euler angles).
        Quaternion p = new(v.x, v.y, v.z, 0f);
        Quaternion conjugate = new(-q.x, -q.y, -q.z, q.w);
        Quaternion result = mul(q, mul(p, conjugate));
        return new Vector3(result.x, result.y, result.z);
    }
    private static Quaternion mul(Quaternion a, Quaternion b) => a * b;
    private static float Wrap360(float value) { float v = value % 360f; return v < 0f ? v + 360f : v; }
    public override string ToString() => $"Quat({x:0.###}, {y:0.###}, {z:0.###}, {w:0.###})";
}

/// <summary>Base type for components returned by GameObject.GetComponent.</summary>
public abstract class Component
{
    public GameObject gameObject { get; private set; } = null!;
    public Transform transform => gameObject.transform;
    public bool enabled { get; set; } = true;
    internal bool NativeOwned { get; set; }
    public string name => gameObject.name;
    public T? GetComponent<T>() where T : Component => gameObject.GetComponent<T>();
    public T[] GetComponents<T>() where T : Component => gameObject.GetComponents<T>();
    public void Bind(GameObject owner) => gameObject = owner;
}

/// <summary>Transform component. Position is world-space; rotation is Euler degrees; scale is local scale.</summary>
public sealed class Transform : Component
{
    private readonly RuntimeObjectData _state;
    internal Transform(RuntimeObjectData state) => _state = state;
    public Vector3 position { get => _state.Position; set => _state.Position = value; }
    public Vector3 localPosition { get => position; set => position = value; }
    public Vector3 rotation { get => _state.Rotation; set => _state.Rotation = value; }
    public Vector3 eulerAngles { get => rotation; set => rotation = value; }
    public Vector3 localEulerAngles { get => rotation; set => rotation = value; }
    public Quaternion quaternion { get => Quaternion.Euler(rotation); set => rotation = value.eulerAngles; }
    public Vector3 localScale { get => _state.Scale; set => _state.Scale = value; }
    public Vector3 lossyScale { get => _state.Scale; }
    public Vector3 forward => RotateDirection(Vector3.forward);
    public Vector3 right => RotateDirection(Vector3.right);
    public Vector3 up => RotateDirection(Vector3.up);

    public void Translate(Vector3 translation, Space relativeTo = Space.Self)
        => position += relativeTo == Space.World ? translation : RotateDirection(translation);
    public void Rotate(Vector3 eulerDegrees, Space relativeTo = Space.Self)
        => rotation += eulerDegrees;
    public void Rotate(float xAngle, float yAngle, float zAngle, Space relativeTo = Space.Self)
        => Rotate(new Vector3(xAngle, yAngle, zAngle), relativeTo);
    public void Rotate(Quaternion orientation) => quaternion = orientation * quaternion;
    public Vector3 TransformDirection(Vector3 direction) => RotateDirection(direction);
    public Vector3 InverseTransformDirection(Vector3 direction)
    {
        // Inverse of the XYZ Euler rotation; rotation is applied in reverse order.
        float rx = -rotation.x * Mathf.Deg2Rad, ry = -rotation.y * Mathf.Deg2Rad, rz = -rotation.z * Mathf.Deg2Rad;
        float cx = MathF.Cos(rx), sx = MathF.Sin(rx), cy = MathF.Cos(ry), sy = MathF.Sin(ry), cz = MathF.Cos(rz), sz = MathF.Sin(rz);
        float x1 = cz*direction.x - sz*direction.y, y1 = sz*direction.x + cz*direction.y, z1 = direction.z;
        float x2 = cy*x1 + sy*z1, y2 = y1, z2 = -sy*x1 + cy*z1;
        return new Vector3(x2, cx*y2 - sx*z2, sx*y2 + cx*z2);
    }
    public Vector3 TransformPoint(Vector3 point) => position + RotateDirection(new Vector3(point.x*localScale.x, point.y*localScale.y, point.z*localScale.z));
    public Vector3 InverseTransformPoint(Vector3 point)
    {
        Vector3 local = InverseTransformDirection(point - position);
        return new Vector3(MathF.Abs(localScale.x)<1e-6f?0:local.x/localScale.x,
            MathF.Abs(localScale.y)<1e-6f?0:local.y/localScale.y,
            MathF.Abs(localScale.z)<1e-6f?0:local.z/localScale.z);
    }
    public void LookAt(Vector3 target)
    {
        Vector3 direction = (target - position).normalized;
        if (direction.sqrMagnitude < 1e-8f) return;
        float pitch = -MathF.Asin(Mathf.Clamp(direction.y, -1, 1)) * Mathf.Rad2Deg;
        float yaw = MathF.Atan2(direction.x, direction.z) * Mathf.Rad2Deg;
        rotation = new Vector3(pitch, yaw, 0);
    }

    private Vector3 RotateDirection(Vector3 v)
    {
        float rx = rotation.x * Mathf.Deg2Rad, ry = rotation.y * Mathf.Deg2Rad, rz = rotation.z * Mathf.Deg2Rad;
        float cx = MathF.Cos(rx), sx = MathF.Sin(rx), cy = MathF.Cos(ry), sy = MathF.Sin(ry), cz = MathF.Cos(rz), sz = MathF.Sin(rz);
        // XYZ Euler rotation, consistent for common movement scripts.
        float x1 = v.x, y1 = cx*v.y - sx*v.z, z1 = sx*v.y + cx*v.z;
        float x2 = cy*x1 + sy*z1, y2 = y1, z2 = -sy*x1 + cy*z1;
        return new Vector3(cz*x2 - sz*y2, sz*x2 + cz*y2, z2);
    }
}

/// <summary>Runtime state synchronized with the native DirectX 12 scene each frame.</summary>
public sealed class RuntimeObjectData
{
    public int Id { get; set; }
    public string Name { get; set; } = "GameObject";
    public Vector3 Position { get; set; }
    public Vector3 Rotation { get; set; }
    public Vector3 Scale { get; set; } = Vector3.one;
    public bool HasMeshRenderer { get; set; } = true;
    public bool MeshRendererEnabled { get; set; } = true;
    public bool HasCollider { get; set; }
    public bool HasAudioSource { get; set; }
    public bool HasCamera { get; set; }
    public int LightType { get; set; }
    public bool LightEnabled { get; set; } = true;
    public Vector3 LightColor { get; set; } = Vector3.one;
    public float LightIntensity { get; set; } = 1f;
    public float LightRange { get; set; } = 10f;
    public float LightInnerAngle { get; set; } = 18f;
    public float LightOuterAngle { get; set; } = 30f;
    public string Tag { get; set; } = "Untagged";
    public int Layer { get; set; }
    public int ColliderType { get; set; } // 0=Box, 1=Sphere, 2=Capsule
    public Vector3 ColliderSize { get; set; } = Vector3.one;

    /// <summary>Offset of the collision shape from the object origin, in local space.</summary>
    public Vector3 ColliderCenter { get; set; }

    /// <summary>Native hierarchy parent, or -1 for a root object.</summary>
    public int ParentIndex { get; set; } = -1;
    public bool IsTrigger { get; set; }
    public bool HasRigidbody { get; set; }
    public float Mass { get; set; } = 1f;
    public bool UseGravity { get; set; } = true;
    public bool IsKinematic { get; set; }
    public bool HasMaterial { get; set; } = true;
    public Vector3 BaseColor { get; set; } = Vector3.one;
    public float Metallic { get; set; }
    public float Roughness { get; set; } = 0.6f;
    public Vector3 EmissiveColor { get; set; }
    public float EmissiveIntensity { get; set; }
    public float NormalMapScale { get; set; } = 1f;
    public bool DoubleSided { get; set; }
    public float CameraFieldOfView { get; set; } = 60f;
    public float CameraNearClip { get; set; } = 0.05f;
    public float CameraFarClip { get; set; } = 1000f;
    public bool CameraOrthographic { get; set; }
    public float CameraOrthographicSize { get; set; } = 5f;
    public int CameraDepth { get; set; }
    public Vector4 CameraBackgroundColor { get; set; } = new Vector4(0.02f, 0.03f, 0.05f, 1f);
    public bool CameraDrawSkybox { get; set; } = true;
    public bool CameraDrawGizmos { get; set; } = true;
    public int CameraCullingMask { get; set; } = 0x7FFFFFFF;

    // --- Character controller -----------------------------------------------------
    public bool HasCharacterController { get; set; }
    public float CharacterHeight { get; set; } = 1.8f;
    public float CharacterRadius { get; set; } = 0.3f;
    public float CharacterSlopeLimit { get; set; } = 45f;
    public float CharacterStepOffset { get; set; } = 0.3f;
    public float CharacterSkinWidth { get; set; } = 0.02f;
    public float CharacterGravity { get; set; } = -9.81f;
    public float CharacterSpeed { get; set; } = 6f;
    public float CharacterJumpHeight { get; set; } = 1.2f;
    public Vector3 CharacterVelocity { get; set; }
    public bool CharacterIsGrounded { get; set; }

    // --- Particle system ----------------------------------------------------------
    public bool HasParticleSystem { get; set; }
    public int ParticleMaxCount { get; set; } = 512;
    public float ParticleLifetime { get; set; } = 2f;
    public float ParticleStartSpeed { get; set; } = 2f;
    public float ParticleStartSize { get; set; } = 0.15f;
    public float ParticleGravity { get; set; } = -1.5f;
    public float ParticleEmissionRate { get; set; } = 40f;
    public Vector3 ParticleStartColor { get; set; } = new Vector3(1f, 0.85f, 0.45f);
    public Vector3 ParticleDirection { get; set; } = new Vector3(0f, 1f, 0f);
    public float ParticleSpread { get; set; } = 20f;
    public bool ParticleLooping { get; set; } = true;

    // --- Animator -----------------------------------------------------------------
    public bool HasAnimator { get; set; }
    public string AnimatorClipName { get; set; } = string.Empty;
    public float AnimatorSpeed { get; set; } = 1f;
    public bool AnimatorLoop { get; set; } = true;
    public bool AnimatorPlaying { get; set; } = true;
    public float AnimatorTime { get; set; }

    // --- Audio listener / source --------------------------------------------------
    public bool HasAudioListener { get; set; }
    public float AudioListenerVolume { get; set; } = 1f;
    public float AudioSourceVolume { get; set; } = 1f;
    public float AudioSourcePitch { get; set; } = 1f;
    public float AudioSourceMinDistance { get; set; } = 1f;
    public float AudioSourceMaxDistance { get; set; } = 500f;
    public bool AudioSourceLoop { get; set; }
    public bool AudioSourcePlayOnAwake { get; set; }
    public string AudioSourceClipPath { get; set; } = string.Empty;
    public bool AudioSourceSpatial { get; set; } = true;

    public RuntimeObjectData Clone(int newId)
        => new()
        {
            Id = newId, Name = Name, Position = Position, Rotation = Rotation, Scale = Scale,
            HasMeshRenderer = HasMeshRenderer, MeshRendererEnabled = MeshRendererEnabled,
            HasCollider = HasCollider, HasAudioSource = HasAudioSource, HasCamera = HasCamera,
            LightType = LightType, LightEnabled = LightEnabled, LightColor = LightColor,
            LightIntensity = LightIntensity, LightRange = LightRange,
            LightInnerAngle = LightInnerAngle, LightOuterAngle = LightOuterAngle,
            Tag = Tag, Layer = Layer, ColliderType = ColliderType, ColliderSize = ColliderSize,
            ColliderCenter = ColliderCenter, ParentIndex = ParentIndex,
            IsTrigger = IsTrigger, HasRigidbody = HasRigidbody, Mass = Mass,
            UseGravity = UseGravity, IsKinematic = IsKinematic,
            HasMaterial = HasMaterial, BaseColor = BaseColor, Metallic = Metallic, Roughness = Roughness,
            EmissiveColor = EmissiveColor, EmissiveIntensity = EmissiveIntensity,
            NormalMapScale = NormalMapScale, DoubleSided = DoubleSided,
            CameraFieldOfView = CameraFieldOfView, CameraNearClip = CameraNearClip,
            CameraFarClip = CameraFarClip, CameraOrthographic = CameraOrthographic,
            CameraOrthographicSize = CameraOrthographicSize, CameraDepth = CameraDepth,
            CameraBackgroundColor = CameraBackgroundColor, CameraDrawSkybox = CameraDrawSkybox,
            CameraDrawGizmos = CameraDrawGizmos, CameraCullingMask = CameraCullingMask,
            HasCharacterController = HasCharacterController, CharacterHeight = CharacterHeight,
            CharacterRadius = CharacterRadius, CharacterSlopeLimit = CharacterSlopeLimit,
            CharacterStepOffset = CharacterStepOffset, CharacterSkinWidth = CharacterSkinWidth,
            CharacterGravity = CharacterGravity, CharacterSpeed = CharacterSpeed,
            CharacterJumpHeight = CharacterJumpHeight, CharacterVelocity = CharacterVelocity,
            CharacterIsGrounded = CharacterIsGrounded,
            HasParticleSystem = HasParticleSystem, ParticleMaxCount = ParticleMaxCount,
            ParticleLifetime = ParticleLifetime, ParticleStartSpeed = ParticleStartSpeed,
            ParticleStartSize = ParticleStartSize, ParticleGravity = ParticleGravity,
            ParticleEmissionRate = ParticleEmissionRate, ParticleStartColor = ParticleStartColor,
            ParticleDirection = ParticleDirection, ParticleSpread = ParticleSpread,
            ParticleLooping = ParticleLooping,
            HasAnimator = HasAnimator, AnimatorClipName = AnimatorClipName,
            AnimatorSpeed = AnimatorSpeed, AnimatorLoop = AnimatorLoop,
            AnimatorPlaying = AnimatorPlaying, AnimatorTime = AnimatorTime,
            HasAudioListener = HasAudioListener, AudioListenerVolume = AudioListenerVolume,
            AudioSourceVolume = AudioSourceVolume, AudioSourcePitch = AudioSourcePitch,
            AudioSourceMinDistance = AudioSourceMinDistance, AudioSourceMaxDistance = AudioSourceMaxDistance,
            AudioSourceLoop = AudioSourceLoop, AudioSourcePlayOnAwake = AudioSourcePlayOnAwake,
            AudioSourceClipPath = AudioSourceClipPath, AudioSourceSpatial = AudioSourceSpatial
        };

    public void CopyFrom(RuntimeObjectData value)
    {
        Id=value.Id; Name=value.Name; Position=value.Position; Rotation=value.Rotation; Scale=value.Scale;
        HasMeshRenderer=value.HasMeshRenderer; MeshRendererEnabled=value.MeshRendererEnabled;
        HasCollider=value.HasCollider; HasAudioSource=value.HasAudioSource; HasCamera=value.HasCamera;
        LightType=value.LightType; LightEnabled=value.LightEnabled; LightColor=value.LightColor;
        LightIntensity=value.LightIntensity; LightRange=value.LightRange;
        LightInnerAngle=value.LightInnerAngle; LightOuterAngle=value.LightOuterAngle;
        Tag=value.Tag; Layer=value.Layer; ColliderType=value.ColliderType; ColliderSize=value.ColliderSize;
        ColliderCenter=value.ColliderCenter; ParentIndex=value.ParentIndex;
        IsTrigger=value.IsTrigger; HasRigidbody=value.HasRigidbody; Mass=value.Mass;
        UseGravity=value.UseGravity; IsKinematic=value.IsKinematic;
        HasMaterial=value.HasMaterial; BaseColor=value.BaseColor; Metallic=value.Metallic;
        Roughness=value.Roughness; EmissiveColor=value.EmissiveColor;
        EmissiveIntensity=value.EmissiveIntensity; NormalMapScale=value.NormalMapScale;
        DoubleSided=value.DoubleSided;
        CameraFieldOfView=value.CameraFieldOfView; CameraNearClip=value.CameraNearClip;
        CameraFarClip=value.CameraFarClip; CameraOrthographic=value.CameraOrthographic;
        CameraOrthographicSize=value.CameraOrthographicSize; CameraDepth=value.CameraDepth;
        CameraBackgroundColor=value.CameraBackgroundColor; CameraDrawSkybox=value.CameraDrawSkybox;
        CameraDrawGizmos=value.CameraDrawGizmos; CameraCullingMask=value.CameraCullingMask;
        HasCharacterController=value.HasCharacterController; CharacterHeight=value.CharacterHeight;
        CharacterRadius=value.CharacterRadius; CharacterSlopeLimit=value.CharacterSlopeLimit;
        CharacterStepOffset=value.CharacterStepOffset; CharacterSkinWidth=value.CharacterSkinWidth;
        CharacterGravity=value.CharacterGravity; CharacterSpeed=value.CharacterSpeed;
        CharacterJumpHeight=value.CharacterJumpHeight; CharacterVelocity=value.CharacterVelocity;
        CharacterIsGrounded=value.CharacterIsGrounded;
        HasParticleSystem=value.HasParticleSystem; ParticleMaxCount=value.ParticleMaxCount;
        ParticleLifetime=value.ParticleLifetime; ParticleStartSpeed=value.ParticleStartSpeed;
        ParticleStartSize=value.ParticleStartSize; ParticleGravity=value.ParticleGravity;
        ParticleEmissionRate=value.ParticleEmissionRate; ParticleStartColor=value.ParticleStartColor;
        ParticleDirection=value.ParticleDirection; ParticleSpread=value.ParticleSpread;
        ParticleLooping=value.ParticleLooping;
        HasAnimator=value.HasAnimator; AnimatorClipName=value.AnimatorClipName;
        AnimatorSpeed=value.AnimatorSpeed; AnimatorLoop=value.AnimatorLoop;
        AnimatorPlaying=value.AnimatorPlaying; AnimatorTime=value.AnimatorTime;
        HasAudioListener=value.HasAudioListener; AudioListenerVolume=value.AudioListenerVolume;
        AudioSourceVolume=value.AudioSourceVolume; AudioSourcePitch=value.AudioSourcePitch;
        AudioSourceMinDistance=value.AudioSourceMinDistance; AudioSourceMaxDistance=value.AudioSourceMaxDistance;
        AudioSourceLoop=value.AudioSourceLoop; AudioSourcePlayOnAwake=value.AudioSourcePlayOnAwake;
        AudioSourceClipPath=value.AudioSourceClipPath; AudioSourceSpatial=value.AudioSourceSpatial;
    }
}

public sealed class GameObject
{
    private readonly List<Component> _components = new();
    public RuntimeObjectData State { get; }
    public string name { get => State.Name; set => State.Name = value; }
    public string tag { get => State.Tag; set => State.Tag = string.IsNullOrWhiteSpace(value) ? "Untagged" : value.Trim(); }
    public int layer { get => State.Layer; set => State.Layer = (int)Mathf.Clamp(value, 0, 31); }
    public bool activeSelf { get; private set; } = true;
    public Transform transform { get; }

    public GameObject(RuntimeObjectData state)
    {
        State = state;
        transform = new Transform(state);
        transform.Bind(this);
        _components.Add(transform);
        if (state.HasMeshRenderer)
        {
            var renderer = new MeshRenderer(); renderer.Bind(this); _components.Add(renderer);
        }
        RefreshNativeComponents();
        RefreshLightComponent();
    }

    public void SetActive(bool value) => activeSelf = value;
    public bool CompareTag(string other) => string.Equals(tag, other, StringComparison.Ordinal);

    public T? GetComponent<T>() where T : Component => _components.OfType<T>().FirstOrDefault();
    public Component? GetComponent(Type componentType) => _components.FirstOrDefault(componentType.IsInstanceOfType);
    public T[] GetComponents<T>() where T : Component => _components.OfType<T>().ToArray();
    public bool TryGetComponent<T>(out T? component) where T : Component
    {
        component = GetComponent<T>();
        return component is not null;
    }
    public T AddComponent<T>() where T : Component, new()
    {
        if (GetComponent<T>() is { } existing) return existing;
        var component = new T(); component.Bind(this); _components.Add(component);
        if (component is Rigidbody) State.HasRigidbody = true;
        if (component is Collider) State.HasCollider = true;
        return component;
    }
    public bool RemoveComponent<T>() where T : Component
    {
        var component = GetComponent<T>();
        if (component is null || component is Transform || component is MeshRenderer && State.HasMeshRenderer || component is Light && State.LightType != 0)
            return false;
        if (!_components.Remove(component)) return false;
        if (component is Rigidbody) State.HasRigidbody = false;
        if (component is Collider && GetComponent<Collider>() is null) State.HasCollider = false;
        return true;
    }
    public void AttachComponent(Component component)
    {
        if (_components.Contains(component)) return;
        component.Bind(this);
        _components.Add(component);
    }
    public void DetachComponent(Component component) => _components.Remove(component);
    public void RefreshComponents()
    {
        if (State.HasMeshRenderer && GetComponent<MeshRenderer>() is null)
        {
            var renderer = new MeshRenderer(); renderer.Bind(this); _components.Add(renderer);
        }
        RefreshNativeComponents();
        RefreshLightComponent();
    }
    private void RefreshNativeComponents()
    {
        SyncNativeCollider();
        SyncNativeComponent<AudioSource>(State.HasAudioSource);
        SyncNativeComponent<Camera>(State.HasCamera);
        SyncNativeRigidbody();
        SyncNativeComponent<CharacterController>(State.HasCharacterController);
        SyncNativeComponent<ParticleSystem>(State.HasParticleSystem);
        SyncNativeComponent<Animator>(State.HasAnimator);
        SyncNativeComponent<AudioListener>(State.HasAudioListener);
    }
    private void SyncNativeCollider()
    {
        var existing = GetComponent<Collider>();
        if (State.HasCollider && existing is null)
        {
            Collider created = State.ColliderType switch
            {
                1 => new SphereCollider(),
                2 => new CapsuleCollider(),
                _ => new BoxCollider()
            };
            created.Bind(this);
            created.NativeOwned = true;
            _components.Add(created);
        }
        else if (!State.HasCollider && existing is not null && existing.NativeOwned)
            _components.Remove(existing);
    }
    private void SyncNativeRigidbody()
    {
        var existing = GetComponent<Rigidbody>();
        if (State.HasRigidbody && existing is null)
        {
            var created = new Rigidbody();
            created.Bind(this);
            created.NativeOwned = true;
            _components.Add(created);
        }
        else if (!State.HasRigidbody && existing is not null && existing.NativeOwned)
            _components.Remove(existing);
    }
    private void SyncNativeComponent<T>(bool shouldExist) where T : Component, new()
    {
        var existing = GetComponent<T>();
        if (shouldExist && existing is null)
        {
            var created = AddComponent<T>();
            created.NativeOwned = true;
        }
        else if (!shouldExist && existing is not null && existing.NativeOwned)
            _components.Remove(existing);
    }
    private void RefreshLightComponent()
    {
        var light = GetComponent<Light>();
        if (State.LightType != 0 && light is null)
        {
            var c = new Light(); c.Bind(this); _components.Add(c);
        }
        else if (State.LightType == 0 && light is not null) _components.Remove(light);
    }
    public static GameObject? Find(string objectName) => Scene.Find(objectName);
    public static GameObject? FindWithTag(string targetTag) => Scene.FindWithTag(targetTag);
    public static GameObject[] FindGameObjectsWithTag(string targetTag) => Scene.FindGameObjectsWithTag(targetTag);

    /// <summary>Clones an existing object. The native scene receives a duplicate of the source object at the same transform.</summary>
    public static GameObject? Instantiate(GameObject original)
        => original is null ? null
            : Instantiate(original, original.transform.position, original.transform.rotation, original.transform.localScale);

    public static GameObject? Instantiate(GameObject original, Vector3 position, Quaternion rotation)
        => original is null ? null : Instantiate(original, position, rotation.eulerAngles, original.State.Scale);

    public static GameObject? Instantiate(GameObject original, Vector3 position, Vector3 eulerAnglesDegrees, Vector3 scale)
    {
        if (original is null) return null;
        int tempId = Scene.NextTemporaryId();
        var cloneState = original.State.Clone(tempId);
        cloneState.Position = position;
        cloneState.Rotation = eulerAnglesDegrees;
        cloneState.Scale = scale;
        var clone = Scene.RegisterTemporary(cloneState);
        ForgeRuntimeCommands.EnqueueInstantiate(tempId, original.State.Id, cloneState.Name,
            position, eulerAnglesDegrees, scale);
        return clone;
    }

    public static void Destroy(GameObject? target)
    {
        if (target is null) return;
        int id = target.State.Id;
        Scene.Remove(id);
        if (id >= 0) ForgeRuntimeCommands.EnqueueDestroy(id);
    }
    public static void Destroy(Component? target) { if (target is not null) Destroy(target.gameObject); }
    public static void DestroyImmediate(GameObject? target) => Destroy(target);
}

public sealed class MeshRenderer : Component
{
    public new bool enabled
    {
        get => gameObject.State.MeshRendererEnabled;
        set => gameObject.State.MeshRendererEnabled = value;
    }

    /// <summary>Backing material. Modifying it writes straight into the native PBR constants.</summary>
    public Material material { get; }
    internal MeshRenderer() => material = new Material(this);
}

/// <summary>PBR surface values (albedo, metallic, roughness, emission, normal scale, two-sided).</summary>
public sealed class Material
{
    private readonly Component _owner;
    internal Material(Component owner) => _owner = owner;
    private RuntimeObjectData State => _owner.gameObject.State;

    public Color color
    {
        get { var c = State.BaseColor; return new Color(c.x, c.y, c.z, 1f); }
        set => State.BaseColor = new Vector3(value.r, value.g, value.b);
    }
    public Vector3 baseColor { get => State.BaseColor; set => State.BaseColor = value; }
    public float metallic { get => State.Metallic; set => State.Metallic = Mathf.Clamp01(value); }
    public float roughness
    {
        get => State.Roughness;
        set => State.Roughness = Mathf.Clamp(value, 0.045f, 1f);
    }
    public Color emissiveColor
    {
        get { var c = State.EmissiveColor; return new Color(c.x, c.y, c.z, 1f); }
        set => State.EmissiveColor = new Vector3(value.r, value.g, value.b);
    }
    public float emissiveIntensity { get => State.EmissiveIntensity; set => State.EmissiveIntensity = MathF.Max(0f, value); }
    public float normalMapScale { get => State.NormalMapScale; set => State.NormalMapScale = MathF.Max(0f, value); }
    public bool doubleSided { get => State.DoubleSided; set => State.DoubleSided = value; }

    /// <summary>Adds emission so the surface glows and picks up bloom in the post-process pass.</summary>
    public void EnableEmission(Color tint, float intensity = 2f)
    {
        emissiveColor = tint;
        emissiveIntensity = intensity;
    }
    public void DisableEmission() => emissiveIntensity = 0f;
}

public sealed class Light : Component
{
    public bool isActiveAndEnabled => gameObject.State.LightEnabled && enabled;
    public LightType type => (LightType)gameObject.State.LightType;
    public Vector3 color { get => gameObject.State.LightColor; set => gameObject.State.LightColor = value; }
    public float intensity { get => gameObject.State.LightIntensity; set => gameObject.State.LightIntensity = MathF.Max(0, value); }
    public float range { get => gameObject.State.LightRange; set => gameObject.State.LightRange = MathF.Max(0.1f, value); }
    public float spotAngle { get => gameObject.State.LightOuterAngle; set => gameObject.State.LightOuterAngle = Mathf.Clamp(value, 2, 89); }
    public float innerSpotAngle { get => gameObject.State.LightInnerAngle; set => gameObject.State.LightInnerAngle = Mathf.Clamp(value, 1, 87); }
    public new bool enabled { get => gameObject.State.LightEnabled; set => gameObject.State.LightEnabled = value; }
}

public struct ContactPoint
{
    public Vector3 point;
    public Vector3 normal;
}

/// <summary>Axis-aligned bounding box, Unity-style.</summary>
public struct Bounds
{
    public Vector3 center;
    public Vector3 extents;

    public Bounds(Vector3 center, Vector3 size)
    {
        this.center = center;
        extents = new Vector3(MathF.Abs(size.x) * 0.5f, MathF.Abs(size.y) * 0.5f, MathF.Abs(size.z) * 0.5f);
    }

    public Vector3 size { get => extents * 2f; set => extents = new Vector3(MathF.Abs(value.x) * 0.5f, MathF.Abs(value.y) * 0.5f, MathF.Abs(value.z) * 0.5f); }
    public Vector3 min => center - extents;
    public Vector3 max => center + extents;

    public bool Contains(Vector3 point)
        => point.x >= center.x - extents.x && point.x <= center.x + extents.x
        && point.y >= center.y - extents.y && point.y <= center.y + extents.y
        && point.z >= center.z - extents.z && point.z <= center.z + extents.z;

    /// <summary>Grows the box so it also covers <paramref name="point"/>.</summary>
    public void Encapsulate(Vector3 point)
    {
        Vector3 lo = new(MathF.Min(min.x, point.x), MathF.Min(min.y, point.y), MathF.Min(min.z, point.z));
        Vector3 hi = new(MathF.Max(max.x, point.x), MathF.Max(max.y, point.y), MathF.Max(max.z, point.z));
        center = (lo + hi) * 0.5f;
        extents = (hi - lo) * 0.5f;
    }

    /// <summary>Grows the box so it also covers <paramref name="bounds"/>.</summary>
    public void Encapsulate(Bounds bounds)
    {
        Encapsulate(bounds.min);
        Encapsulate(bounds.max);
    }

    public void Expand(float amount)
    {
        extents += new Vector3(MathF.Max(0f, amount), MathF.Max(0f, amount), MathF.Max(0f, amount));
    }

    /// <summary>Closest point inside the box to <paramref name="point"/>.</summary>
    public Vector3 ClosestPoint(Vector3 point)
        => new(
            Mathf.Clamp(point.x, center.x - extents.x, center.x + extents.x),
            Mathf.Clamp(point.y, center.y - extents.y, center.y + extents.y),
            Mathf.Clamp(point.z, center.z - extents.z, center.z + extents.z));

    public bool Intersects(Bounds other)
        => min.x <= other.max.x && max.x >= other.min.x
        && min.y <= other.max.y && max.y >= other.min.y
        && min.z <= other.max.z && max.z >= other.min.z;

    public override string ToString() => $"Bounds(center {center}, size {size})";
}

/// <summary>Collision data passed to OnCollisionEnter/Stay/Exit, Unity-style.</summary>
public struct Collision
{
    public GameObject gameObject;
    public Collider collider;
    public Vector3 relativeVelocity;
    public Vector3 impulse;
    public ContactPoint[] contacts;
}

/// <summary>Collider base component. Fields are stored in the shared runtime state and synchronized with the native scene.</summary>
public class Collider : Component
{
    public float radius { get => gameObject.State.ColliderSize.y * 0.5f; set => SetSizeY(value * 2f); }
    public bool isTrigger { get => gameObject.State.IsTrigger; set => gameObject.State.IsTrigger = value; }

    /// <summary>
    /// Local-space size of the collision shape. The solver multiplies this by the object's
    /// scale, so the inspector value stays meaningful when the object is resized.
    /// </summary>
    public Vector3 size { get => gameObject.State.ColliderSize; set => gameObject.State.ColliderSize = MaxSize(value); }

    /// <summary>Local-space offset of the shape from the object origin.</summary>
    public Vector3 center
    {
        get => gameObject.State.ColliderCenter;
        set => gameObject.State.ColliderCenter = value;
    }

    /// <summary>Size of the shape in world space, after the object's scale.</summary>
    public Vector3 worldSize
    {
        get
        {
            Vector3 s = gameObject.State.ColliderSize;
            Vector3 l = gameObject.State.Scale;
            return new Vector3(MathF.Abs(s.x * l.x), MathF.Abs(s.y * l.y), MathF.Abs(s.z * l.z));
        }
    }

    /// <summary>World-space centre of the shape, offset by <see cref="center"/> and the object's rotation.</summary>
    public Vector3 boundsCenter
    {
        get
        {
            var t = gameObject.transform;
            return t.position + t.quaternion * new Vector3(center.x * t.localScale.x, center.y * t.localScale.y, center.z * t.localScale.z);
        }
    }

    /// <summary>World-space bounding box of the shape, ignoring rotation of the box itself.</summary>
    public Bounds bounds
    {
        get
        {
            Vector3 half = worldSize * 0.5f;
            return new Bounds(boundsCenter, half * 2f);
        }
    }

    /// <summary>Nearest point on this collider to a world position.</summary>
    public Vector3 ClosestPoint(Vector3 position)
    {
        Vector3 c = boundsCenter;
        Vector3 half = worldSize * 0.5f;
        return new Vector3(
            Mathf.Clamp(position.x, c.x - half.x, c.x + half.x),
            Mathf.Clamp(position.y, c.y - half.y, c.y + half.y),
            Mathf.Clamp(position.z, c.z - half.z, c.z + half.z));
    }

    /// <summary>True when a world position lies inside the shape.</summary>
    public bool Contains(Vector3 position)
    {
        Vector3 c = boundsCenter;
        Vector3 half = worldSize * 0.5f;
        return MathF.Abs(position.x - c.x) <= half.x &&
               MathF.Abs(position.y - c.y) <= half.y &&
               MathF.Abs(position.z - c.z) <= half.z;
    }

    /// <summary>Fires when the collider hits a solid collider (no trigger response).</summary>
    public event Action<Collision>? onCollisionEnter;
    public event Action<Collision>? onCollisionStay;
    public event Action<Collision>? onCollisionExit;

    /// <summary>Fires while another collider overlaps this trigger.</summary>
    public event Action<Collider>? onTriggerEnter;
    public event Action<Collider>? onTriggerStay;
    public event Action<Collider>? onTriggerExit;

    internal void RaiseCollisionEnter(Collision c) => onCollisionEnter?.Invoke(c);
    internal void RaiseCollisionStay(Collision c) => onCollisionStay?.Invoke(c);
    internal void RaiseCollisionExit(Collision c) => onCollisionExit?.Invoke(c);
    internal void RaiseTriggerEnter(Collider c) => onTriggerEnter?.Invoke(c);
    internal void RaiseTriggerStay(Collider c) => onTriggerStay?.Invoke(c);
    internal void RaiseTriggerExit(Collider c) => onTriggerExit?.Invoke(c);

    /// <summary>Surface friction used by the solver. Defaults to <see cref="Physics.defaultFriction"/>.</summary>
    public float friction { get; set; } = Physics.defaultFriction;

    /// <summary>Bounciness (restitution) used by the solver. Defaults to <see cref="Physics.defaultRestitution"/>.</summary>
    public float bounciness { get; set; } = Physics.defaultRestitution;

    /// <summary>How much this collider resists interpenetration, 0..1.</summary>
    public float contactOffset { get; set; } = 0.01f;

    /// <summary>Physics radius in world space: half of the largest collider axis after the object's scale, so the collision grows with the scale gizmo.</summary>
    public float effectiveRadius
    {
        get
        {
            Vector3 s = gameObject.State.ColliderSize;
            Vector3 l = gameObject.State.Scale;
            float scaled = MathF.Max(MathF.Abs(s.x * l.x), MathF.Max(MathF.Abs(s.y * l.y), MathF.Abs(s.z * l.z)));
            return MathF.Max(0.05f, scaled * 0.5f);
        }
    }
    protected void SetSizeY(float y)
    {
        var s = gameObject.State.ColliderSize;
        gameObject.State.ColliderSize = new Vector3(s.x, MathF.Max(0.01f, y), s.z);
    }
    internal static Vector3 MaxSize(Vector3 value)
        => new(MathF.Max(0.01f, value.x), MathF.Max(0.01f, value.y), MathF.Max(0.01f, value.z));
}

/// <summary>Sphere collider; radius maps to half of the synchronized size height.</summary>
public sealed class SphereCollider : Collider
{
    public new float radius
    {
        get => gameObject.State.ColliderSize.y * 0.5f;
        set { float d = MathF.Max(0.01f, value * 2f); gameObject.State.ColliderSize = new Vector3(d, d, d); }
    }
}

public sealed class BoxCollider : Collider
{
}

public sealed class CapsuleCollider : Collider
{
    public float height { get => gameObject.State.ColliderSize.y; set => SetSizeY(value); }
}

/// <summary>
/// Rigid body simulated by Bullet. The body is created from this object's
/// Collider, so a Collider component is required for the shape.
/// </summary>
public sealed class Rigidbody : Component
{
    /// <summary>Linear velocity in world units per second, read and written through the solver.</summary>
    public Vector3 velocity
    {
        get => Physics.ReadVelocity(gameObject.State.Id, _cachedVelocity);
        set { _cachedVelocity = value; Physics.WriteVelocity(gameObject.State.Id, value); }
    }

    /// <summary>Angular velocity in degrees per second.</summary>
    public Vector3 angularVelocity
    {
        get => Physics.ReadAngularVelocity(gameObject.State.Id, Vector3.zero);
        set => Physics.WriteAngularVelocity(gameObject.State.Id, value);
    }

    public float mass { get => gameObject.State.Mass; set => gameObject.State.Mass = MathF.Max(0.01f, value); }
    public float drag { get; set; }
    public float angularDrag { get; set; } = 0.05f;
    public bool useGravity { get => gameObject.State.UseGravity; set => gameObject.State.UseGravity = value; }
    public bool isKinematic { get => gameObject.State.IsKinematic; set => gameObject.State.IsKinematic = value; }

    /// <summary>Freezes motion along individual world axes.</summary>
    public bool freezePositionX { get; set; }
    public bool freezePositionY { get; set; }
    public bool freezePositionZ { get; set; }
    public bool freezeRotationX { get; set; }
    public bool freezeRotationY { get; set; }
    public bool freezeRotationZ { get; set; }

    /// <summary>
    /// Continuous collision detection for this body. A fast body can otherwise pass through a
    /// thin wall between two fixed steps, because the solver only sees start and end positions.
    /// </summary>
    public bool continuousCollisionDetection { get; set; }

    /// <summary>Distance the body may travel in one step before CCD takes over, in metres.</summary>
    public float continuousCollisionDetectionThreshold { get; set; } = 0.02f;

    /// <summary>Gap kept between this body's shape and others, in metres.</summary>
    public float collisionMargin
    {
        get => _collisionMargin;
        set => _collisionMargin = MathF.Max(0.001f, value);
    }
    private float _collisionMargin = 0.02f;

    /// <summary>Maximum angular speed in degrees per second. Zero leaves the body unlimited.</summary>
    public float maxAngularVelocity
    {
        get => _maxAngularVelocity;
        set
        {
            _maxAngularVelocity = MathF.Max(0f, value);
            if (_maxAngularVelocity > 0f) Physics.ClampAngularVelocity(gameObject.State.Id, _maxAngularVelocity);
        }
    }
    private float _maxAngularVelocity;

    /// <summary>Current speed of the body, in metres per second.</summary>
    public float speed => velocity.magnitude;

    /// <summary>Interpolates this body's transform between the last two fixed steps, for smooth rendering.</summary>
    public void SetInterpolation(bool enable)
    {
        Physics.SetInterpolation(gameObject.State.Id, enable);
        _interpolation = enable;
    }
    private bool _interpolation;

    private Vector3 _cachedVelocity;
    private Vector3 _pendingForce;

    /// <summary>Queues a continuous force, applied during the next fixed step.</summary>
    public void AddForce(Vector3 force) { if (!isKinematic) _pendingForce += force; }
    public void AddForce(Vector3 force, ForceMode mode)
    {
        if (isKinematic) return;
        switch (mode)
        {
            case ForceMode.Impulse: AddImpulse(force); break;
            case ForceMode.VelocityChange: velocity += force; break;
            case ForceMode.Acceleration: _pendingForce += force * mass; break;
            default: _pendingForce += force; break;
        }
    }
    public void AddForceAtPosition(Vector3 force, Vector3 position)
    {
        if (isKinematic) return;
        Physics.ApplyForceAtPosition(gameObject.State.Id, force, position);
    }
    public void AddImpulse(Vector3 impulse) { if (!isKinematic) Physics.ApplyImpulse(gameObject.State.Id, impulse); }
    public void AddTorque(Vector3 torque) { if (!isKinematic) Physics.ApplyTorque(gameObject.State.Id, torque); }

    /// <summary>Puts the body to sleep until something touches it or <see cref="WakeUp"/> is called.</summary>
    public void Sleep() => Physics.Sleep(gameObject.State.Id);
    public void WakeUp() => Physics.WakeUp(gameObject.State.Id);
    public bool IsSleeping() => Physics.IsSleeping(gameObject.State.Id);

    internal void Simulate(float dt)
    {
        if (!enabled || isKinematic || dt <= 0) return;
        if (_pendingForce != Vector3.zero)
        {
            Physics.ApplyForce(gameObject.State.Id, _pendingForce);
            _pendingForce = Vector3.zero;
        }
        Physics.SetConstraints(gameObject.State.Id, freezePositionX, freezePositionY, freezePositionZ,
            freezeRotationX, freezeRotationY, freezeRotationZ);
        Physics.SetInterpolation(gameObject.State.Id, _interpolation);
        if (_maxAngularVelocity > 0f) Physics.ClampAngularVelocity(gameObject.State.Id, _maxAngularVelocity);
    }
}

/// <summary>How an added force should be interpreted, mirroring Unity.</summary>
public enum ForceMode
{
    Force = 0,
    Acceleration = 1,
    Impulse = 2,
    VelocityChange = 3
}

/// <summary>Camera component. Mirrors the native camera fields and exposes Unity-style helpers.</summary>
public sealed class Camera : Component
{
    public float fieldOfView { get => gameObject.State.CameraFieldOfView; set => gameObject.State.CameraFieldOfView = Mathf.Clamp(value, 1f, 179f); }
    public float nearClipPlane { get => gameObject.State.CameraNearClip; set => gameObject.State.CameraNearClip = MathF.Max(0.001f, value); }
    public float farClipPlane { get => gameObject.State.CameraFarClip; set => gameObject.State.CameraFarClip = MathF.Max(nearClipPlane + 0.01f, value); }
    public bool orthographic { get => gameObject.State.CameraOrthographic; set => gameObject.State.CameraOrthographic = value; }
    public float orthographicSize { get => gameObject.State.CameraOrthographicSize; set => gameObject.State.CameraOrthographicSize = MathF.Max(0.01f, value); }
    public int depth { get => gameObject.State.CameraDepth; set => gameObject.State.CameraDepth = value; }
    public Color backgroundColor
    {
        get { var c = gameObject.State.CameraBackgroundColor; return new Color(c.x, c.y, c.z, c.w); }
        set => gameObject.State.CameraBackgroundColor = new Vector4(value.r, value.g, value.b, value.a);
    }
    public bool drawSkybox { get => gameObject.State.CameraDrawSkybox; set => gameObject.State.CameraDrawSkybox = value; }
    public bool drawGizmos { get => gameObject.State.CameraDrawGizmos; set => gameObject.State.CameraDrawGizmos = value; }
    public int cullingMask { get => gameObject.State.CameraCullingMask; set => gameObject.State.CameraCullingMask = value; }
    public float aspect => MathF.Max(0.01f, Screen.width / (float)MathF.Max(1, Screen.height));

    /// <summary>The scene's main camera (the first enabled camera), matching Unity's Camera.main.</summary>
    public static Camera? main => Scene.FindObjectOfType<Camera>();

    public static Camera[] allCameras => Scene.FindObjectsOfType<Camera>();

    /// <summary>World point to viewport space (0..1). Mirrors Camera.WorldToViewportPoint.</summary>
    public Vector3 WorldToViewportPoint(Vector3 worldPoint)
    {
        Vector3 local = transform.InverseTransformPoint(worldPoint);
        float depth = local.z;
        if (MathF.Abs(depth) < 1e-5f) depth = 1e-5f;
        float halfHeight = orthographic ? orthographicSize : MathF.Tan(fieldOfView * Mathf.Deg2Rad * 0.5f) * depth;
        float halfWidth = halfHeight * aspect;
        return new Vector3(0.5f + local.x / (2f * MathF.Max(halfWidth, 1e-5f)),
                            0.5f + local.y / (2f * MathF.Max(halfHeight, 1e-5f)), depth);
    }

    /// <summary>Viewport point (0..1) to a world-space ray. Mirrors Camera.ViewportPointToRay.</summary>
    public Ray ViewportPointToRay(Vector3 viewportPoint)
    {
        Vector3 forward = transform.forward;
        if (orthographic)
        {
            float halfHeight = orthographicSize;
            float halfWidth = halfHeight * aspect;
            Vector3 origin = transform.position
                + transform.right * ((viewportPoint.x - 0.5f) * 2f * halfWidth)
                + transform.up * ((viewportPoint.y - 0.5f) * 2f * halfHeight);
            return new Ray(origin, forward);
        }
        float tanHalf = MathF.Tan(fieldOfView * Mathf.Deg2Rad * 0.5f);
        Vector3 direction = (forward
            + transform.right * ((viewportPoint.x - 0.5f) * 2f * tanHalf * aspect)
            + transform.up * ((viewportPoint.y - 0.5f) * 2f * tanHalf)).normalized;
        return new Ray(transform.position, direction);
    }

    /// <summary>Screen pixel position to a world-space ray.</summary>
    public Ray ScreenPointToRay(Vector3 screenPoint)
        => ViewportPointToRay(new Vector3(screenPoint.x / MathF.Max(1, Screen.width),
                                          screenPoint.y / MathF.Max(1, Screen.height), 0f));

    /// <summary>World point to screen pixel position.</summary>
    public Vector3 WorldToScreenPoint(Vector3 worldPoint)
    {
        Vector3 viewport = WorldToViewportPoint(worldPoint);
        return new Vector3(viewport.x * Screen.width, viewport.y * Screen.height, viewport.z);
    }
}

/// <summary>WAV playback via the Windows multimedia API. Supports one active PlaySound stream per process.</summary>
public sealed class AudioSource : Component
{
    private const uint SndAsync = 0x0001, SndFilename = 0x00020000, SndLoop = 0x0008, SndPurge = 0x0040;
    [System.Runtime.InteropServices.DllImport("winmm.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
    private static extern bool PlaySoundNative(string? sound, IntPtr module, uint flags);
    public string clip { get; set; } = string.Empty;
    public float volume { get; set; } = 1f;
    public float pitch { get; set; } = 1f;
    public bool loop { get; set; }
    public bool isPlaying { get; private set; }
    public void Play()
    {
        if (string.IsNullOrWhiteSpace(clip)) { Debug.LogWarning($"AudioSource on '{name}' has no WAV clip assigned."); return; }
        try
        {
            string resolved = clip;
            if (!System.IO.File.Exists(resolved))
            {
                string normalized = clip.Replace('/', System.IO.Path.DirectorySeparatorChar);
                var candidates = new[]
                {
                    System.IO.Path.GetFullPath(System.IO.Path.Combine(Environment.CurrentDirectory, normalized)),
                    System.IO.Path.GetFullPath(System.IO.Path.Combine(Environment.CurrentDirectory, "..", normalized)),
                    System.IO.Path.GetFullPath(System.IO.Path.Combine(Environment.CurrentDirectory, "..", "..", normalized))
                };
                resolved = candidates.FirstOrDefault(System.IO.File.Exists) ?? normalized;
            }
            isPlaying = PlaySoundNative(resolved, IntPtr.Zero, SndAsync | SndFilename | (loop ? SndLoop : 0));
            if (!isPlaying) Debug.LogError($"AudioSource could not play '{resolved}'. Check the path and use a WAV file.");
        }
        catch (Exception ex) { Debug.LogError("AudioSource.Play failed: " + ex.Message); }
    }
    /// <summary>Plays a one-shot clip without changing the assigned clip or loop flag.</summary>
    public void PlayOneShot() => Play();
    public void PlayOneShot(string clipPath)
    {
        string previousClip = clip;
        bool previousLoop = loop;
        clip = clipPath;
        loop = false;
        Play();
        clip = previousClip;
        loop = previousLoop;
    }
    public void Stop()
    {
        try { PlaySoundNative(null, IntPtr.Zero, SndPurge); } catch { }
        isPlaying = false;
    }
    public void Pause() => Stop();
}

/// <summary>Ear transform for 3D audio. Place exactly one in the scene.</summary>
public sealed class AudioListener : Component
{
    public float volume
    {
        get => gameObject.State.AudioListenerVolume;
        set => gameObject.State.AudioListenerVolume = Mathf.Clamp(value, 0f, 4f);
    }

    /// <summary>Applies distance attenuation for a source at <paramref name="sourcePosition"/>.</summary>
    public float ComputeAttenuation(Vector3 sourcePosition, float minDistance, float maxDistance)
    {
        float distance = Vector3.Distance(transform.position, sourcePosition);
        float near = MathF.Max(0.01f, minDistance);
        float far = MathF.Max(near, maxDistance);
        if (distance <= near) return volume;
        if (distance >= far) return 0f;
        return volume * (1f - (distance - near) / (far - near));
    }

    /// <summary>The active listener, or null when the scene has none.</summary>
    public static AudioListener? current => Scene.FindObjectOfType<AudioListener>();
}

/// <summary>
/// Kinematic capsule controller with gravity, jumping and ground probing. The solver resolves
/// the capsule against the world; this component owns the intent (move direction, jump).
/// </summary>
public sealed class CharacterController : Component
{
    public float height
    {
        get => gameObject.State.CharacterHeight;
        set => gameObject.State.CharacterHeight = Mathf.Clamp(value, 0.2f, 20f);
    }
    public float radius
    {
        get => gameObject.State.CharacterRadius;
        set => gameObject.State.CharacterRadius = Mathf.Clamp(value, 0.05f, 10f);
    }
    public float slopeLimit
    {
        get => gameObject.State.CharacterSlopeLimit;
        set => gameObject.State.CharacterSlopeLimit = Mathf.Clamp(value, 0f, 89f);
    }
    public float stepOffset
    {
        get => gameObject.State.CharacterStepOffset;
        set => gameObject.State.CharacterStepOffset = Mathf.Clamp(value, 0f, 2f);
    }
    public float skinWidth
    {
        get => gameObject.State.CharacterSkinWidth;
        set => gameObject.State.CharacterSkinWidth = Mathf.Clamp(value, 0f, 0.5f);
    }
    public float gravity
    {
        get => gameObject.State.CharacterGravity;
        set => gameObject.State.CharacterGravity = value;
    }
    public float speed
    {
        get => gameObject.State.CharacterSpeed;
        set => gameObject.State.CharacterSpeed = MathF.Max(0f, value);
    }
    public float jumpHeight
    {
        get => gameObject.State.CharacterJumpHeight;
        set => gameObject.State.CharacterJumpHeight = MathF.Max(0f, value);
    }
    public bool isGrounded
    {
        get => gameObject.State.CharacterIsGrounded;
        internal set => gameObject.State.CharacterIsGrounded = value;
    }

    /// <summary>Current world-space velocity, including the gravity component.</summary>
    public Vector3 velocity
    {
        get => gameObject.State.CharacterVelocity;
        set => gameObject.State.CharacterVelocity = value;
    }

    /// <summary>World-space velocity from the last <see cref="Move"/> call.</summary>
    public Vector3 moveVelocity { get; private set; }
    public float stepDeltaTime { get; private set; }

    /// <summary>Vertical speed needed to reach <see cref="jumpHeight"/> under the current gravity.</summary>
    public float jumpSpeed => MathF.Sqrt(2f * MathF.Abs(gravity) * jumpHeight);

    /// <summary>Half of the capsule's total height, measured from the transform origin.</summary>
    private float halfHeight => MathF.Max(radius, height * 0.5f);

    /// <summary>
    /// Integrates one step of horizontal motion plus gravity and writes the resulting transform.
    /// Horizontal motion is swept against the world, so a wall stops the character and it slides
    /// along the surface; vertical motion is swept separately so floors and ceilings are found.
    /// </summary>
    public void Move(Vector3 motion)
    {
        float dt = MathF.Max(1e-4f, Time.deltaTime);
        stepDeltaTime = dt;
        Vector3 velocity = gameObject.State.CharacterVelocity;
        velocity.x = motion.x / dt;
        velocity.z = motion.z / dt;

        var collider = GetComponent<Collider>();
        Vector3 position = transform.position;

        Vector3 horizontal = new(motion.x, 0f, motion.z);
        if (horizontal.sqrMagnitude > 1e-10f)
            position = MoveWithSlide(position, horizontal, collider);

        velocity.y += gravity * dt;
        Vector3 vertical = new(0f, velocity.y * dt, 0f);
        bool grounded = false;

        if (MathF.Abs(vertical.y) > 1e-8f)
        {
            if (Physics.SweepCapsule(position, radius, height, vertical, collider, out RaycastHit hit))
            {
                position += vertical.normalized * MathF.Max(0f, hit.distance - skinWidth);
                grounded = velocity.y < 0f;
                velocity.y = 0f;
            }
            else position += vertical;
        }

        // While walking, keep the capsule glued to the ground so it does not float above
        // slopes and steps. Only snap when the drop is within the step offset.
        if (!grounded && velocity.y <= 0f && TryFindGround(position, collider, out float groundY) &&
            position.y - groundY <= stepOffset + skinWidth)
        {
            position.y = groundY;
            grounded = true;
            velocity.y = 0f;
        }

        transform.position = position;
        gameObject.State.CharacterVelocity = velocity;
        isGrounded = grounded;
        moveVelocity = velocity;
    }

    /// <summary>Applies the jump impulse when the character is grounded.</summary>
    public bool Jump()
    {
        if (!isGrounded) return false;
        Vector3 velocity = gameObject.State.CharacterVelocity;
        velocity.y = jumpSpeed;
        gameObject.State.CharacterVelocity = velocity;
        isGrounded = false;
        return true;
    }

    /// <summary>Clears accumulated velocity, for teleports or respawns.</summary>
    public void ResetVelocity()
    {
        gameObject.State.CharacterVelocity = Vector3.zero;
        moveVelocity = Vector3.zero;
    }

    /// <summary>
    /// Sweeps the capsule along <paramref name="motion"/>. A low obstacle or a walkable slope is
    /// climbed, anything else is slid along. Without this the character would walk through walls.
    /// </summary>
    private Vector3 MoveWithSlide(Vector3 position, Vector3 motion, Collider? collider)
    {
        // Two passes resolve a single wall and the common case of a wall meeting a step.
        Vector3 remaining = motion;
        for (int pass = 0; pass < 2 && remaining.sqrMagnitude > 1e-10f; pass++)
        {
            if (!Physics.SweepCapsule(position, radius, height, remaining, collider, out RaycastHit hit))
            {
                position += remaining;
                break;
            }

            if (TryStepUp(position, remaining, collider, out Vector3 stepped))
            {
                position = stepped;
                break;
            }

            float allowed = MathF.Max(0f, hit.distance - skinWidth);
            position += remaining.normalized * allowed;
            remaining -= remaining.normalized * allowed;

            // Slide along the obstacle: drop only the part of the motion that pushes into it.
            // The normal is flattened to horizontal so a step's rounded edge cannot be mistaken
            // for a slope and lift the character, and vertical motion is left to the sweep in Move.
            Vector3 normal = new(hit.normal.x, 0f, hit.normal.z);
            if (normal.sqrMagnitude < 1e-8f) remaining = Vector3.zero;
            else
            {
                normal = normal.normalized;
                remaining -= normal * Vector3.Dot(remaining, normal);
            }
        }
        return position;
    }

    /// <summary>
    /// Raises the capsule by the step offset and retries the move, which is how a low step or a
    /// walkable slope is climbed. The move is only accepted when the surface it lands on is
    /// inside the slope limit, so a steep slope or a wall taller than the offset is still refused.
    /// </summary>
    private bool TryStepUp(Vector3 position, Vector3 motion, Collider? collider, out Vector3 result)
    {
        result = position;
        if (stepOffset <= 1e-4f) return false;

        Vector3 raised = position + new Vector3(0f, stepOffset, 0f);
        if (Physics.SweepCapsule(raised, radius, height, motion, collider, out _)) return false;

        // Judging walkability from the surface we would stand on avoids the contact normal, which
        // at a box edge points diagonally and would read as a shallow ramp.
        Vector3 landed = raised + motion;
        if (!TryFindGround(landed, collider, out _)) return false;

        result = landed;
        return true;
    }

    /// <summary>Probes down from the capsule centre, ignoring the character's own collider.</summary>
    private bool TryFindGround(Vector3 position, Collider? collider, out float groundY)
    {
        groundY = position.y;
        float probe = MathF.Max(0.05f, stepOffset + skinWidth);
        if (Physics.RaycastIgnoring(position, Vector3.down, halfHeight + probe, collider, out RaycastHit hit))
        {
            // Reject surfaces steeper than the slope limit so the character slides off cliffs.
            if (Vector3.Angle(hit.normal, Vector3.up) <= slopeLimit)
            {
                groundY = hit.point.y + halfHeight;
                return true;
            }
        }
        return false;
    }
}

/// <summary>Configuration for a particle system component. Simulation lives in <see cref="ParticleSystem"/>.</summary>
public sealed class ParticleSystem : Component
{
    public int maxParticles
    {
        get => gameObject.State.ParticleMaxCount;
        set => gameObject.State.ParticleMaxCount = Math.Clamp(value, 1, 100000);
    }
    public float lifetime
    {
        get => gameObject.State.ParticleLifetime;
        set => gameObject.State.ParticleLifetime = Mathf.Clamp(value, 0.05f, 120f);
    }
    public float startSpeed
    {
        get => gameObject.State.ParticleStartSpeed;
        set => gameObject.State.ParticleStartSpeed = MathF.Max(0f, value);
    }
    public float startSize
    {
        get => gameObject.State.ParticleStartSize;
        set => gameObject.State.ParticleStartSize = MathF.Max(0.001f, value);
    }
    public float gravity
    {
        get => gameObject.State.ParticleGravity;
        set => gameObject.State.ParticleGravity = value;
    }
    public float emissionRate
    {
        get => gameObject.State.ParticleEmissionRate;
        set => gameObject.State.ParticleEmissionRate = MathF.Max(0f, value);
    }
    public Color startColor
    {
        get { var c = gameObject.State.ParticleStartColor; return new Color(c.x, c.y, c.z, 1f); }
        set => gameObject.State.ParticleStartColor = new Vector3(value.r, value.g, value.b);
    }
    public Vector3 direction
    {
        get => gameObject.State.ParticleDirection;
        set => gameObject.State.ParticleDirection = value.sqrMagnitude > 1e-8f ? value.normalized : Vector3.up;
    }
    public float spread
    {
        get => gameObject.State.ParticleSpread;
        set => gameObject.State.ParticleSpread = Mathf.Clamp(value, 0f, 180f);
    }
    public bool looping
    {
        get => gameObject.State.ParticleLooping;
        set => gameObject.State.ParticleLooping = value;
    }

    /// <summary>Number of particles currently alive.</summary>
    public int particleCount => _particles.Count;

    /// <summary>True while the system is emitting. Independent of the component's enabled flag.</summary>
    public bool isEmitting { get; private set; } = true;

    private sealed class Particle
    {
        public Vector3 position;
        public Vector3 velocity;
        public float age;
        public float size;
        public Color color;
    }

    private readonly List<Particle> _particles = new();
    private float _emissionAccumulator;
    private readonly System.Random _random = new();

    public void Play() => isEmitting = true;
    public void Stop() => isEmitting = false;

    /// <summary>Removes every live particle without changing the emitting state.</summary>
    public void Clear()
    {
        _particles.Clear();
        _emissionAccumulator = 0f;
    }

    /// <summary>Spawns a single particle at a world position with an explicit velocity.</summary>
    public void Emit(Vector3 position, Vector3 velocity, float size, Color color)
    {
        if (_particles.Count >= maxParticles) return;
        _particles.Add(new Particle { position = position, velocity = velocity, age = 0f, size = size, color = color });
    }

    /// <summary>Spawns a burst of particles using the configured direction and spread.</summary>
    public void Emit(int count)
    {
        for (int i = 0; i < count; i++) EmitOne();
    }

    public void Simulate(float deltaTime)
    {
        float dt = MathF.Max(0f, deltaTime);
        if (isEmitting && enabled && emissionRate > 0f)
        {
            _emissionAccumulator += emissionRate * dt;
            int budget = _particles.Count < maxParticles ? (int)_emissionAccumulator : 0;
            _emissionAccumulator -= budget;
            if (_emissionAccumulator > 1f) _emissionAccumulator = 1f;
            for (int i = 0; i < budget; i++) EmitOne();
        }

        for (int i = _particles.Count - 1; i >= 0; i--)
        {
            var particle = _particles[i];
            particle.age += dt;
            if (particle.age >= lifetime || (!looping && !isEmitting && particle.age >= lifetime))
            {
                _particles.RemoveAt(i);
                continue;
            }
            particle.velocity += new Vector3(0f, gravity, 0f) * dt;
            particle.position += particle.velocity * dt;
        }
    }

    /// <summary>World-space positions of every live particle, for a renderer or debug draw.</summary>
    public Vector3[] GetPositions()
    {
        var result = new Vector3[_particles.Count];
        for (int i = 0; i < _particles.Count; i++) result[i] = _particles[i].position;
        return result;
    }

    /// <summary>World-space positions and normalized age (0..1) of every live particle.</summary>
    public (Vector3 position, float normalizedAge, float size)[] GetParticles()
    {
        var result = new (Vector3, float, float)[_particles.Count];
        for (int i = 0; i < _particles.Count; i++)
        {
            var p = _particles[i];
            result[i] = (p.position, Mathf.Clamp01(p.age / MathF.Max(1e-4f, lifetime)), p.size);
        }
        return result;
    }

    private void EmitOne()
    {
        if (_particles.Count >= maxParticles) return;
        Vector3 dir = direction.sqrMagnitude > 1e-8f ? direction.normalized : Vector3.up;
        // Cone sampling around the base direction: blend towards a random unit vector by the
        // sine of half the spread angle, then normalize so the speed stays uniform.
        float spreadRad = spread * Mathf.Deg2Rad;
        Vector3 randomDir = new Vector3(
            (float)(_random.NextDouble() * 2.0 - 1.0),
            (float)(_random.NextDouble() * 2.0 - 1.0),
            (float)(_random.NextDouble() * 2.0 - 1.0));
        if (randomDir.sqrMagnitude < 1e-8f) randomDir = Vector3.up;
        randomDir = randomDir.normalized;
        Vector3 blended = Vector3.Lerp(dir, randomDir, MathF.Abs(MathF.Sin(spreadRad * 0.5f)));
        Vector3 velocity = (blended.sqrMagnitude > 1e-8f ? blended.normalized : dir) * startSpeed;
        Emit(transform.position, velocity, startSize, startColor);
    }
}

/// <summary>
/// Drives transform animation. The engine has no skinned-mesh pipeline, so clips animate the
/// object's position, rotation and scale over time.
/// </summary>
public sealed class Animator : Component
{
    public string clipName
    {
        get => gameObject.State.AnimatorClipName;
        set => gameObject.State.AnimatorClipName = value ?? string.Empty;
    }
    public float speed
    {
        get => gameObject.State.AnimatorSpeed;
        set => gameObject.State.AnimatorSpeed = Mathf.Clamp(value, -10f, 10f);
    }
    public bool loop
    {
        get => gameObject.State.AnimatorLoop;
        set => gameObject.State.AnimatorLoop = value;
    }
    public bool playing
    {
        get => gameObject.State.AnimatorPlaying;
        set => gameObject.State.AnimatorPlaying = value;
    }
    public float time
    {
        get => gameObject.State.AnimatorTime;
        set => gameObject.State.AnimatorTime = MathF.Max(0f, value);
    }

    /// <summary>Normalized playback position (0..1) within the current clip.</summary>
    public float normalizedTime
    {
        get
        {
            float length = clipLength;
            return length <= 1e-4f ? 0f : Mathf.Clamp01(time / length);
        }
    }

    /// <summary>Length in seconds of the clip currently assigned by name, or 0 when unset.</summary>
    public float clipLength => string.IsNullOrEmpty(clipName) ? 0f : 1f;

    public void Play(string name)
    {
        clipName = name;
        time = 0f;
        playing = true;
    }

    public void Stop()
    {
        playing = false;
        time = 0f;
    }

    public void Pause() => playing = false;
    public void Resume() => playing = true;

    public void Simulate(float deltaTime)
    {
        if (!playing || speed == 0f) return;
        float next = time + deltaTime * speed;
        if (loop && clipLength > 1e-4f) next = Mathf.Repeat(next, clipLength);
        gameObject.State.AnimatorTime = MathF.Max(0f, next);
    }
}

public readonly struct Ray
{
    public Vector3 origin { get; }
    public Vector3 direction { get; }
    public Ray(Vector3 origin, Vector3 direction) { this.origin = origin; this.direction = direction.normalized; }
    public Vector3 GetPoint(float distance) => origin + direction * distance;
}
public struct RaycastHit
{
    public Vector3 point;
    public Vector3 normal;
    public float distance;
    public Collider? collider;
    public GameObject? gameObject => collider?.gameObject;
}

/// <summary>Applies a continuous force to this object's Rigidbody on every fixed step (without a Rigidbody the object is treated as static).</summary>
public sealed class ConstantForce : Component
{
    public Vector3 force { get; set; }
    internal void Simulate(float dt)
    {
        if (!enabled) return;
        var rb = gameObject.GetComponent<Rigidbody>();
        if (rb is null || !rb.enabled || rb.isKinematic) return;
        rb.velocity += force * dt / MathF.Max(0.01f, rb.mass);
    }
}

/// <summary>Destroys its GameObject after <see cref="lifetime"/> seconds of enabled time. Useful for bullets and effects.</summary>
public sealed class AutoDestroy : Component
{
    public float lifetime { get; set; } = 2f;
    private float _elapsed;
    internal void Tick(float dt)
    {
        if (!enabled) return;
        _elapsed += dt;
        if (_elapsed < lifetime) return;
        GameObject.Destroy(gameObject);
    }
}

/// <summary>Rotates its object to face a target on every fixed step.</summary>
public sealed class LookAtTarget : Component
{
    public GameObject? target { get; set; }
    internal void Apply()
    {
        if (!enabled || target is null || !target.activeSelf) return;
        Vector3 direction = target.transform.position - transform.position;
        if (direction.sqrMagnitude < 1e-8f) return;
        transform.rotation = Quaternion.LookRotation(direction).eulerAngles;
    }
}

/// <summary>Spins its GameObject around a local axis every frame. Handy for pickups and props.</summary>
public sealed class Rotator : Component
{
    public Vector3 axis { get; set; } = Vector3.up;
    public float degreesPerSecond { get; set; } = 90f;
    internal void Tick(float dt)
    {
        if (!enabled || dt <= 0f) return;
        transform.Rotate(axis.normalized * (degreesPerSecond * dt));
    }
}

/// <summary>Moves its GameObject back and forth between two local offsets (platforms, doors, hover).</summary>
public sealed class Oscillator : Component
{
    public Vector3 amplitude { get; set; } = new Vector3(0f, 1f, 0f);
    public float frequency { get; set; } = 1f;
    private Vector3 _origin;
    private bool _captured;
    internal void Tick(float dt)
    {
        if (!enabled) return;
        if (!_captured) { _origin = transform.position; _captured = true; }
        float phase = Time.time * frequency * Mathf.PI * 2f;
        transform.position = _origin + amplitude * MathF.Sin(phase);
    }
}

/// <summary>Destroys its GameObject the first time it drops below a world-space Y threshold.</summary>
public sealed class KillBelowY : Component
{
    public float threshold { get; set; } = -50f;
    internal void Tick(float dt)
    {
        if (enabled && transform.position.y < threshold) GameObject.Destroy(gameObject);
    }
}

/// <summary>Drives a smooth colour or emission pulse on the object's PBR material.</summary>
public sealed class MaterialPulse : Component
{
    public Color colorA { get; set; } = Color.white;
    public Color colorB { get; set; } = Color.cyan;
    public float frequency { get; set; } = 1f;
    public bool useEmission { get; set; }
    public float emissionIntensity { get; set; } = 2f;
    internal void Tick(float dt)
    {
        if (!enabled) return;
        var renderer = gameObject.GetComponent<MeshRenderer>();
        if (renderer is null) return;
        float t = 0.5f + 0.5f * MathF.Sin(Time.time * frequency * Mathf.PI * 2f);
        Color blended = Color.Lerp(colorA, colorB, t);
        renderer.material.color = blended;
        if (useEmission) renderer.material.EnableEmission(blended, emissionIntensity);
    }
}

/// <summary>Base script class. Attach subclasses through Inspector → Add Component → Script C#.</summary>
public abstract class MonoBehaviour : Component
{
    private readonly List<Coroutine> _coroutines = new();

    public virtual void Awake() { }
    public virtual void OnEnable() { }
    public virtual void Start() { }
    public virtual void Update() { }
    public virtual void LateUpdate() { }
    public virtual void FixedUpdate() { }
    public virtual void OnDisable() { }
    public virtual void OnDestroy() { }
    public virtual void OnCollisionEnter(Collision collision) { }
    public virtual void OnCollisionStay(Collision collision) { }
    public virtual void OnCollisionExit(Collision collision) { }
    public virtual void OnTriggerEnter(Collider other) { }
    public virtual void OnTriggerStay(Collider other) { }
    public virtual void OnTriggerExit(Collider other) { }

    public Coroutine StartCoroutine(System.Collections.IEnumerator routine)
    {
        var c = new Coroutine(routine);
        _coroutines.Add(c);
        return c;
    }
    public void StopCoroutine(Coroutine coroutine) { if (coroutine != null) coroutine.Running = false; }
    public void StopAllCoroutines() { foreach (var c in _coroutines) c.Running = false; }

    internal void TickCoroutines(float deltaTime)
    {
        for (int i = _coroutines.Count - 1; i >= 0; i--)
        {
            var c = _coroutines[i];
            if (!c.Running) { _coroutines.RemoveAt(i); continue; }
            if (c.WaitForSecondsRemaining > 0f)
            {
                c.WaitForSecondsRemaining -= deltaTime;
                continue;
            }
            bool moveNext;
            try { moveNext = c.Enumerator.MoveNext(); } catch { moveNext = false; }
            if (!moveNext) { _coroutines.RemoveAt(i); continue; }
            var current = c.Enumerator.Current;
            if (current is WaitForSeconds wfs) c.WaitForSecondsRemaining = wfs.Seconds;
            else if (current is WaitForEndOfFrame) { /* resume next frame */ }
        }
    }
}

/// <summary>A pending scene mutation requested from scripts; drained by the host and forwarded to the native engine.</summary>
public struct ForgePendingCommand
{
    public ForgePendingCommand() { }
    public int kind;          // 0 = instantiate, 1 = destroy
    public int tempId;        // script-side id for the clone until the native engine assigns the real one
    public int sourceId;      // native id of the object being cloned
    public int targetId;      // native id for destroy
    public string name = "";
    public Vector3 position;
    public Vector3 rotationDegrees;
    public Vector3 scale = Vector3.one;
}

public static class ForgeRuntimeCommands
{
    private static readonly List<ForgePendingCommand> Queue = new();
    public static int Count => Queue.Count;

    internal static void EnqueueInstantiate(int tempId, int sourceId, string name, Vector3 position, Vector3 rotationDegrees, Vector3 scale)
    {
        Queue.Add(new ForgePendingCommand { kind = 0, tempId = tempId, sourceId = sourceId, name = name, position = position, rotationDegrees = rotationDegrees, scale = scale });
    }
    internal static void EnqueueDestroy(int targetId)
        => Queue.Add(new ForgePendingCommand { kind = 1, targetId = targetId });

    /// <summary>Removes and returns all queued commands. Called once per frame by the script host.</summary>
    public static ForgePendingCommand[] Drain()
    {
        if (Queue.Count == 0) return Array.Empty<ForgePendingCommand>();
        var copy = Queue.ToArray();
        Queue.Clear();
        return copy;
    }
}

public static class Scene
{
    private static readonly Dictionary<int, GameObject> Objects = new();
    private static int NextTempId = -1;
    public static IReadOnlyCollection<GameObject> gameObjects => Objects.Values;
    public static GameObject? Find(string objectName) => Objects.Values.FirstOrDefault(o => string.Equals(o.name, objectName, StringComparison.Ordinal));
    public static GameObject? FindWithTag(string targetTag)
        => Objects.Values.FirstOrDefault(o => string.Equals(o.tag, targetTag, StringComparison.Ordinal));
    public static GameObject[] FindGameObjectsWithTag(string targetTag)
        => Objects.Values.Where(o => string.Equals(o.tag, targetTag, StringComparison.Ordinal)).ToArray();
    public static T? FindObjectOfType<T>() where T : Component => Objects.Values.Select(o => o.GetComponent<T>()).FirstOrDefault(c => c is not null);
    public static T[] FindObjectsOfType<T>() where T : Component => Objects.Values.SelectMany(o => o.GetComponents<T>()).ToArray();

    public static GameObject? FindById(int id) => Objects.TryGetValue(id, out var value) ? value : null;
    internal static int NextTemporaryId() => NextTempId--;
    internal static GameObject RegisterTemporary(RuntimeObjectData state)
    {
        var clone = new GameObject(state);
        Objects[state.Id] = clone;
        return clone;
    }
    internal static void Remove(int id) => Objects.Remove(id);

    /// <summary>Re-keys a script-created clone to the id assigned by the native engine (NEWID protocol line).</summary>
    public static void RemapId(int from, int to)
    {
        if (from == to || !Objects.TryGetValue(from, out var gameObject)) return;
        Objects.Remove(from);
        gameObject.State.Id = to;
        Objects[to] = gameObject;
    }

    public static void SyncObjects(IEnumerable<RuntimeObjectData> values)
    {
        var seen = new HashSet<int>();
        foreach (var state in values)
        {
            seen.Add(state.Id);
            if (Objects.TryGetValue(state.Id, out var existing))
            {
                existing.State.CopyFrom(state);
                existing.RefreshComponents();
            }
            else Objects[state.Id] = new GameObject(state);
        }
        foreach (var stale in Objects.Keys.Where(id => !seen.Contains(id)).ToArray()) Objects.Remove(stale);
    }
}

public enum KeyCode
{
    None=0, Backspace=8, Tab=9, Return=13, Escape=27, Space=32,
    LeftArrow=37, UpArrow=38, RightArrow=39, DownArrow=40,
    A=65, B=66, C=67, D=68, E=69, F=70, G=71, H=72, I=73, J=74, K=75, L=76, M=77,
    N=78, O=79, P=80, Q=81, R=82, S=83, T=84, U=85, V=86, W=87, X=88, Y=89, Z=90,
    LeftShift=160, RightShift=161, LeftControl=162, RightControl=163, LeftAlt=164, RightAlt=165,
    F1=112, F2=113, F3=114, F4=115, F5=116, F6=117, F7=118, F8=119, F9=120, F10=121, F11=122, F12=123
}

public static class Input
{
    private static HashSet<int> _keys = new();
    private static HashSet<int> _previousKeys = new();
    private static int _mouseButtons;
    private static int _previousMouseButtons;
    private static float _mouseX, _mouseY;
    private static Vector3 _mousePosition;
    private static readonly Dictionary<string, float> _smoothed = new(StringComparer.OrdinalIgnoreCase);
    public static bool anyKey => _keys.Count > 0;
    public static bool anyKeyDown => _keys.Any(key => !_previousKeys.Contains(key));
    /// <summary>Mouse position relative to the game's editor viewport (origin at its top-left).</summary>
    public static Vector3 mousePosition => _mousePosition;
    /// <summary>Mouse movement in the current frame, in pixels.</summary>
    public static Vector2 mouseDelta => new(_mouseX, _mouseY);

    public static void BeginFrame(IEnumerable<int> keys, int mouseButtons, float mouseX, float mouseY,
        float mousePositionX, float mousePositionY)
    {
        if (!InputMap.isLoaded) InputMap.Load();
        _previousKeys = _keys;
        _keys = keys.ToHashSet();
        _previousMouseButtons = _mouseButtons;
        _mouseButtons = mouseButtons;
        _mouseX = mouseX; _mouseY = mouseY;
        _mousePosition = new Vector3(mousePositionX, mousePositionY, 0);
    }
    public static bool GetKey(KeyCode key) => _keys.Contains((int)key);
    public static bool GetKeyDown(KeyCode key) => _keys.Contains((int)key) && !_previousKeys.Contains((int)key);
    public static bool GetKeyUp(KeyCode key) => !_keys.Contains((int)key) && _previousKeys.Contains((int)key);
    public static bool GetMouseButton(int button) => button is >= 0 and < 3 && (_mouseButtons & (1 << button)) != 0;
    public static bool GetMouseButtonDown(int button) => button is >= 0 and < 3 && (_mouseButtons & (1 << button)) != 0 && (_previousMouseButtons & (1 << button)) == 0;
    public static bool GetMouseButtonUp(int button) => button is >= 0 and < 3 && (_mouseButtons & (1 << button)) == 0 && (_previousMouseButtons & (1 << button)) != 0;

    /// <summary>Button action defined in Config/Inputs.json (falls back to the literal key name).</summary>
    public static bool GetButton(string name)
    {
        if (InputMap.TryGetButtonBinding(name, out var binding))
            return binding.Keys.Any(_keys.Contains) || binding.MouseButtons.Any(b => (_mouseButtons & (1 << b)) != 0);
        return false;
    }
    public static bool GetButtonDown(string name)
    {
        if (InputMap.TryGetButtonBinding(name, out var binding))
        {
            bool now = binding.Keys.Any(_keys.Contains) || binding.MouseButtons.Any(b => (_mouseButtons & (1 << b)) != 0);
            bool before = binding.Keys.Any(_previousKeys.Contains) || binding.MouseButtons.Any(b => (_previousMouseButtons & (1 << b)) != 0);
            return now && !before;
        }
        return false;
    }
    public static bool GetButtonUp(string name)
    {
        if (InputMap.TryGetButtonBinding(name, out var binding))
        {
            bool now = binding.Keys.Any(_keys.Contains) || binding.MouseButtons.Any(b => (_mouseButtons & (1 << b)) != 0);
            bool before = binding.Keys.Any(_previousKeys.Contains) || binding.MouseButtons.Any(b => (_previousMouseButtons & (1 << b)) != 0);
            return !now && before;
        }
        return false;
    }

    public static float GetAxis(string axisName)
    {
        float raw = GetAxisRaw(axisName);
        float smoothing = InputMap.TryGetAxisBinding(axisName, out var binding) ? binding.Smoothing : 0f;
        if (smoothing <= 0.01f) return raw;
        float previous = _smoothed.TryGetValue(axisName, out var last) ? last : 0f;
        float factor = Math.Clamp(raw == 0f ? smoothing * 2f : smoothing, 0.02f, 1f);
        float value = previous + (raw - previous) * factor;
        _smoothed[axisName] = value;
        return value;
    }
    public static float GetAxisRaw(string axisName)
    {
        if (InputMap.TryGetAxisBinding(axisName, out var binding))
        {
            if (binding.Source != InputMap.AxisSource.Keys)
                return binding.Source switch
                {
                    InputMap.AxisSource.MouseDeltaX => _mouseX,
                    InputMap.AxisSource.MouseDeltaY => _mouseY,
                    InputMap.AxisSource.MousePositionX => _mousePosition.x,
                    InputMap.AxisSource.MousePositionY => _mousePosition.y,
                    _ => 0f
                };
            return (binding.PositiveKeys.Any(_keys.Contains) ? 1f : 0f) - (binding.NegativeKeys.Any(_keys.Contains) ? 1f : 0f);
        }
        // Legacy built-ins keep working even when Config/Inputs.json is missing.
        if (string.Equals(axisName, "Horizontal", StringComparison.OrdinalIgnoreCase)) return (GetKey(KeyCode.D) ? 1 : 0) - (GetKey(KeyCode.A) ? 1 : 0);
        if (string.Equals(axisName, "Vertical", StringComparison.OrdinalIgnoreCase)) return (GetKey(KeyCode.W) ? 1 : 0) - (GetKey(KeyCode.S) ? 1 : 0);
        if (string.Equals(axisName, "Mouse X", StringComparison.OrdinalIgnoreCase)) return _mouseX;
        if (string.Equals(axisName, "Mouse Y", StringComparison.OrdinalIgnoreCase)) return _mouseY;
        if (string.Equals(axisName, "Mouse ScrollWheel", StringComparison.OrdinalIgnoreCase)) return 0;
        return 0;
    }
}

public static class Time
{
    private static float _rawDelta;
    public static float timeScale { get; set; } = 1f;
    public static float deltaTime { get; private set; }
    public static float unscaledDeltaTime => _rawDelta;
    public static float smoothDeltaTime => deltaTime;
    public static float fixedDeltaTime => 0.02f;
    public static float time { get; private set; }
    public static float realtimeSinceStartup { get; private set; }
    public static float unscaledTime => realtimeSinceStartup;
    public static int frameCount { get; private set; }
    public static void BeginFrame(float delta)
    {
        _rawDelta = MathF.Max(0, delta);
        deltaTime = _rawDelta * MathF.Max(0, timeScale);
        realtimeSinceStartup += _rawDelta;
        time += deltaTime;
        frameCount++;
    }
    public static void SetDeltaTimeFromHost(float delta) => deltaTime = MathF.Max(0, delta) * MathF.Max(0, timeScale);
}

public static class Debug
{
    public static Action<string, string>? Sink { get; set; }
    public static void Log(object? message) => Sink?.Invoke("INFO", message?.ToString() ?? "null");
    public static void LogWarning(object? message) => Sink?.Invoke("WARN", message?.ToString() ?? "null");
    public static void LogError(object? message) => Sink?.Invoke("ERROR", message?.ToString() ?? "null");
    public static void Assert(bool condition, object? message = null) { if (!condition) LogError("Assert failed: " + (message?.ToString() ?? "")); }
}

public static class Mathf
{
    public const float PI = MathF.PI;
    public const float Deg2Rad = MathF.PI / 180f;
    public const float Rad2Deg = 180f / MathF.PI;
    public static float Abs(float value) => MathF.Abs(value);
    public static float Sin(float value) => MathF.Sin(value);
    public static float Cos(float value) => MathF.Cos(value);
    public static float Tan(float value) => MathF.Tan(value);
    public static float Sqrt(float value) => MathF.Sqrt(value);
    public static float Clamp(float value, float min, float max) => Math.Clamp(value, min, max);
    public static float Clamp01(float value) => Clamp(value, 0, 1);
    public static float Min(float a, float b) => MathF.Min(a, b);
    public static float Max(float a, float b) => MathF.Max(a, b);
    public static float Lerp(float a, float b, float t) => a + (b-a)*Clamp01(t);
    public static float MoveTowards(float current, float target, float maxDelta) => MathF.Abs(target-current) <= maxDelta ? target : current + MathF.Sign(target-current)*maxDelta;
    public static float Sign(float value) => MathF.Sign(value);
    public static float Floor(float value) => MathF.Floor(value);
    public static float Ceil(float value) => MathF.Ceiling(value);
    public static float Round(float value) => MathF.Round(value);
    public static float Pow(float value, float power) => MathF.Pow(value, power);
    public static float Exp(float value) => MathF.Exp(value);
    public static float Log(float value) => MathF.Log(value);
    public static float Repeat(float value, float length) => length <= 1e-6f ? 0 : Clamp(value - MathF.Floor(value / length) * length, 0, length);
    public static float PingPong(float value, float length) { float t=Repeat(value, length*2); return length-MathF.Abs(t-length); }
    public static float SmoothStep(float from, float to, float t) { t=Clamp01((t-from)/MathF.Max(1e-6f,to-from)); return t*t*(3-2*t); }
    public static float LerpAngle(float a, float b, float t) { float delta=Repeat(b-a+180f,360f)-180f; return a+delta*Clamp01(t); }
    public static bool Approximately(float a, float b) => MathF.Abs(b-a) < MathF.Max(1e-6f*MathF.Max(MathF.Abs(a),MathF.Abs(b)), 1e-6f);
    public static float Atan(float value) => MathF.Atan(value);
    public static float Atan2(float y, float x) => MathF.Atan2(y, x);
    public static float Asin(float value) => MathF.Asin(Clamp(value, -1f, 1f));
    public static float Acos(float value) => MathF.Acos(Clamp(value, -1f, 1f));
    public static float Fract(float value) => value - MathF.Floor(value);
    public static float InverseLerp(float a, float b, float value) => MathF.Abs(b-a) > 1e-6f ? Clamp01((value-a)/(b-a)) : 0f;
    public static int Clamp(int value, int min, int max) => Math.Clamp(value, min, max);
    public static int Min(int a, int b) => Math.Min(a, b);
    public static int Max(int a, int b) => Math.Max(a, b);
    public static int FloorToInt(float value) => (int)MathF.Floor(value);
    public static int CeilToInt(float value) => (int)MathF.Ceiling(value);
    public static int RoundToInt(float value) => (int)MathF.Round(value);
    /// <summary>Shortest signed difference between two angles in degrees (result in [-180, 180)).</summary>
    public static float DeltaAngle(float current, float target)
    {
        float delta = Repeat(target - current, 360f);
        return delta > 180f ? delta - 360f : delta;
    }
    /// <summary>Wraps a value into [min, max) cyclically.</summary>
    public static float Wrap(float value, float min, float max)
    {
        float length = MathF.Abs(max - min);
        if (length <= 1e-6f) return min;
        return min + Repeat(value - min, length);
    }
}

/// <summary>Deterministic API surface for script-friendly random values.</summary>
public static class Random
{
    private static readonly System.Random Generator = new();
    public static int Range(int minInclusive, int maxExclusive) => maxExclusive <= minInclusive ? minInclusive : Generator.Next(minInclusive, maxExclusive);
    public static float Range(float minInclusive, float maxInclusive) => minInclusive + (float)Generator.NextDouble() * (maxInclusive-minInclusive);
    public static float value => (float)Generator.NextDouble();
    public static Vector3 insideUnitSphere
    {
        get
        {
            for (int i=0;i<32;i++) { var v=new Vector3(Range(-1f,1f),Range(-1f,1f),Range(-1f,1f)); if (v.sqrMagnitude<=1) return v; }
            return Vector3.zero;
        }
    }
}

/// <summary>Persistent key/value storage saved next to the running scripts (ForgePlayerPrefs.txt in the working directory).</summary>
public static class PlayerPrefs
{
    private static readonly Dictionary<string, string> Values = Load();
    private static string FilePath => System.IO.Path.Combine(Environment.CurrentDirectory, "ForgePlayerPrefs.txt");

    private static Dictionary<string, string> Load()
    {
        var map = new Dictionary<string, string>(StringComparer.Ordinal);
        try
        {
            foreach (string raw in System.IO.File.ReadAllLines(FilePath))
            {
                int eq = raw.IndexOf('=');
                if (eq > 0) map[raw[..eq]] = raw[(eq + 1)..];
            }
        }
        catch { }
        return map;
    }
    public static void SetInt(string key, int value) => Values[key] = value.ToString(CultureInfo.InvariantCulture);
    public static int GetInt(string key, int defaultValue = 0) => Values.TryGetValue(key, out var v) && int.TryParse(v, NumberStyles.Integer, CultureInfo.InvariantCulture, out int n) ? n : defaultValue;
    public static void SetFloat(string key, float value) => Values[key] = value.ToString("R", CultureInfo.InvariantCulture);
    public static float GetFloat(string key, float defaultValue = 0f) => Values.TryGetValue(key, out var v) && float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out float n) ? n : defaultValue;
    public static void SetString(string key, string value) => Values[key] = value ?? string.Empty;
    public static string GetString(string key, string defaultValue = "") => Values.TryGetValue(key, out var v) ? v : defaultValue;
    public static bool HasKey(string key) => Values.ContainsKey(key);
    public static void DeleteKey(string key) => Values.Remove(key);
    public static void DeleteAll() => Values.Clear();
    public static void Save()
    {
        try
        {
            var lines = Values.Select(kvp => kvp.Key + "=" + kvp.Value);
            System.IO.File.WriteAllLines(FilePath, lines);
        }
        catch (Exception ex) { Debug.LogWarning("PlayerPrefs.Save failed: " + ex.Message); }
    }
}

/// <summary>Runtime information for scripts. Reads the exported forge_game.ini (product/company/creator) when present.</summary>
public static class Application
{
    private static readonly Dictionary<string, string> GameConfig = LoadGameConfig();
    public static bool isPlaying { get; set; }
    public static bool quitRequested { get; private set; }
    public static string productName => Get("product", "Forge Engine Project");
    public static string companyName => Get("company", "Forge Engine");
    public static string creatorName => Get("creator", "");
    public static string version => "0.5.10";
    public static string dataPath => Environment.CurrentDirectory;
    public static string targetFrameRate { get => "60"; set { } }
    public static void Quit() => quitRequested = true;
    public static void CancelQuit() => quitRequested = false;
    private static string Get(string key, string fallback) => GameConfig.TryGetValue(key, out var v) && !string.IsNullOrWhiteSpace(v) ? v : fallback;

    private static Dictionary<string, string> LoadGameConfig()
    {
        var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        try
        {
            var directory = new System.IO.DirectoryInfo(AppContext.BaseDirectory);
            while (directory is not null)
            {
                string candidate = System.IO.Path.Combine(directory.FullName, "forge_game.ini");
                if (System.IO.File.Exists(candidate))
                {
                    foreach (string raw in System.IO.File.ReadAllLines(candidate))
                    {
                        string line = raw.Trim();
                        if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#') || line.StartsWith('[')) continue;
                        int eq = line.IndexOf('=');
                        if (eq > 0) map[line[..eq].Trim()] = line[(eq + 1)..].Trim();
                    }
                    return map;
                }
                directory = directory.Parent;
            }
        }
        catch { }
        return map;
    }
}

/// <summary>Screen dimensions stub. The native host reports actual viewport size via protocol.</summary>
public static class Screen
{
    public static int width { get; internal set; } = 1920;
    public static int height { get; internal set; } = 1080;
    public static float dpi => 96f;
    public static bool fullScreen { get; set; }
}

/// <summary>Minimal coroutine support. StartCoroutine runs an IEnumerator alongside Update each frame.</summary>
public class Coroutine
{
    internal readonly System.Collections.IEnumerator Enumerator;
    internal bool Running = true;
    internal float WaitForSecondsRemaining = -1f;
    internal Coroutine(System.Collections.IEnumerator enumerator) { Enumerator = enumerator; }
}

/// <summary>Yield instruction: wait for a number of seconds before resuming the coroutine.</summary>
public class WaitForSeconds
{
    public readonly float Seconds;
    public WaitForSeconds(float seconds) { Seconds = seconds; }
}

/// <summary>Yield instruction: resume at the end of the current frame.</summary>
public sealed class WaitForEndOfFrame { }
