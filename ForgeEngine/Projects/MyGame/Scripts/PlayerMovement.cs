// Template example: create a new Script C# through Object Inspector > Add Component,
// then paste this code into the generated .cs file and name its class PlayerMovement.
// This .cs.example file is intentionally not compiled by the project.
using ForgeEngine;

public sealed class PlayerMovement : MonoBehaviour
{
    public float moveSpeed = 5f;
    public float rotateSpeed = 90f;

    public override void Start()
    {
        Debug.Log($"{gameObject.name}: PlayerMovement started");
    }

    public override void Update()
    {
        var direction = new Vector3(
            Input.GetAxisRaw("Horizontal"),
            0f,
            Input.GetAxisRaw("Vertical"));

        if (direction.sqrMagnitude > 1f)
            direction = direction.normalized;

        transform.position += direction * moveSpeed * Time.deltaTime;
        if (Input.GetKey(KeyCode.Q))
            transform.Rotate(0f, -rotateSpeed * Time.deltaTime, 0f);
        if (Input.GetKey(KeyCode.E))
            transform.Rotate(0f, rotateSpeed * Time.deltaTime, 0f);

        if (Input.GetMouseButtonDown(0))
            Debug.Log($"Viewport cursor: {Input.mousePosition}");
    }
}
