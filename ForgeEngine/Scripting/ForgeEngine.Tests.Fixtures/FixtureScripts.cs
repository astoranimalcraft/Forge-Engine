using ForgeEngine;

namespace ForgeEngine.Tests.Fixtures;

public sealed class CoroutineProbe : MonoBehaviour
{
    public override void Start() => StartCoroutine(Routine());

    private System.Collections.IEnumerator Routine()
    {
        Debug.Log("co:start");
        yield return null;
        Debug.Log("co:next-frame");
        yield return new WaitUntil(() => Input.GetKey(KeyCode.Space));
        Debug.Log("co:space");
    }
}

public sealed class Spawner : MonoBehaviour
{
    private bool _done;
    public override void Update()
    {
        if (_done) return;
        _done = true;
        var spawned = Instantiate(GameObject.Find("Template")!);
        spawned!.name = "Spawned";
        Destroy(GameObject.Find("Victim1"));
        Destroy(GameObject.Find("Victim2"));
        var temporary = Instantiate(GameObject.Find("Template")!);
        Destroy(temporary);
    }
}

public sealed class InputProbe : MonoBehaviour
{
    public override void Update()
    {
        if (Input.mouseScrollDelta.y != 0f)
            Debug.Log($"scroll:{Input.mouseScrollDelta.y}:{Input.GetAxisRaw("Mouse ScrollWheel")}:{Screen.width}x{Screen.height}");
    }
}

public sealed class SelfRemover : MonoBehaviour
{
    public override void Start() => Destroy(this);
    public override void OnDestroy() => Debug.Log("self-removed");
}

public sealed class CollisionProbe : MonoBehaviour
{
    public override void OnCollisionEnter(Collision collision) => Debug.Log("collision:enter:" + collision.gameObject.name);
    public override void OnCollisionExit(Collision collision) => Debug.Log("collision:exit:" + collision.gameObject.name);
}

public sealed class Hider : MonoBehaviour
{
    public override void Update()
    {
        if (Input.GetKeyDown(KeyCode.H)) gameObject.SetActive(false);
    }
}
