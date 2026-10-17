# Patch Notes v0.6.0 — IDs estáveis, Play/Stop seguro e API C# revisada

## Correções no runtime nativo (C++/DX12)
- **IDs estáveis para scripts.** Antes, o ID de um objeto era o índice em `m_objects`; um `Destroy` deslocava os IDs de todos os objetos seguintes, fazendo scripts reiniciarem ou apontarem para o objeto errado, e dois `Destroy` no mesmo frame podiam apagar o objeto errado. Agora cada objeto recebe `scriptObjectId` ao entrar em Play e o protocolo usa esse ID.
- **Stop restaura a cena inteira.** O Play Mode tira uma cópia completa da cena; ao parar, objetos destruídos durante o Play voltam e clones criados por `Instantiate` são removidos (antes a restauração era por índice e perdia objetos).
- **Destroy seguro para a GPU.** `DestroyObjectForScript` espera a GPU antes de liberar os constant buffers do objeto, evitando liberar recursos ainda usados por command lists em voo.
- `Instantiate` e `Destroy` são aplicados no fim do mesmo frame do script (antes era no frame seguinte). Destruir um clone criado no mesmo frame agora funciona, assim como clonar um clone recém-criado.
- `GameObject.SetActive(false)` esconde o objeto e seus filhos no renderer (mesh, sombras, luzes e picking).
- Point/spot lights filhas de outros objetos usam a posição/direção do mundo.
- O índice da câmera do viewport é ajustado quando um objeto anterior é destruído.

## Protocolo C++ ↔ C#
Campos novos sempre no fim das linhas (compatível com hosts antigos):
- `FRAME`: `mouseScroll`, `screenWidth`, `screenHeight`.
- `OBJ`/`SET`: `parentId`, `activeSelf`, `cameraFieldOfView`, `cameraNearClip`, `cameraFarClip`.
- IDs temporários de clones começam em `-1000` (`-1` continua significando “nenhum”).

## Correções na API C#
- **Corrotinas funcionam.** `TickCoroutines` nunca era chamado pelo host. Agora rodam até o primeiro `yield` em `StartCoroutine` e retomam depois de todos os `Update` e antes dos `LateUpdate`. Exceções em corrotinas aparecem no Console.
- **`OnCollisionExit`** era despachado como `OnCollisionStay`; agora chama `OnCollisionExit`.
- **Ordem Euler igual à do renderer.** `Quaternion.Euler`/`eulerAngles` usavam X→Y→Z, enquanto o DX12 usa Z→X→Y (igual à Unity). `eulerAngles` também tinha fórmula errada. Corrigido, com round-trip testado.
- `Transform.Rotate(..., Space)` respeita `Space.Self`/`Space.World`.
- `Time.timeScale` agora afeta `FixedUpdate` e a física (o acumulador usa tempo escalado; dentro de `FixedUpdate`, `deltaTime == fixedDeltaTime`).
- `LateUpdate` roda depois do `Update` de **todos** os scripts (antes era intercalado por script).
- `Destroy(component)` remove só o componente (antes apagava o GameObject inteiro). Para scripts, `OnDisable`/`OnDestroy` são chamados no fim do frame e o script não é recriado.
- `Destroy(gameObject)` também destrói os filhos.
- Alterações feitas logo após `Instantiate` (nome, pai, `SetActive`...) não são mais perdidas quando o motor confirma o clone.
- `KeyCode.Alpha0`–`Alpha9` e `KeyCode.Delete` (o motor já enviava essas teclas).
- `GameObject.Find`, `FindWithTag`, `FindObjectOfType` e similares ignoram objetos inativos, como na Unity.

## Recursos novos na API C#
- **Hierarquia:** `transform.parent`, `SetParent(parent, worldPositionStays)`, `childCount`, `GetChild`, `Find("Filho/Neto")`, `root`, `IsChildOf`, `DetachChildren`, `foreach (Transform child in transform)`.
- **Espaços local/mundo reais:** `localPosition`/`localEulerAngles`/`localRotation` são relativos ao pai; `position`/`rotation`/`quaternion`/`lossyScale` são do mundo. `TransformPoint`/`InverseTransformPoint`/`TransformVector` seguem a mesma composição do renderer. Novos: `RotateAround`, `Rotate(axis, angle)`, `SetPositionAndRotation`, `LookAt(target, worldUp)`, `LookAt(Transform)`, setter de `forward`.
- **`GameObject.SetActive` sincronizado** com o motor, `activeInHierarchy`, `isDestroyed`.
- `GetComponentInChildren`, `GetComponentsInChildren`, `GetComponentInParent`, `Scene.GetRootGameObjects`.
- `Destroy(obj, delay)`, `Instantiate(original, parent)` e `Instantiate(original, position, rotation, parent)`; `Destroy`/`Instantiate` podem ser chamados sem prefixo dentro de scripts.
- **Invoke:** `Invoke`, `InvokeRepeating`, `CancelInvoke`, `IsInvoking`.
- **Yield instructions:** `WaitUntil`, `WaitWhile`, `WaitForSecondsRealtime`, `WaitForFixedUpdate`, `CustomYieldInstruction`, `yield return` de outra corrotina/`IEnumerator`; `StopCoroutine(IEnumerator)`, `Coroutine.isRunning`.
- **Input:** `Input.mouseScrollDelta` e eixo `Mouse ScrollWheel` (fonte `mouseScroll` do `Inputs.json`) com a roda real do mouse.
- **Screen:** `Screen.width`/`height` = tamanho real do viewport do jogo.
- **Camera:** `fieldOfView`, `nearClipPlane` e `farClipPlane` sincronizados com a câmera nativa; `ScreenPointToRay`, `WorldToScreenPoint`, `aspect`.
- **Física:** `layerMask` em `Raycast`/`OverlapSphere`, `Raycast` sem `out hit`, `ForceMode`, `Rigidbody.MovePosition`/`MoveRotation`; objetos inativos não colidem.
- **Quaternion:** `LookRotation(forward, up)`, `FromToRotation`, `ToAngleAxis`, `Normalize`.
- **Mathf:** `SmoothDamp`, `SmoothDampAngle`, `MoveTowardsAngle`, `LerpUnclamped`, `Log(v, base)`, `Log10`, `Infinity`, `Epsilon`, `IsPowerOfTwo`, `NextPowerOfTwo`.
- **Random:** `InitState(seed)`, `insideUnitCircle`, `onUnitSphere`, `rotation`.
- **Time:** `fixedDeltaTime` configurável, `fixedTime`, `inFixedTimeStep`, `fixedUnscaledDeltaTime`.
- `MonoBehaviour.isActiveAndEnabled`, `print()`; `Component.tag`/`CompareTag`/`TryGetComponent`.

## Testes
- Novo projeto `Scripting/ForgeEngine.Tests` (xUnit): testes da API e testes que executam o `ForgeScriptHost` real pelo protocolo de linhas com scripts de `Scripting/ForgeEngine.Tests.Fixtures`.
- Rodar: `dotnet test Scripting/ForgeEngine.Tests` (funciona também fora do Windows).

## Limitações conhecidas
- Scripts anexados ao objeto original começam no clone apenas no frame seguinte ao `Instantiate`.
- `SetActive` e mudanças de pai feitas por scripts valem só durante o Play (a cena salva não muda).
- O build nativo (MSVC/DX12) destas mudanças precisa ser confirmado no Windows.
