using System.Collections.Concurrent;
using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;

[assembly: InternalsVisibleTo("ForgeScriptHost")]
[assembly: InternalsVisibleTo("ForgeEngine.Tests")]

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

/// <summary>Rotation quaternion. Euler angles are applied Z, then X, then Y (Unity convention), which is the
/// same order the DirectX 12 renderer uses (XMMatrixRotationRollPitchYaw).</summary>
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
        => AngleAxis(yDegrees, Vector3.up) * AngleAxis(xDegrees, Vector3.right) * AngleAxis(zDegrees, Vector3.forward);
    public static Quaternion Euler(Vector3 eulerDegrees) => Euler(eulerDegrees.x, eulerDegrees.y, eulerDegrees.z);
    public static Quaternion LookRotation(Vector3 forward) => LookRotation(forward, Vector3.up);
    public static Quaternion LookRotation(Vector3 forward, Vector3 upwards)
    {
        Vector3 f = forward.normalized;
        if (f.sqrMagnitude < 1e-8f) return identity;
        Vector3 r = Vector3.Cross(upwards, f);
        if (r.sqrMagnitude < 1e-8f) r = Vector3.Cross(MathF.Abs(f.y) < 0.99f ? Vector3.up : Vector3.forward, f);
        r = r.normalized;
        Vector3 u = Vector3.Cross(f, r);
        return FromBasis(r, u, f);
    }
    /// <summary>Rotation that turns direction <paramref name="from"/> into <paramref name="to"/>.</summary>
    public static Quaternion FromToRotation(Vector3 from, Vector3 to)
    {
        Vector3 a = from.normalized, b = to.normalized;
        if (a.sqrMagnitude < 1e-8f || b.sqrMagnitude < 1e-8f) return identity;
        float dot = Mathf.Clamp(Vector3.Dot(a, b), -1f, 1f);
        if (dot > 0.999999f) return identity;
        if (dot < -0.999999f)
        {
            Vector3 axis = Vector3.Cross(Vector3.right, a);
            if (axis.sqrMagnitude < 1e-6f) axis = Vector3.Cross(Vector3.up, a);
            return AngleAxis(180f, axis);
        }
        return AngleAxis(MathF.Acos(dot) * Mathf.Rad2Deg, Vector3.Cross(a, b));
    }
    public static Quaternion Normalize(Quaternion q) => q.normalized;
    public void ToAngleAxis(out float angle, out Vector3 axis)
    {
        Quaternion q = normalized;
        if (q.w < 0f) q = new Quaternion(-q.x, -q.y, -q.z, -q.w);
        angle = 2f * MathF.Acos(Mathf.Clamp(q.w, -1f, 1f)) * Mathf.Rad2Deg;
        float s = MathF.Sqrt(MathF.Max(0f, 1f - q.w * q.w));
        axis = s < 1e-5f ? Vector3.right : new Vector3(q.x / s, q.y / s, q.z / s);
    }
    private static Quaternion FromBasis(Vector3 r, Vector3 u, Vector3 f)
    {
        float m00 = r.x, m01 = u.x, m02 = f.x, m10 = r.y, m11 = u.y, m12 = f.y, m20 = r.z, m21 = u.z, m22 = f.z;
        float trace = m00 + m11 + m22;
        if (trace > 0f)
        {
            float s = MathF.Sqrt(trace + 1f) * 2f;
            return new Quaternion((m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s).normalized;
        }
        if (m00 > m11 && m00 > m22)
        {
            float s = MathF.Sqrt(1f + m00 - m11 - m22) * 2f;
            return new Quaternion(0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s).normalized;
        }
        if (m11 > m22)
        {
            float s = MathF.Sqrt(1f + m11 - m00 - m22) * 2f;
            return new Quaternion((m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s).normalized;
        }
        float t = MathF.Sqrt(1f + m22 - m00 - m11) * 2f;
        return new Quaternion((m02 + m20) / t, (m12 + m21) / t, 0.25f * t, (m10 - m01) / t).normalized;
    }

    public Vector3 eulerAngles
    {
        get
        {
            Quaternion q = normalized;
            // Inverse of Euler(): q = Y * X * Z.
            float sinX = Mathf.Clamp(2f*(q.w*q.x - q.y*q.z), -1f, 1f);
            float xDeg = MathF.Asin(sinX) * Mathf.Rad2Deg;
            float yDeg, zDeg;
            if (MathF.Abs(sinX) > 0.99999f)
            {
                // Gimbal lock: Y and Z rotate around the same axis, so fold everything into Y.
                yDeg = MathF.Atan2(-2f*(q.x*q.z - q.w*q.y), 1f - 2f*(q.y*q.y + q.z*q.z)) * Mathf.Rad2Deg;
                zDeg = 0f;
            }
            else
            {
                yDeg = MathF.Atan2(2f*(q.x*q.z + q.w*q.y), 1f - 2f*(q.x*q.x + q.y*q.y)) * Mathf.Rad2Deg;
                zDeg = MathF.Atan2(2f*(q.x*q.y + q.w*q.z), 1f - 2f*(q.x*q.x + q.z*q.z)) * Mathf.Rad2Deg;
            }
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
    public Quaternion inverse { get { Quaternion n = normalized; return new(-n.x, -n.y, -n.z, n.w); } }
    public static Quaternion Inverse(Quaternion q) => q.inverse;
    public static Quaternion RotateTowards(Quaternion current, Quaternion target, float maxDegreesDelta)
    {
        float angle = Angle(current, target);
        return angle <= maxDegreesDelta || angle < 1e-4f ? target : Slerp(current, target, maxDegreesDelta / angle);
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
    public bool TryGetComponent<T>(out T? component) where T : Component => gameObject.TryGetComponent(out component);
    public T? GetComponentInChildren<T>(bool includeInactive = false) where T : Component => gameObject.GetComponentInChildren<T>(includeInactive);
    public T[] GetComponentsInChildren<T>(bool includeInactive = false) where T : Component => gameObject.GetComponentsInChildren<T>(includeInactive);
    public T? GetComponentInParent<T>() where T : Component => gameObject.GetComponentInParent<T>();
    public string tag { get => gameObject.tag; set => gameObject.tag = value; }
    public bool CompareTag(string other) => gameObject.CompareTag(other);
    public void Bind(GameObject owner) => gameObject = owner;

    // Unity-style shortcuts so scripts can call Destroy(gameObject) / Instantiate(prefab) without a prefix.
    public static void Destroy(GameObject? target, float delaySeconds = 0f) => GameObject.Destroy(target, delaySeconds);
    public static void Destroy(Component? target) => GameObject.Destroy(target);
    public static GameObject? Instantiate(GameObject original) => GameObject.Instantiate(original);
    public static GameObject? Instantiate(GameObject original, Transform? parent) => GameObject.Instantiate(original, parent);
    public static GameObject? Instantiate(GameObject original, Vector3 position, Quaternion rotation) => GameObject.Instantiate(original, position, rotation);
    public static GameObject? Instantiate(GameObject original, Vector3 position, Quaternion rotation, Transform? parent) => GameObject.Instantiate(original, position, rotation, parent);
}

/// <summary>Transform component. localPosition/localEulerAngles/localScale are relative to the parent (the values
/// the native scene stores); position/rotation/eulerAngles are world-space and follow the parent chain.
/// For root objects local and world values are identical.</summary>
public sealed class Transform : Component, System.Collections.Generic.IEnumerable<Transform>
{
    private readonly RuntimeObjectData _state;
    internal Transform(RuntimeObjectData state) => _state = state;

    public Vector3 localPosition { get => _state.Position; set => _state.Position = value; }
    public Vector3 localEulerAngles { get => _state.Rotation; set => _state.Rotation = value; }
    public Quaternion localRotation { get => Quaternion.Euler(_state.Rotation); set => _state.Rotation = value.eulerAngles; }
    public Vector3 localScale { get => _state.Scale; set => _state.Scale = value; }

    public Vector3 position
    {
        get => parent is { } p ? p.TransformPoint(localPosition) : localPosition;
        set => localPosition = parent is { } p ? p.InverseTransformPoint(value) : value;
    }
    /// <summary>World rotation as a quaternion (Unity's Transform.rotation).</summary>
    public Quaternion quaternion
    {
        get => parent is { } p ? p.quaternion * localRotation : localRotation;
        set => localRotation = parent is { } p ? p.quaternion.inverse * value : value;
    }
    /// <summary>World rotation as Euler degrees (kept as Vector3 for compatibility with existing scripts).</summary>
    public Vector3 rotation
    {
        get => parent is null ? localEulerAngles : quaternion.eulerAngles;
        set { if (parent is null) localEulerAngles = value; else quaternion = Quaternion.Euler(value); }
    }
    public Vector3 eulerAngles { get => rotation; set => rotation = value; }
    public Vector3 lossyScale
    {
        get
        {
            Vector3 s = localScale;
            for (Transform? p = parent; p is not null; p = p.parent)
                s = new Vector3(s.x * p.localScale.x, s.y * p.localScale.y, s.z * p.localScale.z);
            return s;
        }
    }
    public Vector3 forward { get => quaternion * Vector3.forward; set => quaternion = Quaternion.LookRotation(value); }
    public Vector3 right => quaternion * Vector3.right;
    public Vector3 up => quaternion * Vector3.up;

    // ---- Hierarchy ----
    public Transform? parent
    {
        get => _state.ParentId == -1 ? null : Scene.FindById(_state.ParentId)?.transform;
        set => SetParent(value, true);
    }
    public Transform root { get { Transform t = this; while (t.parent is { } p) t = p; return t; } }
    public int childCount => Children().Count();
    public Transform GetChild(int index)
    {
        var children = Children().ToArray();
        if (index < 0 || index >= children.Length) throw new ArgumentOutOfRangeException(nameof(index), $"'{name}' has {children.Length} children.");
        return children[index];
    }
    public Transform? Find(string childName)
    {
        // Supports "Child/GrandChild" paths like Unity.
        Transform? current = this;
        foreach (string part in childName.Split('/', StringSplitOptions.RemoveEmptyEntries))
        {
            current = current?.Children().FirstOrDefault(c => string.Equals(c.name, part, StringComparison.Ordinal));
            if (current is null) return null;
        }
        return current;
    }
    public bool IsChildOf(Transform other)
    {
        for (Transform? t = this; t is not null; t = t.parent) if (ReferenceEquals(t, other)) return true;
        return false;
    }
    public void SetParent(Transform? newParent, bool worldPositionStays = true)
    {
        if (newParent is not null && newParent.IsChildOf(this))
        {
            Debug.LogWarning($"Cannot parent '{name}' to '{newParent.name}': it would create a hierarchy cycle.");
            return;
        }
        if (ReferenceEquals(parent, newParent)) return;
        Vector3 worldPosition = position;
        Quaternion worldRotation = quaternion;
        Vector3 worldScale = lossyScale;
        _state.ParentId = newParent?.gameObject.State.Id ?? -1;
        if (!worldPositionStays) return;
        position = worldPosition;
        quaternion = worldRotation;
        Vector3 parentScale = newParent?.lossyScale ?? Vector3.one;
        localScale = new Vector3(SafeDivide(worldScale.x, parentScale.x), SafeDivide(worldScale.y, parentScale.y), SafeDivide(worldScale.z, parentScale.z));
    }
    public void DetachChildren() { foreach (var child in Children().ToArray()) child.SetParent(null, true); }
    public System.Collections.Generic.IEnumerator<Transform> GetEnumerator() => Children().ToList().GetEnumerator();
    System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator() => GetEnumerator();
    internal IEnumerable<Transform> Children()
    {
        int id = _state.Id;
        return Scene.AllGameObjects.Where(o => o.State.ParentId == id && !ReferenceEquals(o.State, _state)).Select(o => o.transform);
    }

    // ---- Movement ----
    public void SetPositionAndRotation(Vector3 worldPosition, Quaternion worldRotation) { position = worldPosition; quaternion = worldRotation; }
    public void Translate(Vector3 translation, Space relativeTo = Space.Self)
        => position += relativeTo == Space.World ? translation : TransformDirection(translation);
    public void Translate(float x, float y, float z, Space relativeTo = Space.Self) => Translate(new Vector3(x, y, z), relativeTo);
    public void Rotate(Vector3 eulerDegrees, Space relativeTo = Space.Self)
    {
        Quaternion delta = Quaternion.Euler(eulerDegrees);
        if (relativeTo == Space.Self) localRotation = localRotation * delta;
        else quaternion = delta * quaternion;
    }
    public void Rotate(float xAngle, float yAngle, float zAngle, Space relativeTo = Space.Self)
        => Rotate(new Vector3(xAngle, yAngle, zAngle), relativeTo);
    public void Rotate(Vector3 axis, float angle, Space relativeTo = Space.Self)
    {
        if (relativeTo == Space.Self) localRotation = localRotation * Quaternion.AngleAxis(angle, axis);
        else quaternion = Quaternion.AngleAxis(angle, axis) * quaternion;
    }
    public void Rotate(Quaternion orientation) => quaternion = orientation * quaternion;
    public void RotateAround(Vector3 point, Vector3 axis, float angle)
    {
        Quaternion q = Quaternion.AngleAxis(angle, axis);
        position = point + q * (position - point);
        quaternion = q * quaternion;
    }
    public void LookAt(Vector3 target) => LookAt(target, Vector3.up);
    public void LookAt(Vector3 target, Vector3 worldUp)
    {
        Vector3 direction = target - position;
        if (direction.sqrMagnitude < 1e-8f) return;
        quaternion = Quaternion.LookRotation(direction, worldUp);
    }
    public void LookAt(Transform target) { if (target is not null) LookAt(target.position, Vector3.up); }

    // ---- Space conversion ----
    public Vector3 TransformDirection(Vector3 direction) => quaternion * direction;
    public Vector3 InverseTransformDirection(Vector3 direction) => quaternion.inverse * direction;
    public Vector3 TransformVector(Vector3 vector)
    {
        Vector3 local = localRotation * Scale(vector, localScale);
        return parent is { } p ? p.TransformVector(local) : local;
    }
    public Vector3 TransformPoint(Vector3 point)
    {
        // Same composition as the renderer: scale, rotate, translate, then the parent's transform.
        Vector3 local = localRotation * Scale(point, localScale) + localPosition;
        return parent is { } p ? p.TransformPoint(local) : local;
    }
    public Vector3 InverseTransformPoint(Vector3 point)
    {
        Vector3 inParent = parent is { } p ? p.InverseTransformPoint(point) : point;
        Vector3 unrotated = localRotation.inverse * (inParent - localPosition);
        return new Vector3(SafeDivide(unrotated.x, localScale.x), SafeDivide(unrotated.y, localScale.y), SafeDivide(unrotated.z, localScale.z));
    }

    private static Vector3 Scale(Vector3 a, Vector3 b) => new(a.x * b.x, a.y * b.y, a.z * b.z);
    private static float SafeDivide(float value, float divisor) => MathF.Abs(divisor) < 1e-6f ? 0f : value / divisor;
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
    public bool IsTrigger { get; set; }
    public bool HasRigidbody { get; set; }
    public float Mass { get; set; } = 1f;
    public bool UseGravity { get; set; } = true;
    public bool IsKinematic { get; set; }
    /// <summary>Stable id of the parent object, or -1 for root objects.</summary>
    public int ParentId { get; set; } = -1;
    public bool ActiveSelf { get; set; } = true;
    public float CameraFieldOfView { get; set; } = 60f;
    public float CameraNearClip { get; set; } = 0.05f;
    public float CameraFarClip { get; set; } = 1000f;

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
            IsTrigger = IsTrigger, HasRigidbody = HasRigidbody, Mass = Mass,
            UseGravity = UseGravity, IsKinematic = IsKinematic,
            ParentId = ParentId, ActiveSelf = ActiveSelf, CameraFieldOfView = CameraFieldOfView,
            CameraNearClip = CameraNearClip, CameraFarClip = CameraFarClip
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
        IsTrigger=value.IsTrigger; HasRigidbody=value.HasRigidbody; Mass=value.Mass;
        UseGravity=value.UseGravity; IsKinematic=value.IsKinematic;
        ParentId=value.ParentId; ActiveSelf=value.ActiveSelf; CameraFieldOfView=value.CameraFieldOfView;
        CameraNearClip=value.CameraNearClip; CameraFarClip=value.CameraFarClip;
    }
}

public sealed class GameObject
{
    private readonly List<Component> _components = new();
    public RuntimeObjectData State { get; }
    public string name { get => State.Name; set => State.Name = value; }
    public string tag { get => State.Tag; set => State.Tag = string.IsNullOrWhiteSpace(value) ? "Untagged" : value.Trim(); }
    public int layer { get => State.Layer; set => State.Layer = (int)Mathf.Clamp(value, 0, 31); }
    /// <summary>Local active flag set by SetActive (synchronized with the renderer).</summary>
    public bool activeSelf => State.ActiveSelf;
    /// <summary>False when this object or any of its parents is inactive.</summary>
    public bool activeInHierarchy
    {
        get
        {
            int guard = 256;
            for (Transform? t = transform; t is not null && guard-- > 0; t = t.parent)
                if (!t.gameObject.State.ActiveSelf) return false;
            return true;
        }
    }
    /// <summary>True once Destroy has been called on this object.</summary>
    public bool isDestroyed { get; internal set; }
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

    public void SetActive(bool value) => State.ActiveSelf = value;
    public bool CompareTag(string other) => string.Equals(tag, other, StringComparison.Ordinal);

    public T? GetComponent<T>() where T : Component => _components.OfType<T>().FirstOrDefault();
    public Component? GetComponent(Type componentType) => _components.FirstOrDefault(componentType.IsInstanceOfType);
    public T[] GetComponents<T>() where T : Component => _components.OfType<T>().ToArray();
    public T? GetComponentInChildren<T>(bool includeInactive = false) where T : Component
        => SelfAndDescendants(includeInactive).Select(o => o.GetComponent<T>()).FirstOrDefault(c => c is not null);
    public T[] GetComponentsInChildren<T>(bool includeInactive = false) where T : Component
        => SelfAndDescendants(includeInactive).SelectMany(o => o.GetComponents<T>()).ToArray();
    public T? GetComponentInParent<T>() where T : Component
    {
        for (Transform? t = transform; t is not null; t = t.parent)
            if (t.gameObject.GetComponent<T>() is { } found) return found;
        return null;
    }
    internal IEnumerable<GameObject> SelfAndDescendants(bool includeInactive = true)
    {
        if (!includeInactive && !activeInHierarchy) yield break;
        yield return this;
        foreach (var child in transform.Children().ToArray())
            foreach (var descendant in child.gameObject.SelfAndDescendants(includeInactive))
                yield return descendant;
    }
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
    public bool RemoveComponent<T>() where T : Component => RemoveComponent(GetComponent<T>());
    public bool RemoveComponent(Component? component)
    {
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

    /// <summary>Clones an existing object. The native scene receives a duplicate of the source object at the same transform.
    /// Scripts attached to the source start on the clone in the next frame.</summary>
    public static GameObject? Instantiate(GameObject original)
        => original is null ? null
            : Instantiate(original, original.transform.position, original.transform.rotation, original.transform.localScale);

    public static GameObject? Instantiate(GameObject original, Transform? parent)
    {
        var clone = Instantiate(original);
        if (clone is not null && parent is not null) clone.transform.SetParent(parent, false);
        return clone;
    }

    public static GameObject? Instantiate(GameObject original, Vector3 position, Quaternion rotation)
        => original is null ? null : Instantiate(original, position, rotation.eulerAngles, original.State.Scale);

    public static GameObject? Instantiate(GameObject original, Vector3 position, Quaternion rotation, Transform? parent)
    {
        var clone = Instantiate(original, position, rotation);
        if (clone is not null && parent is not null) clone.transform.SetParent(parent, true);
        return clone;
    }

    public static GameObject? Instantiate(GameObject original, Vector3 position, Vector3 eulerAnglesDegrees, Vector3 scale)
    {
        if (original is null) return null;
        int tempId = Scene.NextTemporaryId();
        var cloneState = original.State.Clone(tempId);
        cloneState.Position = position;
        cloneState.Rotation = eulerAnglesDegrees;
        cloneState.Scale = scale;
        cloneState.ParentId = -1;
        var clone = Scene.RegisterTemporary(cloneState);
        ForgeRuntimeCommands.EnqueueInstantiate(tempId, original.State.Id, cloneState.Name,
            position, eulerAnglesDegrees, scale);
        return clone;
    }

    /// <summary>Destroys the object and all of its children at the end of the frame (or after <paramref name="delaySeconds"/>).</summary>
    public static void Destroy(GameObject? target, float delaySeconds = 0f)
    {
        if (target is null || target.isDestroyed) return;
        if (delaySeconds > 0f) { Scene.ScheduleDestroy(target, delaySeconds); return; }
        foreach (var go in target.SelfAndDescendants().ToArray())
        {
            go.isDestroyed = true;
            int id = go.State.Id;
            Scene.Remove(id);
            ForgeRuntimeCommands.EnqueueDestroy(id);
        }
    }
    /// <summary>Removes a single component. Scripts receive OnDisable/OnDestroy at the end of the frame;
    /// destroying the Transform is not allowed (destroy the GameObject instead).</summary>
    public static void Destroy(Component? target)
    {
        if (target is null) return;
        if (target is Transform) { Debug.LogError("Destroying the Transform component is not allowed. Use Destroy(gameObject)."); return; }
        if (target is MonoBehaviour behaviour) { behaviour.DestroyRequested = true; return; }
        if (!target.gameObject.RemoveComponent(target))
            Debug.LogWarning($"{target.GetType().Name} on '{target.gameObject.name}' is owned by the native scene and cannot be removed at runtime.");
    }
    public static void DestroyImmediate(GameObject? target) => Destroy(target);
}

public sealed class MeshRenderer : Component
{
    public new bool enabled
    {
        get => gameObject.State.MeshRendererEnabled;
        set => gameObject.State.MeshRendererEnabled = value;
    }
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
    public Vector3 size { get => gameObject.State.ColliderSize; set => gameObject.State.ColliderSize = MaxSize(value); }

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

/// <summary>Rigid body synchronized with the native object. Velocity is simulated by the scripting runtime.</summary>
public enum ForceMode { Force, Acceleration, Impulse, VelocityChange }

public sealed class Rigidbody : Component
{
    public Vector3 velocity { get; set; } = Vector3.zero;
    public float mass { get => gameObject.State.Mass; set => gameObject.State.Mass = MathF.Max(0.01f, value); }
    public float drag { get; set; }
    public bool useGravity { get => gameObject.State.UseGravity; set => gameObject.State.UseGravity = value; }
    public bool isKinematic { get => gameObject.State.IsKinematic; set => gameObject.State.IsKinematic = value; }
    public void AddForce(Vector3 force) { if (!isKinematic) _pendingForce += force; }
    public void AddForce(Vector3 force, ForceMode mode)
    {
        if (isKinematic) return;
        switch (mode)
        {
            case ForceMode.Acceleration: _pendingForce += force * MathF.Max(0.001f, mass); break;
            case ForceMode.Impulse: velocity += force / MathF.Max(0.001f, mass); break;
            case ForceMode.VelocityChange: velocity += force; break;
            default: _pendingForce += force; break;
        }
    }
    public void MovePosition(Vector3 worldPosition) => transform.position = worldPosition;
    public void MoveRotation(Quaternion worldRotation) => transform.quaternion = worldRotation;
    public void AddImpulse(Vector3 impulse) { if (!isKinematic) velocity += impulse / MathF.Max(0.001f, mass); }
    private Vector3 _pendingForce;
    internal void Simulate(float dt)
    {
        if (!enabled || isKinematic || dt <= 0) return;
        if (useGravity) velocity += Physics.gravity * dt;
        velocity += _pendingForce * dt / MathF.Max(0.001f, mass);
        _pendingForce = Vector3.zero;
        velocity *= 1f / (1f + MathF.Max(0, drag) * dt);
        transform.position += velocity * dt;
        var collider = gameObject.GetComponent<Collider>();
        if (collider is not null && transform.position.y < collider.effectiveRadius)
        {
            transform.position = new Vector3(transform.position.x, collider.effectiveRadius, transform.position.z);
            if (velocity.y < 0) velocity = new Vector3(velocity.x, 0, velocity.z);
        }
    }
}

/// <summary>Camera component. Field of view and clip planes are synchronized with the native camera object.</summary>
public sealed class Camera : Component
{
    public float fieldOfView { get => gameObject.State.CameraFieldOfView; set => gameObject.State.CameraFieldOfView = Mathf.Clamp(value, 20f, 110f); }
    public float nearClipPlane { get => gameObject.State.CameraNearClip; set => gameObject.State.CameraNearClip = Mathf.Clamp(value, 0.01f, 100f); }
    public float farClipPlane { get => gameObject.State.CameraFarClip; set => gameObject.State.CameraFarClip = MathF.Max(nearClipPlane + 0.1f, value); }
    public float aspect => Screen.height > 0 ? (float)Screen.width / Screen.height : 16f / 9f;
    /// <summary>First active camera in the scene.</summary>
    public static Camera? main => Scene.FindObjectOfType<Camera>();

    /// <summary>Ray from the camera through a viewport pixel (origin at the top-left, like Input.mousePosition).</summary>
    public Ray ScreenPointToRay(Vector3 screenPosition)
    {
        float width = MathF.Max(1, Screen.width), height = MathF.Max(1, Screen.height);
        float ndcX = screenPosition.x / width * 2f - 1f;
        float ndcY = 1f - screenPosition.y / height * 2f;
        float tanHalf = MathF.Tan(fieldOfView * 0.5f * Mathf.Deg2Rad);
        Vector3 local = new(ndcX * tanHalf * aspect, ndcY * tanHalf, 1f);
        return new Ray(transform.position, transform.TransformDirection(local));
    }
    /// <summary>Projects a world point to viewport pixels (top-left origin); z is the distance along the camera's forward axis.</summary>
    public Vector3 WorldToScreenPoint(Vector3 worldPosition)
    {
        Vector3 local = transform.InverseTransformDirection(worldPosition - transform.position);
        float tanHalf = MathF.Tan(fieldOfView * 0.5f * Mathf.Deg2Rad);
        if (MathF.Abs(local.z) < 1e-6f) return new Vector3(0, 0, 0);
        float ndcX = local.x / (local.z * tanHalf * aspect);
        float ndcY = local.y / (local.z * tanHalf);
        return new Vector3((ndcX + 1f) * 0.5f * Screen.width, (1f - ndcY) * 0.5f * Screen.height, local.z);
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

public static class Physics
{
    public static Vector3 gravity { get; set; } = new(0, -9.81f, 0);

    private static readonly Dictionary<long, (Collider A, Collider B)> CollisionContacts = new();
    private static readonly Dictionary<long, (Collider A, Collider B)> TriggerContacts = new();
    private static long PairKey(int a, int b) => a < b ? ((long)a << 32) | (uint)b : ((long)b << 32) | (uint)a;

    public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hit, float maxDistance = 1000f, int layerMask = -1)
        => Raycast(new Ray(origin, direction), out hit, maxDistance, layerMask);
    public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance = 1000f, int layerMask = -1)
        => Raycast(new Ray(origin, direction), out _, maxDistance, layerMask);

    public static bool Raycast(Ray ray, out RaycastHit hit, float maxDistance = 1000f, int layerMask = -1)
    {
        hit = default;
        float closest = MathF.Max(0, maxDistance);
        bool found = false;
        foreach (var go in Scene.gameObjects)
        {
            if (!go.activeInHierarchy || (layerMask & (1 << go.layer)) == 0) continue;
            var collider = go.GetComponent<Collider>();
            if (collider is null || !collider.enabled) continue;
            float radius = collider.effectiveRadius;
            Vector3 offset = ray.origin - go.transform.position;
            float b = Vector3.Dot(offset, ray.direction);
            float c = offset.sqrMagnitude - radius * radius;
            float discriminant = b * b - c;
            if (discriminant < 0) continue;
            float distance = -b - MathF.Sqrt(discriminant);
            if (distance < 0) distance = -b + MathF.Sqrt(discriminant);
            if (distance < 0 || distance > closest) continue;
            closest = distance; found = true;
            Vector3 point = ray.GetPoint(distance);
            hit = new RaycastHit { point = point, normal = (point - go.transform.position).normalized, distance = distance, collider = collider };
        }
        return found;
    }
    public static Collider[] OverlapSphere(Vector3 center, float radius, int layerMask = -1)
    {
        var results = new List<Collider>();
        foreach (var go in Scene.gameObjects)
        {
            if (!go.activeInHierarchy || (layerMask & (1 << go.layer)) == 0) continue;
            var collider = go.GetComponent<Collider>();
            if (collider is null || !collider.enabled) continue;
            if (Vector3.Distance(center, go.transform.position) <= radius + collider.effectiveRadius)
                results.Add(collider);
        }
        return results.ToArray();
    }
    public static void SimulateRuntimeStep(float fixedDeltaTime)
    {
        foreach (var go in Scene.gameObjects.ToArray())
            if (go.activeInHierarchy)
                foreach (var body in go.GetComponents<Rigidbody>()) body.Simulate(fixedDeltaTime);

        var participants = new List<(GameObject Go, Collider Col)>();
        foreach (var go in Scene.gameObjects)
        {
            if (!go.activeInHierarchy) continue;
            var col = go.GetComponent<Collider>();
            if (col is null || !col.enabled) continue;
            participants.Add((go, col));
        }

        var liveCollision = new HashSet<long>();
        var liveTrigger = new HashSet<long>();

        for (int i = 0; i < participants.Count; ++i)
        {
            for (int j = i + 1; j < participants.Count; ++j)
            {
                var (goA, colA) = participants[i];
                var (goB, colB) = participants[j];
                Vector3 delta = goB.transform.position - goA.transform.position;
                float distance = delta.magnitude;
                float minDistance = colA.effectiveRadius + colB.effectiveRadius;
                if (distance > minDistance) continue;

                long key = PairKey(goA.State.Id, goB.State.Id);
                Vector3 normal = distance > 1e-5f ? delta / distance : Vector3.up;
                Vector3 point = goA.transform.position + normal * (minDistance * 0.5f);
                var contacts = new[] { new ContactPoint { point = point, normal = normal } };

                if (colA.isTrigger || colB.isTrigger)
                {
                    liveTrigger.Add(key);
                    bool entered = !TriggerContacts.ContainsKey(key);
                    TriggerContacts[key] = (colA, colB);
                    DispatchTrigger(goA, colB, entered);
                    DispatchTrigger(goB, colA, entered);
                    continue;
                }

                liveCollision.Add(key);
                Vector3 velocityA = VelocityOf(goA), velocityB = VelocityOf(goB);
                Vector3 relativeVelocity = velocityB - velocityA;
                float invA = InverseMass(goA), invB = InverseMass(goB);
                float total = invA + invB;
                Vector3 impulse = Vector3.zero;
                float penetration = minDistance - distance;
                if (total > 0f)
                {
                    if (penetration > 0f)
                    {
                        Vector3 correction = normal * (penetration / total * 0.8f);
                        goA.transform.position -= correction * invA;
                        goB.transform.position += correction * invB;
                    }
                    float separating = Vector3.Dot(relativeVelocity, normal);
                    if (separating < 0f)
                    {
                        impulse = normal * (-separating / total);
                        ApplyImpulse(goA, -impulse * invA);
                        ApplyImpulse(goB, impulse * invB);
                        relativeVelocity = VelocityOf(goB) - VelocityOf(goA);
                    }
                }
                bool enteredCollision = !CollisionContacts.ContainsKey(key);
                CollisionContacts[key] = (colA, colB);
                DispatchCollision(goA, goB, colB, relativeVelocity, impulse, contacts, enteredCollision);
                var flippedContacts = new[] { new ContactPoint { point = point, normal = -normal } };
                DispatchCollision(goB, goA, colA, -relativeVelocity, -impulse, flippedContacts, enteredCollision);
            }
        }

        CloseOldContacts(CollisionContacts, liveCollision, false);
        CloseOldContacts(TriggerContacts, liveTrigger, true);

        foreach (var go in Scene.gameObjects.ToArray())
        {
            if (!go.activeInHierarchy) continue;
            foreach (var force in go.GetComponents<ConstantForce>()) force.Simulate(fixedDeltaTime);
            foreach (var auto in go.GetComponents<AutoDestroy>()) auto.Tick(fixedDeltaTime);
            foreach (var aim in go.GetComponents<LookAtTarget>()) aim.Apply();
        }
    }

    private static Vector3 VelocityOf(GameObject go)
    {
        var rb = go.GetComponent<Rigidbody>();
        return rb is { enabled: true, isKinematic: false } ? rb.velocity : Vector3.zero;
    }
    private static float InverseMass(GameObject go)
    {
        var rb = go.GetComponent<Rigidbody>();
        if (rb is null || !rb.enabled || rb.isKinematic) return 0f;
        return 1f / MathF.Max(0.01f, rb.mass);
    }
    private static void ApplyImpulse(GameObject go, Vector3 velocityChange)
    {
        var rb = go.GetComponent<Rigidbody>();
        if (rb is { enabled: true, isKinematic: false }) rb.velocity += velocityChange;
    }
    private enum ContactPhase { Enter, Stay, Exit }
    private static void DispatchCollision(GameObject target, GameObject other, Collider otherCollider,
        Vector3 relativeVelocity, Vector3 impulse, ContactPoint[] contacts, bool entered)
        => DispatchCollision(target, other, otherCollider, relativeVelocity, impulse, contacts, entered ? ContactPhase.Enter : ContactPhase.Stay);
    private static void DispatchCollision(GameObject target, GameObject other, Collider otherCollider,
        Vector3 relativeVelocity, Vector3 impulse, ContactPoint[] contacts, ContactPhase phase)
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
            if (!script.enabled || !script.gameObject.activeInHierarchy) continue;
            try
            {
                if (phase == ContactPhase.Enter) script.OnCollisionEnter(collision);
                else if (phase == ContactPhase.Stay) script.OnCollisionStay(collision);
                else script.OnCollisionExit(collision);
            }
            catch (Exception ex) { Debug.LogError($"{script.GetType().Name}.OnCollision{phase}() on '{target.name}': {ex.GetBaseException().Message}"); }
        }
    }
    private static void DispatchTrigger(GameObject target, Collider other, bool entered)
    {
        foreach (var script in target.GetComponents<MonoBehaviour>())
        {
            if (!script.enabled || !script.gameObject.activeInHierarchy) continue;
            try
            {
                if (entered) script.OnTriggerEnter(other);
                else script.OnTriggerStay(other);
            }
            catch (Exception ex) { Debug.LogError($"{script.GetType().Name}.OnTrigger{(entered ? "Enter" : "Stay")}() on '{target.name}': {ex.GetBaseException().Message}"); }
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
                DispatchCollision(goA, goB, pair.B, Vector3.zero, Vector3.zero, contacts, ContactPhase.Exit);
                DispatchCollision(goB, goA, pair.A, Vector3.zero, Vector3.zero, contacts, ContactPhase.Exit);
            }
        }
    }
    private static void DispatchTriggerExit(GameObject target, Collider other)
    {
        foreach (var script in target.GetComponents<MonoBehaviour>())
        {
            if (!script.enabled || !script.gameObject.activeInHierarchy) continue;
            try { script.OnTriggerExit(other); }
            catch (Exception ex) { Debug.LogError($"{script.GetType().Name}.OnTriggerExit() on '{target.name}': {ex.GetBaseException().Message}"); }
        }
    }
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
        if (!enabled || target is null || !target.activeInHierarchy) return;
        Vector3 direction = target.transform.position - transform.position;
        if (direction.sqrMagnitude < 1e-8f) return;
        transform.rotation = Quaternion.LookRotation(direction).eulerAngles;
    }
}

/// <summary>Base script class. Attach subclasses through Inspector → Add Component → Script C#.</summary>
public abstract class MonoBehaviour : Component
{
    private readonly List<Coroutine> _coroutines = new();
    private readonly List<ScheduledInvoke> _invokes = new();
    private sealed class ScheduledInvoke
    {
        public string MethodName = "";
        public Action Callback = null!;
        public float Remaining;
        public float RepeatRate = -1f;
    }

    /// <summary>Set by Destroy(this); the host runs OnDisable/OnDestroy and removes the script at the end of the frame.</summary>
    internal bool DestroyRequested { get; set; }
    public bool isActiveAndEnabled => enabled && !DestroyRequested && gameObject.activeInHierarchy;

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

    public static void print(object? message) => Debug.Log(message);

    // ---- Coroutines ----
    /// <summary>Starts a coroutine. It runs immediately until its first yield, then resumes after Update every frame
    /// (null / WaitForEndOfFrame / WaitForFixedUpdate), after a delay (WaitForSeconds, WaitForSecondsRealtime),
    /// when a condition holds (WaitUntil, WaitWhile, CustomYieldInstruction) or when a nested coroutine finishes.</summary>
    public Coroutine StartCoroutine(System.Collections.IEnumerator routine)
    {
        var c = new Coroutine(routine);
        _coroutines.Add(c);
        Step(c);
        return c;
    }
    public void StopCoroutine(Coroutine? coroutine) { if (coroutine != null) coroutine.Stop(); }
    public void StopCoroutine(System.Collections.IEnumerator? routine)
    {
        if (routine is null) return;
        foreach (var c in _coroutines) if (ReferenceEquals(c.Enumerator, routine)) c.Stop();
    }
    public void StopAllCoroutines() { foreach (var c in _coroutines.ToArray()) c.Stop(); }
    internal int ActiveCoroutineCount => _coroutines.Count(c => c.Running);

    internal void TickCoroutines(float deltaTime) => TickCoroutines(deltaTime, deltaTime);
    internal void TickCoroutines(float deltaTime, float unscaledDeltaTime)
    {
        foreach (var c in _coroutines.ToArray())
        {
            if (!c.Running || c.StartedFrame == Time.frameCount) continue;
            if (c.WaitForSecondsRemaining > 0f) { c.WaitForSecondsRemaining -= deltaTime; if (c.WaitForSecondsRemaining > 0f) continue; }
            if (c.WaitRealtimeRemaining > 0f) { c.WaitRealtimeRemaining -= unscaledDeltaTime; if (c.WaitRealtimeRemaining > 0f) continue; }
            if (c.WaitCondition is not null)
            {
                bool keepWaiting;
                try { keepWaiting = c.WaitCondition(); }
                catch (Exception ex) { LogCoroutineError(ex); c.Stop(); continue; }
                if (keepWaiting) continue;
                c.WaitCondition = null;
            }
            Step(c);
        }
        _coroutines.RemoveAll(c => !c.Running);
    }
    private void Step(Coroutine c)
    {
        c.StartedFrame = Time.frameCount;
        bool moveNext;
        try { moveNext = c.Enumerator.MoveNext(); }
        catch (Exception ex) { LogCoroutineError(ex); moveNext = false; }
        if (!moveNext) { c.Stop(); return; }
        switch (c.Enumerator.Current)
        {
            case WaitForSeconds wait: c.WaitForSecondsRemaining = wait.Seconds; break;
            case WaitForSecondsRealtime wait: c.WaitRealtimeRemaining = wait.Seconds; break;
            case CustomYieldInstruction custom: c.WaitCondition = () => custom.keepWaiting; break;
            case Coroutine nested: c.WaitCondition = () => nested.Running; break;
            case System.Collections.IEnumerator routine:
                var child = StartCoroutine(routine);
                c.WaitCondition = () => child.Running;
                break;
        }
    }
    private void LogCoroutineError(Exception ex)
        => Debug.LogError($"{GetType().Name} coroutine on '{(gameObject is null ? "?" : gameObject.name)}': {ex.GetBaseException().Message}");

    // ---- Invoke ----
    /// <summary>Calls a parameterless method of this script after <paramref name="time"/> seconds (scaled time).</summary>
    public void Invoke(string methodName, float time) => Schedule(methodName, time, -1f);
    /// <summary>Calls a parameterless method after <paramref name="time"/> seconds and then every <paramref name="repeatRate"/> seconds.</summary>
    public void InvokeRepeating(string methodName, float time, float repeatRate)
    {
        if (repeatRate <= 0f) { Debug.LogError("InvokeRepeating: repeatRate must be greater than zero."); return; }
        Schedule(methodName, time, repeatRate);
    }
    public void CancelInvoke() => _invokes.Clear();
    public void CancelInvoke(string methodName) => _invokes.RemoveAll(i => i.MethodName == methodName);
    public bool IsInvoking() => _invokes.Count > 0;
    public bool IsInvoking(string methodName) => _invokes.Any(i => i.MethodName == methodName);
    private void Schedule(string methodName, float time, float repeatRate)
    {
        var method = GetType().GetMethod(methodName, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic, null, Type.EmptyTypes, null);
        if (method is null) { Debug.LogError($"Invoke: method '{methodName}()' was not found on {GetType().Name}."); return; }
        _invokes.Add(new ScheduledInvoke { MethodName = methodName, Callback = () => method.Invoke(this, null), Remaining = MathF.Max(0f, time), RepeatRate = repeatRate });
    }
    internal void TickInvokes(float deltaTime)
    {
        foreach (var invoke in _invokes.ToArray())
        {
            if (!_invokes.Contains(invoke)) continue;
            invoke.Remaining -= deltaTime;
            if (invoke.Remaining > 0f) continue;
            if (invoke.RepeatRate > 0f) invoke.Remaining += invoke.RepeatRate;
            else _invokes.Remove(invoke);
            try { invoke.Callback(); }
            catch (Exception ex) { Debug.LogError($"{GetType().Name}.{invoke.MethodName}() (Invoke) on '{gameObject.name}': {ex.GetBaseException().Message}"); }
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
    private static readonly HashSet<int> JustCreated = new();
    private static readonly List<(GameObject Target, float Remaining)> DelayedDestroys = new();
    // Temporary ids start far from -1, which the protocol uses for "no object".
    private const int FirstTemporaryId = -1000;
    private static int NextTempId = FirstTemporaryId;
    public static IReadOnlyCollection<GameObject> gameObjects => Objects.Values;
    internal static IEnumerable<GameObject> AllGameObjects => Objects.Values;
    private static IEnumerable<GameObject> Active => Objects.Values.Where(o => o.activeInHierarchy);
    /// <summary>Finds an active object by name (inactive objects are skipped, like Unity).</summary>
    public static GameObject? Find(string objectName) => Active.FirstOrDefault(o => string.Equals(o.name, objectName, StringComparison.Ordinal));
    public static GameObject? FindWithTag(string targetTag)
        => Active.FirstOrDefault(o => string.Equals(o.tag, targetTag, StringComparison.Ordinal));
    public static GameObject[] FindGameObjectsWithTag(string targetTag)
        => Active.Where(o => string.Equals(o.tag, targetTag, StringComparison.Ordinal)).ToArray();
    public static T? FindObjectOfType<T>() where T : Component => Active.Select(o => o.GetComponent<T>()).FirstOrDefault(c => c is not null);
    public static T[] FindObjectsOfType<T>() where T : Component => Active.SelectMany(o => o.GetComponents<T>()).ToArray();
    public static GameObject[] GetRootGameObjects() => Objects.Values.Where(o => o.transform.parent is null).ToArray();

    public static GameObject? FindById(int id) => Objects.TryGetValue(id, out var value) ? value : null;
    internal static int NextTemporaryId() => NextTempId--;
    internal static GameObject RegisterTemporary(RuntimeObjectData state)
    {
        var clone = new GameObject(state);
        Objects[state.Id] = clone;
        return clone;
    }
    internal static void Remove(int id) => Objects.Remove(id);
    internal static void Clear()
    {
        Objects.Clear(); JustCreated.Clear(); DelayedDestroys.Clear();
        NextTempId = FirstTemporaryId;
    }

    internal static void ScheduleDestroy(GameObject target, float delaySeconds)
    {
        DelayedDestroys.RemoveAll(d => ReferenceEquals(d.Target, target));
        DelayedDestroys.Add((target, delaySeconds));
    }
    internal static void TickDelayedDestroys(float deltaTime)
    {
        if (DelayedDestroys.Count == 0) return;
        var due = new List<GameObject>();
        for (int i = DelayedDestroys.Count - 1; i >= 0; --i)
        {
            var (target, remaining) = DelayedDestroys[i];
            remaining -= deltaTime;
            if (remaining > 0f && !target.isDestroyed) { DelayedDestroys[i] = (target, remaining); continue; }
            DelayedDestroys.RemoveAt(i);
            due.Add(target);
        }
        foreach (var target in due) GameObject.Destroy(target);
    }

    /// <summary>Re-keys a script-created clone to the id assigned by the native engine (NEWID protocol line).</summary>
    public static void RemapId(int from, int to)
    {
        if (from == to || !Objects.TryGetValue(from, out var gameObject)) return;
        Objects.Remove(from);
        gameObject.State.Id = to;
        Objects[to] = gameObject;
        foreach (var other in Objects.Values)
            if (other.State.ParentId == from) other.State.ParentId = to;
        // The native clone only received the spawn transform; keep the script-side state for this frame
        // so changes made right after Instantiate (name, parent, SetActive...) are sent back instead of lost.
        JustCreated.Add(to);
    }

    public static void SyncObjects(IEnumerable<RuntimeObjectData> values)
    {
        var seen = new HashSet<int>();
        foreach (var state in values)
        {
            seen.Add(state.Id);
            if (Objects.TryGetValue(state.Id, out var existing))
            {
                if (!JustCreated.Contains(state.Id)) existing.State.CopyFrom(state);
                existing.RefreshComponents();
            }
            else Objects[state.Id] = new GameObject(state);
        }
        JustCreated.Clear();
        foreach (var stale in Objects.Keys.Where(id => id >= 0 && !seen.Contains(id)).ToArray())
        {
            Objects[stale].isDestroyed = true;
            Objects.Remove(stale);
        }
    }
}

public enum KeyCode
{
    None=0, Backspace=8, Tab=9, Return=13, Escape=27, Space=32,
    LeftArrow=37, UpArrow=38, RightArrow=39, DownArrow=40, Delete=46,
    Alpha0=48, Alpha1=49, Alpha2=50, Alpha3=51, Alpha4=52, Alpha5=53, Alpha6=54, Alpha7=55, Alpha8=56, Alpha9=57,
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
    private static float _mouseX, _mouseY, _mouseScroll;
    private static Vector3 _mousePosition;
    private static readonly Dictionary<string, float> _smoothed = new(StringComparer.OrdinalIgnoreCase);
    public static bool anyKey => _keys.Count > 0;
    public static bool anyKeyDown => _keys.Any(key => !_previousKeys.Contains(key));
    /// <summary>Mouse position relative to the game's editor viewport (origin at its top-left).</summary>
    public static Vector3 mousePosition => _mousePosition;
    /// <summary>Mouse movement in the current frame, in pixels.</summary>
    public static Vector2 mouseDelta => new(_mouseX, _mouseY);
    /// <summary>Mouse wheel movement this frame in notches (y &gt; 0 = scrolled away from the user).</summary>
    public static Vector2 mouseScrollDelta => new(0f, _mouseScroll);

    public static void BeginFrame(IEnumerable<int> keys, int mouseButtons, float mouseX, float mouseY,
        float mousePositionX, float mousePositionY)
        => BeginFrame(keys, mouseButtons, mouseX, mouseY, mousePositionX, mousePositionY, 0f);
    public static void BeginFrame(IEnumerable<int> keys, int mouseButtons, float mouseX, float mouseY,
        float mousePositionX, float mousePositionY, float mouseScroll)
    {
        _mouseScroll = mouseScroll;
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
                    InputMap.AxisSource.MouseScroll => _mouseScroll,
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
        if (string.Equals(axisName, "Mouse ScrollWheel", StringComparison.OrdinalIgnoreCase)) return _mouseScroll;
        return 0;
    }
}

public static class Time
{
    private static float _rawDelta;
    private static float _scaledDelta;
    private static float _fixedDeltaTime = 0.02f;
    public static float timeScale { get; set; } = 1f;
    public static float deltaTime { get; private set; }
    public static float unscaledDeltaTime => _rawDelta;
    public static float smoothDeltaTime => deltaTime;
    /// <summary>Interval of FixedUpdate/physics in scaled seconds. Inside FixedUpdate, deltaTime equals this value.</summary>
    public static float fixedDeltaTime { get => _fixedDeltaTime; set => _fixedDeltaTime = Math.Clamp(value, 0.001f, 0.5f); }
    public static float fixedUnscaledDeltaTime => timeScale > 0f ? _fixedDeltaTime / timeScale : _fixedDeltaTime;
    public static bool inFixedTimeStep { get; private set; }
    public static float time { get; private set; }
    public static float fixedTime { get; private set; }
    public static float realtimeSinceStartup { get; private set; }
    public static float unscaledTime => realtimeSinceStartup;
    public static int frameCount { get; private set; }
    public static void BeginFrame(float delta)
    {
        _rawDelta = MathF.Max(0, delta);
        _scaledDelta = _rawDelta * MathF.Max(0, timeScale);
        deltaTime = _scaledDelta;
        realtimeSinceStartup += _rawDelta;
        time += _scaledDelta;
        frameCount++;
    }
    public static void SetDeltaTimeFromHost(float delta) => deltaTime = MathF.Max(0, delta) * MathF.Max(0, timeScale);
    internal static void BeginFixedStep() { inFixedTimeStep = true; deltaTime = _fixedDeltaTime; }
    internal static void EndFixedStep() { inFixedTimeStep = false; fixedTime += _fixedDeltaTime; deltaTime = _scaledDelta; }
    internal static void Reset()
    {
        _rawDelta = _scaledDelta = 0f; _fixedDeltaTime = 0.02f; timeScale = 1f; deltaTime = 0f;
        inFixedTimeStep = false; time = fixedTime = realtimeSinceStartup = 0f; frameCount = 0;
    }
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
    public const float Infinity = float.PositiveInfinity;
    public const float NegativeInfinity = float.NegativeInfinity;
    public const float Epsilon = float.Epsilon;
    public static float LerpUnclamped(float a, float b, float t) => a + (b - a) * t;
    public static float Log(float value, float newBase) => MathF.Log(value) / MathF.Log(newBase);
    public static float Log10(float value) => MathF.Log10(value);
    public static float MoveTowardsAngle(float current, float target, float maxDelta)
    {
        float delta = DeltaAngle(current, target);
        return -maxDelta < delta && delta < maxDelta ? target : MoveTowards(current, current + delta, maxDelta);
    }
    /// <summary>Critically damped spring towards target (same algorithm as Unity's Mathf.SmoothDamp).</summary>
    public static float SmoothDamp(float current, float target, ref float currentVelocity, float smoothTime, float maxSpeed = float.PositiveInfinity, float deltaTime = -1f)
    {
        if (deltaTime < 0f) deltaTime = Time.deltaTime;
        smoothTime = MathF.Max(0.0001f, smoothTime);
        float omega = 2f / smoothTime;
        float x = omega * deltaTime;
        float exp = 1f / (1f + x + 0.48f * x * x + 0.235f * x * x * x);
        float change = current - target;
        float originalTo = target;
        float maxChange = maxSpeed * smoothTime;
        change = Clamp(change, -maxChange, maxChange);
        target = current - change;
        float temp = (currentVelocity + omega * change) * deltaTime;
        currentVelocity = (currentVelocity - omega * temp) * exp;
        float output = target + (change + temp) * exp;
        if (originalTo - current > 0f == output > originalTo) { output = originalTo; currentVelocity = deltaTime > 0f ? (output - originalTo) / deltaTime : 0f; }
        return output;
    }
    public static float SmoothDampAngle(float current, float target, ref float currentVelocity, float smoothTime, float maxSpeed = float.PositiveInfinity, float deltaTime = -1f)
        => SmoothDamp(current, current + DeltaAngle(current, target), ref currentVelocity, smoothTime, maxSpeed, deltaTime);
    public static bool IsPowerOfTwo(int value) => value > 0 && (value & (value - 1)) == 0;
    public static int NextPowerOfTwo(int value) { if (value <= 1) return 1; int p = 1; while (p < value && p < (1 << 30)) p <<= 1; return p; }
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
    private static System.Random Generator = new();
    /// <summary>Re-seeds the generator so the following values are reproducible.</summary>
    public static void InitState(int seed) => Generator = new System.Random(seed);
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
    public static Vector2 insideUnitCircle
    {
        get
        {
            float angle = Range(0f, MathF.PI * 2f), radius = MathF.Sqrt(value);
            return new Vector2(MathF.Cos(angle) * radius, MathF.Sin(angle) * radius);
        }
    }
    public static Vector3 onUnitSphere
    {
        get
        {
            float z = Range(-1f, 1f), angle = Range(0f, MathF.PI * 2f), r = MathF.Sqrt(1f - z * z);
            return new Vector3(r * MathF.Cos(angle), r * MathF.Sin(angle), z);
        }
    }
    public static Quaternion rotation => Quaternion.Euler(Range(0f, 360f), Range(0f, 360f), Range(0f, 360f));
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
    public static string productName => Get("product", "DX12ForgeEngine Project");
    public static string companyName => Get("company", "DX12 Forge");
    public static string creatorName => Get("creator", "");
    public static string version => "0.5.9";
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

/// <summary>Size of the game viewport in pixels, reported by the native engine every frame.</summary>
public static class Screen
{
    public static int width { get; internal set; } = 1920;
    public static int height { get; internal set; } = 1080;
    internal static void SetSize(int newWidth, int newHeight)
    {
        if (newWidth > 0) width = newWidth;
        if (newHeight > 0) height = newHeight;
    }
    public static float dpi => 96f;
    public static bool fullScreen { get; set; }
}

/// <summary>Handle returned by StartCoroutine. The host resumes coroutines after Update and before LateUpdate.</summary>
public class Coroutine
{
    internal readonly System.Collections.IEnumerator Enumerator;
    internal bool Running = true;
    internal float WaitForSecondsRemaining = -1f;
    internal float WaitRealtimeRemaining = -1f;
    internal Func<bool>? WaitCondition;
    internal int StartedFrame = -1;
    internal Coroutine(System.Collections.IEnumerator enumerator) { Enumerator = enumerator; }
    internal void Stop() { Running = false; WaitCondition = null; }
    public bool isRunning => Running;
}

/// <summary>Yield instruction: wait for a number of seconds before resuming the coroutine.</summary>
public class WaitForSeconds
{
    public readonly float Seconds;
    public WaitForSeconds(float seconds) { Seconds = seconds; }
}

/// <summary>Yield instruction: resume at the end of the current frame.</summary>
public sealed class WaitForEndOfFrame { }

/// <summary>Yield instruction: resume after the next physics step (resumes on the next frame).</summary>
public sealed class WaitForFixedUpdate { }

/// <summary>Yield instruction: wait using unscaled time (ignores Time.timeScale).</summary>
public sealed class WaitForSecondsRealtime
{
    public readonly float Seconds;
    public WaitForSecondsRealtime(float seconds) { Seconds = seconds; }
}

/// <summary>Base class for custom yield instructions: the coroutine waits while keepWaiting is true.</summary>
public abstract class CustomYieldInstruction
{
    public abstract bool keepWaiting { get; }
}

/// <summary>Yield instruction: wait until the predicate returns true.</summary>
public sealed class WaitUntil : CustomYieldInstruction
{
    private readonly Func<bool> _predicate;
    public WaitUntil(Func<bool> predicate) { _predicate = predicate; }
    public override bool keepWaiting => !_predicate();
}

/// <summary>Yield instruction: wait while the predicate returns true.</summary>
public sealed class WaitWhile : CustomYieldInstruction
{
    private readonly Func<bool> _predicate;
    public WaitWhile(Func<bool> predicate) { _predicate = predicate; }
    public override bool keepWaiting => _predicate();
}
