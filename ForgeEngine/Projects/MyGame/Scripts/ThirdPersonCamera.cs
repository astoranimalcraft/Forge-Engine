// Third-person orbit camera: follows a target and orbits with the mouse.
// Setup in the editor:
//   1. Create the player object (with its own movement script).
//   2. Create an empty object named "CameraRig", add a Camera component, then attach this script.
//   3. Drag the player into the "target" field in the Inspector, or leave it empty to find "Player" by name.
//
// The camera is moved in LateUpdate so it follows the player after the player has moved this
// frame; moving it in Update would make the view lag by one frame.
using ForgeEngine;

public sealed class ThirdPersonCamera : MonoBehaviour
{
    public GameObject? target;
    public float distance = 6f;
    public float height = 2f;
    public float mouseSensitivity = 3f;
    public float minPitch = -35f;
    public float maxPitch = 70f;

    private float _yaw;
    private float _pitch = 15f;

    public override void Start()
    {
        target ??= GameObject.Find("Player");
        _yaw = transform.rotation.y;
    }

    public override void LateUpdate()
    {
        if (target is null) return;

        _yaw += Input.GetAxisRaw("Mouse X") * mouseSensitivity;
        _pitch = Mathf.Clamp(_pitch + Input.GetAxisRaw("Mouse Y") * mouseSensitivity, minPitch, maxPitch);

        Vector3 focus = target.transform.position + Vector3.up * height;
        Quaternion rotation = Quaternion.Euler(_pitch, _yaw, 0f);
        Vector3 offset = rotation * new Vector3(0f, 0f, -distance);

        transform.position = focus + offset;
        transform.LookAt(focus);
    }
}
