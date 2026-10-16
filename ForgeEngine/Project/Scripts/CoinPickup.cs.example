// Modelo de exemplo copiado para Scripts/CoinPickup.cs em todo projeto novo.
// Demonstra: trigger + tag + som + Destroy.
// No editor: dê a este objeto a Tag "Coin", adicione um Collider e marque Is Trigger.
// No jogador, adicione Tag "Player" e um Collider (sem trigger).
using ForgeEngine;

public sealed class CoinPickup : MonoBehaviour
{
    public float spinSpeed = 60f;
    public string collectSound = "Assets/Audio/click.wav";

    public override void Update()
    {
        transform.Rotate(0f, spinSpeed * Time.deltaTime, 0f);
    }

    public override void OnTriggerEnter(Collider other)
    {
        GameObject picker = other.gameObject;
        int remaining = GameObject.FindGameObjectsWithTag("Coin").Length - 1;
        Debug.Log($"Moeda coletada por {picker.name}. Restam {remaining} no cenário.");

        GetComponent<AudioSource>()?.PlayOneShot(collectSound);
        GameObject.Destroy(gameObject);
    }
}
