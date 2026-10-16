# Guia do editor

## 1. Interface

| Área | Uso |
|---|---|
| Barra de menus | Arquivo, criação de objetos, assets, janelas e ajuda. |
| Scene Hierarchy | Lista e seleção dos objetos da cena. |
| Viewport central | Visualização 3D, seleção e manipulação com gizmos. |
| Object Inspector | Nome, Transform, renderer, material e componentes do objeto selecionado. |
| Project Files | Navegação e importação de assets do projeto. |
| Console | Mensagens de status, erros de build e logs dos scripts. |
| Lighting | Ajustes da iluminação e do céu procedural. |

O layout é dockable. Se a disposição ficar inadequada, use as opções de layout/reset do menu Window, quando disponíveis na build.

## 2. Navegação 3D

- Segure o botão direito do mouse sobre o viewport para olhar ao redor.
- Com o botão direito pressionado, use **W/A/S/D** para mover a câmera.
- Use **Q/E** para descer/subir.
- Segure **Shift** para acelerar.
- Solte o botão direito para recuperar o cursor.
- Clique com o botão esquerdo em um objeto para selecioná-lo.
- Os atalhos **1**, **2** e **3** selecionam, respectivamente, mover, rotacionar e escalar quando o viewport/editor está recebendo o teclado.

## 3. Objetos e componentes

Use **Create Object** para criar objetos disponíveis na build, como formas básicas e luzes. Selecione um objeto na Hierarchy para editar suas propriedades no Inspector.

O botão **+ Add Component** permite adicionar um componente de script C#. Componentes Collider e Audio Source podem aparecer como marcadores de interface, mas não representam sistemas completos de física ou áudio nesta versão.

## 4. Assets

O navegador de projeto usa `Project/Assets/` e organiza arquivos em:

- `Models/` — modelos 3D suportados pelo importador disponível na build.
- `Textures/` — imagens/texturas.
- `Materials/` — reservado para materiais.
- `Scenes/` — cenas salvas.
- `Shaders/` — assets de shader.
- `Audio/` — reservado para assets de áudio.
- `Misc/` — outros arquivos.

Importar um asset copia o arquivo para a pasta de categoria correspondente. O fluxo de arrastar modelos para a cena depende do viewport e do formato; se não funcionar, use a ação alternativa de adicionar à cena disponível na build e consulte o Console. Texturas embutidas em modelos não são extraídas automaticamente.

### Texturas e materiais

O slot Albedo/Base Color participa atualmente da renderização PBR. Os slots Normal, Metallic/Roughness e AO podem guardar referências, mas não estão todos conectados ao shader de renderização nesta versão. Use caminhos dentro do projeto para facilitar a portabilidade.

## 5. Cenas

- **Ctrl+N:** criar cena nova.
- **Ctrl+O:** abrir cena.
- **Ctrl+Shift+S:** salvar cena como.
- **File → Save Scene:** salvar a cena atual.
- **Delete:** remover o objeto selecionado.
- **Ctrl+D:** duplicar o objeto selecionado.

As cenas usam a extensão `.forgeScene`. O arquivo guarda transforms, dados básicos de material/renderer, referências relativas de textura e metadados dos componentes de script. Salve a cena depois de adicionar/remover componentes para persistir as alterações.

> **Atenção:** nesta versão, `Ctrl+S` está reservado para **compilar scripts C#**; ele não é o atalho para salvar a cena. Use o menu File para salvar.

## 6. Iluminação e céu

Abra **Window → Lighting** para ajustar os parâmetros de iluminação expostos pelo editor. A engine possui céu procedural sem cubemap, iluminação PBR e uma luz direcional, uma point light e um spotlight no pipeline atual. O mapa de sombras direcional usa resolução 4096×4096 e filtro PCF 5×5. Point light e spotlight ainda não geram mapas de sombras próprios.

O céu procedural responde à iluminação/direção solar disponível na implementação, mas não deve ser interpretado como um sistema meteorológico ou astronômico completo.

## 7. Play Mode

- **F5** inicia o Play Mode e tenta compilar os scripts antes de iniciar.
- **Shift+F5** encerra o Play Mode.
- O editor guarda um snapshot dos transforms e restaura-os ao parar.
- Se a compilação falhar, o Play Mode é cancelado e os erros aparecem no Console.
- Ao salvar um `.cs`, o editor detecta a alteração, espera um curto intervalo e tenta recompilar; se já estiver em Play, tenta reiniciar a sessão dos scripts.

O Play Mode executa os scripts C# e sincroniza os campos de Transform, renderer e luz suportados com a cena nativa. Não é ainda um sistema completo de runtime/exportação de jogo.


## Correções e notas da revisão v0.5.0

- O gizmo é enviado explicitamente para a foreground draw list e é habilitado antes de manipular a transformação.
- A interação de seleção/escultura na área central não depende exclusivamente de `WantCaptureMouse`, que pode ser ativado pelo dock host em tela cheia. Os handles do ImGuizmo continuam tendo prioridade.
- O disco solar e o halo foram reduzidos para diminuir a aparência de branco estourado no target SDR.
- A lista completa de componentes e APIs está em [API_REFERENCE_COMPLETE.md](API_REFERENCE_COMPLETE.md).


## Versão 0.5.3 — fluxo de projetos

### Project Hub

Ao iniciar, a engine apresenta um splash de carregamento dos recursos nativos e abre o Project Hub. Use **New Project** para criar um projeto ou **Open Project...** para selecionar `DX12Forge.project`. A lista de projetos recentes usa cartões com uma ilustração genérica e a data de criação. Ao abrir um projeto, outra tela mostra as etapas de preparação de pastas e carregamento da cena.

### Pastas e assets

No painel **Project Files**, selecione a pasta de destino na árvore à esquerda. **+ Folder** cria uma pasta ou subpasta no diretório ativo. Use **Rename** na seleção ou o menu de contexto da árvore. O sistema impede nomes vazios, `.` e `..`.

### Hierarquia pai/filho

Arraste um objeto da árvore e solte em outro objeto para definir uma relação pai/filho. O menu de contexto também oferece *Parent selected object here* e *Unparent*. A árvore impede autorreferências e ciclos e persiste as relações no `.forgeScene`. Neste estágio, a relação é organizacional: mover o pai ainda não transforma automaticamente os filhos.

### Pós-processamento

Abra **Create Object → Post Processing Settings...** ou **Window → Post Processing**. Há controles para Bloom, AO, Motion Blur, Vignette, Chromatic Aberration, Sharpen, Saturation e Exposure. *Cinematic preset* e *Neutral / Reset* aplicam configurações prontas. Os valores do projeto são salvos automaticamente em `Presets/ActivePostProcessing.forgePreset` e recarregados ao abrir o projeto.

### Barra da janela

A faixa superior da janela é desenhada pelo editor e contém minimizar, maximizar/restaurar e fechar. Arraste uma região vazia da faixa superior para mover a janela. A borda nativa continua ativa para redimensionamento.

### Compilação dos scripts C#

Use **Run → Compile C# Scripts** ou `Ctrl+S`. A engine procura `dotnet.exe` no `PATH`, nas variáveis `DOTNET_ROOT*` e em diretórios de instalação comuns. Antes da compilação, a referência a `ForgeEngine.API.csproj` é ajustada para o caminho real da engine, evitando a referência relativa errada para projetos criados fora do diretório da engine. Os logs estão em `Scripts/build.log` e `Scripts/host-build.log`.


## Seleção, câmera e materiais (v0.5.4)

- Clique no objeto visível dentro do retângulo central da Scene para selecionar pela geometria triangular real; cliques nos manipuladores do ImGuizmo não mudam a seleção.
- Objetos selecionados recebem um contorno ciano/violeta. Se um objeto estiver sendo editado, o culling conservador não o remove da viewport.
- A resolução de sombras direcionais foi aumentada para 4096 × 4096 com PCF 5×5. Se a GPU ficar sem memória ou a taxa de quadros cair muito, reduza `ShadowMapSize` para 2048 e sincronize `texel` em `assets/shaders/Shaders.hlsl`.
- Em uma câmera selecionada, use **Look Through Camera** para mudar a renderização principal da Scene para a transformação/FOV reais dessa câmera. Segure RMB sobre o centro da Scene e use WASD/QE para navegar; sair de Camera View devolve o controle à câmera livre do editor. A janela Camera Preview agora mostra uma cópia pós-processada real do Scene. Para que essa imagem seja da câmera selecionada, ative Look Through Camera; a janela de preview não faz uma segunda renderização independente.
- Em **Inspector → Material / PBR**, configure UV Tiling e Offset e arraste/importa mapas: Albedo, Normal, Metallic/Roughness e Ambient Occlusion. O mapa MR usa o padrão glTF (`G=roughness`, `B=metallic`); AO usa canal R. As texturas usam amostragem anisotrópica com repetição.
