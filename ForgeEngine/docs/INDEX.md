# Documentação — DX12 Forge Engine

Bem-vindo à documentação da DX12 Forge Engine. O projeto é uma engine/editor 3D para Windows, com renderer C++17/DirectX 12 e scripting de gameplay em C#.

## Comece por aqui

1. [Instalação e compilação](BUILD_AND_TROUBLESHOOTING.md)
2. [Guia do editor](USER_GUIDE.md)
3. [Scripts C# e API](SCRIPTING_API.md)
4. [Referência completa da API](API_REFERENCE_COMPLETE.md)
5. [Arquitetura interna](ARCHITECTURE.md)
6. [Limitações e estado atual](LIMITATIONS.md)
7. [Notas da versão v0.5.3](PATCH_NOTES_v053.md)

## Visão geral

- **Runtime nativo:** C++17, Win32 e DirectX 12.
- **Editor:** Dear ImGui (docking) e ImGuizmo.
- **Gameplay:** C# em .NET 8, com `ForgeEngine.API` e um processo host separado.
- **Build nativo:** CMake + Ninja + MSVC/Windows SDK.
- **Formato de cena:** `.forgeScene`.

> **Estado de validação:** esta documentação descreve o código que acompanha o pacote v0.5.3. O pacote foi inspecionado estaticamente, mas não foi possível executar uma compilação completa de MSVC/DirectX 12 nesta sessão. Confirme o build e os fluxos do editor no Windows antes de usar em produção.


## v0.5.0

Inclui pincéis de terreno, objeto Camera, ajustes de exposição do céu procedural, gizmo no draw list foreground, otimização de sombras e APIs C# de física/áudio básica. Consulte `SCRIPTING_API.md` e `LIMITATIONS.md` para os limites atuais.

## v0.5.3

Projeto Hub com fluxo de carregamento, caption da janela personalizada, organização de pastas, hierarquia em árvore e controles de pós-processamento. Veja [PATCH_NOTES_v053.md](PATCH_NOTES_v053.md) para detalhes e limitações.


## Última revisão
- [Patch notes v0.5.4 — sombras, picking, culling, câmera e PBR](PATCH_NOTES_v054.md)

- [Patch notes v0.5.6 — compilação C#, painel Project Files, céu azul procedural, câmera na viewport, screenshot/stats/busca](PATCH_NOTES_v056.md)

- [Patch notes v0.5.7 — Export Game standalone, API C# Unity-like (Quaternion, tag/layer, Instantiate/Destroy, PlayerPrefs), céu azul UE5, hierarquia pai/filho, snapping do gizmo e fim do flicker do Project Explorer](PATCH_NOTES_v057.md)

- [Patch notes v0.5.8 — Config/Inputs.json (teclas/eixos/comandos), scaffold de projeto com README e exemplos, eventos OnCollision/OnTrigger, colider escala+junto do gizmo, wireframe de colisão no viewport, ConstantForce/AutoDestroy/LookAtTarget, céu azul definitivo, fim do flicker da barra Assets, texturas/normal map de modelos](PATCH_NOTES_v058.md)

- [Patch notes v0.5.9 — Céu azul UE5 corrigido, UI moderna com ícones/fonte Segoe, pasta do projeto + recentes persistentes, export Unity-like (resolução/fullscreen/build-and-run), corrotinas C#, Quaternion.Slerp/Inverse, Vector3.SmoothDamp](PATCH_NOTES_v059.md)
