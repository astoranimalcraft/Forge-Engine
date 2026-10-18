// Top-down camera for 2D-style games. Looks straight down (or at a tilt) and follows a target.
// Setup: create an empty object, add a Camera component, then attach this script.
// For a flat 2D game set Camera > Projection to Orthographic and pitch to 90.
using ForgeEngine;

public sealed class TopDownCamera : MonoBehaviour
{
    public GameObject? target;
    public float height = 12f;
    public float pitch = 90f;
    public float smoothTime = 0.15f;
    public float zoomSpeed = 4f;
    public float minHeight = 4f;
    public float maxHeight = 40f;

    private Vector3 _velocity;

    public override void Start()
    {
        target ??= GameObject.Find("Player");
    }

    public override void LateUpdate()
    {
        if (target is null) return;

        // Follow smoothly so the camera does not jitter when the target steps on physics.
        Vector3 focus = target.transform.position + Vector3.up * height;
        transform.position = Vector3.SmoothDamp(transform.position, focus, ref _velocity, smoothTime);
        transform.rotation = new Vector3(pitch, transform.rotation.y, 0f);
    }
}
