# Arquitetura interna

## 1. Visão geral

A engine combina um editor Win32/C++17 com renderer DirectX 12 e um runtime de scripts C# hospedado em outro processo .NET 8.

```text
ForgeEngine.exe (C++ / Win32 / DirectX 12)
  ├─ Engine (device, swap chain, command queues, render targets, camera, scene objects)
  ├─ EditorUI (Dear ImGui docking, Inspector, Hierarchy, Project Files, Console, ImGuizmo)
  └─ ScriptRuntime (process management, build, pipes, frame synchronization)
        │ stdin/stdout line protocol
        ▼
ForgeScriptHost.exe/dll (.NET 8)
  ├─ carrega ForgeGameScripts.dll
  ├─ mantém instâncias MonoBehaviour
  ├─ atualiza Scene / Input / Time
  └─ devolve transforms, renderer/lights e logs
        │ referência de projeto
        ▼
ForgeEngine.API.dll
  ├─ Vector2 / Vector3 / Mathf
  ├─ Component / GameObject / Transform / Scene
  ├─ MonoBehaviour / MeshRenderer / Light
  ├─ Input / Time / Debug
  └─ enums Space / LightType / KeyCode
```

## 2. Renderer e engine nativa

`src/Engine/Engine.h` e `Engine.cpp` gerenciam janela Win32, dispositivo Direct3D 12, swap chain, sincronização de frames, recursos de renderização, câmera, objetos e renderização. `assets/shaders/Shaders.hlsl` contém shaders do pipeline gráfico.

O renderizador atual usa target HDR, tone mapping ACES, bloom de baixo custo, vinheta, BRDF Cook–Torrance/GGX, Fresnel Schlick, geometria Smith, céu procedural e shadow map direcional 4096² com PCF 5×5. A configuração de luz inclui direcional, point e spot, com sombras próprias apenas para a luz direcional.

## 3. Editor

`src/Editor/EditorUI.h` e `EditorUI.cpp` implementam a UI Dear ImGui e o bridge para ImGuizmo. O editor manipula os objetos da engine diretamente (o editor é friend da classe da engine), por isso ainda não existe uma API pública nativa de entidades desacoplada.

Responsabilidades principais:

- Hierarchy e seleção.
- Inspector e componentes.
- Navegador/importação de assets.
- Salvar/abrir `.forgeScene`.
- Play/Stop, build automático e console de scripts.
- Comunicação com `ForgeScriptRuntime`.

## 4. Sistema de scripting

### Projetos .NET

- `Scripting/ForgeEngine.API/ForgeEngine.API.csproj`: API gerenciada compartilhada.
- `Scripting/ForgeScriptHost/ForgeScriptHost.csproj`: processo de execução.
- `Project/Scripts/ForgeGameScripts.csproj`: assembly de gameplay.

Todos têm como alvo `net8.0` e usam C# moderno habilitado pelo SDK instalado.

### Build

`ForgeScriptRuntime::Build` localiza `dotnet.exe`, compila o projeto de scripts e depois o host. Logs são redirecionados para arquivos sob `Project/Scripts/` e para o Console do editor.

### Ciclo de frame

1. O editor recolhe transforms, estado de renderer/luzes, scripts habilitados e input.
2. O runtime nativo envia uma mensagem de frame ao host via stdin.
3. O host sincroniza a cena gerenciada e associa instâncias por `(ObjectId, ClassName)`.
4. O host executa callbacks de ciclo de vida e atualiza `Time`/`Input`.
5. O host envia estados alterados e logs via stdout.
6. O runtime nativo aplica os dados devolvidos aos objetos e reconstrói as matrizes de mundo.

O protocolo é textual por linhas; strings que podem conter caracteres especiais são codificadas em Base64. O processo é iniciado sem janela de console visível.

### Isolamento e limites

O processo separado reduz o risco de uma exceção gerenciada derrubar diretamente o processo nativo, mas não é sandbox e não impede scripts de usar arquivos, rede, processos ou consumir CPU. A troca de estado ocorre por frame e só cobre campos explicitamente serializados pelo protocolo.

## 5. Cenas e assets

O projeto guarda assets em `Project/Assets`. Arquivos `.forgeScene` persistem dados da cena, incluindo transforms, valores básicos de material/renderer, caminhos relativos de textura e componentes de script. Não é um formato de serialização genérico de todos os recursos.

## 6. Dependências externas

O CMake obtém Dear ImGui da branch `docking` e ImGuizmo da branch `master` por FetchContent. Essas dependências são baixadas para o diretório de build e não precisam ser copiadas manualmente para `src`.


## Rendering and editor changes v0.4.9

The scene pass now uses conservative bounding-sphere frustum culling based on the active editor camera projection. Directional shadow-map resolution defaults to 2048² to reduce memory/bandwidth. Terrain sculpting stores a CPU height grid and throttles GPU mesh uploads while the mouse is held. The camera preview UI is a lens/frustum schematic, not a separate scene render pass.
