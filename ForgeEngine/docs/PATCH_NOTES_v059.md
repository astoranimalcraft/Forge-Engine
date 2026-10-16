# Patch Notes v0.5.9 — Céu azul UE5, UI moderna, export Unity-like, corrotinas

## Céu procedural corrigido
- Gradiente azul forte estilo UE5 (zenith 0.72 blue, horizon 0.88 cyan) — o céu agora é azul de verdade, não mais cinza/branco.
- Nuvens com alpha reduzido (max 0.42) e coverage padrão 0.30 para o azul dominar.
- Drift das nuvens 3-4x mais rápido (0.038/0.022 cumulus, 0.045/0.028 cirrus) — movimento visível.
- Cores de nuvens em faixa HDR moderada para não esmagar o azul após tonemap.

## Interface moderna
- Fonte Segoe UI 18px + Segoe MDL2 Assets como fallback para ícones inline.
- Ícones MDL2 em todos os menus (File, Create Object, Assets, Run, Window, Help), botões da toolbar, títulos de painéis e chrome buttons (min/max/close).
- Estilo polish: bordas zero, arredondamento maior (9/7/9), padding mais generoso (12,10 / 10,6 / 10,7).
- Botões da window chrome agora usam ícones MDL2 em vez de texto ASCII.

## Sistema de pasta do projeto + recentes persistentes
- Modal "Create Project" tem botão Browse com IFileOpenDialog FOS_PICKFOLDERS moderno.
- Projetos recentes persistidos em `%APPDATA%\DX12Forge\RecentProjects.ini` — sobrevivem entre sessões.
- Lista carregada na inicialização, salva ao abrir/criar projeto, deduplicada, cap 12.

## Exportação estilo Unity
- Build Settings no modal: resolução (640x480 a 2560x1440), fullscreen toggle.
- forge_game.ini inclui width/height/fullscreen para o player honrar.
- Opção "Build and Run" — exporta e lança o jogo automaticamente.

## API C# expandida
- **Corrotinas:** `StartCoroutine`, `StopCoroutine`, `StopAllCoroutines`, `WaitForSeconds`, `WaitForEndOfFrame`.
- **Quaternion:** `Slerp`, `Inverse`, `RotateTowards`, `Angle`.
- **Vector3:** `SmoothDamp`, `RotateTowards`.
- **Screen:** stub com width/height/dpi/fullScreen.
- `Application.version` atualizado para "0.5.9".

## Texturas corrigidas
- Engine agora usa `m_textureSearchRoot` (definido pelo editor como project root) para resolver caminhos relativos de texturas.
- Resolução de texturas busca primeiro no project root, depois working dir e exe dir.
