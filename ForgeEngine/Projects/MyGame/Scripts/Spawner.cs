// Spawns a cube above this object every time you press the fire button, then lets physics
// drop it. Shows: Instantiate, Rigidbody setup from script, AddComponent, Destroy with a timer.
//
// Setup: put this on any object. Requires Config/Inputs.json (the "Fire1" button).
using ForgeEngine;

public sealed class Spawner : MonoBehaviour
{
    public float spawnInterval = 0.5f;
    public float lifetime = 6f;
    public float launchForce = 120f;

    private float _timer;

    public override void Update()
    {
        _timer -= Time.deltaTime;
        if (!Input.GetButton("Fire1") || _timer > 0f) return;
        _timer = spawnInterval;

        // Clone an existing object. If none is assigned, find one named "SpawnTemplate".
        GameObject? template = GameObject.Find("SpawnTemplate");
        if (template is null) { Debug.LogWarning("Spawner: no object named 'SpawnTemplate' in the scene."); return; }

        Vector3 spawnPoint = transform.position + Vector3.up * 1.5f;
        GameObject? clone = GameObject.Instantiate(template, spawnPoint, Quaternion.Euler(template.transform.rotation));
        if (clone is null) return;

        // A clone of an object that already has a Collider and Rigidbody keeps both.
        // If the template has neither, add them here. Physics needs the Collider for the shape.
        if (clone.GetComponent<Collider>() is null) clone.AddComponent<BoxCollider>();
        Rigidbody body = clone.GetComponent<Rigidbody>() ?? clone.AddComponent<Rigidbody>();
        body.useGravity = true;
        body.mass = 1f;

        body.AddForce(new Vector3(0f, launchForce, 0f), ForceMode.Impulse);

        StartCoroutine(DestroyAfter(clone, lifetime));
    }

    private System.Collections.IEnumerator DestroyAfter(GameObject target, float seconds)
    {
        yield return new WaitForSeconds(seconds);
        if (target is not null) GameObject.Destroy(target);
    }
}
