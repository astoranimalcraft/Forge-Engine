using ForgeEngine;
using Xunit;

namespace ForgeEngine.Tests;

public sealed class QuaternionTests : RuntimeTest
{
    // Reference implementation of XMMatrixRotationRollPitchYaw (row vectors): roll Z, then pitch X, then yaw Y.
    private static Vector3 NativeRotate(Vector3 v, Vector3 eulerDegrees)
    {
        float x = eulerDegrees.x * Mathf.Deg2Rad, y = eulerDegrees.y * Mathf.Deg2Rad, z = eulerDegrees.z * Mathf.Deg2Rad;
        v = new Vector3(v.x * MathF.Cos(z) - v.y * MathF.Sin(z), v.x * MathF.Sin(z) + v.y * MathF.Cos(z), v.z);
        v = new Vector3(v.x, v.y * MathF.Cos(x) - v.z * MathF.Sin(x), v.y * MathF.Sin(x) + v.z * MathF.Cos(x));
        return new Vector3(v.x * MathF.Cos(y) + v.z * MathF.Sin(y), v.y, -v.x * MathF.Sin(y) + v.z * MathF.Cos(y));
    }

    public static IEnumerable<object[]> Angles()
    {
        yield return new object[] { 30f, 45f, 60f };
        yield return new object[] { -20f, 170f, 10f };
        yield return new object[] { 89.9f, 30f, 0f };
        yield return new object[] { 0f, 0f, 90f };
        yield return new object[] { 45f, -135f, 200f };
    }

    [Theory, MemberData(nameof(Angles))]
    public void EulerMatchesRendererRotationOrder(float x, float y, float z)
    {
        var euler = new Vector3(x, y, z);
        foreach (var v in new[] { Vector3.forward, Vector3.right, Vector3.up, new Vector3(1, 2, 3) })
            AssertClose(NativeRotate(v, euler), Quaternion.Euler(euler) * v);
    }

    [Theory, MemberData(nameof(Angles))]
    public void EulerAnglesRoundTrip(float x, float y, float z)
    {
        var q = Quaternion.Euler(x, y, z);
        var back = Quaternion.Euler(q.eulerAngles);
        foreach (var v in new[] { Vector3.forward, Vector3.right, Vector3.up })
            AssertClose(q * v, back * v);
    }

    [Fact]
    public void GimbalLockStillRoundTrips()
    {
        var q = Quaternion.Euler(90f, 40f, 25f);
        var back = Quaternion.Euler(q.eulerAngles);
        foreach (var v in new[] { Vector3.forward, Vector3.right, Vector3.up })
            AssertClose(q * v, back * v);
    }

    [Fact]
    public void LookRotationPointsForwardAndKeepsUp()
    {
        var direction = new Vector3(1, 1, -2).normalized;
        var q = Quaternion.LookRotation(direction);
        AssertClose(direction, q * Vector3.forward);
        Assert.True((q * Vector3.up).y > 0f);
        AssertClose(new Vector3(0, 0, -1), Quaternion.FromToRotation(Vector3.right, Vector3.back) * Vector3.right);
    }
}

public sealed class TransformTests : RuntimeTest
{
    [Fact]
    public void ChildWorldPositionFollowsParentLikeTheRenderer()
    {
        var parent = Spawn(1, "Parent", new Vector3(10, 0, 0));
        parent.transform.localEulerAngles = new Vector3(0, 90, 0);
        parent.transform.localScale = new Vector3(2, 2, 2);
        var child = Spawn(2, "Child", new Vector3(1, 0, 0), parentId: 1);

        AssertClose(new Vector3(10, 0, -2), child.transform.position);
        Assert.Same(parent.transform, child.transform.parent);
        Assert.Equal(1, parent.transform.childCount);
        Assert.Same(child.transform, parent.transform.Find("Child"));

        child.transform.position = new Vector3(10, 4, 0);
        AssertClose(new Vector3(0, 2, 0), child.transform.localPosition);
        AssertClose(new Vector3(10, 4, 0), child.transform.position);
    }

    [Fact]
    public void SetParentKeepsWorldPose()
    {
        var parent = Spawn(1, "Parent", new Vector3(5, 1, 0));
        parent.transform.localEulerAngles = new Vector3(0, 45, 0);
        var child = Spawn(2, "Child", new Vector3(2, 3, 4));
        child.transform.SetParent(parent.transform);

        Assert.Equal(1, child.State.ParentId);
        AssertClose(new Vector3(2, 3, 4), child.transform.position);
        AssertClose(Vector3.forward, child.transform.forward);

        child.transform.SetParent(null);
        Assert.Equal(-1, child.State.ParentId);
        AssertClose(new Vector3(2, 3, 4), child.transform.position);
    }

    [Fact]
    public void SetParentRejectsCycles()
    {
        var a = Spawn(1, "A");
        var b = Spawn(2, "B", parentId: 1);
        a.transform.SetParent(b.transform);
        Assert.Equal(-1, a.State.ParentId);
    }

    [Fact]
    public void RotateHonoursSpace()
    {
        var self = Spawn(1, "Self");
        self.transform.localEulerAngles = new Vector3(0, 0, 90);
        self.transform.Rotate(0, 90, 0, Space.Self);
        var world = Spawn(2, "World");
        world.transform.localEulerAngles = new Vector3(0, 0, 90);
        world.transform.Rotate(0, 90, 0, Space.World);

        AssertClose(Quaternion.Euler(0, 0, 90) * (Quaternion.Euler(0, 90, 0) * Vector3.forward), self.transform.forward);
        AssertClose(Quaternion.Euler(0, 90, 0) * (Quaternion.Euler(0, 0, 90) * Vector3.forward), world.transform.forward);
        AssertClose(Vector3.right, world.transform.forward);
    }

    [Fact]
    public void LookAtFacesTarget()
    {
        var go = Spawn(1, "Looker", new Vector3(1, 2, 3));
        go.transform.LookAt(new Vector3(4, 2, 7));
        AssertClose(new Vector3(3, 0, 4).normalized, go.transform.forward);
    }
}

public sealed class GameObjectTests : RuntimeTest
{
    [Fact]
    public void SetActiveIsInheritedByChildren()
    {
        var parent = Spawn(1, "Parent");
        var child = Spawn(2, "Child", parentId: 1);
        parent.SetActive(false);
        Assert.False(parent.State.ActiveSelf);
        Assert.True(child.activeSelf);
        Assert.False(child.activeInHierarchy);
        Assert.Null(GameObject.Find("Child"));
        Assert.Same(child, parent.transform.GetChild(0).gameObject);
    }

    [Fact]
    public void DestroyRemovesChildrenToo()
    {
        var parent = Spawn(1, "Parent");
        Spawn(2, "Child", parentId: 1);
        Spawn(3, "Other");
        GameObject.Destroy(parent);
        var destroyed = ForgeRuntimeCommands.Drain().Where(c => c.kind == 1).Select(c => c.targetId).OrderBy(i => i);
        Assert.Equal(new[] { 1, 2 }, destroyed);
        Assert.NotNull(Scene.FindById(3));
        Assert.True(parent.isDestroyed);
    }

    [Fact]
    public void DestroyingACloneInTheSameFrameIsForwarded()
    {
        var template = Spawn(1, "Template");
        var clone = GameObject.Instantiate(template)!;
        Assert.True(clone.State.Id < -1);
        GameObject.Destroy(clone);
        var commands = ForgeRuntimeCommands.Drain();
        Assert.Equal(0, commands[0].kind);
        Assert.Equal(1, commands[1].kind);
        Assert.Equal(clone.State.Id, commands[1].targetId);
    }

    [Fact]
    public void DelayedDestroyWaitsForTheDelay()
    {
        var go = Spawn(1, "Bullet");
        GameObject.Destroy(go, 0.5f);
        Scene.TickDelayedDestroys(0.3f);
        Assert.NotNull(Scene.FindById(1));
        Scene.TickDelayedDestroys(0.3f);
        Assert.Null(Scene.FindById(1));
        Assert.Contains(ForgeRuntimeCommands.Drain(), c => c.kind == 1 && c.targetId == 1);
    }

    [Fact]
    public void DestroyComponentOnlyRemovesThatComponent()
    {
        var go = Spawn(1, "Body");
        var body = go.AddComponent<Rigidbody>();
        var script = new Probe();
        go.AttachComponent(script);

        GameObject.Destroy(body);
        Assert.Null(go.GetComponent<Rigidbody>());
        Assert.False(go.State.HasRigidbody);

        GameObject.Destroy(script);
        Assert.True(script.DestroyRequested);
        Assert.Empty(ForgeRuntimeCommands.Drain());
        Assert.NotNull(Scene.FindById(1));
    }

    [Fact]
    public void RemappedCloneKeepsScriptSideChangesForOneSync()
    {
        var parent = Spawn(1, "Parent");
        var clone = GameObject.Instantiate(Spawn(2, "Template"), parent.transform)!;
        clone.name = "Renamed";
        int temp = clone.State.Id;

        Scene.RemapId(temp, 7);
        // Native echo of the fresh clone only knows the spawn data (root, original name).
        Scene.SyncObjects(new[]
        {
            new RuntimeObjectData { Id = 1, Name = "Parent" },
            new RuntimeObjectData { Id = 2, Name = "Template" },
            new RuntimeObjectData { Id = 7, Name = "Template (Clone)", ParentId = -1 }
        });
        Assert.Same(clone, Scene.FindById(7));
        Assert.Equal("Renamed", clone.name);
        Assert.Same(parent.transform, clone.transform.parent);
    }

    [Fact]
    public void ComponentLookupsWalkTheHierarchy()
    {
        var root = Spawn(1, "Root");
        var child = Spawn(2, "Child", parentId: 1);
        var probe = new Probe();
        child.AttachComponent(probe);
        Assert.Same(probe, root.GetComponentInChildren<Probe>());
        Assert.Null(root.GetComponent<Probe>());
        root.AttachComponent(new Probe());
        Assert.Equal(2, root.GetComponentsInChildren<Probe>().Length);
        Assert.Same(probe, child.GetComponentInParent<Probe>());
        child.DetachComponent(probe);
        Assert.Same(root.GetComponent<Probe>(), child.GetComponentInParent<Probe>());
    }

    private sealed class Probe : MonoBehaviour { }
}

public sealed class MonoBehaviourTests : RuntimeTest
{
    private sealed class Scripted : MonoBehaviour
    {
        public readonly List<string> Log = new();
        public bool Flag;
        public System.Collections.IEnumerator Steps()
        {
            Log.Add("a");
            yield return null;
            Log.Add("b");
            yield return new WaitForSeconds(0.1f);
            Log.Add("c");
            yield return new WaitUntil(() => Flag);
            Log.Add("d");
            yield return Inner();
            Log.Add("f");
        }
        private System.Collections.IEnumerator Inner()
        {
            Log.Add("e");
            yield return null;
        }
        private void Ping() => Log.Add("ping");
    }

    private static Scripted Create()
    {
        var go = Spawn(1, "Host");
        var script = new Scripted();
        go.AttachComponent(script);
        return script;
    }

    private static void Frame(Scripted script, float delta = 0.05f)
    {
        NextFrame(delta);
        script.TickInvokes(Time.deltaTime);
        script.TickCoroutines(Time.deltaTime, Time.unscaledDeltaTime);
    }

    [Fact]
    public void CoroutinesHonourYieldInstructions()
    {
        var script = Create();
        NextFrame();
        script.StartCoroutine(script.Steps());
        Assert.Equal(new[] { "a" }, script.Log);
        script.TickCoroutines(0.05f, 0.05f); // same frame: must not advance twice
        Assert.Equal(new[] { "a" }, script.Log);

        Frame(script); Assert.Equal("b", script.Log.Last());
        Frame(script); Assert.Equal("b", script.Log.Last());
        Frame(script); Assert.Equal("c", script.Log.Last());
        Frame(script); Assert.Equal("c", script.Log.Last());
        script.Flag = true;
        Frame(script); Assert.Equal(new[] { "a", "b", "c", "d", "e" }, script.Log);
        Frame(script); Frame(script);
        Assert.Equal("f", script.Log.Last());
        Assert.Equal(0, script.ActiveCoroutineCount);
    }

    [Fact]
    public void StopCoroutineStopsIt()
    {
        var script = Create();
        NextFrame();
        var routine = script.StartCoroutine(script.Steps());
        script.StopCoroutine(routine);
        Frame(script); Frame(script);
        Assert.Equal(new[] { "a" }, script.Log);
        Assert.False(routine.isRunning);
    }

    [Fact]
    public void InvokeAndInvokeRepeating()
    {
        var script = Create();
        script.Invoke("Ping", 0.1f);
        Frame(script); Assert.Empty(script.Log);
        Frame(script); Assert.Equal(new[] { "ping" }, script.Log);
        Frame(script); Assert.Single(script.Log);

        script.InvokeRepeating("Ping", 0f, 0.1f);
        Assert.True(script.IsInvoking("Ping"));
        for (int i = 0; i < 4; i++) Frame(script, 0.06f); // fires at 0.06, 0.12 and 0.24
        Assert.Equal(4, script.Log.Count);
        script.CancelInvoke("Ping");
        Frame(script); Frame(script);
        Assert.Equal(4, script.Log.Count);
    }
}

public sealed class InputTimeScreenTests : RuntimeTest
{
    [Fact]
    public void MouseScrollIsExposed()
    {
        Input.BeginFrame(Array.Empty<int>(), 0, 0, 0, 0, 0, 2f);
        Assert.Equal(2f, Input.mouseScrollDelta.y);
        Assert.Equal(2f, Input.GetAxisRaw("Mouse ScrollWheel"));
    }

    [Fact]
    public void NumberRowKeysExist()
    {
        Input.BeginFrame(new[] { (int)'1', 46 }, 0, 0, 0, 0, 0, 0);
        Assert.True(Input.GetKeyDown(KeyCode.Alpha1));
        Assert.True(Input.GetKey(KeyCode.Delete));
    }

    [Fact]
    public void TimeScaleScalesDeltaButNotUnscaled()
    {
        Time.timeScale = 0.5f;
        Time.BeginFrame(0.1f);
        Assert.Equal(0.05f, Time.deltaTime, 5);
        Assert.Equal(0.1f, Time.unscaledDeltaTime, 5);
        Time.BeginFixedStep();
        Assert.Equal(Time.fixedDeltaTime, Time.deltaTime);
        Assert.True(Time.inFixedTimeStep);
        Time.EndFixedStep();
        Assert.Equal(0.05f, Time.deltaTime, 5);
    }

    [Fact]
    public void CameraRaysUseFieldOfViewAndScreenSize()
    {
        Screen.SetSize(800, 600);
        var go = Spawn(1, "Camera", new Vector3(0, 1, 0));
        go.State.HasCamera = true;
        go.RefreshComponents();
        var camera = go.GetComponent<Camera>()!;
        camera.fieldOfView = 90f;
        go.transform.localEulerAngles = new Vector3(0, 90, 0);

        var center = camera.ScreenPointToRay(new Vector3(400, 300, 0));
        AssertClose(new Vector3(0, 1, 0), center.origin);
        AssertClose(Vector3.right, center.direction);
        var screen = camera.WorldToScreenPoint(new Vector3(5, 1, 0));
        AssertClose(new Vector3(400, 300, 5), screen);
        Assert.Same(camera, Camera.main);
    }
}
