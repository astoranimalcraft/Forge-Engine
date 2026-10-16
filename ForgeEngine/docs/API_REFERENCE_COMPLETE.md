# DX12 Forge Engine — Referência da API (v0.5.0)

Esta referência descreve a superfície pública que existe neste pacote. Ela não promete funcionalidades que ainda não estão ligadas ao renderer nativo.

## 1. Arquitetura e tecnologias

- **Nativo:** C++17, Win32, Direct3D 12, DirectXMath.
- **Editor:** Dear ImGui com docking e ImGuizmo.
- **Importação:** Assimp (formatos suportados dependem da build da biblioteca).
- **Shaders:** HLSL compilado pelo pipeline CMake do projeto.
- **Gameplay:** biblioteca `ForgeEngine.API` em C#/.NET 8 e host de scripts separado.
- **Cena:** `Project/Assets/Scenes/*.forgeScene`.

## 2. Atalhos do editor

| Ação | Controle |
|---|---|
| Selecionar objeto | clique esquerdo na Scene ou Hierarchy |
| Câmera de editor | segurar botão direito na Scene e usar WASD; Q/E para altura |
| Mover gizmo | `1` |
| Rotacionar gizmo | `2` |
| Escalar gizmo | `3` |
| Salvar / compilar scripts | `Ctrl+S` |
| Criar terreno/câmera | menu `Create Object` ou menu de criação da Hierarchy |
| Esculpir terreno | selecione Terrain, ajuste Brush no Inspector e arraste com botão esquerdo |

Se o gizmo não estiver visível, selecione um objeto válido na Hierarchy, confirme que o painel Scene central tem tamanho maior que zero e pressione `1`. Se o objeto for selecionado mas não tiver mesh, os eixos ainda devem aparecer no pivot do objeto.

## 3. Componentes nativos no Inspector

| Componente | Propriedades/estado disponível | Limites atuais |
|---|---|---|
| Transform | Position, Rotation, Scale; gizmo de mover/rotacionar/escalar | rotação nativa armazenada em radianos |
| Mesh Renderer | renderização habilitada/desabilitada | não é um sistema ECS genérico |
| Material | cor base, metallic, roughness e mapas de textura suportados | conjunto PBR simplificado; não é um material graph |
| Camera | FOV, near clip, far clip; marcador de câmera | câmera secundária não substitui automaticamente a câmera do editor |
| Light | Directional, Point, Spot; cor, intensidade, alcance, cones e sombras | quantidade de luzes por passe limitada pelas constantes do shader |
| Collider | flag de collider do objeto | não equivale a um solver completo de física |
| Audio Source | referência/estado básico de áudio | backend WAV/WinMM descrito em `SCRIPTING_API.md` |
| C# Script | classe, habilitação, abrir script, remover componente | scripts executam no host gerenciado separado |
| Terrain | malha de altura 65×65 e pincéis Raise/Lower/Smooth/Flatten | ferramenta de escultura baseada em heightfield, não escultura de malha arbitrária |

## 4. API C#

O namespace é `ForgeEngine`. A referência detalhada de membros, exemplos e limitações está em [`SCRIPTING_API.md`](SCRIPTING_API.md). Resumo:

- **Ciclo de vida:** `MonoBehaviour.Start()`, `Update()`, `FixedUpdate()`, `LateUpdate()`, `OnEnable()`, `OnDisable()`, `OnDestroy()`.
- **Objetos/cena:** `GameObject`, `Scene`, `Component`, `GetComponent<T>()`, `GetComponents<T>()`, `TryGetComponent<T>()`, `AddComponent<T>()`, `RemoveComponent<T>()`, `Find()`.
- **Transformação:** `Transform.position`, `rotation`, `eulerAngles`, `localScale`, `forward/right/up`, `Translate`, `Rotate`, `LookAt`, `TransformPoint`, `InverseTransformPoint`.
- **Matemática:** `Vector2`, `Vector3`, `Vector4`, `Color`, `Mathf`, `Random`.
- **Entrada e tempo:** `Input`, `KeyCode`, `Time`, `Application`.
- **Render e luz:** `MeshRenderer`, `Light`, `LightType`.
- **Física básica:** `Rigidbody`, `Collider`, `BoxCollider`, `SphereCollider`, `Physics.Raycast`, `RaycastHit`.
- **Áudio:** `AudioSource` para WAV, com backend atual limitado.
- **Diagnóstico:** `Debug.Log`, `LogWarning`, `LogError`, `Assert`.

### Exemplo de script

```csharp
using ForgeEngine;

public sealed class SpinObject : MonoBehaviour
{
    public float degreesPerSecond = 60f;

    public override void Update()
    {
        transform.Rotate(0f, degreesPerSecond * Time.deltaTime, 0f);
    }
}
```

### Exemplo de movimento com Input

```csharp
using ForgeEngine;

public sealed class SimpleMovement : MonoBehaviour
{
    public float speed = 4f;

    public override void Update()
    {
        var direction = new Vector3(
            Input.GetAxisRaw("Horizontal"),
            0f,
            Input.GetAxisRaw("Vertical"));
        if (direction.sqrMagnitude > 1f) direction = direction.normalized;
        transform.position += direction * speed * Time.deltaTime;
    }
}
```

## 5. Build e estrutura do projeto

- `src/Engine/`: dispositivo D3D12, renderer, recursos, cena, câmera e buffers.
- `src/Editor/`: UI, Inspector, Hierarchy, importação e ferramentas de edição.
- `src/Scripting/`: integração do processo host C#.
- `assets/shaders/Shaders.hlsl`: shaders de materiais, iluminação, céu, grid e pós-processamento.
- `Scripting/ForgeEngine.API/`: assembly referenciado pelos scripts do usuário.
- `Scripting/ForgeScriptHost/`: executável host gerenciado.
- `Project/Scripts/`: scripts do projeto e `.csproj`.
- `Project/Assets/`: modelos, texturas, materiais, áudio, cenas e shaders.

Siga `BUILD_AND_TROUBLESHOOTING.md` para instalar MSVC, Windows SDK, CMake, Ninja e dependências. A build precisa ser validada no Windows com o conjunto de dependências correto.

## 6. Notas de renderização

O shader inclui iluminação direta, roughness/metallic simplificados, céu procedural com gradiente atmosférico, nuvens, disco solar, estrelas, grid do editor e pós-processamento com tone mapping tipo ACES e bloom aproximado. Não é uma implementação integral do Unreal Engine 5: não há Lumen, Nanite, Virtual Shadow Maps ou pipeline HDR de produção nesta versão.

## 7. Limites conhecidos

- A API gerenciada não implementa toda a Unity API; ver `LIMITATIONS.md`.
- Hierarquia pai/filho completa não está implementada no Transform gerenciado.
- Física e colisão são aproximadas.
- A API de áudio não oferece mixer, áudio espacial 3D ou múltiplos canais completos.
- Componentes criados somente dentro do processo C# não são automaticamente persistidos na cena nativa.
- O código desta revisão foi modificado e inspecionado estaticamente; a compilação completa requer ambiente Windows com dependências nativas configuradas.
