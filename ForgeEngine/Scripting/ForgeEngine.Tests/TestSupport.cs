using ForgeEngine;
using Xunit;

[assembly: CollectionBehavior(DisableTestParallelization = true)]

namespace ForgeEngine.Tests;

/// <summary>Resets the API's static runtime state between tests.</summary>
public abstract class RuntimeTest
{
    protected RuntimeTest()
    {
        Scene.Clear();
        Time.Reset();
        ForgeRuntimeCommands.Drain();
        Input.BeginFrame(Array.Empty<int>(), 0, 0, 0, 0, 0, 0);
        Input.BeginFrame(Array.Empty<int>(), 0, 0, 0, 0, 0, 0);
    }

    protected static GameObject Spawn(int id, string name, Vector3? position = null, int parentId = -1)
    {
        Scene.SyncObjects(Scene.AllGameObjects.Select(o => o.State).Append(new RuntimeObjectData
        {
            Id = id, Name = name, Position = position ?? Vector3.zero, ParentId = parentId
        }).ToList());
        return Scene.FindById(id)!;
    }

    protected static void AssertClose(Vector3 expected, Vector3 actual, float tolerance = 1e-3f)
    {
        Assert.True(Vector3.Distance(expected, actual) <= tolerance, $"Expected {expected} but got {actual}");
    }

    protected static void NextFrame(float delta = 0.016f)
    {
        Time.BeginFrame(delta);
    }
}
