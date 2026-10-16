# Forge Engine v0.5.7 — Exportação de jogo, API Unity-like, céu azul, hierarquia e gizmo

## 1. Export Game (botão + Ctrl+B)
- Novo `File → Export Game...` (atalho **Ctrl+B**) abre o modal com: nome do jogo, empresa, criador, ícone (.ico/.png/.bmp) e pasta de saída.
- A exportação gera uma pasta independente com: exe renomeado, DLLs, `Shaders.hlsl`, `forge_game.ini`, ícone copiado, `Scripting/ForgeScriptHost/bin/...` e o projeto completo (`Project/`, sem `obj/.vs/Logs`).
- `forge_game.ini` ativa o **modo jogador standalone**: sem painéis do editor, sem gizmo, sem grade e sem overlay de debug. **Ctrl+G** liga/desliga o overlay caso queira depurar no build exportado.
- Nome da janela, ícone do executável e "criador" vêm do ini; `Application.productName/companyName/creatorName` no C# leem o mesmo arquivo.
- A cena atual é salva como `Assets/Scenes/Default.forgeScene` antes de copiar.

## 2. Céu branco de dia corrigido
- As faixas do gradiente (zênite, meio, horizonte) foram recalculadas em HDR pré-invertido para compensar o tonemap, produzindo o degradê azul estilo UE5 em vez de céu branco lavado.
- Sombras das nuvens escurecidas, vélo de cirrus reduzido; estrelas aparecem apenas quando `daylight < 0.55`.
- A grade do viewport agora é controlada por `gSkyParameters.w`: no jogo exportado o céu fica limpo (sem grade).

## 3. Hierarquia pai/filho corrigida
- `RebuildWorldTransform` compõe as matrizes LOCAIS dos ancestrais (independente da ordem de reconstrução) com proteção a ciclos.
- Mover um pai agora propaga para todos os descendentes (cascada BFS com guarda de reentrância).
- Duplicar, excluir, carregar cena (novo formato `DX12FORGE_SCENE 7`) e reparentear via drag no hierarchy atualizam o mundo corretamente.

## 4. Gizmo (ImGuizmo) corrigido
- Arrastar um **filho** não teleporta mais para coordenadas de mundo: a matriz editada é reconvertida para espaço local subtraindo o mundo do pai (com fallback seguro quando a matriz é singular).
- **Ctrl** durante o arraste ativa snapping (0.25 transladar / 5° girar / 0.1 escalar).
- Tamanho do gizmo padronizado (`SetGizmoSizeClipSpace(0.14)`), preview de frustum da câmera usa a matriz de mundo real do objeto.

## 5. Project Explorer sem flicker
- A listagem da pasta e a árvore de subpastas agora são cacheadas com re-scan no máximo a cada 500 ms; erros transitórios de `directory_iterator` nunca apagam a listagem anterior (só substitui em scan completo com sucesso).
- Criação/renomeação/importação de assets invalidam o cache imediatamente.

## 6. API C# expandida (estilo Unity)
- `Quaternion` completo (Euler/AngleAxis/LookRotation/operadores) e `Transform.quaternion`/`Rotate(Quaternion)`.
- `GameObject`: `tag`, `layer`, `activeSelf`/`SetActive`, `CompareTag`, `FindWithTag`, `FindGameObjectsWithTag`.
- `GameObject.Instantiate(original[,position,rotation[,scale]])` e `Destroy(go|component)` agora criam/removem objetos reais na cena nativa via protocolo de comandos (`CMD|INST`/`CMD|DEST` + remapeamento `NEWID`).
- Coliders armazenados no estado sincronizado: `BoxCollider`, `SphereCollider`, `CapsuleCollider` (tipo/tamanho/trigger), `Rigidbody` (mass/useGravity/isKinematic) com presença espelhada no objeto nativo.
- `Time.timeScale`, `Time.realtimeSinceStartup`; `Mathf`: `Atan/Atan2/Asin/Acos/Fract/InverseLerp/DeltaAngle/Wrap/Clamp(int)/FloorToInt...`; `Color` com mais cores/operadores; `Physics.OverlapSphere`.
- `PlayerPrefs` (GetInt/SetInt/GetFloat/SetFloat/GetString/SetString/Save → `ForgePlayerPrefs.txt`), `Application.Quit()` (fecha o host; no jogo exportado encerra o processo) e `Application.productName/companyName/creatorName/version=0.5.7`.

## 7. Formato de cena v7
- Cabeçalho `DX12FORGE_SCENE 7` gravando/ lendo tag, layer, tipo e tamanho de colider, trigger e campos de rigidbody por objeto. Carrega cenas v6 normalmente.

## Limitações conhecidas
- IDs de objeto são índices: destruir objetos desloca IDs posteriores (instâncias de script antigas podem passar a apontar para outro objeto).
- `hasCollider` (presença) continua controlado pelo editor; scripts ajustam tipo/tamanho/trigger/rigidbody.
- O jogo exportado exige o mesmo layout de pastas gerado pelo Export Game.
