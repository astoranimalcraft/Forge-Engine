# Instalação, compilação e solução de problemas

## 1. Requisitos

- Windows 10/11 x64.
- Visual Studio 2022 ou 2026 compatível, com a carga de trabalho **Desenvolvimento para desktop com C++** e MSVC.
- Windows SDK compatível com DirectX 12.
- CMake 3.21+.
- Ninja Multi-Config.
- Git no `PATH`.
- .NET 8 SDK (`dotnet --list-sdks` deve listar uma versão `8.0.x`).
- Internet na primeira configuração CMake para obter Dear ImGui e ImGuizmo.

A detecção automática do Visual Studio procura as versões 18.0, 17.0, 16.0 e 15.0 no Registro do Windows. Se não encontrar, o script pode abrir pelo aplicativo associado a `.cs` ou pedir abertura manual.

## 2. Compilar a engine nativa

Abra **Developer PowerShell** do Visual Studio e navegue para a pasta `ForgeEngine` que contém `CMakeLists.txt`:

```powershell
cmake --version
cmake --preset windows-ninja
cmake --build --preset debug
```

Build Release:

```powershell
cmake --build --preset release
```

Executável Debug esperado:

```text
out/build/windows-ninja/Debug/ForgeEngine.exe
```

Se precisar reconfigurar do zero:

```powershell
Remove-Item -Recurse -Force .\out\build\windows-ninja -ErrorAction SilentlyContinue
cmake --preset windows-ninja
cmake --build --preset debug
```

A primeira configuração requer Git e acesso à internet. Se a rede estiver bloqueada, o FetchContent não conseguirá baixar as dependências.

## 3. Compilar scripts manualmente

No terminal, na raiz do projeto:

```powershell
dotnet --info
dotnet build .\Project\Scripts\ForgeGameScripts.csproj --configuration Debug
dotnet build .\Scripting\ForgeScriptHost\ForgeScriptHost.csproj --configuration Debug
```

Depois abra `out/build/windows-ninja/Debug/ForgeEngine.exe`. O editor também compila os dois projetos ao usar **Ctrl+S** ou iniciar Play Mode. Antes do build, o runtime ajusta o `ProjectReference` de `ForgeEngine.API.csproj` ao caminho real da cópia da engine. Se ainda houver erro, consulte `Scripts/build.log` e `Scripts/host-build.log`; o executável precisa do .NET 8 SDK instalado para compilar e do runtime .NET 8 para executar o host.

## 4. Problemas comuns

### “.NET SDK not found”

Instale o .NET 8 SDK x64, confirme `dotnet --list-sdks` e reinicie a engine/terminal. Instalar apenas o runtime não basta para compilar.

### Erros de compilação C#

Leia as mensagens `[C# build]` no Console e abra `Project/Scripts/build.log`. Confira `using ForgeEngine;`, a herança `: MonoBehaviour`, os `override` e o nome de classe. A classe precisa ser concreta e ter construtor público sem parâmetros.

### “Script class was not found”

Verifique se o nome da classe no componente corresponde ao nome da classe compilada e se o build terminou com sucesso. Pressione Ctrl+S na engine e confirme que a DLL foi gerada.

### O Visual Studio não abre

O editor tenta localizar `devenv.exe` no Registro do Windows. Se falhar, abra manualmente `Project/Scripts/ForgeGameScripts.csproj` ou o `.cs` criado. Confirme a associação de arquivos `.cs` no Windows.

### Play Mode é cancelado

A engine compila antes de iniciar Play. Corrija erros no Console e tente F5 novamente. Se o host sair inesperadamente, confira os erros de inicialização do host e `Project/Scripts/host-build.log`.

### Script compila, mas não move o objeto

- Confirme que o componente está habilitado no Inspector.
- Confirme que o objeto correto tem o script associado.
- Confirme que a janela está em foco e o mouse está dentro do viewport para input de mouse.
- Confira se `Update()` foi declarado como `public override void Update()`.
- Use `Debug.Log()` em `Start()` e `Update()` para confirmar que os callbacks executam.
- Lembre-se de que `Ctrl+S` na engine compila scripts; para salvar a cena use **File → Save Scene**.

### Texture/model não carrega

Confira o Console, extensão do arquivo e caminho dentro de `Project/Assets`. Modelos com materiais/texturas embutidos não têm todos os recursos extraídos automaticamente. O importador e a renderização variam com o formato e os recursos suportados.

### Erro de CMake / dependências

Confirme CMake 3.21+, Git no PATH e conectividade com GitHub. Apague `out/build/windows-ninja` e execute o preset novamente. Leia o primeiro erro real do CMake, não apenas as mensagens finais de falha.

### Assert ou crash em Debug

Copie a expressão, arquivo e linha do diálogo de assert do Visual Studio, além do Call Stack. O texto genérico “breakpoint” não identifica a origem.

## 5. Diagnóstico a enviar para suporte

Inclua:

1. Versão do Windows e do Visual Studio.
2. Saída de `cmake --version`, `ninja --version` e `dotnet --list-sdks`.
3. Comando executado e primeiro erro completo.
4. Mensagens do Console da engine.
5. `Project/Scripts/build.log` ou `host-build.log`, se o erro for C#.
6. Se o erro acontece ao compilar, iniciar, importar asset ou entrar em Play.

## 6. Projeto Hub e cenas

- Um projeto novo usa `ForgeEngine.project` como marcador. Selecione esse arquivo em **Open Project...**.
- A cena padrão está em `Assets/Scenes/Default.forgeScene`; versões 1–4 continuam aceitas e a versão 5 guarda relação pai/filho, caminhos de modelos e alturas do terreno.
- O editor procura fontes da engine subindo as pastas a partir do diretório do executável. Mantenha o executável dentro de uma árvore que contenha `CMakeLists.txt` e `src/Engine/Engine.h`, ou ajuste a descoberta de `sourceRoot` para um layout de distribuição diferente.
- Se o céu parecer superexposto, abra `Window → Post Processing` e reduza `Exposure`; o céu é HDR antes do tone mapping.


## v0.5.4 notes

- If Visual Studio reports `SelectObject` overload/argument errors, use the engine API `SelectSceneObject(index)` for scene selection; Win32 GDI `::SelectObject(dc, font)` is only for GDI handles.
- An error mentioning an identifier named `template` can be a parser cascade because `template` is a reserved C++ keyword; the project asset path variable is `templateProject`.
- Calls to `std::filesystem::exists(path, ec)` require a local `std::error_code ec`; `LoadScene` now declares it.
- Shadow resolution is set to 4096 × 4096; reduce `ShadowMapSize` to 2048 if VRAM/bandwidth is a constraint, and keep `CalculateShadow`'s texel size in HLSL in sync.
