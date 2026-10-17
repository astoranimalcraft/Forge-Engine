// First-person controller: walks with WASD, looks with the mouse, jumps with Space.
// Setup in the editor:
//   1. Create an object (GameObject > Create > Capsule) and name it "Player".
//   2. Add Component > Physics > Collider  (Capsule) and Rigidbody.
//   3. Add Component > Camera & Audio > Camera. The camera sits at this object's origin,
//      so raise the view by putting the Camera on a child later, or just offset the eye height below.
//   4. Add Component > Script C# > FirstPersonController.
//
// How physics is enabled: a Rigidbody alone does nothing. Bullet creates the body from the
// Collider, so you need BOTH. The collider's size comes from the object's scale, so a
// Capsule with scale (1, 2, 1) gives a 2-unit-tall capsule.
using ForgeEngine;

public sealed class FirstPersonController : MonoBehaviour
{
    public float moveSpeed = 5f;
    public float sprintSpeed = 9f;
    public float jumpSpeed = 5f;
    public float mouseSensitivity = 2.5f;
    public float gravity = 20f;

    private Rigidbody _body = null!;
    private float _pitch;
    private float _verticalVelocity;

    public override void Start()
    {
        _body = GetComponent<Rigidbody>()!;
        _pitch = transform.rotation.x;

        // The player must not tumble when it hits a wall.
        _body.freezeRotationX = true;
        _body.freezeRotationY = true;
        _body.freezeRotationZ = true;
    }

    public override void Update()
    {
        // Mouse look. "Mouse X"/"Mouse Y" come from Config/Inputs.json.
        float mouseX = Input.GetAxisRaw("Mouse X") * mouseSensitivity;
        float mouseY = Input.GetAxisRaw("Mouse Y") * mouseSensitivity;
        _pitch = Mathf.Clamp(_pitch - mouseY, -89f, 89f);
        transform.rotation = new Vector3(_pitch, transform.rotation.y + mouseX, 0f);

        // Horizontal movement is done by setting velocity, which is what the solver reads.
        float speed = Input.GetButton("Run") ? sprintSpeed : moveSpeed;
        Vector3 wish = new Vector3(Input.GetAxisRaw("Horizontal"), 0f, Input.GetAxisRaw("Vertical")).normalized;
        Vector3 world = transform.TransformDirection(wish) * speed;

        Vector3 velocity = _body.velocity;
        _body.velocity = new Vector3(world.x, velocity.y, world.z);
    }

    public override void FixedUpdate()
    {
        // Jump and gravity run on the fixed step so the solver sees a stable velocity.
        Vector3 velocity = _body.velocity;
        if (Input.GetButton("Jump") && Mathf.Abs(velocity.y) < 0.05f)
            _verticalVelocity = jumpSpeed;
        else
            _verticalVelocity -= gravity * Time.fixedDeltaTime;

        _body.velocity = new Vector3(velocity.x, _verticalVelocity, velocity.z);
    }
}
