// First-person controller: walks with WASD, looks with the mouse, jumps with Space.
//
// Setup in the editor:
//   1. Create a Capsule (GameObject > Create > Capsule) and name it "Player".
//   2. Add Component > Physics > Character Controller. Remove any Rigidbody: the controller
//      owns the movement, and a Rigidbody on the same object would fight it.
//   3. Create a second object named "Camera", add Component > Camera, and drag it into the
//      "cameraObject" field below. The camera is moved to the player's eye each frame.
//      (Leaving it empty falls back to Camera.main, or to a Camera on the player itself.)
//   4. Add Component > Script C# > FirstPersonController.
//
// Collision is resolved by CharacterController.Move, which sweeps the capsule against the
// world, so the player stops at walls, slides along them and steps over low obstacles.
using ForgeEngine;

public sealed class FirstPersonController : MonoBehaviour
{
    /// <summary>Camera to drive. Empty falls back to the main camera, or one on this object.</summary>
    public GameObject? cameraObject;

    public float walkSpeed = 4.5f;
    public float sprintSpeed = 8f;
    public float mouseSensitivity = 2.5f;
    public float eyeHeight = 0.75f;
    public float minPitch = -89f;
    public float maxPitch = 89f;
    public bool canJump = true;

    private CharacterController _controller = null!;
    private Camera? _camera;
    private float _yaw;
    private float _pitch;

    public override void Start()
    {
        _controller = GetComponent<CharacterController>()!;
        _camera = cameraObject is not null ? cameraObject.GetComponent<Camera>() : Camera.main;

        // A Camera on the player itself has no separate object to pitch, so it is driven in place.
        if (_camera is null) _camera = GetComponent<Camera>();

        _yaw = transform.rotation.y;
        _pitch = 0f;
        ApplyLook();
    }

    public override void Update()
    {
        // Mouse look. "Mouse X"/"Mouse Y" come from Config/Inputs.json. Mouse Y is positive
        // when the mouse moves up, so pitch subtracts it to look up.
        _yaw += Input.GetAxisRaw("Mouse X") * mouseSensitivity;
        _pitch = Mathf.Clamp(_pitch - Input.GetAxisRaw("Mouse Y") * mouseSensitivity, minPitch, maxPitch);

        // The body only turns on the spot; pitch belongs to the camera so the capsule stays upright.
        transform.rotation = new Vector3(0f, _yaw, 0f);
        ApplyLook();

        if (canJump && Input.GetButton("Jump") && _controller.isGrounded)
            _controller.Jump();

        float speed = Input.GetButton("Run") ? sprintSpeed : walkSpeed;
        Vector3 wish = new Vector3(Input.GetAxisRaw("Horizontal"), 0f, Input.GetAxisRaw("Vertical"));
        if (wish.sqrMagnitude > 1f) wish = wish.normalized;

        // Move takes a displacement for this frame; gravity and collisions are handled inside.
        Vector3 world = Quaternion.Euler(0f, _yaw, 0f) * wish * (speed * Time.deltaTime);
        _controller.Move(world);
    }

    /// <summary>Places the camera at the eye and applies the pitch, or tilts it in place.</summary>
    private void ApplyLook()
    {
        if (_camera is null) return;

        if (ReferenceEquals(_camera.gameObject, gameObject))
        {
            transform.rotation = new Vector3(_pitch, _yaw, 0f);
            return;
        }

        _camera.transform.position = transform.position + Vector3.up * eyeHeight;
        _camera.transform.rotation = new Vector3(_pitch, _yaw, 0f);
    }
}
