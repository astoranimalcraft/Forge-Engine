# Meu Projeto DX12Forge

Projeto criado pelo editor **DX12 Forge Engine**. Esta pasta é autocontida: abra-a no editor
pela tela inicial (Open Project) e ela manterá cena, scripts, configurações e presets aqui dentro.

## Estrutura de pastas

| Pasta | Conteúdo |
|---|---|
| `Assets/Textures` | Texturas (PNG/JPG/BMP/TGA) usadas por albedo, normal map, metallic/roughness e AO |
| `Assets/Models` | Modelos 3D (FBX/OBJ/glTF...) importados pelo editor |
| `Assets/Audio` | Efeitos sonoros e músicas em WAV (tocados por `AudioSource`) |
| `Assets/Videos` | Vídeos de referência do projeto |
| `Assets/Materials` | Materiais exportados |
| `Assets/Scenes` | Cenas `.forgeScene` (a cena padrão é `Default.forgeScene`) |
| `Assets/Shaders` | Sombras/customização de shaders do projeto |
| `Assets/Misc` | Qualquer outro recurso |
| `Config/Inputs.json` | **Todas as teclas, eixos e comandos de teclado/mouse do jogo** |
| `Config/EditorLayout.ini` | Layout do editor e configuração do jogo (nome, empresa, criador, ícone) |
| `Scripts` | Scripts C# do jogo (`ForgeGameScripts.csproj` + seus `.cs`) |
| `Plugins` | DLLs/plugins opcionais |
| `Presets` | Presets de pós-processamento salvos pelo editor |

## Config/Inputs.json — o mapa de entradas do jogo

É aqui que ficam armazenadas todas as teclas, eixos e comandos. Os scripts C# nunca citam
teclas fixas: eles pedem o **nome** do eixo ou botão e o motor resolve pelo arquivo.
Para trocar o "WASD" por setas, ou "Jump" por "J", basta editar o JSON e clicar em **Compile**
(ou reiniciar o Play).

```json
{ "name": "Horizontal", "source": "Keys", "negative": ["A", "LeftArrow"], "positive": ["D", "RightArrow"], "smoothing": 0 }
{ "name": "Jump", "keys": ["Space"] }
```

- **axes**: eixos contínuos (-1..1). `source` pode ser `Keys`, `mouseDeltaX`, `mouseDeltaY`,
  `mouseScroll`, `mousePositionX`, `mousePositionY`. `smoothing` (0 = instantâneo) suaviza o valor.
- **buttons**: comandos nomeados que aceitam várias teclas e botões do mouse.
- Nomes de teclas válidos: `A-Z`, `0-9`, `Space`, `Enter`, `Escape`, `Tab`, `Backspace`,
  `LeftArrow/RightArrow/UpArrow/DownArrow`, `LeftShift/RightShift/LeftCtrl/RightCtrl/Alt`,
  `F1`–`F12`, `Comma`, `Period`, `Slash`, `Minus`, `Equals`, `LeftBracket`, `RightBracket`, `Backslash`, `BackQuote`.
  Nos botões também valem `MouseLeft`, `MouseRight`, `MouseMiddle`.
- Eixos padrão já criados: `Horizontal`, `Vertical`, `Turn`, `Mouse X`, `Mouse Y`, `Mouse ScrollWheel`.
- Botões padrão já criados: `Jump`, `Fire1`, `Fire2`, `Fire3`, `Run`, `Submit`, `Cancel`.

No script:

```csharp
float x = Input.GetAxisRaw("Horizontal");     // -1, 0 ou 1 (ou suavizado com GetAxis)
if (Input.GetButtonDown("Jump")) { ... }      // botão nomeado do Inputs.json
if (Input.GetButton("Fire1")) { ... }
```

## Scripts C#

Todo arquivo `.cs` dentro de `Scripts/` é compilado junto. A API é estilo Unity (`using ForgeEngine;`):

- `MonoBehaviour`: `Awake, Start, Update, LateUpdate, FixedUpdate, OnEnable, OnDisable, OnDestroy`,
  `OnCollisionEnter/Stay/Exit`, `OnTriggerEnter/Stay/Exit`
- `GameObject`: `transform`, `tag`, `layer`, `SetActive`, `Find`, `FindWithTag`, `FindObjectOfType`,
  `AddComponent`, `Instantiate`, `Destroy`
- `Transform`: `position`, `rotation` (graus), `quaternion`, `scale`, `LookAt`, `Rotate`, `Translate`
- Componentes: `MeshRenderer`, `Light`, `Camera`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`,
  `Rigidbody`, `AudioSource` (`Play`, `PlayOneShot`, `Stop`, `loop`), `ConstantForce`, `AutoDestroy`, `LookAtTarget`
- Serviços: `Input`, `Time`, `Physics` (`Raycast`, `OverlapSphere`, gravidade), `Mathf`, `Debug.Log`, `PlayerPrefs`, `Application`

Exemplos incluídos: `PlayerMovement.cs` (move, gira e pula usando os nomes do Inputs.json) e
`CoinPickup.cs` (trigger + tag + som + destruição). Há também `RotateExample.cs.example` para copiar.

## Física e colisão

1. Selecione o objeto e adicione **Collider** (Box/Sphere/Capsule) pelo Inspetor. O tamanho do
   colisor aparece como arame no viewport e **cresce junto com o gizmo de escala**.
2. Marque **Is Trigger** para gerar `OnTriggerEnter/Stay/Exit` sem bloqueio físico.
3. Adicione **Rigidbody** para gravidade, massa, arrasto e eventos `OnCollisionEnter/Stay/Exit`.
4. Objetos sem Rigidbody são estáticos (empurram quem tem massa, não se movem).

## Exportar o jogo

Menu **File → Export Game...** (ou Ctrl+B): exporta scripts compilados, cena, recursos, ícone e
`forge_game.ini` para uma pasta de jogo standalone — sem viewport, gizmos e grade do editor.
