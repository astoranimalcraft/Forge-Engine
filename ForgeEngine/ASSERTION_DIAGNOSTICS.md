# DX12 Forge Engine — assertion diagnostics

Version 0.3.2 adds a scope guard for every editor child window. Each child now closes exactly once at its original logical boundary, with destructor cleanup as a safety net for future early returns. The dock layout, ImGuizmo matrix validation, and SRV descriptor pool safeguards are retained.

For a clean rebuild in Visual Studio 2026 Developer PowerShell:

```powershell
Remove-Item -Recurse -Force .\out\build\windows-ninja -ErrorAction SilentlyContinue
cmake --preset windows-ninja
cmake --build --preset debug
```

If Visual Studio still breaks into the debugger, do not disable assertions globally. In the Assertion Failed dialog, copy the **Expression**, **File**, and **Line**, or look at **Debug > Windows > Call Stack** and copy the first frame belonging to `DX12ForgeEngine`, `imgui.cpp`, `imgui_impl_dx12.cpp`, or `ImGuizmo.cpp`. The generic `__debugbreak()` message alone is not sufficient to prove which assertion was triggered.


## Begin/End balance — version 0.3.4

`DrawHierarchy`, `DrawInspector`, `DrawProjectBrowser`, and `DrawConsole` now keep the matching `ImGui::End()` at the bottom of the function. A false return from `ImGui::Begin()` skips only the body, never the matching `End()`. Child sections are similarly gated by `ImGuiChildScope::Visible()` and closed once through `EndNow()`. The dock host is closed before independent docked windows are submitted; that placement is intentional.


## v0.4.9 build and editor changes

CMake now enables both C and CXX because Assimp may configure C-language dependency targets. For a clean diagnostic build, remove the previous build directory and run `cmake --preset windows-ninja` followed by `cmake --build --preset debug --verbose`. ImGuizmo is submitted to `ImGui::GetForegroundDrawList()` so the dock host's draw order does not hide the handles. Shadow map default is 2048² to reduce memory and bandwidth use.
