# Forge Engine v0.5.5 — ImGui frame assertion hotfix

## Fix

The Project Hub path in `ForgeEngineEditorUI::Draw()` returned before calling `ImGui::Render()`. Since `ForgeEngine::Render()` calls `BeginFrame()` every frame, the next `ImGui::NewFrame()` detected that the previous frame had not ended and triggered `IM_ASSERT(g.FrameCountEnded == g.FrameCount)` with the message `Forgot to call Render() or EndFrame() at the end of the previous frame?`.

The Project Hub path now calls `ImGui::Render()` before returning. It still passes the draw data to the existing DirectX 12 renderer, which renders the full-screen hub normally. Assertions have not been disabled.

## If an assertion remains

The code snippet containing multiple `IM_ASSERT` expressions is not enough to diagnose a different assertion. In Visual Studio, copy the exact highlighted expression and the file/line. In particular, an assertion involving `MouseSingleClickDelay`, `DeltaTime`, `DisplaySize`, or `TreeLinesFlags` would be a different failure from the missing `Render()` call described above.

## Validation

Source-level check confirms the `m_showWelcome` branch calls `ImGui::Render()` before returning, and its braces are balanced. A Windows/MSVC/DirectX 12 build could not be run in this environment.
