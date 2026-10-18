// First-person controller: walks with WASD, looks with the mouse, jumps with Space.
//
// Setup in the editor:
//   1. Create a Capsule (GameObject > Create > Capsule) and name it "Player".
//   2. Add Component > Physics > Character Controller. Remove any Rigidbody: the controller
//      owns the movement, and a Rigidbody on the same object would fight it.
//   3. Create a separate object named "Camera", add Component > Camera, and drag it into the
//      "cameraObject" field below. Leaving the field empty falls back to Camera.main.
//      The camera must be a separate object: this drives it to the eye every frame.
//   4. Add Component > Script C# > FirstPersonController.
//
// Collision is resolved by CharacterController.Move, which sweeps the capsule against the
// world, so the player stops at walls, slides along them and steps over low obstacles.
using ForgeEngine;

public sealed class FirstPersonController : MonoBehaviour
{
    /// <summary>Camera to drive. Empty falls back to the main camera.</summary>
    public GameObject? cameraObject;

    public float walkSpeed = 4.5f;
    public float sprintSpeed = 8f;
    public float mouseSensitivity = 2.5f;
    public float eyeHeight = 0.75f;
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

        // A camera on the player itself has nowhere to be offset to, and driving it would push
        // it up by eyeHeight every frame, so it is left alone.
        if (_camera is not null && ReferenceEquals(_camera.gameObject, gameObject)) _camera = null;

        _yaw = transform.rotation.y;
    }

    public override void Update()
    {
        Look();
        Walk();
    }

    /// <summary>Mouse look. "Mouse X"/"Mouse Y" come from Config/Inputs.json.</summary>
    private void Look()
    {
        _yaw += Input.GetAxisRaw("Mouse X") * mouseSensitivity;
        // Mouse Y is positive when the mouse moves up, so pitch subtracts it to look up.
        _pitch = Mathf.Clamp(_pitch - Input.GetAxisRaw("Mouse Y") * mouseSensitivity, -maxPitch, maxPitch);

        // The body only turns on the spot; pitch belongs to the camera so the capsule stays upright.
        transform.rotation = new Vector3(0f, _yaw, 0f);
        if (_camera is null) return;

        _camera.transform.position = transform.position + Vector3.up * eyeHeight;
        _camera.transform.rotation = new Vector3(_pitch, _yaw, 0f);
    }

    private void Walk()
    {
        if (canJump && _controller.isGrounded && Input.GetButton("Jump")) _controller.Jump();

        Vector3 wish = new(Input.GetAxisRaw("Horizontal"), 0f, Input.GetAxisRaw("Vertical"));
        if (wish.sqrMagnitude > 1f) wish = wish.normalized;

        float speed = Input.GetButton("Run") ? sprintSpeed : walkSpeed;

        // Move takes a displacement for this frame; gravity and collisions are handled inside.
        _controller.Move(Quaternion.Euler(0f, _yaw, 0f) * wish * (speed * Time.deltaTime));
    }
}
