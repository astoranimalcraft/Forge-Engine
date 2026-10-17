# Scripts C# — guia e referência da API

## 1. Visão geral

A lógica de gameplay é escrita em C# sobre `ForgeEngine.API`. O projeto de scripts é `Project/Scripts/ForgeGameScripts.csproj`; a assembly compilada é `ForgeGameScripts.dll`. O processo `ForgeScriptHost` carrega a assembly e executa os componentes derivados de `MonoBehaviour`.

O host .NET é um processo separado do renderer nativo. A engine envia a cada frame os estados suportados e o input; o host executa os callbacks e devolve transforms, propriedades de renderer/luz e logs. Isso isola parte das falhas do runtime gerenciado, mas **não é um sandbox de segurança**.

## 2. Criar e associar um script

1. Selecione um objeto na **Scene Hierarchy**.
2. No Inspector, clique em **+ Add Component → Script C#**.
3. Informe um nome de classe C# válido, por exemplo `PlayerMovement`.
4. A engine cria `Project/Scripts/PlayerMovement.cs`, associa a classe ao objeto e tenta abrir o arquivo no Visual Studio.
5. Edite o script e salve.
6. Use **Ctrl+S** na engine para compilar explicitamente, ou aguarde a detecção automática da alteração.
7. Pressione **F5** para iniciar o Play Mode.

O nome da classe deve começar com letra ou `_` e conter apenas letras, números ou `_`. A classe deve derivar de `MonoBehaviour`, ser concreta e ter construtor público sem parâmetros. Evite criar duas classes com o mesmo nome simples.

## 3. Exemplo: mover e girar

```csharp
using ForgeEngine;

public sealed class PlayerMovement : MonoBehaviour
{
    public float moveSpeed = 5f;
    public float rotateSpeed = 90f;

    public override void Start()
    {
        Debug.Log($"{gameObject.name}: pronto!");
    }

    public override void Update()
    {
        var direction = new Vector3(
            Input.GetAxis("Horizontal"),
            0f,
            Input.GetAxis("Vertical"));

        if (direction.sqrMagnitude > 1f)
            direction = direction.normalized;

        transform.position += direction * moveSpeed * Time.deltaTime;

        if (Input.GetKey(KeyCode.Q))
            transform.Rotate(0f, -rotateSpeed * Time.deltaTime, 0f);
        if (Input.GetKey(KeyCode.E))
            transform.Rotate(0f, rotateSpeed * Time.deltaTime, 0f);
    }
}
```

`moveSpeed` e `rotateSpeed` são campos C# comuns. A build atual não os serializa como campos editáveis automaticamente no Inspector; altere os valores no código.

## 4. Ciclo de vida

`MonoBehaviour` disponibiliza estes callbacks:

| Callback | Quando é chamado |
|---|---|
| `Awake()` | Na primeira ativação da instância. |
| `OnEnable()` | Quando o componente fica ativo. |
| `Start()` | Uma vez antes da primeira atualização ativa. |
| `FixedUpdate()` | Em passos fixos de `Time.fixedDeltaTime` (0,02 s por padrão) em tempo escalado, com limite de até cinco passos por frame. |
| `Update()` | Uma vez por frame enquanto o componente estiver ativo. |
| `LateUpdate()` | Depois do `Update()` de todos os scripts e das corrotinas do frame. |
| `OnDisable()` | Quando o componente passa de ativo para desativado. |
| `OnDestroy()` | Quando a instância é removida/encerrada. |

Todos são métodos `public override void`. Uma exceção em callback é reportada pelo host no Console. Se o processo host terminar, o editor encerra o Play Mode.

## 5. Tipos e API

### `MonoBehaviour` e `Component`

- `gameObject`: objeto ao qual o componente está associado.
- `transform`: atalho para o Transform do objeto.
- `name`: nome do objeto.
- `enabled`: ativa/desativa o componente.
- `GetComponent<T>()`: retorna o primeiro componente compatível no mesmo objeto ou `null`.
- `GetComponents<T>()`: retorna todos os componentes compatíveis no mesmo objeto.

### `GameObject` e `Scene`

- `gameObject.name`: ler/alterar o nome.
- `GameObject.Find(string name)` / `Scene.Find(string name)`: procurar objeto pelo nome exato.
- `Scene.FindObjectOfType<T>()`: primeiro componente do tipo encontrado.
- `Scene.FindObjectsOfType<T>()`: componentes do tipo encontrado.
- `Scene.FindById(int id)`: localizar objeto pelo ID de runtime.
- `Scene.gameObjects`: coleção dos objetos sincronizados no runtime.

IDs são internos à cena em execução; não os persista como identificadores estáveis entre sessões.

### `Transform`

- `position`: posição no mundo; `localPosition`: posição relativa ao pai (valor salvo na cena).
- `rotation` / `eulerAngles`: rotação Euler do mundo em graus; `localEulerAngles` / `localRotation`: relativas ao pai; `quaternion`: rotação do mundo como `Quaternion`.
- Ordem Euler: Z, depois X, depois Y (igual à Unity e ao renderer DX12).
- `localScale`; `lossyScale` = produto das escalas da cadeia de pais.
- Hierarquia (v0.6.0): `parent`, `SetParent(parent, worldPositionStays = true)`, `childCount`, `GetChild(i)`, `Find("Filho/Neto")`, `root`, `IsChildOf`, `DetachChildren`.
- `forward`, `right`, `up`: direções calculadas a partir da rotação.
- `Translate(Vector3 value, Space relativeTo = Space.Self)`.
- `Rotate(Vector3 eulerDegrees, Space relativeTo = Space.Self)` e overload com três floats.
- `LookAt(Vector3 target)`.

`Rotate` respeita `Space.Self` (eixos locais) e `Space.World` (eixos do mundo). Também existem `Rotate(axis, angle)`, `RotateAround(point, axis, angle)` e `SetPositionAndRotation`.

### `Vector2`

Campos `x`, `y`; `magnitude`, `normalized`; constantes `zero`, `one`; operadores `+`, `-`, `*`, `/` e `ToString()`.

### `Vector3`

Campos `x`, `y`, `z`; `magnitude`, `sqrMagnitude`, `normalized`; constantes `zero`, `one`, `up`, `down`, `right`, `left`, `forward`, `back`; operadores `+`, `-`, unário `-`, `*`, `/`; métodos estáticos `Dot`, `Cross`, `Distance`, `Lerp`; `ToString()`.

### `MeshRenderer`

- `enabled`: liga/desliga a renderização do mesh suportado.
- O componente só representa o renderer nativo existente; não é uma API completa de materiais/meshes.

### `Light`

- `type`: `LightType.None`, `Directional`, `Point` ou `Spot`.
- `color`: cor RGB.
- `intensity`: intensidade não negativa.
- `range`: alcance mínimo de 0,1.
- `spotAngle`: ângulo externo limitado entre 2 e 89 graus.
- `innerSpotAngle`: ângulo interno limitado entre 1 e 87 graus.
- `isActiveAndEnabled`: estado de ativação combinado.

### `Input`

- `GetKey(KeyCode key)`, `GetKeyDown(KeyCode key)`, `GetKeyUp(KeyCode key)`.
- `GetMouseButton(int button)`, `GetMouseButtonDown(int button)`, `GetMouseButtonUp(int button)`; `0` = esquerdo, `1` = direito, `2` = botão central.
- `anyKey`, `anyKeyDown`.
- `mousePosition`: posição relativa ao viewport, origem no canto superior esquerdo.
- `mouseDelta`: deslocamento do mouse em pixels no frame.
- `GetAxis` / `GetAxisRaw` aceitam `Horizontal`, `Vertical`, `Mouse X`, `Mouse Y` e `Mouse ScrollWheel`.

Mapeamento de eixos: `Horizontal = D - A`, `Vertical = W - S`; Mouse X/Y usam o delta do mouse. `Mouse ScrollWheel` e `Input.mouseScrollDelta.y` devolvem os "cliques" da roda do mouse no frame (positivo = para frente). Input do mouse é preenchido quando o cursor está dentro do viewport e a janela está em foco.

`KeyCode` inclui letras A–Z, números `Alpha0`–`Alpha9`, `Delete`, setas, espaço, Return, Escape, Backspace, Tab, Shift, Control, Alt e F1–F12. Nem todos os estados de tecla distinguem modificadores da mesma forma que um sistema de input completo.

### `Time`

- `deltaTime`: tempo do frame atual em segundos.
- `unscaledDeltaTime`: tempo do frame sem `Time.timeScale`.
- `fixedDeltaTime`: intervalo do `FixedUpdate` (0,02 s por padrão, configurável); dentro de `FixedUpdate`, `deltaTime` vale `fixedDeltaTime`.
- `timeScale`: escala de tempo; `0` pausa `FixedUpdate`, física e `WaitForSeconds`.
- `time`: tempo acumulado no processo host.
- `frameCount`: número de frames processados.

### `Debug`

- `Debug.Log(object?)`: nível INFO.
- `Debug.LogWarning(object?)`: nível WARN.
- `Debug.LogError(object?)`: nível ERROR.
- `Debug.Assert(bool, object? message = null)`: escreve erro quando a condição é falsa.

### `Mathf`

`PI`, `Deg2Rad`, `Rad2Deg`, `Abs`, `Sin`, `Cos`, `Tan`, `Sqrt`, `Clamp`, `Clamp01`, `Min`, `Max`, `Lerp` e `MoveTowards`.

## 6. Compilação, recarga e DLL

- **Ctrl+S na janela da engine:** chama `dotnet build` para `ForgeGameScripts.csproj` e para `ForgeScriptHost.csproj`.
- A detecção automática verifica arquivos `.cs` e `.csproj` a cada ~200 ms e espera ~850 ms sem alteração antes de compilar.
- Se compilar enquanto estiver em Play, o host atual é reiniciado e `Start()` é executado novamente para a nova sessão.
- Se o build falhar, a DLL anterior pode continuar no disco, mas o Play iniciado pelo editor é cancelado se a compilação solicitada falhar.

Saídas padrão:

- Scripts: `Project/Scripts/bin/Debug/net8.0/ForgeGameScripts.dll`
- Host: `Scripting/ForgeScriptHost/bin/Debug/net8.0/ForgeScriptHost.dll`
- Log de build: `Project/Scripts/build.log` e `Project/Scripts/host-build.log`

## 7. Dicas de desempenho e segurança

- Não faça operações pesadas de arquivo/rede em `Update()`.
- Evite loops infinitos; o processo separado protege o renderer de parte das falhas, mas pode deixar o editor sem resposta enquanto aguarda a resposta do frame.
- Valide valores numéricos e evite dividir por zero.
- Scripts têm as permissões do usuário que executa a engine. O host separado **não** bloqueia acesso a arquivos, processos ou rede.
- Use um nome de classe por arquivo como convenção e mantenha as classes públicas e com construtor padrão.

## 6. APIs adicionais na v0.4.9 (e anteriores)

### Vetores

- `Vector2.Dot(a, b)`, `Vector2.Distance(a, b)`, `Vector2.Lerp(a, b, t)`, `Vector2.Min/Max(a, b)`.
- `Vector3.Angle(a, b)`, `Vector3.MoveTowards(current, target, maxDistanceDelta)`, `Vector3.ClampMagnitude(v, maxLength)`, `Vector3.Project(v, normal)`, `Vector3.Reflect(v, normal)`, `Vector3.Min/Max(a, b)`.
- `Vector4` oferece campos `x/y/z/w`, `zero`, `one` e operadores básicos.
- `Color` usa canais RGBA normalizados entre 0 e 1, com `white`, `black`, `red`, `green`, `blue`, `clear` e `Color.Lerp`.

### Transform

- `TransformDirection(direction)` converte uma direção local para a direção rotacionada no mundo.
- `InverseTransformDirection(direction)` aplica a rotação inversa.
- `TransformPoint(point)` transforma um ponto local incluindo escala e posição.
- `InverseTransformPoint(point)` converte um ponto do mundo para o espaço local.

Desde a v0.6.0 essas conversões seguem a cadeia de pais com a mesma composição do renderer (escala, rotação, translação e depois o pai).

### Matemática e aleatoriedade

`Mathf` agora inclui `Sign`, `Floor`, `Ceil`, `Round`, `Pow`, `Exp`, `Log`, `Repeat`, `PingPong`, `SmoothStep`, `LerpAngle` e `Approximately`, além das funções já listadas.

`Random` fornece `value`, `Range(int minInclusive, int maxExclusive)`, `Range(float minInclusive, float maxInclusive)` e `insideUnitSphere`.

### Tempo e aplicação

- `Time.smoothDeltaTime`, `Time.unscaledTime`, `Time.unscaledDeltaTime`, `Time.fixedDeltaTime`, `Time.time` e `Time.frameCount`.
- `Application.isPlaying`, `Application.productName` e `Application.version`.

### Exemplo: movimento suave e direção

```csharp
using ForgeEngine;

public sealed class Patrol : MonoBehaviour
{
    public Vector3 target = new Vector3(4, 0, 4);
    public float speed = 2f;

    public override void Update()
    {
        transform.position = Vector3.MoveTowards(
            transform.position, target, speed * Time.deltaTime);

        Vector3 direction = (target - transform.position).normalized;
        if (direction.sqrMagnitude > 0.0001f)
            transform.LookAt(target);
    }
}
```

### Limites importantes

A API pode alterar as propriedades que o protocolo sincroniza com o renderer nativo. A física é básica e não substitui um solver completo; áudio aceita WAV via WinMM; não há reprodução de vídeo. Corrotinas, `Invoke` e `Instantiate`/`Destroy` estão disponíveis (veja PATCH_NOTES_v060.md). Não confunda essas APIs iniciais com sistemas completos de produção.


## 7. Componentes adicionais na v0.4.9

### Criar e consultar componentes

```csharp
var body = gameObject.AddComponent<Rigidbody>();
var collider = gameObject.AddComponent<SphereCollider>();
if (gameObject.TryGetComponent<AudioSource>(out var audio))
    Debug.Log("Audio source disponível");
```

`AddComponent<T>()`, `TryGetComponent<T>()` e `RemoveComponent<T>()` funcionam no runtime gerenciado. Componentes nativos presentes no Inspector são sincronizados para o host C#; componentes adicionados somente pelo script vivem no processo de scripts e não são persistidos ao salvar a cena.

### Física básica

- `Rigidbody.velocity`, `mass`, `drag`, `useGravity`, `isKinematic`, `AddForce()` e `AddImpulse()`.
- `Collider.radius`, `isTrigger`, `SphereCollider` e `BoxCollider.size`.
- `Physics.gravity`, `Physics.Raycast(Ray, out RaycastHit, maxDistance)` e `RaycastHit.point`, `.normal`, `.distance`, `.collider`, `.gameObject`.

A física atual é deliberadamente simples: integração em passo fixo, colisão aproximada por esfera e resposta contra o plano de chão. `BoxCollider` também usa a aproximação esférica nas consultas. Não é um substituto para PhysX/Jolt e ainda não resolve colisões gerais entre corpos.

### Câmera

`Camera.main`, `fieldOfView`, `nearClipPlane` e `farClipPlane` estão disponíveis para scripts e são sincronizados com o objeto Camera nativo (v0.6.0). `ScreenPointToRay(Input.mousePosition)` e `WorldToScreenPoint` usam o tamanho do viewport (`Screen.width`/`height`).

### Áudio WAV

```csharp
var audio = gameObject.GetComponent<AudioSource>() ?? gameObject.AddComponent<AudioSource>();
audio.clip = @"..\Assets\Audio\click.wav";
audio.volume = 0.8f;
audio.Play();
```

O backend atual usa `winmm.PlaySound`, aceita WAV e suporta apenas um stream ativo no processo. `Pause()` interrompe o stream; reprodução espacial 3D, mixer, pitch-shifting e múltiplos canais ainda não estão implementados.


## 8. Novidades da v0.6.0

### Corrotinas e Invoke

```csharp
public sealed class Blink : MonoBehaviour
{
    public override void Start()
    {
        StartCoroutine(BlinkRoutine());
        InvokeRepeating(nameof(Tick), 1f, 1f);
    }

    private System.Collections.IEnumerator BlinkRoutine()
    {
        var renderer = GetComponent<MeshRenderer>();
        while (true)
        {
            renderer!.enabled = !renderer.enabled;
            yield return new WaitForSeconds(0.5f);
            yield return new WaitUntil(() => !Input.GetKey(KeyCode.Space));
        }
    }

    private void Tick() => Debug.Log($"t = {Time.time:0.0}");
}
```

- Yield suportados: `null`, `WaitForSeconds`, `WaitForSecondsRealtime`, `WaitForEndOfFrame`, `WaitForFixedUpdate`, `WaitUntil`, `WaitWhile`, `CustomYieldInstruction`, outra `Coroutine` ou `IEnumerator`.
- As corrotinas retomam depois de todos os `Update` e antes dos `LateUpdate`. Desativar o GameObject para as corrotinas dele.

### Ativação, hierarquia e destruição

- `gameObject.SetActive(bool)` é sincronizado com o renderer; `activeInHierarchy` considera os pais.
- `Destroy(gameObject)` destrói também os filhos; `Destroy(gameObject, 2f)` agenda a destruição; `Destroy(this)` remove só o script.
- `Instantiate(prefab, parent)` / `Instantiate(prefab, position, rotation, parent)`.
- `GetComponentInChildren<T>()`, `GetComponentsInChildren<T>()`, `GetComponentInParent<T>()`.

### Testes automatizados

`dotnet test Scripting/ForgeEngine.Tests` executa testes da API e do host real (`ForgeScriptHost`) pelo protocolo de linhas.
