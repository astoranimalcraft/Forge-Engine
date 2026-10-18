# Forge Engine v0.5.8

This build adds the Export Game workflow (standalone player with name/icon/creator), a much larger Unity-style C# API (Quaternion, tag/layer, Instantiate/Destroy, State-backed colliders and rigidbodies, PlayerPrefs, Application config), a fixed blue UE5-style daytime sky, parent/child hierarchy corrections, gizmo snapping with parent-aware dragging and a stable (non-flickering) Project Explorer. See [`docs/PATCH_NOTES_v057.md`](docs/PATCH_NOTES_v057.md).

# Forge Engine

Forge Engine v0.5.8 é uma base de engine/editor 3D em C++17, Win32 e DirectX 12, com API C# expandida, céu procedural e pós-processamento HDR. A interface integrada usa **Dear ImGui (branch docking)** + **ImGuizmo**, com a cena DirectX 12 renderizada sob a região central transparente do docking (sem janela Scene separada).


## Documentação completa

A documentação foi dividida por assunto para facilitar consulta:

- [`docs/INDEX.md`](docs/INDEX.md) — índice e visão geral.
- [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) — interface, controles, cenas, assets, iluminação e Play Mode.
- [`docs/SCRIPTING_API.md`](docs/SCRIPTING_API.md) — criação de scripts, ciclo de vida e referência da API C# (inclui vetores, Color, Transform, Mathf e Random).
- [`docs/BUILD_AND_TROUBLESHOOTING.md`](docs/BUILD_AND_TROUBLESHOOTING.md) — requisitos, comandos de build e diagnóstico de erros.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — componentes internos e protocolo C++/C#.
- [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md) — funcionalidades existentes e limitações conhecidas.
- [`docs/PATCH_NOTES_v053.md`](docs/PATCH_NOTES_v053.md) — novidades do Projeto Hub, pós-processamento, scripts e hierarquia.


## Requisitos

- Windows 10/11 x64
- Visual Studio 2026 (ou uma instalação MSVC compatível) com a carga de trabalho **Desenvolvimento para desktop com C++**
- Windows SDK
- CMake 3.21 ou superior
- .NET 8 SDK (para compilar e executar scripts C#)
- Ninja Multi-Config e Git no `PATH`
- Internet durante a primeira configuração, para buscar Dear ImGui e ImGuizmo dos repositórios oficiais

Repositórios de dependências baixados pelo CMake:

- Dear ImGui: https://github.com/ocornut/imgui (branch `docking`)
- ImGuizmo: https://github.com/CedricGuillemet/ImGuizmo (branch `master`)

As fontes das dependências ficam no diretório de build do CMake (`_deps`) e não precisam ser copiadas manualmente para `src/`.

## Compilar no Visual Studio 2026

Abra o Developer PowerShell do Visual Studio 2026 e entre na pasta que contém `CMakeLists.txt`:

```powershell
cmake --preset windows-ninja
cmake --build --preset debug
```

Para Release:

```powershell
cmake --build --preset release
```

Para limpar a configuração anterior:

```powershell
Remove-Item -Recurse -Force .\out\build\windows-ninja -ErrorAction SilentlyContinue
cmake --preset windows-ninja
cmake --build --preset debug
```

Executável Debug: `out/build/windows-ninja/Debug/ForgeEngine.exe`.

## Inicialização e projetos

- Tela de carregamento durante a inicialização dos recursos do motor.
- Project Hub com criação/abertura de projeto, cartões de projetos recentes, miniatura ilustrativa e data.
- Ao abrir um projeto, o editor mostra uma segunda tela de carregamento, prepara as pastas, carrega a cena padrão e reabre caminhos de modelos/scripts registrados.
- Caption personalizada escura com minimizar, maximizar/restaurar, fechar e arrastar pela faixa superior.

## Interface do editor

- Barra superior com **File**, **Create Object**, **Assets**, **Window** e **Help**
- Layout dockable persistido em `Project/Config/EditorLayout_v033_NoSceneViewport.ini`
- **Scene Hierarchy** à esquerda: seleção, adicionar cubo/esfera, duplicar e remover objetos
- **Centro transparente**: a cena DirectX 12 é renderizada diretamente atrás da interface, sem uma janela Scene/SceneViewport separada; gizmo ImGuizmo desenhado sobre a região central
- **Object Inspector** à direita: nome, posição, rotação, escala, Mesh Renderer, cor PBR, Metallic, Roughness e referências para texturas Albedo/Normal/Metallic-Roughness/AO
- **Project Files** embaixo: árvore recursiva de pastas, criar subpastas, renomear arquivos/pastas e importar/copiar assets
- **Console** embaixo: mensagens e status da engine
- **Post Processing**: SSAO aproximado, bloom, blur por movimento de câmera, aberração cromática, sharpen, saturação, exposição, vinheta e tone mapping ACES
- Salvar/abrir cenas `.forgeScene`

## Controles

- Clique em **Create Object → Post Processing Settings...** para ajustar os efeitos de imagem.
- Na Hierarchy, arraste um objeto sobre outro para torná-lo filho; use o menu de contexto para remover a relação. A relação ainda é organizacional e não herda transformações.

- `W/A/S/D`: movimentar a câmera quando o botão direito está pressionado no centro 3D
- `Q/E`: descer/subir a câmera durante RMB
- `Shift`: acelerar a câmera
- Botão direito no centro 3D: olhar ao redor; cursor escondido enquanto segura
- Botão esquerdo: selecionar objetos ou manipular um gizmo
- `1`: mover; `2`: rotacionar; `3`: escalar
- `Ctrl+N`: nova cena; `Ctrl+O`: abrir; `Ctrl+S`: compilar os scripts C#; `Ctrl+Shift+S`: salvar cena como; use **File → Save Scene** para salvar a cena atual
- `Ctrl+I`: importar asset; `Ctrl+D`: duplicar objeto; `Delete`: excluir seleção

## Estrutura

```text
ForgeEngine/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── Project/
│   ├── Config/                 # Layout persistente do editor
│   └── Assets/
│       ├── Audio/
│       ├── Materials/
│       ├── Misc/
│       ├── Models/
│       ├── Scenes/
│       ├── Videos/                 # Vídeos importados (playback ainda não implementado)
│       ├── Shaders/
│       └── Textures/
├── assets/shaders/Shaders.hlsl
├── Scripting/
│   ├── ForgeEngine.API/          # API gerenciada pública em C#
│   └── ForgeScriptHost/          # Host .NET isolado do renderer nativo
├── Project/Scripts/ForgeGameScripts.csproj
├── docs/ARCHITECTURE.md
└── src/
    ├── App/main.cpp
    ├── Editor/EditorUI.h, EditorUI.cpp
    ├── Scripting/ScriptRuntime.h, ScriptRuntime.cpp
    └── Engine/Engine.h, Engine.cpp
```

## Renderização atual

- Render target HDR e tone mapping ACES com bloom de baixo custo e vinheta
- BRDF Cook–Torrance/GGX, Fresnel Schlick, geometria Smith e tone mapping ACES
- Luz direcional, uma point light e um spotlight configuráveis em tempo real
- Iluminação ambiente e reflexos especulares aproximados pelo céu procedural (sem cubemap)
- Shadow map direcional 4096×4096 com PCF 5×5
- Céu atmosférico procedural com gradiente, nuvens FBM, disco solar e contribuição atmosférica das luzes
- Grade infinita, eixos XYZ, previews ao arrastar assets e seleção com outline
- Gizmos 3D Move / Rotate / Scale nativos em DirectX 12

## Limitações conhecidas

- O importador Assimp instancia malhas de formatos comuns suportados pela build (OBJ, FBX, glTF/GLB, DAE, STL, PLY, 3DS). Materiais e texturas embutidas dos modelos não são extraídos/aplicados automaticamente.
- O slot Albedo/Base Color carrega texturas para a GPU e é amostrado pelo shader PBR. Os slots Normal, Metallic/Roughness e AO ainda armazenam referências sem participar da renderização.
- O shadow map 4096² com PCF 5×5 é gerado apenas pela luz direcional; point light e spotlight iluminam objetos e contribuem para o céu procedural, mas ainda não geram sombras próprias.
- Collider e Audio Source ainda são marcadores no Inspector. O sistema de scripts C# é executado por um processo .NET isolado e sincroniza Transform, Mesh Renderer e Light com a cena nativa; física e áudio não são implementados por esses marcadores.
- ImGui e ImGuizmo são obtidos de upstream no primeiro configure, portanto a primeira compilação precisa de Git, internet e compatibilidade com as APIs atuais desses projetos.
- Este pacote foi revisado estaticamente, mas não foi possível executar MSVC/DirectX 12 neste ambiente. A compilação e a validação visual final precisam ser feitas no Windows.


## Correções de compilação desta revisão

- A declaração de `ImGui_ImplWin32_WndProcHandler` foi adicionada explicitamente no `.cpp` da UI.
- O construtor de `ForgeEngine` foi movido para o `.cpp`, onde `ForgeEngineEditorUI` está completo, evitando a instanciação de `unique_ptr` com tipo incompleto.
- A cópia do nome do arquivo de cena usa cópia limitada terminada em NUL, sem a chamada CRT insegura `wcsncpy`.

A compilação definitiva requer Windows, MSVC, Windows SDK, CMake, Ninja, Git e acesso à internet para obter Dear ImGui e ImGuizmo na primeira configuração.


## Runtime assertion hardening

The editor now uses explicit DockBuilder split output IDs, stores the actual dockspace ID for Reset Layout, guards ImGuizmo against invalid or zero-sized viewport matrices, increases the ImGui SRV descriptor heap to 1024 entries, and logs descriptor exhaustion/resource omissions to the Visual Studio Output window rather than triggering the editor's own unconditional assertion.


## Version 0.3.2 — runtime assertion hardening

- Increased the Dear ImGui SRV descriptor pool to 1024 entries and made allocation failure emit a diagnostic to the Visual Studio Output window instead of immediately breaking in the engine's own allocator.
- Reworked the initial docking layout to use both explicit output IDs from `DockBuilderSplitNode`.
- Deferred Reset Dock Layout until the next safe point in the frame, avoiding destruction of dock nodes from inside a menu popup.
- Added finite-matrix and viewport-size checks before calling ImGuizmo.
- Added diagnostic output if Win32/DirectX 12 ImGui backend initialization fails.

If the program still breaks in Debug, copy the assertion's **Expression**, **File**, and **Line** from the Visual Studio assertion dialog or the Call Stack window. The generic breakpoint message alone does not identify which assertion fired.

A revisão 0.3.2 usava um arquivo INI separado; a versão 0.3.3 usa `EditorLayout_v033_NoSceneViewport.ini` para evitar restaurar a janela Scene removida.


## Versão 0.3.3 — renderização no centro transparente

- Removida a janela/painel `Scene` (SceneViewport). A cena DirectX 12 aparece por trás da área central passthrough do docking, sem uma janela ImGui vazia cobrindo-a.
- A área de renderização central agora vem do nó central do DockSpace; a projeção, grade infinita, picking e câmera usam o tamanho desse nó.
- Corrigida a amostragem do pós-processamento para usar o tamanho real do render target em vez do tamanho do centro do editor, evitando UVs deslocadas e a cena invisível.
- ImGuizmo é desenhado diretamente sobre a região central transparente e continua permitindo mover, rotacionar e escalar sem uma janela Scene separada.
- Seleção de objetos e navegação RMB tratam a área de UI separadamente da região central.
- Corrigidos caminhos de retorno de `ImGui::Begin`: `ImGui::End()` é chamado mesmo quando `Begin()` retorna `false`.
- Foi selecionado um novo arquivo INI do editor para evitar que o docking antigo reabra um painel Scene.

A compilação e o teste visual final ainda precisam ser executados no Windows com MSVC e DirectX 12.


## Versão 0.3.4 — pareamento explícito de janelas ImGui

- Corrigido o pareamento de `ImGui::Begin()` / `ImGui::End()` nas janelas Scene Hierarchy, Object Inspector, Project Files e Console.
- Cada janela agora chama `ImGui::Begin()` uma vez, desenha seu conteúdo somente se o retorno for `true` e chama `ImGui::End()` uma única vez no final da função. Não há mais `ImGui::End()` dentro do ramo de `Begin() == false` nessas janelas.
- O Inspector não usa retorno antecipado para o caso sem objeto selecionado; a janela é finalizada no ponto único do final.
- O conteúdo dos filhos (`BeginChild`) só é desenhado quando o filho está visível; cada filho continua sendo encerrado exatamente uma vez.
- `DrawMainMenu` só chama `EndMenuBar()` se `BeginMenuBar()` retornar `true`.
- O `ImGui::End()` do Dock Host permanece antes das janelas dockadas independentes, como exige a estrutura do Dear ImGui; ele não deve ser movido para depois delas.

A verificação estática desta revisão encontrou sete chamadas `ImGui::Begin()` e sete `ImGui::End()` correspondentes, delimitadores balanceados e nenhum retorno antecipado entre `Begin()` e o `End()` final nas quatro janelas corrigidas. A compilação real com MSVC/DirectX 12 ainda deve ser confirmada no Windows.


## Version 0.3.5 — editor interaction update

- Fixed ImGuizmo matrix handoff to use the DirectXMath matrix memory layout expected by ImGuizmo, avoiding the previous double-transpose of view, projection and object transforms.
- Added a built-in model/primitive library: Cube, Sphere, Plane, Cylinder, Cone and Torus.
- Added Windows Explorer file drop import into `Project/Assets`, routed by extension (textures, models, shaders, materials, audio, scenes and misc).
- Project Files assets can be dragged onto the Scene viewport. PNG/JPG image drops assign the selected object's albedo path; `.obj` drops load triangle geometry into the scene.
- Added a basic OBJ importer supporting `v`, `vn`, polygon triangulation, positive/negative position indices and `v/vt/vn` or `v//vn` normal references.

### Current renderer limitations

The material inspector stores albedo, normal, metallic/roughness and AO asset paths in the scene, but this build does **not yet upload image files to DirectX 12 GPU texture resources or sample them in the PBR shader**. Therefore assigning PNG/normal maps now fixes the editor assignment workflow but will not visibly change shading until the GPU texture pipeline is implemented. FBX/glTF/GLB files can be imported into Project Files, but only OBJ geometry is currently instantiated by dragging into the Scene. `.mat` and custom shader files are imported as assets; material parsing and runtime shader hot reload are not implemented.


## Version 0.4.0 — Assimp model importing and material-slot drag & drop

- Added Assimp as a CMake FetchContent dependency (`v5.4.3`). The first configure/build requires Git and internet access to fetch Assimp, Dear ImGui, and ImGuizmo.
- Replaced the limited handwritten OBJ parser with Assimp import processing (triangulation, generated smooth normals, UV channel 0, mesh flattening, and common model formats such as OBJ, FBX, glTF/GLB, DAE, STL, PLY, and 3DS where supported by the Assimp build).
- Added drag targets directly to each Inspector material texture slot, and improved Scene texture drop to import/copy the image into `Project/Assets/Textures` before assigning its project-relative path.
- Added WIC-based image decoding and DirectX 12 texture upload/SRV creation for the Albedo / Base Color slot. Albedo textures are sampled by the PBR shader; primitive UVs and imported UV channel 0 are passed to the renderer. Normal, Metallic/Roughness and AO slots currently save references but are not yet sampled by the shader.
- Assimp model materials and embedded texture extraction are not automatic yet; assign the albedo image in the Inspector after importing a model.


## Version 0.4.1 — model drop, sky and grid fixes

- Dropped 3D models are normalized around their bounds and placed at a predictable size and ground level.
- Windows Explorer drops now import and instantiate supported 3D model formats; external image drops assign to the selected object's Albedo or create a textured preview plane when nothing is selected.
- Model files are parsed from their original source during a Windows Explorer drop so external glTF buffer sidecars can still be found.
- Sky cloud projection fades more gently near the horizon to reduce stretched cloud bands.
- The procedural ground grid now has 10 cm sub-grid lines, 1 m lines and 10 m major lines, composited with a lower-opacity tint so the sky remains visible.

Build/runtime validation still needs to be performed on Windows with MSVC and DirectX 12.


## Version 0.4.2 — Scene drag previews, sky and grid

- Grid now composites only the grid lines over the sky; the cells between lines remain transparent. Added clear visual hierarchy for 0.1 m, 1 m and 10 m lines.
- Reworked procedural sky gradient and lowered cloud frequency to avoid the previous pixelated/stretched cloud pattern.
- Importing a model from Windows Explorer adds it to Project Files only. Dragging a model from Project Files over Scene creates a temporary cyan-tinted 3D preview; releasing commits it, cancelling removes it.
- Dragging a texture over Scene previews it on the selected mesh or a temporary plane, commits on release, and rolls back on cancellation.
- Removed Assimp's unconditional winding-order flip and enabled `aiProcess_FixInfacingNormals` to reduce inward-facing model normals.

The project still needs a real Windows/MSVC + DirectX 12 build/runtime test; this environment cannot validate the graphics output.


## Version 0.4.3 — drag & drop robusto, iluminação e sombras

- Corrigido o destino de drag & drop da Scene: o alvo agora é criado depois que o painel Project Files registra o item de origem, usando uma área invisível de tamanho igual ao viewport central. Durante o arraste aparece uma borda ciano e uma instrução de soltura.
- Ao soltar um modelo suportado no centro da Scene, a geometria é importada com Assimp, posicionada sob o cursor e selecionada. Ao soltar uma imagem, ela é aplicada ao objeto selecionado; sem seleção, a engine cria um plano com a textura.
- O céu continua 100% procedural (gradiente atmosférico, nuvens FBM, disco solar e scattering), sem CubeMap nem textura de ambiente importada. O PBR usa amostragem do próprio céu procedural para o IBL aproximado.
- Renovado o shader PBR com GGX/Fresnel/Smith, luz direcional, point light com queda por distância, spotlight com cones interno/externo e iluminação ambiente baseada no céu procedural. Point/spotlight também contribuem com brilho atmosférico analítico no céu.
- Adicionado painel **Lighting**, acessível em Window > Lighting, com controles para cor, intensidade, posição, alcance e direção das luzes, além dos ângulos do cone do spotlight e iluminação ambiente.
- Resolução do shadow map direcional elevada de 2048² para 4096²; mantida filtragem PCF 5x5 e recalibrado o depth bias para preservar contato das sombras.

**Limitações importantes:** o shadow map atual é direcional; point light e spotlight iluminam materiais e influenciam o céu procedural, mas ainda não geram mapas de sombra independentes. A compilação e o teste gráfico real devem ser confirmados no Windows com MSVC/DirectX 12; o pacote foi revisado estaticamente neste ambiente.


## Version 0.4.4 — luzes como objetos, ciclo dia/noite e preview corrigido

- **Create Object** agora oferece `Directional Light`, `Point Light` e `Spot Light`. Os objetos aparecem na hierarquia e têm um componente de luz editável no Inspector, além dos controles do painel `Window > Lighting`.
- O ciclo dia/noite usa o relógio do céu procedural: 06:00 nascer do sol, 12:00 meio-dia, 18:00 pôr do sol e 00:00 noite. `Time of day` altera a hora manualmente; `Enable automatic cycle` avança o relógio pela velocidade selecionada. Cor, intensidade e ativação da Directional Light também afetam a cor do céu e o disco solar. A rotação do objeto Directional Light pode controlar a direção do sol quando o ciclo automático está desligado.
- Corrigidos artefatos no campo estelar: agora as estrelas têm posição subcélula, forma radial e antialiasing em vez de quadrados de célula. O céu continua renderizado proceduralmente em fullscreen, sem cubemap nem esfera tessellada.
- Point Lights e Spot Lights são coletados dos objetos na cena (até quatro de cada tipo), afetam o PBR e adicionam brilho atmosférico localizado no céu. A Directional Light continua sendo a fonte solar/direcional que dirige o mapa de sombras.
- O preview 3D durante o arraste ganhou um material ciano emissivo para não desaparecer em céus escuros ou claros. Quando a direção do mouse não cruza o plano do chão, o preview usa uma posição projetada à frente da câmera em vez de ficar preso na origem.
- A atribuição de textura agora usa uma zona de drop maior no slot do material, valida o caminho recebido, evita reimportar cópias do mesmo arquivo e mostra erros reais de localização/decodificação/upload no painel Console. Ao substituir uma textura, o recurso anterior só é liberado depois de drenar os comandos anteriores da GPU.
- Outline de seleção reforçado com contorno ciano mais legível, variação sutil violeta e expansão de silhueta menos discreta.

**Limitações:** a Directional Light controla as sombras com shadow map 4096² + PCF 5×5. Point/Spot Lights afetam os materiais e o céu, mas ainda não têm mapas de sombra próprios. O build e o teste visual finais devem ser executados no Windows com MSVC e DirectX 12; este pacote foi verificado estaticamente neste ambiente.


## v0.4.5 — Reliable Project Files model placement

- Added a manual mouse-drag path for model rows. It records the physical mouse-down and threshold-crossing movement and does not require Dear ImGui to accept its drag payload for the Scene viewport.
- A cyan emissive 3D preview follows the cursor over the central Scene viewport; release over Scene commits the object, while release outside/Escape cancels it.
- Added an **Add to Scene** action for the selected model asset as a reliable fallback when a particular mouse/input setup interferes with dragging.
- Kept native ImGui drag/drop enabled for texture/material slots; native Scene delivery is skipped while the manual model-drag path owns that gesture, preventing duplicate instantiation.
- Added Console messages for manual drag start, preview load, placement, cancellation and import failure.

Validation: ZIP archive integrity and static source consistency are checked when packaging. A native MSVC/DirectX 12 runtime test must still be performed on Windows.


## v0.4.7 — C# scripts via ForgeEngine.API

Esta versão inclui um sistema de scripts com API própria em C#, usando um processo host .NET 8 separado da engine DirectX 12. O projeto de scripts fica em `Project/Scripts/ForgeGameScripts.csproj`; a API fica em `Scripting/ForgeEngine.API/ForgeEngine.API.csproj`, e o host de execução fica em `Scripting/ForgeScriptHost/ForgeScriptHost.csproj`.

### Requisitos de script

- Instale o **.NET 8 SDK x64** no Windows e mantenha `dotnet.exe` no `PATH` (o editor também procura a instalação padrão em `C:\Program Files\dotnet`).
- Para editar, instale Visual Studio 2022 ou Visual Studio 2026 com a carga de trabalho de desenvolvimento .NET. Use o botão **Open in VS** no componente para abrir o projeto/código.
- Selecione um objeto da Hierarchy, abra **Object Inspector → Add Component → Script C#**, informe um nome de classe e confirme. A engine cria um `.cs`, compila a DLL e abre o arquivo no Visual Studio.
- **Ctrl+S dentro do editor Forge** compila os scripts. Salvar `.cs` no Visual Studio é detectado automaticamente; após um breve intervalo a engine recompila. Se estiver em Play, o host é reiniciado para carregar a nova DLL.
- **F5** compila e entra em Play. **Shift+F5** para a execução e restaura os transforms e atributos de luz/renderer salvos antes do Play. O ciclo de vida suporta `Awake`, `OnEnable`, `Start`, `FixedUpdate`, `Update`, `LateUpdate`, `OnDisable` e `OnDestroy`. `Debug.Log`, `Debug.LogWarning` e `Debug.LogError` aparecem no painel Console.

### Exemplo de movimento

```csharp
using ForgeEngine;

public sealed class PlayerMovement : MonoBehaviour
{
    public float speed = 5f;

    public override void Start()
    {
        Debug.Log($"{gameObject.name} pronto!");
    }

    public override void Update()
    {
        var direction = new Vector3(
            Input.GetAxisRaw("Horizontal"),
            0f,
            Input.GetAxisRaw("Vertical"));

        if (direction.sqrMagnitude > 1f)
            direction = direction.normalized;

        transform.position += direction * speed * Time.deltaTime;

        if (Input.GetKeyDown(KeyCode.Space))
            Debug.Log("Space pressionado");

        if (Input.GetMouseButtonDown(0))
            Debug.Log($"Mouse em {Input.mousePosition}");
    }
}
```

### API disponível nesta versão

- `Vector2`, `Vector3`, `Mathf`, `Time.deltaTime`, `Time.fixedDeltaTime`, `Time.time` e `Time.frameCount`.
- `transform.position`, `localPosition`, `rotation`/`eulerAngles`, `localScale`, `forward`, `right`, `up`, `Translate`, `Rotate` e `LookAt`. Rotações são expressas em **graus**.
- `GetComponent<T>()`, `GetComponents<T>()`, `gameObject`, `Component.enabled`, `MeshRenderer.enabled` e `Light` (cor, intensidade, alcance, cone e ativação).
- `Input.GetKey`, `GetKeyDown`, `GetKeyUp`, `GetMouseButton`, `GetMouseButtonDown`, `GetMouseButtonUp`, `GetAxis`/`GetAxisRaw` para `Horizontal`, `Vertical`, `Mouse X` e `Mouse Y`, além de `Input.mousePosition` e `Input.mouseDelta`.
- `Scene.Find`, `Scene.FindObjectOfType<T>()` e `Scene.FindObjectsOfType<T>()`.

O host separado ajuda a proteger o renderer caso um script lance uma exceção ou bloqueie indefinidamente; um frame de script que não responda em três segundos encerra o host e interrompe Play, com uma mensagem no Console. Isso não é um sandbox de segurança: só execute scripts em que confia. A API é uma base funcional e ainda não oferece todos os subsistemas do Unity, como física real, coroutines, serialização de campos no Inspector ou API completa de câmera/áudio.

**Validação:** o pacote foi verificado estaticamente e sua integridade ZIP é checada ao empacotar. Este ambiente não tem .NET SDK nem Windows SDK/MSVC, então a compilação C# e o teste DirectX 12 precisam ser confirmados no Windows.


## Atualizações v0.4.8

- API C# ampliada com Vector2/Vector3 helpers, Vector4, Color, funções adicionais de Mathf, Random e operações de Transform.
- `Application.isPlaying` acompanha o ciclo de frames do host C#.
- ImGuizmo recebe matrizes transpostas para conciliar o layout column-major do ImGuizmo com DirectXMath.
- Drag manual no Project Files suporta imagens além de modelos; soltar uma imagem na cena aplica-a ao objeto selecionado ou cria um plano com a textura.
- Importação de vídeos para `Project/Assets/Videos` e filtro de arquivos ampliado. A reprodução de vídeo ainda não está implementada.
- Estado de validação: alterações verificadas estaticamente; build MSVC/DirectX 12 e compilação C# não executados neste ambiente.


## v0.4.9 — editor stability, terrain, camera, and atmospheric sky

- Enabled C and C++ in CMake so Assimp's dependency graph can configure projects that contain C targets.
- Reduced the default directional shadow map from 4096² to 2048² to reduce GPU memory/bandwidth pressure.
- Moved ImGuizmo drawing to ImGui's foreground draw list so dock windows do not cover its handles.
- Added Create Object → Terrain with a 64×64 editable grid and Raise/Lower/Smooth/Flatten brush controls. Terrain buffer uploads are throttled during strokes to reduce GPU stalls.
- Added Create Object → Camera with FOV and near/far clip properties and a small camera component preview/reference panel. The current panel is a scene-reference placeholder; an independent camera render pass is not implemented yet.
- Reworked the procedural sky with atmospheric gradient, twilight, anti-aliased sun, layered clouds and soft stars; no cubemap or sky sphere is used.
- Scene format v4 stores camera/terrain component flags and camera lens values. Sculpted terrain heights are reconstructed as flat on load and are not yet serialized.

Build validation is still required on Windows with MSVC, the Windows SDK and network access for CMake FetchContent. This environment cannot run the DirectX 12 application.


## Atualizações v0.4.9

- Corrigido o desenho do ImGuizmo para usar o draw list de primeiro plano, evitando que a ordem das janelas dockáveis esconda as alças.
- CMake habilita C e C++ para compatibilidade com o grafo de dependências do Assimp.
- Mapa de sombras direcionais reduzido para 2048² como padrão mais leve de GPU/memória.
- Culling conservador por esfera no frustum da câmera evita desenhar meshes completamente fora da viewport.
- Novo Terrain 64×64 em **Create Object**, com pincéis Raise, Lower, Smooth e Flatten; upload de mesh limitado durante o arraste e normais recalculadas.
- Novo Camera em **Create Object**, com FOV e planos near/far no Inspector. A janela Camera Preview é um painel de referência do editor, não uma segunda renderização da câmera.
- Céu procedural refeito com gradiente atmosférico, crepúsculo, disco solar suavizado, nuvens em camadas e estrelas anti-alias.
- API C# ganhou `AddComponent<T>()`, `TryGetComponent<T>()`, `RemoveComponent<T>()`, `Rigidbody`, `Collider`, `SphereCollider`, `BoxCollider`, `Physics.Raycast`, `Camera` e `AudioSource` para WAV via Windows `PlaySound`. A física é básica (passo fixo e colisão simplificada com o chão); AudioSource suporta um stream WAV ativo por processo.
- Formato de cena v4 guarda flags Terrain/Camera e parâmetros de lente; alturas esculpidas do terreno ainda são restauradas como terreno plano ao reabrir.

**Validação:** arquivos e protocolo foram inspecionados estaticamente. Não foi possível executar MSVC/DirectX 12 ou o SDK .NET neste ambiente; compile e teste no Windows antes de considerar esta versão pronta para produção.

## Latest patch

See [`docs/PATCH_NOTES_v051.md`](docs/PATCH_NOTES_v051.md) for the latest fixes to the native gizmo, blue atmospheric sky, terrain ray-sculting/culling, camera FOV frustum lines, and Look Through Camera workflow.


## v0.5.5 ImGui assertion hotfix

The Project Hub (welcome screen) now finalizes its ImGui frame with `ImGui::Render()` before returning from `ForgeEngineEditorUI::Draw()`. See `docs/PATCH_NOTES_v055.md`.

## v0.5.6 C# host fix, sky, camera viewport and tooling

- Fixed the `CS0200` build error in the C# scripting host and the 23/26-field `OBJ` protocol bug.
- Fixed the Project Files panel flickering (dangling `ImGui::PushID` string seeds).
- Recreated the procedural sky: three-band blue gradient with cumulus + cirrus layers and live coverage/altitude/wind sliders.
- Selecting a Camera object now switches the Scene viewport to that camera automatically.
- Added PNG screenshots (F12), a stats overlay (F3) and hierarchy search.
- See [`docs/PATCH_NOTES_v056.md`](docs/PATCH_NOTES_v056.md).

## v0.5.8 Inputs mapping file, physics events, collider overlay and fixes

- **`Config/Inputs.json`** ships with every new project: all keyboard/mouse axes and named commands live there; `Input.GetAxis("Horizontal")`, `GetAxisRaw`, `GetButton("Jump")` resolve through the file with no code changes. Editing the JSON and recompiling rebinds everything.
- **Physics events Unity-style:** `OnCollisionEnter/Stay/Exit` and `OnTriggerEnter/Stay/Exit` dispatched by the scripting runtime, with `Collision`/`ContactPoint`, pairwise detection, penetration resolution by inverse mass and impulses.
- **Collider scales with the gizmo:** `Collider.effectiveRadius` and every physics query now use collider size × object scale.
- **Collider wireframe in the viewport:** box/sphere/capsule markers drawn for every collider (green), triggers (blue) and the selected object (orange).
- **New components:** `ConstantForce`, `AutoDestroy`, `LookAtTarget`, `AudioSource.PlayOneShot`.
- **New-project scaffold:** `Inputs.json`, `README.md` in Portuguese, `PlayerMovement.cs` (Inputs-driven, jump via button) and `CoinPickup.cs` (trigger + tag + sound) copied on project creation.
- **Assets bar flicker definitively fixed:** folder/asset/project `PushID` seeds were wide strings hashing the pointer value (unstable ID per frame); they now hash content.
- **White sky fixed at the root:** horizon/mid gradient bands and cloud lighting were too bright for ACES+gamma; bands re-inverted, cloud-lit darkened, default coverage raised so the dome reads deep azure→blue like UE5.
- **Textures/normal maps on imported models:** Assimp materials are now read on import and their diffuse/normal textures pre-fill the material slots; shader copy next to the executable refreshed by the build.
- `Application.version` reports `0.5.8`.

## v0.5.7 Export Game, Unity-style C# API, hierarchy and gizmo

- **Export Game (Ctrl+B):** standalone build with configurable name, icon and creator; editor overlay (gizmo, grid, panels) disabled, Ctrl+G toggles debug overlay.
- **C# API:** `Quaternion`, `tag`/`layer`/`activeSelf`, `GameObject.Instantiate`/`Destroy` (real native objects via the new command protocol), State-backed colliders/rigidbodies, `PlayerPrefs`, `Time.timeScale`, `Physics.OverlapSphere`, `Application.Quit`.
- Daytime sky fixed to a blue UE5-style gradient; editor grid is now a separate flag so exports render a clean sky.
- Parent/child transforms rebuild correctly (cascade updates, parent-aware gizmo dragging, Ctrl snapping).
- Project Explorer listing is cached/throttled — flicker is gone. Scene format is now v7 (tag/layer/collider/rigidbody persisted).
- See [`docs/PATCH_NOTES_v057.md`](docs/PATCH_NOTES_v057.md).
