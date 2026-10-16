#include "Engine.h"
#include <exception>

namespace
{
    DX12ForgeEngine* g_engine = nullptr;
}

LRESULT CALLBACK DX12ForgeEngine::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_engine)
    {
        LRESULT result = 0;
        if (g_engine->HandleMessage(hwnd, message, wParam, lParam, result))
            return result;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    DX12ForgeEngine engine;
    g_engine = &engine;

    try
    {
        if (!engine.Initialize(instance, showCommand))
            return 1;
        return engine.Run();
    }
    catch (const std::exception& exception)
    {
        MessageBoxA(nullptr, exception.what(), "DX12 Forge Engine - Erro", MB_OK | MB_ICONERROR);
        return 1;
    }
}
