# DX12 Forge Engine v0.5.8 — Inputs.json, física com eventos, overlay de colideres e correções

## 1. `Config/Inputs.json`: todas as teclas, eixos e comandos num só arquivo

- Todo projeto novo nasce com `Project/Config/Inputs.json` mapeando **eixos** (`axes`) e **comandos nomeados** (`buttons`) do teclado e do mouse.
- O runtime C# (`InputMap`, em `ForgeEngine.API/InputMapping.cs`) localiza o arquivo subindo a partir do diretório do host e o carrega automaticamente no primeiro frame.
- `Input.GetAxis(nome)`, `Input.GetAxisRaw(nome)` e `Input.GetButton/GetButtonDown/GetButtonUp(nome)` agora resolvem pelo JSON; suavização por eixo (`smoothing`) e fontes `Keys`, `mouseDeltaX/Y`, `mouseScroll`, `mousePositionX/Y`.
- Eixos padrão: `Horizontal` (WASD/setas), `Vertical`, `Turn` (Q/E), `Mouse X`, `Mouse Y`, `Mouse ScrollWheel`. Botões padrão: `Jump` (Espaço), `Fire1/2/3` (mouse), `Run` (Shift), `Submit` (Enter), `Cancel` (Esc).
- Trocar uma tecla é editar o JSON e recompilar — nenhum script precisa mudar.

## 2. Scaffold completo de novo projeto

`OpenProject`/criação de projeto copia agora, se ausentes:

- `Config/Inputs.json` (com fallback embutido no editor),
- `README.md` do projeto em português (estrutura de pastas, mapa de entradas, componentes, física, exportação),
- `Scripts/PlayerMovement.cs` (movimento/giro via nomes do Inputs.json, pulo com botão + `Rigidbody` + som),
- `Scripts/CoinPickup.cs` (`OnTriggerEnter`, tags, `PlayOneShot`, `Destroy`),
- `RotateExample.cs.example` continua disponível para copiar.

## 3. Física, colisão e som acessíveis pelos scripts C#

- **Eventos estilo Unity** despachados pelo passo fixo (50 Hz): `OnCollisionEnter/Stay/Exit(Collision)` e `OnTriggerEnter/Stay/Exit(Collider)` — structs `Collision`/`ContactPoint` com `gameObject`, `collider`, `relativeVelocity`, `impulse` e `contacts`.
- Detecção par-a-par por esferas efetivas, resolução de penetração por massa inversa (cinemático/estático = massa infinita) e cancelamento da velocidade normal (restituição ~0); contatos persistentes entram/permanecem/saem corretamente e são limpos quando objetos são destruídos.
- `Collider.effectiveRadius`: raio físico = tamanho do colider × escala do objeto — **a colisão cresce junto com o gizmo de escala**; `Physics.Raycast`, `OverlapSphere` e o clamp de chão do `Rigidbody` passaram a usá-la.
- **Componentes novos:** `ConstantForce` (força contínua por passo fixo), `AutoDestroy` (autodestruição por tempo de vida) e `LookAtTarget` (rotação contínua para um alvo). `AudioSource.PlayOneShot(clip)` toca sem substituir o clip/loop.

## 4. Marcações de colisão no viewport

- Wireframes de **Box, Sphere e Capsule** desenhados para todo objeto com colider, usando o pipeline de linhas do gizmo (profundidade sempre): verde = colider, azul = trigger, laranja = objeto selecionado.
- O arame usa `world × colliderSize`, portanto acompanha posição, rotação, escala (gizmo) e hierarquia. Some no jogo exportado (overlay desligado no player).

## 5. Céu azul UE5 de verdade

- Diagnóstico: só o zênite havia sido escurecido; as faixas de **horizonte/meia-altura e o topo das nuvens** permaneciam claros, e após ACES+gamma viravam azul-desbotado→branco justamente onde a câmera olha.
- Correção em `Shaders.hlsl`: `midDay` → (0.0056, 0.0284, 0.115), `horizonDay` → (0.03, 0.07, 0.17), `cloudLit` escurecido (~3×), e cobertura padrão de nuvens 0.35 → 0.55 (campo médio ≈0.47, antes cobria ~70% da cúpula).

## 6. Flicker da barra Assets — causa raiz eliminada

- `ImGui::PushID(folder.wstring().c_str())` selecionava a sobrecarga `PushID(const void*)`: o ID era o **endereço de heap** de uma string temporária por frame, alternando a cada frame e resetando o estado aberto/fechado da árvore.
- Seeds de ID de pastas, assets e cards de projeto viraram `u8string().c_str()` (hash de conteúdo) em `DrawProjectBrowser`/tela inicial.

## 7. Texturas e normal map

- Cadeia nativa confirmada correta (upload WIC → SRV → tabelas 4/6/7/8 → `t2/t4/t5/t6`); o build agora atualiza a cópia de `Shaders.hlsl` ao lado do executável (era ela a versão antiga que amostrava só albedo).
- Importação de modelos lê os **materiais do Assimp**: texturas `DIFFUSE` e `NORMALS`/`HEIGHT` encontradas no disco pré-preenchem os slots de Albedo e Normal Map do objeto recém-colocado.

## 8. Versão

- `CMakeLists.txt` 0.5.8, menus/about "v0.5.8"/"Editor 0.5.8", `Application.version => "0.5.8"`, marcadores `DX12Forge.project` gravam EngineVersion 0.5.8.
