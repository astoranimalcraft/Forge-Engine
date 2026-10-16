# Limitações e estado atual

Este documento separa recursos presentes de funcionalidades que ainda não devem ser consideradas completas.

## Rendering e assets

- O pipeline tem céu procedural, PBR, bloom/ACES/vinheta e luz direcional/point/spot.
- Apenas a luz direcional produz shadow map nesta versão; point e spot não têm sombras próprias.
- Albedo, Normal Map, Metallic/Roughness e AO participam da renderização PBR (tangentes derivadas em screen space; tampas de cilindre/cone sem UV não recebem normal map). Texturas embutidas em materiais Assimp são pré-carregadas na importação do modelo.
- O importador pode carregar formatos suportados pela build do Assimp (por exemplo OBJ, FBX, glTF/GLB, DAE, STL, PLY e 3DS), mas não extrai/aplica automaticamente todos os materiais e texturas embutidos.
- O fluxo de drag-and-drop de assets pode variar conforme a região do editor, o formato e a interação da UI. Use as ações alternativas da interface e o Console para diagnosticar.

## API e scripting

- A API C# sincroniza Transform, nome, estado do MeshRenderer e propriedades básicas das luzes.
- Não existe hierarquia de Transform pai/filho nem sistema completo de entidades/componentes como Unity.
- `Transform.localPosition` atualmente é alias de `position`; `lossyScale` é alias de `localScale`.
- `Transform.Rotate` soma ângulos Euler; não é uma implementação completa de rotação por quaternion.
- `Input.GetAxis("Mouse ScrollWheel")` retorna zero.
- A API não serializa campos públicos de scripts para o Inspector; parâmetros devem ser editados no código.
- O protocolo não sincroniza todos os componentes nativos ou todos os recursos do renderer.
- O processo host separado não é sandbox de segurança.
- Audio Source e Collider não implementam áudio ou física completos nesta versão.
- Não há ainda sistema completo de exportação de jogo/player, hot reload sem reiniciar o host, animação, networking ou gerenciamento avançado de recursos.

## Validação

O pacote v0.4.7 foi revisado estaticamente e o ZIP foi verificado quanto à integridade. Não foi possível executar compilação MSVC/DirectX 12 nem validar visualmente o runtime no Windows neste ambiente. A build no Windows é necessária para confirmar os resultados.


## v0.4.9 — limites importantes

- A janela Camera Preview é um painel de referência visual e parâmetros; ainda não executa um segundo render pass da câmera selecionada.
- O terreno pode ser esculpido em tempo real, mas o arquivo de cena salva apenas a presença do componente; as alturas personalizadas são reconstruídas planas ao carregar.
- A física C# é simplificada (gravidade, rigidbody, raycast de esfera e plano de chão); não há solver completo de colisões, joints, callbacks de trigger ou broadphase.
- AudioSource só reproduz WAV pelo `winmm.PlaySound`, com um stream global; não há spatial audio/mixer.
- Build completo ainda requer Windows, MSVC, Windows SDK, Git/rede para FetchContent e .NET 8 SDK. Este ambiente não executou o build.


## v0.5.3 — observações adicionais

- Post-processing: SSAO é um cálculo de vizinhança no depth buffer; Motion Blur usa vetor de deslocamento da câmera e não tem buffer temporal por objeto. Bloom usa poucos taps em um único passe, não uma pirâmide multi-resolução.
- Sky: nuvens e atmosfera são procedurais e não usam scattering físico volumétrico real. Aparência final depende da exposição e do conteúdo HDR da cena.
- Project Hub: a miniatura nos cartões é uma ilustração gerada pela UI, não uma captura de tela real do projeto.
- Hierarchy: relações pai/filho e validação contra ciclos são salvas; herança de posição/rotação/escala e exclusão recursiva de filhos não fazem parte desta versão.
- A tela de abertura compila/inicializa os recursos nativos e a tela de projeto carrega a cena e caminhos de assets. Os scripts C# são compilados ao executar **Compile C# Scripts** ou entrar em Play Mode, não automaticamente ao abrir um projeto.
- A barra de título customizada usa controles desenhados em ImGui e uma moldura nativa de redimensionamento; em algumas configurações de DPI, o hit-test das bordas pode precisar de ajuste.
