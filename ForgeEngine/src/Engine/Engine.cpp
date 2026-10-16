#include "Engine.h"
#include "EditorUI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <wincodec.h>
#include <objbase.h>
#include <dwmapi.h>
#include <fstream>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
    {
        D3D12_HEAP_PROPERTIES properties{};
        properties.Type = type;
        properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        properties.CreationNodeMask = 1;
        properties.VisibleNodeMask = 1;
        return properties;
    }

    D3D12_RESOURCE_DESC BufferDescription(UINT64 size)
    {
        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Alignment = 0;
        description.Width = size;
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;
        description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        description.Flags = D3D12_RESOURCE_FLAG_NONE;
        return description;
    }

    D3D12_RASTERIZER_DESC DefaultRasterizer(D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK)
    {
        D3D12_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = cullMode;
        rasterizer.FrontCounterClockwise = FALSE;
        rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer.DepthClipEnable = TRUE;
        rasterizer.MultisampleEnable = FALSE;
        rasterizer.AntialiasedLineEnable = FALSE;
        rasterizer.ForcedSampleCount = 0;
        rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        return rasterizer;
    }

    D3D12_BLEND_DESC DefaultBlend()
    {
        D3D12_BLEND_DESC blend{};
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;
        auto& target = blend.RenderTarget[0];
        target.BlendEnable = FALSE;
        target.LogicOpEnable = FALSE;
        target.SrcBlend = D3D12_BLEND_ONE;
        target.DestBlend = D3D12_BLEND_ZERO;
        target.BlendOp = D3D12_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlendAlpha = D3D12_BLEND_ZERO;
        target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        target.LogicOp = D3D12_LOGIC_OP_NOOP;
        target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        for (UINT i = 1; i < 8; ++i)
            blend.RenderTarget[i] = target;
        return blend;
    }

    D3D12_DEPTH_STENCIL_DESC DefaultDepthStencil(D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_DEPTH_WRITE_MASK writeMask = D3D12_DEPTH_WRITE_MASK_ALL)
    {
        D3D12_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = writeMask;
        depth.DepthFunc = comparison;
        depth.StencilEnable = FALSE;
        depth.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depth.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        depth.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        depth.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        depth.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        depth.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        depth.BackFace = depth.FrontFace;
        return depth;
    }

    XMFLOAT3 Normalize3(const XMFLOAT3& value)
    {
        XMVECTOR normalized = XMVector3Normalize(XMLoadFloat3(&value));
        XMFLOAT3 result{};
        XMStoreFloat3(&result, normalized);
        return result;
    }

    DXGI_FORMAT SwapChainFormat()
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    constexpr int IdObjectList = 100;
    constexpr int IdMoveMode = 101;
    constexpr int IdRotateMode = 102;
    constexpr int IdScaleMode = 103;
    constexpr int IdApplyTransform = 104;
    constexpr int IdResetTransform = 105;
    constexpr int IdApplyMaterial = 106;
    constexpr int IdMetallicEdit = 120;
    constexpr int IdRoughnessEdit = 121;
}

ForgeEngine::ForgeEngine() = default;

ForgeEngine::~ForgeEngine()
{
    SetCursorHidden(false);
    DestroyStartupSplash();
    if (m_commandQueue && m_fence && m_fenceEvent)
    {
        try { WaitForGpu(); }
        catch (...) {}
    }
    if (m_editorUI)
    {
        m_editorUI->Shutdown();
        m_editorUI.reset();
    }
    if (m_editorHwnd && IsWindow(m_editorHwnd))
        DestroyWindow(m_editorHwnd);
    if (m_editorFont) DeleteObject(m_editorFont);
    if (m_editorBackgroundBrush) DeleteObject(m_editorBackgroundBrush);
    if (m_editorEditBrush) DeleteObject(m_editorEditBrush);
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    if (m_hwnd && IsWindow(m_hwnd))
        DestroyWindow(m_hwnd);
    if (m_comInitialized) CoUninitialize();
}

void ForgeEngine::ThrowIfFailed(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
    {
        std::ostringstream message;
        message << operation << " falhou. HRESULT=0x" << std::hex << std::uppercase
                << static_cast<unsigned long>(hr);
        throw std::runtime_error(message.str());
    }
}

D3D12_RESOURCE_BARRIER ForgeEngine::TransitionBarrier(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

UINT ForgeEngine::AlignConstantBufferSize(UINT size)
{
    return (size + ConstantBufferAlignment - 1u) & ~(ConstantBufferAlignment - 1u);
}

namespace
{
    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty()) return std::wstring();
        const int needed = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (needed <= 0) return std::wstring(text.begin(), text.end());
        std::wstring wide(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), needed);
        return wide;
    }
}

void ForgeEngine::LoadGameConfig()
{
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return;
    std::error_code ec;
    const auto iniPath = std::filesystem::path(buffer.data()).parent_path() / L"forge_game.ini";
    if (!std::filesystem::exists(iniPath, ec)) return;
    std::ifstream ini(iniPath, std::ios::binary);
    if (!ini) return;

    std::wstring product, company, creator, icon, dataFolder;
    std::string line;
    auto trim = [](std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");
        const size_t last = text.find_last_not_of(" \t\r\n");
        text = first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    };
    while (std::getline(ini, line))
    {
        const auto eq = line.find('=');
        if (eq == std::string::npos || line[0] == ';' || line[0] == '[') continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        trim(key);
        trim(value);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        const std::wstring wideValue = Utf8ToWide(value);
        if (key == "product") product = wideValue;
        else if (key == "company") company = wideValue;
        else if (key == "creator") creator = wideValue;
        else if (key == "icon") icon = wideValue;
        else if (key == "datafolder") dataFolder = wideValue;
        else if (key == "project") dataFolder = wideValue; // legacy key kept for older exports
    }

    m_standalonePlayer = true;
    m_editorOverlayEnabled = false;
    m_gameTitle = product.empty() ? std::wstring(L"Forge Engine Game") : product;
    m_gameCreator = creator;
    m_gameProjectFolder = dataFolder;
    if (!icon.empty())
    {
        std::filesystem::path iconPath(icon);
        if (iconPath.is_relative()) iconPath = iniPath.parent_path() / iconPath;
        if (std::filesystem::exists(iconPath, ec)) m_gameIconPath = iconPath.wstring();
    }
    (void)company;
}

void ForgeEngine::ApplyGameIcon()
{
    if (m_gameIconPath.empty() || !m_hwnd) return;
    std::error_code ec;
    if (!std::filesystem::exists(m_gameIconPath, ec)) return;

    HICON largeIcon = nullptr;
    HICON smallIcon = nullptr;
    const auto extension = std::filesystem::path(m_gameIconPath).extension().wstring();
    if (extension == L".ico" || extension == L".cur")
    {
        largeIcon = static_cast<HICON>(LoadImageW(m_instance, m_gameIconPath.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE));
        smallIcon = static_cast<HICON>(LoadImageW(m_instance, m_gameIconPath.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));
    }
    else
    {
        // PNG/JPG/BMP/TGA icons are decoded through WIC and converted with CreateIconIndirect.
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        GUID inputFormat = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateDecoderFromFilename(m_gameIconPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)) &&
            SUCCEEDED(decoder->GetFrame(0, &frame)))
        {
            UINT width = 0, height = 0;
            frame->GetSize(&width, &height);
            if (width > 0 && height > 0 && width <= 256 && height <= 256 &&
                SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
                SUCCEEDED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            {
                std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);
                std::vector<std::uint8_t> maskPixels((static_cast<size_t>(width) * height + 7) / 8, 0);
                WICRect rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
                if (SUCCEEDED(converter->CopyPixels(&rect, width * 4, static_cast<UINT>(pixels.size()), pixels.data())))
                {
                    BITMAPV5HEADER header{};
                    header.bV5Size = sizeof(header);
                    header.bV5Width = static_cast<LONG>(width);
                    header.bV5Height = -static_cast<LONG>(height);
                    header.bV5Planes = 1;
                    header.bV5BitCount = 32;
                    header.bV5Compression = BI_BITFIELDS;
                    header.bV5RedMask = 0x00FF0000;
                    header.bV5GreenMask = 0x0000FF00;
                    header.bV5BlueMask = 0x000000FF;
                    header.bV5AlphaMask = 0xFF000000;
                    void* colorBits = nullptr;
                    void* maskBits = nullptr;
                    HDC dc = GetDC(m_hwnd);
                    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<const BITMAPINFO*>(&header), DIB_RGB_COLORS, &colorBits, nullptr, 0);
                    HBITMAP mask = CreateBitmap(width, height, 1, 1, maskPixels.data());
                    ReleaseDC(m_hwnd, dc);
                    (void)maskBits;
                    if (color && mask)
                    {
                        memcpy(colorBits, pixels.data(), pixels.size());
                        ICONINFO info{};
                        info.fIcon = TRUE;
                        info.hbmMask = mask;
                        info.hbmColor = color;
                        largeIcon = CreateIconIndirect(&info);
                        smallIcon = largeIcon;
                    }
                    if (color) DeleteObject(color);
                    if (mask && mask != reinterpret_cast<HBITMAP>(1)) DeleteObject(mask);
                }
            }
        }
    }
    if (largeIcon) SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    if (smallIcon) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
}

bool ForgeEngine::Initialize(HINSTANCE instance, int showCommand)
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_comInitialized = SUCCEEDED(comResult);
    m_instance = instance;
    LoadGameConfig();
    CreateStartupSplash(instance);
    UpdateStartupSplash(8, L"Creating the native window and DirectX device...");
    CreateWindowAndDevice(instance, showCommand);
    ApplyGameIcon();
    UpdateStartupSplash(22, L"Creating DirectX 12 command queues and synchronization...");
    CreateDeviceAndCommandObjects();
    UpdateStartupSplash(34, L"Creating swap chain and descriptor heaps...");
    CreateSwapChain();
    CreateDescriptorHeaps();
    UpdateStartupSplash(47, L"Allocating HDR, depth and shadow resources...");
    CreateSizeDependentResources();
    CreateShadowMap();
    CreateRootSignature();
    UpdateStartupSplash(62, L"Compiling HLSL shaders and pipeline states...");
    CreateShadersAndPipelines();
    UpdateStartupSplash(76, L"Building default scene and editor tools...");
    CreateScene();
    CreateGizmoMeshes();
    CreateColliderWireMeshes();
    CreateConstantBuffers();
    UpdateStartupSplash(88, L"Loading project files and initializing the editor UI...");
    m_editorUI = std::make_unique<ForgeEditorUI>();
    if (!m_editorUI->Initialize(*this))
        throw std::runtime_error("Nao foi possivel inicializar Dear ImGui + ImGuizmo.");

    m_startTime = std::chrono::steady_clock::now();
    m_initialized = true;
    UpdateStartupSplash(100, L"Ready. Opening Forge Engine Project Hub...");
    DestroyStartupSplash();
    ShowWindow(m_hwnd, showCommand);
    UpdateWindow(m_hwnd);
    return true;
}

LRESULT CALLBACK ForgeEngine::SplashWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ForgeEngine* self = reinterpret_cast<ForgeEngine*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ForgeEngine*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_PAINT)
    {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{}; GetClientRect(hwnd, &r);
        HBRUSH bg = CreateSolidBrush(RGB(12, 16, 27));
        FillRect(dc, &r, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(230, 237, 249));
        HFONT titleFont = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT bodyFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ old = ::SelectObject(dc, reinterpret_cast<HGDIOBJ>(titleFont));
        RECT title{ 34, 38, r.right - 34, 84 };
        DrawTextW(dc, L"FORGE ENGINE", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        ::SelectObject(dc, reinterpret_cast<HGDIOBJ>(bodyFont));
        SetTextColor(dc, RGB(154, 168, 193));
        RECT subtitle{ 36, 84, r.right - 34, 112 };
        DrawTextW(dc, L"Native 3D workspace  /  DirectX 12  /  C# scripting", -1, &subtitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (self)
        {
            SetTextColor(dc, RGB(205, 215, 234));
            RECT msg{ 36, 158, r.right - 30, 188 };
            DrawTextW(dc, self->m_splashMessage.c_str(), -1, &msg, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT track{ 36, 205, r.right - 36, 214 };
            HBRUSH trackBrush = CreateSolidBrush(RGB(36, 45, 64)); FillRect(dc, &track, trackBrush); DeleteObject(trackBrush);
            track.right = track.left + (track.right-track.left) * std::clamp(self->m_splashProgress,0,100) / 100;
            HBRUSH accent = CreateSolidBrush(RGB(95, 103, 255)); FillRect(dc, &track, accent); DeleteObject(accent);
            wchar_t progress[32]{}; swprintf_s(progress, L"%d%%", self->m_splashProgress);
            RECT pct{ r.right-86, 226, r.right-34, 250 };
            SetTextColor(dc, RGB(150, 163, 190)); DrawTextW(dc, progress, -1, &pct, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        ::SelectObject(dc, old); DeleteObject(titleFont); DeleteObject(bodyFont);
        EndPaint(hwnd, &ps); return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ForgeEngine::CreateStartupSplash(HINSTANCE instance)
{
    m_splashProgress = 0;
    m_splashMessage = L"Preparing project...";
    const wchar_t* className = L"ForgeEngineStartupSplash";
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = SplashWindowProc;
    wc.hInstance = instance; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = className;
    RegisterClassExW(&wc);
    const int width = 620, height = 290;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    m_splashHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, className, L"Forge Engine",
        WS_POPUP | WS_BORDER, x, y, width, height, nullptr, nullptr, instance, this);
    if (m_splashHwnd) { ShowWindow(m_splashHwnd, SW_SHOWNORMAL); UpdateWindow(m_splashHwnd); }
}

void ForgeEngine::UpdateStartupSplash(int percent, const wchar_t* message)
{
    m_splashProgress = std::clamp(percent, 0, 100);
    m_splashMessage = message ? message : L"Loading...";
    if (m_splashHwnd && IsWindow(m_splashHwnd))
    {
        InvalidateRect(m_splashHwnd, nullptr, TRUE); UpdateWindow(m_splashHwnd);
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { PostQuitMessage(static_cast<int>(msg.wParam)); continue; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
    }
}

void ForgeEngine::DestroyStartupSplash()
{
    if (m_splashHwnd && IsWindow(m_splashHwnd)) DestroyWindow(m_splashHwnd);
    m_splashHwnd = nullptr;
}

void ForgeEngine::CreateWindowAndDevice(HINSTANCE instance, int showCommand)
{
    (void)showCommand;
    const wchar_t* className = L"ForgeEngineWindowClass";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = className;

    if (!RegisterClassExW(&windowClass))
    {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
            ThrowIfFailed(HRESULT_FROM_WIN32(error), "RegisterClassExW");
    }

    // Frameless client area with a native resize frame. Dear ImGui draws the
    // custom editor caption and its minimize/maximize/close controls.
    const DWORD windowStyle = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    RECT rectangle{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&rectangle, windowStyle, FALSE);
    m_hwnd = CreateWindowExW(WS_EX_APPWINDOW, className,
        L"Forge Engine",
        windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
        nullptr, nullptr, instance, nullptr);
    if (!m_hwnd)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateWindowExW");
    // Attribute 20 is DWMWA_USE_IMMERSIVE_DARK_MODE on current Windows SDKs.
    // Use the numeric value so older Windows SDK headers can still compile this source.
    BOOL darkTitleBar = TRUE;
    const auto darkModeAttribute = static_cast<DWMWINDOWATTRIBUTE>(20);
    HRESULT darkTitleResult = DwmSetWindowAttribute(m_hwnd, darkModeAttribute, &darkTitleBar, sizeof(darkTitleBar));
    if (FAILED(darkTitleResult))
    {
        const auto legacyDarkModeAttribute = static_cast<DWMWINDOWATTRIBUTE>(19);
        DwmSetWindowAttribute(m_hwnd, legacyDarkModeAttribute, &darkTitleBar, sizeof(darkTitleBar));
    }

    RAWINPUTDEVICE rawMouse{};
    rawMouse.usUsagePage = 0x01; // Generic Desktop Controls
    rawMouse.usUsage = 0x02;     // Mouse
    rawMouse.dwFlags = 0;
    rawMouse.hwndTarget = m_hwnd;
    if (!RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse)))
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "RegisterRawInputDevices");
}

void ForgeEngine::CreateDeviceAndCommandObjects()
{
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        debugController->EnableDebugLayer();
#endif

    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory)), "CreateDXGIFactory2");
    HRESULT deviceResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    if (FAILED(deviceResult))
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), "EnumWarpAdapter");
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
            "D3D12CreateDevice (WARP)");
    }

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDescription.NodeMask = 0;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&m_commandQueue)), "CreateCommandQueue");

    for (UINT i = 0; i < FrameCount; ++i)
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])), "CreateCommandAllocator");

    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)), "CreateCommandList");
    ThrowIfFailed(m_commandList->Close(), "CommandList::Close inicial");

    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "CreateFence");
    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEventW para fence");
}

void ForgeEngine::CreateSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = m_width;
    description.Height = m_height;
    description.Format = SwapChainFormat();
    description.Stereo = FALSE;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = FrameCount;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    description.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    description.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hwnd,
        &description, nullptr, nullptr, &swapChain1), "CreateSwapChainForHwnd");
    ThrowIfFailed(swapChain1.As(&m_swapChain), "Query IDXGISwapChain3");
    m_factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void ForgeEngine::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescription{};
    rtvDescription.NumDescriptors = FrameCount * 2; // back buffers + HDR scene targets; preview uses copy-only SRVs
    rtvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvDescription, IID_PPV_ARGS(&m_rtvHeap)), "Create RTV heap");

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescription{};
    dsvDescription.NumDescriptors = 2;
    dsvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvDescription, IID_PPV_ARGS(&m_dsvHeap)), "Create DSV heap");

    D3D12_DESCRIPTOR_HEAP_DESC srvDescription{};
    srvDescription.NumDescriptors = 1024;
    srvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvDescription, IID_PPV_ARGS(&m_srvHeap)), "Create SRV heap");

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void ForgeEngine::CreateSizeDependentResources()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FrameCount; ++i)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])), "SwapChain::GetBuffer");
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    D3D12_RESOURCE_DESC depthDescription{};
    depthDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDescription.Alignment = 0;
    depthDescription.Width = m_width;
    depthDescription.Height = m_height;
    depthDescription.DepthOrArraySize = 1;
    depthDescription.MipLevels = 1;
    // R32_TYPELESS supports both a D32 DSV and an R32_FLOAT shader-resource view for SSAO.
    depthDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.SampleDesc.Quality = 0;
    depthDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClear{};
    depthClear.Format = DXGI_FORMAT_D32_FLOAT;
    depthClear.DepthStencil.Depth = 1.0f;
    depthClear.DepthStencil.Stencil = 0;
    const auto defaultHeap = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeap,
        D3D12_HEAP_FLAG_NONE, &depthDescription, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClear, IID_PPV_ARGS(&m_depthBuffer)), "Create main depth buffer");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDescription{};
    dsvDescription.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDescription.Flags = D3D12_DSV_FLAG_NONE;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDescription, dsvHandle);

    // Descriptor layout: 0=shadow, 1..2=HDR scene targets, 3=scene depth, 4..5=camera preview, 6..=material textures.
    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrv{};
    depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
    depthSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrv.Texture2D.MostDetailedMip = 0;
    depthSrv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE depthSrvCpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvGpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    depthSrvCpu.ptr += static_cast<SIZE_T>(3) * m_srvDescriptorSize;
    depthSrvGpu.ptr += static_cast<UINT64>(3) * m_srvDescriptorSize;
    m_device->CreateShaderResourceView(m_depthBuffer.Get(), &depthSrv, depthSrvCpu);
    m_depthSrvGpuHandle = depthSrvGpu;

    // HDR scene targets: render in linear FP16, then tone-map into the UNORM swap chain.
    D3D12_RESOURCE_DESC hdrDescription{};
    hdrDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    hdrDescription.Alignment = 0;
    hdrDescription.Width = m_width;
    hdrDescription.Height = m_height;
    hdrDescription.DepthOrArraySize = 1;
    hdrDescription.MipLevels = 1;
    hdrDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    hdrDescription.SampleDesc.Count = 1;
    hdrDescription.SampleDesc.Quality = 0;
    hdrDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    hdrDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE hdrClear{};
    hdrClear.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    hdrClear.Color[0] = 0.025f;
    hdrClear.Color[1] = 0.035f;
    hdrClear.Color[2] = 0.055f;
    hdrClear.Color[3] = 1.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE hdrRtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    hdrRtvHandle.ptr += static_cast<SIZE_T>(FrameCount) * m_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    srvCpuHandle.ptr += m_srvDescriptorSize; // slot zero is reserved for the shadow map
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvGpuHandle.ptr += m_srvDescriptorSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC hdrSrv{};
    hdrSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    hdrSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    hdrSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    hdrSrv.Texture2D.MostDetailedMip = 0;
    hdrSrv.Texture2D.MipLevels = 1;
    hdrSrv.Texture2D.PlaneSlice = 0;
    hdrSrv.Texture2D.ResourceMinLODClamp = 0.0f;

    for (UINT i = 0; i < FrameCount; ++i)
    {
        m_hdrTargets[i].Reset();
        ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeap,
            D3D12_HEAP_FLAG_NONE, &hdrDescription, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &hdrClear, IID_PPV_ARGS(&m_hdrTargets[i])), "Create HDR scene target");
        m_device->CreateRenderTargetView(m_hdrTargets[i].Get(), nullptr, hdrRtvHandle);
        m_device->CreateShaderResourceView(m_hdrTargets[i].Get(), &hdrSrv, srvCpuHandle);
        m_sceneColorSrvGpuHandles[i] = srvGpuHandle;
        hdrRtvHandle.ptr += m_rtvDescriptorSize;
        srvCpuHandle.ptr += m_srvDescriptorSize;
        srvGpuHandle.ptr += m_srvDescriptorSize;
    }

    // Camera Preview samples a copy of the final post-processed back buffer. Keeping
    // it in a separate resource avoids sampling a swap-chain buffer while it is the RTV.
    D3D12_RESOURCE_DESC previewDescription{};
    previewDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    previewDescription.Width = m_width;
    previewDescription.Height = m_height;
    previewDescription.DepthOrArraySize = 1;
    previewDescription.MipLevels = 1;
    previewDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    previewDescription.SampleDesc.Count = 1;
    previewDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    previewDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_SHADER_RESOURCE_VIEW_DESC previewSrv{};
    previewSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    previewSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    previewSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    previewSrv.Texture2D.MipLevels = 1;
    for (UINT i = 0; i < FrameCount; ++i)
    {
        m_cameraPreviewTargets[i].Reset();
        ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
            &previewDescription, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
            IID_PPV_ARGS(&m_cameraPreviewTargets[i])), "Create camera preview texture");
        D3D12_CPU_DESCRIPTOR_HANDLE previewCpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE previewGpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        previewCpu.ptr += static_cast<SIZE_T>(4u + i) * m_srvDescriptorSize;
        previewGpu.ptr += static_cast<UINT64>(4u + i) * m_srvDescriptorSize;
        m_device->CreateShaderResourceView(m_cameraPreviewTargets[i].Get(), &previewSrv, previewCpu);
        m_cameraPreviewSrvGpuHandles[i] = previewGpu;
    }

    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<float>(m_width);
    m_viewport.Height = static_cast<float>(m_height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
}

void ForgeEngine::CreateShadowMap()
{
    D3D12_RESOURCE_DESC shadowDescription{};
    shadowDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    shadowDescription.Alignment = 0;
    shadowDescription.Width = ShadowMapSize;
    shadowDescription.Height = ShadowMapSize;
    shadowDescription.DepthOrArraySize = 1;
    shadowDescription.MipLevels = 1;
    shadowDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    shadowDescription.SampleDesc.Count = 1;
    shadowDescription.SampleDesc.Quality = 0;
    shadowDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    shadowDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    const auto defaultHeap = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeap,
        D3D12_HEAP_FLAG_NONE, &shadowDescription, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&m_shadowMap)), "Create shadow map");

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    dsvHandle.ptr += m_dsvDescriptorSize;
    m_shadowDsvHandle = dsvHandle;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags = D3D12_DSV_FLAG_NONE;
    m_device->CreateDepthStencilView(m_shadowMap.Get(), &dsv, m_shadowDsvHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = 1;
    srv.Texture2D.PlaneSlice = 0;
    srv.Texture2D.ResourceMinLODClamp = 0.0f;
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateShaderResourceView(m_shadowMap.Get(), &srv, srvHandle);
    m_shadowSrvGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
}

void ForgeEngine::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE ranges[8]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 1;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[2].NumDescriptors = 1;
    ranges[2].BaseShaderRegister = 2;
    ranges[2].RegisterSpace = 0;
    ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[3].NumDescriptors = 1;
    ranges[3].BaseShaderRegister = 3;
    ranges[3].RegisterSpace = 0;
    ranges[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    for (UINT i = 4; i <= 7; ++i)
    {
        ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[i].NumDescriptors = 1;
        ranges[i].BaseShaderRegister = i;
        ranges[i].RegisterSpace = 0;
        ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    // Root parameters: 0=frame CBV, 1=object CBV, 2=shadow map, 3=post-process scene color,
    // 4=albedo, 5=scene depth, 6=normal map, 7=metallic/roughness, 8=AO, 9=emissive.
    D3D12_ROOT_PARAMETER parameters[10]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].Descriptor.RegisterSpace = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[1].Descriptor.ShaderRegister = 1;
    parameters[1].Descriptor.RegisterSpace = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable.NumDescriptorRanges = 1;
    parameters[2].DescriptorTable.pDescriptorRanges = &ranges[0];
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[3].DescriptorTable.NumDescriptorRanges = 1;
    parameters[3].DescriptorTable.pDescriptorRanges = &ranges[1];
    parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[4].DescriptorTable.NumDescriptorRanges = 1;
    parameters[4].DescriptorTable.pDescriptorRanges = &ranges[2];
    parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[5].DescriptorTable.NumDescriptorRanges = 1;
    parameters[5].DescriptorTable.pDescriptorRanges = &ranges[3];
    parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    for (UINT i = 6; i <= 9; ++i)
    {
        parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[i].DescriptorTable.NumDescriptorRanges = 1;
        parameters[i].DescriptorTable.pDescriptorRanges = &ranges[i - 2]; // t4, t5, t6, t7
        parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    D3D12_STATIC_SAMPLER_DESC samplers[3]{};
    samplers[0].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[0].MaxAnisotropy = 1;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplers[0].MinLOD = 0.0f;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].MaxAnisotropy = 1;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplers[1].MinLOD = 0.0f;
    samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[1].ShaderRegister = 1;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Tiling/offset must repeat texture coordinates rather than clamp at the edge.
    // Use a separate anisotropic sampler so post-processing remains clamped to the viewport.
    samplers[2].Filter = D3D12_FILTER_ANISOTROPIC;
    samplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[2].MipLODBias = 0.0f;
    samplers[2].MaxAnisotropy = 8;
    samplers[2].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplers[2].MinLOD = 0.0f;
    samplers[2].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[2].ShaderRegister = 2;
    samplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootDescription{};
    rootDescription.NumParameters = static_cast<UINT>(_countof(parameters));
    rootDescription.pParameters = parameters;
    rootDescription.NumStaticSamplers = static_cast<UINT>(_countof(samplers));
    rootDescription.pStaticSamplers = samplers;
    rootDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(&rootDescription, D3D_ROOT_SIGNATURE_VERSION_1,
        &signature, &errors);
    if (FAILED(result))
    {
        if (errors)
            throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
        ThrowIfFailed(result, "D3D12SerializeRootSignature");
    }
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)), "CreateRootSignature");
}

std::filesystem::path ForgeEngine::GetShaderPath() const
{
    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        throw std::runtime_error("Nao foi possivel localizar a pasta do executavel para carregar Shaders.hlsl.");
    const std::filesystem::path executableDirectory = std::filesystem::path(executablePath).parent_path();

    // Exported games keep their shader inside the <Product>_Data folder; the editor
    // and any legacy export keep it directly next to the executable.
    std::error_code ec;
    if (!m_gameProjectFolder.empty())
    {
        const auto dataShader = executableDirectory / m_gameProjectFolder / L"Shaders.hlsl";
        if (std::filesystem::exists(dataShader, ec)) return dataShader;
    }
    for (std::filesystem::directory_iterator it(executableDirectory, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code kindEc;
        if (!it->is_directory(kindEc)) continue;
        if (it->path().extension() != L"_Data") continue;
        const auto candidate = it->path() / L"Shaders.hlsl";
        if (std::filesystem::exists(candidate, ec)) return candidate;
    }
    return executableDirectory / L"Shaders.hlsl";
}

void ForgeEngine::CreateShadersAndPipelines()
{
    const auto shaderPath = GetShaderPath();
    if (!std::filesystem::exists(shaderPath))
        throw std::runtime_error("Shaders.hlsl nao foi encontrado ao lado do executavel. Compile o projeto com CMake para copiar o shader automaticamente.");

    auto compile = [&](const char* entry, const char* target, ComPtr<ID3DBlob>& bytecode)
    {
        ComPtr<ID3DBlob> errors;
        const UINT flags =
#ifdef _DEBUG
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        HRESULT result = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, target, flags, 0, &bytecode, &errors);
        if (FAILED(result))
        {
            std::string detail = "Falha ao compilar shader " + std::string(entry) + ": ";
            if (errors && errors->GetBufferPointer())
                detail += static_cast<const char*>(errors->GetBufferPointer());
            else
                detail += "HRESULT de compilacao invalido.";
            throw std::runtime_error(detail);
        }
    };

    ComPtr<ID3DBlob> vsPbr, psPbr, vsShadow, vsSky, psSky, psOutline, psAxis, psPost;
    compile("VSMain", "vs_5_0", vsPbr);
    compile("PSPBR", "ps_5_0", psPbr);
    compile("VSShadow", "vs_5_0", vsShadow);
    compile("VSSkyGrid", "vs_5_0", vsSky);
    compile("PSSkyGrid", "ps_5_0", psSky);
    compile("PSOutline", "ps_5_0", psOutline);
    compile("PSAxis", "ps_5_0", psAxis);
    ComPtr<ID3DBlob> vsPost;
    compile("VSPostProcess", "vs_5_0", vsPost);
    compile("PSPostProcess", "ps_5_0", psPost);

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    D3D12_INPUT_ELEMENT_DESC shadowInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vsPbr->GetBufferPointer(), vsPbr->GetBufferSize() };
    pso.PS = { psPbr->GetBufferPointer(), psPbr->GetBufferSize() };
    pso.BlendState = DefaultBlend();
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = DefaultRasterizer();
    pso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_DEPTH_WRITE_MASK_ALL);
    pso.InputLayout = { inputLayout, static_cast<UINT>(_countof(inputLayout)) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    pso.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pbrPipeline)), "Create PBR PSO");

    // Double-sided variant for authored two-sided materials (foliage, cloth, flags).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC doubleSidedPso = pso;
    doubleSidedPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    doubleSidedPso.RasterizerState.FrontCounterClockwise = FALSE;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&doubleSidedPso, IID_PPV_ARGS(&m_pbrDoubleSidedPipeline)),
        "Create double-sided PBR PSO");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPso{};
    shadowPso.pRootSignature = m_rootSignature.Get();
    shadowPso.VS = { vsShadow->GetBufferPointer(), vsShadow->GetBufferSize() };
    shadowPso.BlendState = DefaultBlend();
    shadowPso.SampleMask = UINT_MAX;
    shadowPso.RasterizerState = DefaultRasterizer(D3D12_CULL_MODE_BACK);
    // Less aggressive bias preserves contact shadows at 4K while the PCF kernel softens edges.
    shadowPso.RasterizerState.DepthBias = 250;
    shadowPso.RasterizerState.SlopeScaledDepthBias = 0.85f;
    shadowPso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_DEPTH_WRITE_MASK_ALL);
    shadowPso.InputLayout = { shadowInputLayout, static_cast<UINT>(_countof(shadowInputLayout)) };
    shadowPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadowPso.NumRenderTargets = 0;
    shadowPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    shadowPso.SampleDesc.Count = 1;
    shadowPso.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&shadowPso, IID_PPV_ARGS(&m_shadowPipeline)), "Create shadow PSO");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPso{};
    skyPso.pRootSignature = m_rootSignature.Get();
    skyPso.VS = { vsSky->GetBufferPointer(), vsSky->GetBufferSize() };
    skyPso.PS = { psSky->GetBufferPointer(), psSky->GetBufferSize() };
    skyPso.BlendState = DefaultBlend();
    skyPso.SampleMask = UINT_MAX;
    skyPso.RasterizerState = DefaultRasterizer(D3D12_CULL_MODE_NONE);
    skyPso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_ALWAYS, D3D12_DEPTH_WRITE_MASK_ALL);
    skyPso.InputLayout = { nullptr, 0 };
    skyPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    skyPso.NumRenderTargets = 1;
    skyPso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    skyPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    skyPso.SampleDesc.Count = 1;
    skyPso.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&skyPso, IID_PPV_ARGS(&m_skyGridPipeline)), "Create sky/grid PSO");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC outlinePso = pso;
    outlinePso.PS = { psOutline->GetBufferPointer(), psOutline->GetBufferSize() };
    outlinePso.RasterizerState = DefaultRasterizer(D3D12_CULL_MODE_FRONT);
    outlinePso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_DEPTH_WRITE_MASK_ZERO);
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&outlinePso, IID_PPV_ARGS(&m_outlinePipeline)), "Create outline PSO");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC axisPso = pso;
    axisPso.PS = { psAxis->GetBufferPointer(), psAxis->GetBufferSize() };
    axisPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    axisPso.RasterizerState = DefaultRasterizer(D3D12_CULL_MODE_NONE);
    axisPso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_DEPTH_WRITE_MASK_ZERO);
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&axisPso, IID_PPV_ARGS(&m_axisPipeline)), "Create axis PSO");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gizmoPso = axisPso;
    gizmoPso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_ALWAYS, D3D12_DEPTH_WRITE_MASK_ZERO);
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&gizmoPso, IID_PPV_ARGS(&m_gizmoPipeline)), "Create gizmo PSO");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC postPso{};
    postPso.pRootSignature = m_rootSignature.Get();
    postPso.VS = { vsPost->GetBufferPointer(), vsPost->GetBufferSize() };
    postPso.PS = { psPost->GetBufferPointer(), psPost->GetBufferSize() };
    postPso.BlendState = DefaultBlend();
    postPso.SampleMask = UINT_MAX;
    postPso.RasterizerState = DefaultRasterizer(D3D12_CULL_MODE_NONE);
    postPso.DepthStencilState = DefaultDepthStencil(D3D12_COMPARISON_FUNC_ALWAYS, D3D12_DEPTH_WRITE_MASK_ZERO);
    postPso.DepthStencilState.DepthEnable = FALSE;
    postPso.DepthStencilState.StencilEnable = FALSE;
    postPso.InputLayout = { nullptr, 0 };
    postPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    postPso.NumRenderTargets = 1;
    postPso.RTVFormats[0] = SwapChainFormat();
    postPso.DSVFormat = DXGI_FORMAT_UNKNOWN;
    postPso.SampleDesc.Count = 1;
    postPso.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&postPso, IID_PPV_ARGS(&m_postProcessPipeline)), "Create post-process PSO");
}

void ForgeEngine::CreateUploadResource(UINT64 size, ComPtr<ID3D12Resource>& resource)
{
    const auto heap = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const auto description = BufferDescription(size);
    ThrowIfFailed(m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)), "Create upload buffer");
}

void ForgeEngine::CreateMesh(Mesh& mesh, const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
        throw std::runtime_error("Tentativa de criar uma malha vazia.");

    const UINT vertexBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    CreateUploadResource(vertexBytes, mesh.vertexBuffer);
    void* mapped = nullptr;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(mesh.vertexBuffer->Map(0, &readRange, &mapped), "Map vertex buffer");
    std::memcpy(mapped, vertices.data(), vertexBytes);
    mesh.vertexBuffer->Unmap(0, nullptr);
    mesh.vertexView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vertexView.SizeInBytes = vertexBytes;
    mesh.vertexView.StrideInBytes = sizeof(Vertex);
    mesh.vertexCount = static_cast<UINT>(vertices.size());

    const UINT indexBytes = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    CreateUploadResource(indexBytes, mesh.indexBuffer);
    mapped = nullptr;
    ThrowIfFailed(mesh.indexBuffer->Map(0, &readRange, &mapped), "Map index buffer");
    std::memcpy(mapped, indices.data(), indexBytes);
    mesh.indexBuffer->Unmap(0, nullptr);
    mesh.indexView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
    mesh.indexView.SizeInBytes = indexBytes;
    mesh.indexView.Format = DXGI_FORMAT_R32_UINT;
    mesh.indexCount = static_cast<UINT>(indices.size());
    mesh.cpuVertices = vertices;
    mesh.cpuIndices = indices;

    // Calculate bounds from the real CPU vertices. Imported meshes often have an
    // origin that is not at their geometric center; culling around (0,0,0) made
    // otherwise visible models disappear when rotated or viewed near a frustum edge.
    XMFLOAT3 boundsMin{ FLT_MAX, FLT_MAX, FLT_MAX };
    XMFLOAT3 boundsMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const Vertex& vertex : vertices)
    {
        boundsMin.x = (std::min)(boundsMin.x, vertex.position.x);
        boundsMin.y = (std::min)(boundsMin.y, vertex.position.y);
        boundsMin.z = (std::min)(boundsMin.z, vertex.position.z);
        boundsMax.x = (std::max)(boundsMax.x, vertex.position.x);
        boundsMax.y = (std::max)(boundsMax.y, vertex.position.y);
        boundsMax.z = (std::max)(boundsMax.z, vertex.position.z);
    }
    mesh.boundsCenter = { (boundsMin.x + boundsMax.x) * 0.5f,
        (boundsMin.y + boundsMax.y) * 0.5f, (boundsMin.z + boundsMax.z) * 0.5f };
    float radiusSquared = 0.0f;
    for (const Vertex& vertex : vertices)
    {
        const float dx = vertex.position.x - mesh.boundsCenter.x;
        const float dy = vertex.position.y - mesh.boundsCenter.y;
        const float dz = vertex.position.z - mesh.boundsCenter.z;
        radiusSquared = (std::max)(radiusSquared, dx*dx + dy*dy + dz*dz);
    }
    mesh.boundsRadius = (std::max)(0.001f, std::sqrt(radiusSquared));
}

void ForgeEngine::CreateVertexOnlyMesh(Mesh& mesh, const std::vector<Vertex>& vertices)
{
    if (vertices.empty())
        throw std::runtime_error("Tentativa de criar uma malha de linhas vazia.");
    const UINT bytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    CreateUploadResource(bytes, mesh.vertexBuffer);
    void* mapped = nullptr;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(mesh.vertexBuffer->Map(0, &readRange, &mapped), "Map line vertex buffer");
    std::memcpy(mapped, vertices.data(), bytes);
    mesh.vertexBuffer->Unmap(0, nullptr);
    mesh.vertexView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vertexView.SizeInBytes = bytes;
    mesh.vertexView.StrideInBytes = sizeof(Vertex);
    mesh.vertexCount = static_cast<UINT>(vertices.size());
    mesh.indexCount = 0;
    mesh.cpuVertices = vertices;
    mesh.cpuIndices.clear();
}

void ForgeEngine::CreateScene()
{
    std::vector<Vertex> cubeVertices;
    std::vector<std::uint32_t> cubeIndices;
    auto addFace = [&](XMFLOAT3 normal, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 d)
    {
        const std::uint32_t start = static_cast<std::uint32_t>(cubeVertices.size());
        cubeVertices.push_back({ a, normal, {0.0f, 1.0f} });
        cubeVertices.push_back({ b, normal, {0.0f, 0.0f} });
        cubeVertices.push_back({ c, normal, {1.0f, 0.0f} });
        cubeVertices.push_back({ d, normal, {1.0f, 1.0f} });
        cubeIndices.insert(cubeIndices.end(), { start, start + 1, start + 2, start, start + 2, start + 3 });
    };
    // Counter-clockwise winding as seen from outside (left-handed coordinate system).
    addFace({ 0, 0, -1 }, { -1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 }, { 1, -1, -1 });
    addFace({ 0, 0, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }, { -1, -1, 1 });
    addFace({ -1, 0, 0 }, { -1, -1, 1 }, { -1, 1, 1 }, { -1, 1, -1 }, { -1, -1, -1 });
    addFace({ 1, 0, 0 }, { 1, -1, -1 }, { 1, 1, -1 }, { 1, 1, 1 }, { 1, -1, 1 });
    addFace({ 0, 1, 0 }, { -1, 1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }, { 1, 1, -1 });
    addFace({ 0, -1, 0 }, { -1, -1, 1 }, { -1, -1, -1 }, { 1, -1, -1 }, { 1, -1, 1 });
    CreateMesh(m_cubeMesh, cubeVertices, cubeIndices);

    std::vector<Vertex> sphereVertices;
    std::vector<std::uint32_t> sphereIndices;
    constexpr int rings = 20;
    constexpr int segments = 28;
    for (int ring = 0; ring <= rings; ++ring)
    {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = v * Pi;
        for (int segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * 2.0f * Pi;
            XMFLOAT3 position{
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            };
            sphereVertices.push_back({ position, Normalize3(position), {u, v} });
        }
    }
    for (int ring = 0; ring < rings; ++ring)
    {
        for (int segment = 0; segment < segments; ++segment)
        {
            const std::uint32_t a = static_cast<std::uint32_t>(ring * (segments + 1) + segment);
            const std::uint32_t b = a + static_cast<std::uint32_t>(segments + 1);
            sphereIndices.insert(sphereIndices.end(), { a, a + 1, b, a + 1, b + 1, b });
        }
    }
    CreateMesh(m_sphereMesh, sphereVertices, sphereIndices);

    // Extra primitives used by the Create menu and model library.
    // Plane
    {
        const std::vector<Vertex> v{
            {{-1,0,-1},{0,1,0},{0,0}}, {{-1,0,1},{0,1,0},{0,1}}, {{1,0,1},{0,1,0},{1,1}}, {{1,0,-1},{0,1,0},{1,0}}
        };
        const std::vector<std::uint32_t> i{0,1,2,0,2,3};
        CreateMesh(m_primitiveMeshes[0], v, i);
    }
    auto buildRadial = [&](Mesh& mesh, bool cone)
    {
        std::vector<Vertex> v; std::vector<std::uint32_t> idx;
        constexpr int segments = 32;
        // Side vertices, duplicated at the seam for stable normals.
        for (int j=0;j<=segments;++j) {
            const float a = 2.0f*Pi*static_cast<float>(j)/segments;
            const float x=std::cos(a), z=std::sin(a);
            if (cone) {
                XMFLOAT3 n=Normalize3({x,0.7f,z});
                v.push_back({{x,-1,z},n,{static_cast<float>(j)/segments,1.0f}}); v.push_back({{0,1,0},n,{static_cast<float>(j)/segments,0.0f}});
            } else {
                v.push_back({{x,-1,z},{x,0,z},{static_cast<float>(j)/segments,1.0f}}); v.push_back({{x,1,z},{x,0,z},{static_cast<float>(j)/segments,0.0f}});
            }
        }
        for (int j=0;j<segments;++j) {
            const std::uint32_t a=static_cast<std::uint32_t>(j*2);
            idx.insert(idx.end(),{a,a+1,a+2,a+1,a+3,a+2});
        }
        if (!cone) {
            const std::uint32_t base=static_cast<std::uint32_t>(v.size());
            v.push_back({{0,-1,0},{0,-1,0}}); v.push_back({{0,1,0},{0,1,0}});
            for (int j=0;j<segments;++j) {
                const std::uint32_t a=static_cast<std::uint32_t>(j*2), b=static_cast<std::uint32_t>((j+1)*2);
                idx.insert(idx.end(),{base,b,a,base+1,a+1,b+1});
            }
        }
        CreateMesh(mesh,v,idx);
    };
    buildRadial(m_primitiveMeshes[1], false); // cylinder
    buildRadial(m_primitiveMeshes[2], true);  // cone
    // Torus (major radius 0.72, tube radius 0.28)
    {
        std::vector<Vertex> v; std::vector<std::uint32_t> idx;
        constexpr int major=32, minor=12;
        for(int a=0;a<=major;++a) for(int b=0;b<=minor;++b) {
            const float u=2.0f*Pi*a/major, w=2.0f*Pi*b/minor;
            const float ring=0.72f+0.28f*std::cos(w);
            XMFLOAT3 pos{ring*std::cos(u),0.28f*std::sin(w),ring*std::sin(u)};
            XMFLOAT3 n{std::cos(w)*std::cos(u),std::sin(w),std::cos(w)*std::sin(u)};
            v.push_back({pos,Normalize3(n),{static_cast<float>(a)/major,static_cast<float>(b)/minor}});
        }
        for(int a=0;a<major;++a) for(int b=0;b<minor;++b) {
            const std::uint32_t x=static_cast<std::uint32_t>(a*(minor+1)+b), y=x+minor+1;
            idx.insert(idx.end(),{x,y,x+1,x+1,y,y+1});
        }
        CreateMesh(m_primitiveMeshes[3],v,idx);
    }

    // Long axis lines are rendered with a line-list pipeline. The ground grid itself is procedural and infinite.
    const std::vector<Vertex> axes{
        { { -250.0f, 0.015f, 0.0f }, { 0, 1, 0 } }, { { 250.0f, 0.015f, 0.0f }, { 0, 1, 0 } },
        { { 0.0f, 0.015f, -250.0f }, { 0, 1, 0 } }, { { 0.0f, 0.015f, 250.0f }, { 0, 1, 0 } },
        { { 0.0f, 0.015f, 0.0f }, { 0, 1, 0 } }, { { 0.0f, 120.0f, 0.0f }, { 0, 1, 0 } }
    };
    CreateVertexOnlyMesh(m_axisMesh, axes);

    auto addObject = [&](const char* name, Mesh* mesh, XMFLOAT3 position, XMFLOAT3 scale,
        XMFLOAT4 color, float metallic, float roughness, float radius)
    {
        GameObject object;
        object.name = name;
        object.mesh = mesh;
        object.position = position;
        object.rotation = { 0.0f, 0.0f, 0.0f };
        object.scale = scale;
        RebuildWorldTransform(object);
        object.baseColor = color;
        object.metallic = metallic;
        object.roughness = roughness;
        object.localBoundsRadius = radius;
        m_objects.push_back(std::move(object));
    };

    addObject("Cube - selecionavel", &m_cubeMesh, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f },
        { 0.18f, 0.45f, 0.92f, 1.0f }, 0.12f, 0.28f, 1.732f);
    addObject("Sphere - metal", &m_sphereMesh, { -2.7f, 0.85f, 1.5f }, { 0.85f, 0.85f, 0.85f },
        { 0.78f, 0.83f, 0.9f, 1.0f }, 0.82f, 0.19f, 1.0f);
    addObject("Tall cube", &m_cubeMesh, { 2.6f, 1.5f, 0.5f }, { 0.7f, 1.5f, 0.7f },
        { 0.94f, 0.34f, 0.16f, 1.0f }, 0.05f, 0.5f, 1.732f);
    addObject("Green sphere", &m_sphereMesh, { 2.2f, 0.65f, 3.0f }, { 0.65f, 0.65f, 0.65f },
        { 0.14f, 0.72f, 0.43f, 1.0f }, 0.0f, 0.42f, 1.0f);
    addObject("Small cube", &m_cubeMesh, { -0.4f, 0.45f, 4.0f }, { 0.45f, 0.45f, 0.45f },
        { 0.72f, 0.28f, 0.82f, 1.0f }, 0.2f, 0.35f, 1.732f);
}

void ForgeEngine::RebuildWorldTransform(GameObject& object)
{
    XMMATRIX world = XMMatrixScaling(object.scale.x, object.scale.y, object.scale.z) *
        XMMatrixRotationRollPitchYaw(object.rotation.x, object.rotation.y, object.rotation.z) *
        XMMatrixTranslation(object.position.x, object.position.y, object.position.z);

    const int selfIndex = IndexOfObject(&object);
    if (selfIndex >= 0)
    {
        int parent = object.parentIndex;
        int guard = static_cast<int>(m_objects.size()) + 1; // cycle safety for malformed scenes
        while (parent >= 0 && parent < static_cast<int>(m_objects.size()) && parent != selfIndex && guard-- > 0)
        {
            const GameObject& ancestor = m_objects[parent];
            world = world *
                XMMatrixScaling(ancestor.scale.x, ancestor.scale.y, ancestor.scale.z) *
                XMMatrixRotationRollPitchYaw(ancestor.rotation.x, ancestor.rotation.y, ancestor.rotation.z) *
                XMMatrixTranslation(ancestor.position.x, ancestor.position.y, ancestor.position.z);
            parent = ancestor.parentIndex;
        }
    }
    XMStoreFloat4x4(&object.world, world);

    if (selfIndex >= 0 && !m_transformCascadeActive)
    {
        m_transformCascadeActive = true;
        std::vector<int> pending{ selfIndex };
        while (!pending.empty())
        {
            const int current = pending.back();
            pending.pop_back();
            for (int i = 0; i < static_cast<int>(m_objects.size()); ++i)
            {
                if (m_objects[i].parentIndex != current) continue;
                RebuildWorldTransform(m_objects[i]);
                pending.push_back(i);
            }
        }
        m_transformCascadeActive = false;
    }
}

int ForgeEngine::IndexOfObject(const GameObject* object) const
{
    for (int i = 0; i < static_cast<int>(m_objects.size()); ++i)
        if (&m_objects[static_cast<size_t>(i)] == object) return i;
    return -1;
}

XMFLOAT4X4 ForgeEngine::GetAncestorWorld(const GameObject& object) const
{
    if (object.parentIndex >= 0 && object.parentIndex < static_cast<int>(m_objects.size()))
        return m_objects[static_cast<size_t>(object.parentIndex)].world;
    XMFLOAT4X4 identity{};
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    return identity;
}

int ForgeEngine::CloneObjectForScript(const GameObject& source, const std::string& newName)
{
    GameObject clone = source;
    clone.name = newName;
    clone.parentIndex = -1; // Instantiate places clones at hierarchy root, like Unity
    clone.isDragPreview = false;
    clone.isDragPreviewEmissive = false;
    for (UINT frame = 0; frame < FrameCount; ++frame)
    {
        clone.objectConstants[frame].Reset();
        clone.outlineConstants[frame].Reset();
        clone.colliderConstants[frame].Reset();
    }
    CreateObjectConstantBuffers(clone);
    const int index = static_cast<int>(m_objects.size());
    m_objects.push_back(std::move(clone));
    RebuildWorldTransform(m_objects[static_cast<size_t>(index)]);
    return index;
}

bool ForgeEngine::DestroyObjectForScript(int id)
{
    if (id < 0 || id >= static_cast<int>(m_objects.size())) return false;
    const int oldParent = m_objects[static_cast<size_t>(id)].parentIndex;
    m_objects.erase(m_objects.begin() + id);
    for (auto& object : m_objects)
    {
        if (object.parentIndex == id) object.parentIndex = oldParent;
        else if (object.parentIndex > id) --object.parentIndex;
    }
    if (m_selectedObject == id) m_selectedObject = -1;
    else if (m_selectedObject > id) --m_selectedObject;
    for (auto& object : m_objects) RebuildWorldTransform(object);
    return true;
}

void ForgeEngine::CreateGizmoMeshes()
{
    const XMFLOAT3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    const XMFLOAT3 perpendicularA[3] = { {0,1,0}, {1,0,0}, {1,0,0} };
    const XMFLOAT3 perpendicularB[3] = { {0,0,1}, {0,0,1}, {0,1,0} };

    for (int axis = 0; axis < 3; ++axis)
    {
        std::vector<Vertex> vertices;
        auto addLine = [&](XMFLOAT3 a, XMFLOAT3 b)
        {
            vertices.push_back({ a, axes[axis] });
            vertices.push_back({ b, axes[axis] });
        };
        const XMFLOAT3 dir = axes[axis];
        const XMFLOAT3 a = perpendicularA[axis];
        const XMFLOAT3 b = perpendicularB[axis];
        auto scaleVector = [](XMFLOAT3 v, float s) { return XMFLOAT3{v.x*s, v.y*s, v.z*s}; };
        auto add = [](XMFLOAT3 p, XMFLOAT3 q) { return XMFLOAT3{p.x+q.x,p.y+q.y,p.z+q.z}; };
        auto sub = [](XMFLOAT3 p, XMFLOAT3 q) { return XMFLOAT3{p.x-q.x,p.y-q.y,p.z-q.z}; };

        // Translation handle: shaft plus a small arrow head.
        m_gizmoMoveRanges[axis].start = static_cast<UINT>(vertices.size());
        addLine({0,0,0}, scaleVector(dir, 1.0f));
        const XMFLOAT3 tip = scaleVector(dir, 1.0f);
        const XMFLOAT3 back = scaleVector(dir, 0.82f);
        addLine(tip, add(back, scaleVector(a, 0.075f)));
        addLine(tip, sub(back, scaleVector(a, 0.075f)));
        addLine(tip, add(back, scaleVector(b, 0.075f)));
        addLine(tip, sub(back, scaleVector(b, 0.075f)));
        m_gizmoMoveRanges[axis].count = static_cast<UINT>(vertices.size()) - m_gizmoMoveRanges[axis].start;

        // Scale handle: a wireframe cube at the end of the axis.
        m_gizmoScaleRanges[axis].start = static_cast<UINT>(vertices.size());
        const float half = 0.055f;
        XMFLOAT3 corners[8]{};
        for (int mask = 0; mask < 8; ++mask)
        {
            XMFLOAT3 p = scaleVector(dir, 0.91f);
            p = add(p, scaleVector(a, (mask & 1) ? half : -half));
            p = add(p, scaleVector(b, (mask & 2) ? half : -half));
            p = add(p, scaleVector(dir, (mask & 4) ? half : -half));
            corners[mask] = p;
        }
        const int edges[12][2] = { {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7} };
        for (const auto& edge : edges) addLine(corners[edge[0]], corners[edge[1]]);
        m_gizmoScaleRanges[axis].count = static_cast<UINT>(vertices.size()) - m_gizmoScaleRanges[axis].start;

        // Rotation handle: a 48-segment ring around the matching world axis.
        m_gizmoRotateRanges[axis].start = static_cast<UINT>(vertices.size());
        constexpr int segments = 48;
        constexpr float radius = 0.82f;
        for (int i = 0; i < segments; ++i)
        {
            const float t0 = (static_cast<float>(i) / segments) * 2.0f * Pi;
            const float t1 = (static_cast<float>(i + 1) / segments) * 2.0f * Pi;
            const XMFLOAT3 p0 = add(scaleVector(a, std::cos(t0) * radius), scaleVector(b, std::sin(t0) * radius));
            const XMFLOAT3 p1 = add(scaleVector(a, std::cos(t1) * radius), scaleVector(b, std::sin(t1) * radius));
            addLine(p0, p1);
        }
        m_gizmoRotateRanges[axis].count = static_cast<UINT>(vertices.size()) - m_gizmoRotateRanges[axis].start;
        CreateVertexOnlyMesh(m_gizmoMeshes[axis], vertices);
    }
}

void ForgeEngine::CreateColliderWireMeshes()
{
    // Unit wireframes centered on the origin (-0.5..0.5). The per-object constant buffer
    // scales them by colliderSize and the object world matrix, so the marker grows with
    // the scale gizmo exactly like the physics radius computed in the C# runtime.
    auto addLine = [](std::vector<Vertex>& vertices, const XMFLOAT3& a, const XMFLOAT3& b)
    {
        vertices.push_back({ a, { 0.0f, 1.0f, 0.0f } });
        vertices.push_back({ b, { 0.0f, 1.0f, 0.0f } });
    };
    auto addRing = [&](std::vector<Vertex>& vertices, const XMFLOAT3& a, const XMFLOAT3& b,
        const XMFLOAT3& center, float radius, int segments)
    {
        for (int i = 0; i < segments; ++i)
        {
            const float t0 = (static_cast<float>(i) / segments) * 2.0f * Pi;
            const float t1 = (static_cast<float>(i + 1) / segments) * 2.0f * Pi;
            auto point = [&](float t)
            {
                return XMFLOAT3{
                    center.x + a.x * std::cos(t) * radius + b.x * std::sin(t) * radius,
                    center.y + a.y * std::cos(t) * radius + b.y * std::sin(t) * radius,
                    center.z + a.z * std::cos(t) * radius + b.z * std::sin(t) * radius };
            };
            addLine(vertices, point(t0), point(t1));
        }
    };

    std::vector<Vertex> box;
    XMFLOAT3 corners[8];
    for (int mask = 0; mask < 8; ++mask)
        corners[mask] = { (mask & 1) ? 0.5f : -0.5f, (mask & 2) ? 0.5f : -0.5f, (mask & 4) ? 0.5f : -0.5f };
    const int edges[12][2] = { {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7} };
    for (const auto& edge : edges) addLine(box, corners[edge[0]], corners[edge[1]]);
    CreateVertexOnlyMesh(m_colliderWireMeshes[0], box);

    std::vector<Vertex> sphere;
    addRing(sphere, { 1,0,0 }, { 0,1,0 }, { 0,0,0 }, 0.5f, 32);
    addRing(sphere, { 1,0,0 }, { 0,0,1 }, { 0,0,0 }, 0.5f, 32);
    addRing(sphere, { 0,1,0 }, { 0,0,1 }, { 0,0,0 }, 0.5f, 32);
    CreateVertexOnlyMesh(m_colliderWireMeshes[1], sphere);

    std::vector<Vertex> capsule;
    addRing(capsule, { 1,0,0 }, { 0,0,1 }, { 0, 0.25f, 0 }, 0.5f, 32);
    addRing(capsule, { 1,0,0 }, { 0,0,1 }, { 0,-0.25f, 0 }, 0.5f, 32);
    addRing(capsule, { 1,0,0 }, { 0,0,1 }, { 0, 0.00f, 0 }, 0.5f, 32);
    addLine(capsule, { 0.5f,-0.25f, 0 }, { 0.5f, 0.25f, 0 });
    addLine(capsule, {-0.5f,-0.25f, 0 }, {-0.5f, 0.25f, 0 });
    addLine(capsule, { 0,-0.25f, 0.5f }, { 0, 0.25f, 0.5f });
    addLine(capsule, { 0,-0.25f,-0.5f }, { 0, 0.25f,-0.5f });
    for (int i = 0; i < 8; ++i)
    {
        const float t0 = (static_cast<float>(i) / 8) * Pi;
        const float t1 = (static_cast<float>(i + 1) / 8) * Pi;
        addLine(capsule,
            { std::cos(t0) * 0.5f, 0.25f + std::sin(t0) * 0.5f, 0 },
            { std::cos(t1) * 0.5f, 0.25f + std::sin(t1) * 0.5f, 0 });
        addLine(capsule,
            { std::cos(t0 + Pi) * 0.5f, -0.25f + std::sin(t0 + Pi) * 0.5f, 0 },
            { std::cos(t1 + Pi) * 0.5f, -0.25f + std::sin(t1 + Pi) * 0.5f, 0 });
    }
    CreateVertexOnlyMesh(m_colliderWireMeshes[2], capsule);
}

void ForgeEngine::CreateConstantBuffers()
{
    for (UINT frame = 0; frame < FrameCount; ++frame)
    {
        CreateUploadResource(AlignConstantBufferSize(sizeof(FrameConstants)), m_frameConstants[frame]);
        for (auto& object : m_objects)
        {
            CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), object.objectConstants[frame]);
            CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), object.outlineConstants[frame]);
            CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), object.colliderConstants[frame]);
        }
        for (auto& axisFrame : m_axisConstants)
            CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), axisFrame[frame]);
        for (auto& gizmoFrame : m_gizmoConstants)
            CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), gizmoFrame[frame]);
    }
}

void ForgeEngine::CreateObjectConstantBuffers(GameObject& object)
{
    for (UINT frame = 0; frame < FrameCount; ++frame)
    {
        CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), object.objectConstants[frame]);
        CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), object.outlineConstants[frame]);
        CreateUploadResource(AlignConstantBufferSize(sizeof(ObjectConstants)), object.colliderConstants[frame]);
    }
}

void ForgeEngine::CreateEditorWindow(HINSTANCE instance)
{
    m_editorBackgroundBrush = CreateSolidBrush(RGB(22, 26, 35));
    m_editorEditBrush = CreateSolidBrush(RGB(35, 41, 53));
    m_editorFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    const wchar_t* className = L"ForgeEngineEditorPanelClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = EditorWindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = m_editorBackgroundBrush;
    wc.lpszClassName = className;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "Register editor panel class");

    RECT mainRect{};
    GetWindowRect(m_hwnd, &mainRect);
    const int panelWidth = 372;
    const int panelHeight = 720;
    int x = mainRect.right + 12;
    int y = std::max(20L, mainRect.top + 40L);
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    if (x + panelWidth > screenWidth) x = std::max(8L, mainRect.left + 70L);
    m_editorHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, className, L"Forge Engine Editor - MOVE",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, panelWidth, panelHeight, nullptr, nullptr, instance, this);
    if (!m_editorHwnd)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "Create editor panel window");

    auto add = [&](const wchar_t* klass, const wchar_t* text, DWORD style,
        int xPos, int yPos, int width, int height, int id, DWORD exStyle = 0) -> HWND
    {
        HWND control = CreateWindowExW(exStyle, klass, text, WS_CHILD | WS_VISIBLE | style,
            xPos, yPos, width, height, m_editorHwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            instance, nullptr);
        if (!control) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "Create editor control");
        if (m_editorFont) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(m_editorFont), TRUE);
        return control;
    };

    add(L"STATIC", L"SCENE HIERARCHY", SS_LEFT, 14, 12, 320, 22, -1);
    m_objectListHwnd = add(L"LISTBOX", L"", LBS_NOTIFY | LBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
        14, 36, 330, 118, IdObjectList, WS_EX_CLIENTEDGE);
    for (const GameObject& object : m_objects)
    {
        std::wstring name(object.name.begin(), object.name.end());
        SendMessageW(m_objectListHwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    }

    add(L"STATIC", L"TRANSFORM TOOL", SS_LEFT, 14, 164, 330, 20, -1);
    add(L"BUTTON", L"Move  [1]", BS_PUSHBUTTON | WS_TABSTOP, 14, 188, 100, 30, IdMoveMode);
    add(L"BUTTON", L"Rotate  [2]", BS_PUSHBUTTON | WS_TABSTOP, 126, 188, 100, 30, IdRotateMode);
    add(L"BUTTON", L"Scale  [3]", BS_PUSHBUTTON | WS_TABSTOP, 238, 188, 106, 30, IdScaleMode);

    add(L"STATIC", L"INSPECTOR  /  TRANSFORM", SS_LEFT, 14, 230, 330, 20, -1);
    add(L"STATIC", L"Position (X / Y / Z)", SS_LEFT, 14, 254, 330, 18, -1);
    m_positionEdits[0] = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 14, 275, 102, 26, 201, WS_EX_CLIENTEDGE);
    m_positionEdits[1] = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 128, 275, 102, 26, 202, WS_EX_CLIENTEDGE);
    m_positionEdits[2] = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 242, 275, 102, 26, 203, WS_EX_CLIENTEDGE);

    add(L"STATIC", L"Rotation in degrees (X / Y / Z)", SS_LEFT, 14, 309, 330, 18, -1);
    m_rotationEdits[0] = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 14, 330, 102, 26, 211, WS_EX_CLIENTEDGE);
    m_rotationEdits[1] = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 128, 330, 102, 26, 212, WS_EX_CLIENTEDGE);
    m_rotationEdits[2] = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 242, 330, 102, 26, 213, WS_EX_CLIENTEDGE);

    add(L"STATIC", L"Scale (X / Y / Z)", SS_LEFT, 14, 364, 330, 18, -1);
    m_scaleEdits[0] = add(L"EDIT", L"1.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 14, 385, 102, 26, 221, WS_EX_CLIENTEDGE);
    m_scaleEdits[1] = add(L"EDIT", L"1.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 128, 385, 102, 26, 222, WS_EX_CLIENTEDGE);
    m_scaleEdits[2] = add(L"EDIT", L"1.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 242, 385, 102, 26, 223, WS_EX_CLIENTEDGE);

    add(L"STATIC", L"PBR MATERIAL", SS_LEFT, 14, 423, 330, 20, -1);
    add(L"STATIC", L"Metallic  (0-1)", SS_LEFT, 14, 447, 145, 18, -1);
    add(L"STATIC", L"Roughness  (0-1)", SS_LEFT, 190, 447, 154, 18, -1);
    m_metallicEdit = add(L"EDIT", L"0.000", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 14, 468, 154, 26, IdMetallicEdit, WS_EX_CLIENTEDGE);
    m_roughnessEdit = add(L"EDIT", L"0.500", ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, 190, 468, 154, 26, IdRoughnessEdit, WS_EX_CLIENTEDGE);

    add(L"BUTTON", L"Apply Inspector Values", BS_PUSHBUTTON | WS_TABSTOP, 14, 507, 330, 32, IdApplyTransform);
    add(L"BUTTON", L"Reset Selected Transform", BS_PUSHBUTTON | WS_TABSTOP, 14, 546, 330, 32, IdResetTransform);
    add(L"STATIC", L"Viewport controls", SS_LEFT, 14, 593, 330, 20, -1);
    add(L"STATIC", L"RMB + mouse: look   |   WASD: move", SS_LEFT, 14, 616, 330, 20, -1);
    add(L"STATIC", L"Q/E: vertical   |   LMB: select/drag gizmo", SS_LEFT, 14, 638, 330, 20, -1);
    UpdateEditorInspector();
}

void ForgeEngine::UpdateEditorInspector()
{
    if (m_editorHwnd && m_objectListHwnd)
    {
        SendMessageW(m_objectListHwnd, LB_SETCURSEL, (m_selectedObject >= 0 ? m_selectedObject : -1), 0);
        const bool valid = m_selectedObject >= 0 && m_selectedObject < static_cast<int>(m_objects.size());
        auto setFloat = [](HWND edit, float value)
        {
            if (!edit) return;
            wchar_t buffer[64]{};
            swprintf_s(buffer, _countof(buffer), L"%.3f", value);
            SetWindowTextW(edit, buffer);
            EnableWindow(edit, TRUE);
        };
        auto setVector = [&](const std::array<HWND,3>& edits, XMFLOAT3 value)
        {
            if (!valid)
            {
                for (HWND edit : edits) if (edit) SetWindowTextW(edit, L"");
                return;
            }
            setFloat(edits[0], value.x); setFloat(edits[1], value.y); setFloat(edits[2], value.z);
        };
        if (valid)
        {
            const GameObject& object = m_objects[static_cast<size_t>(m_selectedObject)];
            setVector(m_positionEdits, object.position);
            setVector(m_rotationEdits, { object.rotation.x * 180.0f / Pi, object.rotation.y * 180.0f / Pi, object.rotation.z * 180.0f / Pi });
            setVector(m_scaleEdits, object.scale);
            setFloat(m_metallicEdit, object.metallic);
            setFloat(m_roughnessEdit, object.roughness);
        }
        else
        {
            setVector(m_positionEdits, {}); setVector(m_rotationEdits, {}); setVector(m_scaleEdits, {});
            SetWindowTextW(m_metallicEdit, L""); SetWindowTextW(m_roughnessEdit, L"");
        }
        for (HWND edit : m_positionEdits) if (edit) EnableWindow(edit, valid);
        for (HWND edit : m_rotationEdits) if (edit) EnableWindow(edit, valid);
        for (HWND edit : m_scaleEdits) if (edit) EnableWindow(edit, valid);
        if (m_metallicEdit) EnableWindow(m_metallicEdit, valid);
        if (m_roughnessEdit) EnableWindow(m_roughnessEdit, valid);
        EnableWindow(GetDlgItem(m_editorHwnd, IdApplyTransform), valid);
        EnableWindow(GetDlgItem(m_editorHwnd, IdResetTransform), valid);
    }
}

bool ForgeEngine::HandleEditorMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    switch (message)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == IdObjectList && HIWORD(wParam) == LBN_SELCHANGE)
        {
            const int index = static_cast<int>(SendMessageW(m_objectListHwnd, LB_GETCURSEL, 0, 0));
            SelectSceneObject(index == LB_ERR ? -1 : index);
            result = 0; return true;
        }
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IdMoveMode: SetGizmoMode(TransformMode::Translate); result = 0; return true;
            case IdRotateMode: SetGizmoMode(TransformMode::Rotate); result = 0; return true;
            case IdScaleMode: SetGizmoMode(TransformMode::Scale); result = 0; return true;
            case IdResetTransform:
                if (m_selectedObject >= 0 && m_selectedObject < static_cast<int>(m_objects.size()))
                {
                    GameObject& object = m_objects[static_cast<size_t>(m_selectedObject)];
                    object.position = {0,0,0};
                    object.rotation = {0,0,0};
                    object.scale = {1,1,1};
                    RebuildWorldTransform(object);
                    UpdateEditorInspector();
                }
                result = 0; return true;
            case IdApplyTransform:
                if (m_selectedObject >= 0 && m_selectedObject < static_cast<int>(m_objects.size()))
                {
                    auto readFloat = [](HWND edit, float fallback)
                    {
                        wchar_t text[96]{}; GetWindowTextW(edit, text, static_cast<int>(_countof(text)));
                        wchar_t* end = nullptr;
                        const float value = std::wcstof(text, &end);
                        return end == text || !std::isfinite(value) ? fallback : value;
                    };
                    GameObject& object = m_objects[static_cast<size_t>(m_selectedObject)];
                    object.position = { readFloat(m_positionEdits[0], object.position.x), readFloat(m_positionEdits[1], object.position.y), readFloat(m_positionEdits[2], object.position.z) };
                    object.rotation = { readFloat(m_rotationEdits[0], object.rotation.x*180.0f/Pi)*Pi/180.0f,
                        readFloat(m_rotationEdits[1], object.rotation.y*180.0f/Pi)*Pi/180.0f,
                        readFloat(m_rotationEdits[2], object.rotation.z*180.0f/Pi)*Pi/180.0f };
                    object.scale = { std::max(0.05f, readFloat(m_scaleEdits[0], object.scale.x)),
                        std::max(0.05f, readFloat(m_scaleEdits[1], object.scale.y)),
                        std::max(0.05f, readFloat(m_scaleEdits[2], object.scale.z)) };
                    object.metallic = std::clamp(readFloat(m_metallicEdit, object.metallic), 0.0f, 1.0f);
                    object.roughness = std::clamp(readFloat(m_roughnessEdit, object.roughness), 0.045f, 1.0f);
                    RebuildWorldTransform(object);
                    UpdateEditorInspector();
                }
                result = 0; return true;
            default: break;
            }
        }
        return false;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(226, 232, 240));
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(22, 26, 35));
        result = reinterpret_cast<LRESULT>(m_editorBackgroundBrush);
        return true;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(235, 240, 247));
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(35, 41, 53));
        result = reinterpret_cast<LRESULT>(m_editorEditBrush);
        return true;
    case WM_ERASEBKGND:
        if (m_editorBackgroundBrush)
        {
            RECT rect{}; GetClientRect(hwnd, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, m_editorBackgroundBrush);
            result = 1; return true;
        }
        return false;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        result = 0; return true;
    default:
        return false;
    }
}

LRESULT CALLBACK ForgeEngine::EditorWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ForgeEngine* engine = reinterpret_cast<ForgeEngine*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        engine = static_cast<ForgeEngine*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(engine));
    }
    if (engine)
    {
        LRESULT result = 0;
        if (engine->HandleEditorMessage(hwnd, message, wParam, lParam, result)) return result;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ForgeEngine::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    if (m_editorUI && hwnd == m_hwnd)
        m_editorUI->HandleWin32Message(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_SIZE:
        if (hwnd != m_hwnd) return false;
        if (wParam == SIZE_MINIMIZED) m_minimized = true;
        else
        {
            m_minimized = false;
            OnResize(static_cast<UINT>(LOWORD(lParam)), static_cast<UINT>(HIWORD(lParam)));
        }
        result = 0;
        return true;

    case WM_KEYDOWN:
        if (hwnd == m_hwnd && (!m_editorUI || !m_editorUI->WantsCaptureKeyboard()))
        {
            if (wParam == '1') { SetGizmoMode(TransformMode::Translate); result = 0; return true; }
            if (wParam == '2') { SetGizmoMode(TransformMode::Rotate); result = 0; return true; }
            if (wParam == '3') { SetGizmoMode(TransformMode::Scale); result = 0; return true; }
            if (wParam == 'G' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
            {
                m_editorOverlayEnabled = !m_editorOverlayEnabled;
                result = 0;
                return true;
            }
        }
        return false;

    case WM_LBUTTONDOWN:
        // Dear ImGui/ImGuizmo needs to process this input first. Object picking
        // is performed after NewFrame inside the central dockspace, when we know
        // whether the click belongs to a gizmo handle or an editor panel.
        result = 0;
        return hwnd == m_hwnd;

    case WM_LBUTTONUP:
        if (m_gizmoDragging)
        {
            m_gizmoDragging = false;
            m_gizmoAxis = -1;
            if (GetCapture() == hwnd) ReleaseCapture();
        }
        result = 0;
        return true;

    case WM_RBUTTONDOWN:
        if (hwnd == m_hwnd && !m_rightMouseDown)
        {
            const float x = static_cast<float>(GET_X_LPARAM(lParam));
            const float y = static_cast<float>(GET_Y_LPARAM(lParam));
            const bool insideScene = x >= m_sceneViewportX && y >= m_sceneViewportY &&
                x < m_sceneViewportX + m_sceneViewportWidth && y < m_sceneViewportY + m_sceneViewportHeight;
            const bool overEditorPanel = m_editorUI && m_editorUI->WantsCaptureMouse();
            if (insideScene && !overEditorPanel)
            {
                m_rightMouseDown = true;
                SetCapture(hwnd);
                SetCursorHidden(true);
            }
        }
        result = 0;
        return true;

    case WM_RBUTTONUP:
        if (hwnd == m_hwnd)
        {
            m_rightMouseDown = false;
            SetCursorHidden(false);
            if (GetCapture() == hwnd) ReleaseCapture();
        }
        result = 0;
        return true;

    case WM_INPUT:
        if (hwnd == m_hwnd && m_rightMouseDown)
        {
            RAWINPUT raw{};
            UINT size = sizeof(raw);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == size &&
                raw.header.dwType == RIM_TYPEMOUSE)
            {
                m_cameraYaw += static_cast<float>(raw.data.mouse.lLastX) * 0.0035f;
                m_cameraPitch -= static_cast<float>(raw.data.mouse.lLastY) * 0.0035f;
                m_cameraPitch = std::clamp(m_cameraPitch, -1.52f, 1.52f);
            }
            // DefWindowProc performs the required cleanup for foreground WM_INPUT packets.
            DefWindowProcW(hwnd, message, wParam, lParam);
            result = 0;
            return true;
        }
        return false;

    case WM_MOUSEMOVE:
        if (hwnd == m_hwnd && m_gizmoDragging && (wParam & MK_LBUTTON))
        {
            UpdateGizmoDrag(
                GET_X_LPARAM(lParam) - static_cast<int>(m_sceneViewportX),
                GET_Y_LPARAM(lParam) - static_cast<int>(m_sceneViewportY));
            result = 0;
            return true;
        }
        // Camera look uses raw mouse deltas while RMB is held; no cursor warping is needed.
        result = 0;
        return true;

    case WM_KILLFOCUS:
        if (hwnd == m_hwnd)
        {
            m_rightMouseDown = false;
            SetCursorHidden(false);
            m_gizmoDragging = false;
            m_gizmoAxis = -1;
            if (GetCapture() == hwnd) ReleaseCapture();
        }
        return false;

    case WM_CAPTURECHANGED:
        if (hwnd == m_hwnd && reinterpret_cast<HWND>(lParam) != hwnd)
        {
            if (m_rightMouseDown)
            {
                m_rightMouseDown = false;
                SetCursorHidden(false);
            }
            m_gizmoDragging = false;
            m_gizmoAxis = -1;
        }
        return false;

    case WM_SETCURSOR:
        if (hwnd == m_hwnd && m_rightMouseDown && LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(nullptr);
            result = TRUE;
            return true;
        }
        return false;

    case WM_DESTROY:
        if (hwnd == m_hwnd) PostQuitMessage(0);
        result = 0;
        return hwnd == m_hwnd;

    default:
        return false;
    }
}

void ForgeEngine::OnResize(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        return;
    m_pendingWidth = width;
    m_pendingHeight = height;
    m_resizePending = true;
}

void ForgeEngine::ResizeIfNeeded()
{
    if (!m_resizePending || m_minimized || !m_swapChain)
        return;

    WaitForGpu();
    for (auto& target : m_renderTargets)
        target.Reset();
    for (auto& target : m_hdrTargets)
        target.Reset();
    m_depthBuffer.Reset();

    m_width = m_pendingWidth;
    m_height = m_pendingHeight;
    ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, SwapChainFormat(), 0), "SwapChain::ResizeBuffers");
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    CreateSizeDependentResources();
    m_resizePending = false;
}

void ForgeEngine::UpdateCamera(float deltaSeconds)
{
    GameObject* viewportCamera = nullptr;
    if (m_viewportCameraObjectIndex >= 0 && m_viewportCameraObjectIndex < static_cast<int>(m_objects.size()) &&
        m_objects[static_cast<size_t>(m_viewportCameraObjectIndex)].isCamera)
        viewportCamera = &m_objects[static_cast<size_t>(m_viewportCameraObjectIndex)];
    else
        m_viewportCameraObjectIndex = -1;

    if (viewportCamera)
    {
        m_cameraPosition = viewportCamera->position;
        // Let Inspector/ImGuizmo edits drive the active camera when navigation is idle.
        // While RMB is held, raw input owns yaw/pitch so it is not overwritten each frame.
        if (!m_rightMouseDown)
        {
            m_cameraYaw = viewportCamera->rotation.y;
            m_cameraPitch = std::clamp(-viewportCamera->rotation.x, -1.52f, 1.52f);
        }
    }

    const float sinYaw = std::sin(m_cameraYaw), cosYaw = std::cos(m_cameraYaw);
    const float sinPitch = std::sin(m_cameraPitch), cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 forwardDirection{ cosPitch * sinYaw, sinPitch, cosPitch * cosYaw };
    const XMFLOAT3 rightDirection{ cosYaw, 0.0f, -sinYaw };
    XMFLOAT3 movement{ 0.0f, 0.0f, 0.0f };
    auto isDown = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };

    // When camera-view is active, navigation moves the actual camera GameObject.
    if (GetForegroundWindow() == m_hwnd && m_rightMouseDown)
    {
        if (isDown('W')) { movement.x += forwardDirection.x; movement.y += forwardDirection.y; movement.z += forwardDirection.z; }
        if (isDown('S')) { movement.x -= forwardDirection.x; movement.y -= forwardDirection.y; movement.z -= forwardDirection.z; }
        if (isDown('D')) { movement.x += rightDirection.x; movement.z += rightDirection.z; }
        if (isDown('A')) { movement.x -= rightDirection.x; movement.z -= rightDirection.z; }
        if (isDown('E')) movement.y += 1.0f;
        if (isDown('Q')) movement.y -= 1.0f;

        XMVECTOR moveVector = XMLoadFloat3(&movement);
        const float moveLength = XMVectorGetX(XMVector3Length(moveVector));
        if (moveLength > 0.0001f)
        {
            moveVector = XMVector3Normalize(moveVector);
            float speed = m_cameraMoveSpeed;
            if (isDown(VK_SHIFT)) speed *= 3.0f;
            moveVector = XMVectorScale(moveVector, speed * std::max(deltaSeconds, 0.0f));
            XMVECTOR position = XMLoadFloat3(viewportCamera ? &viewportCamera->position : &m_cameraPosition);
            position = XMVectorAdd(position, moveVector);
            if (viewportCamera)
            {
                XMStoreFloat3(&viewportCamera->position, position);
                RebuildWorldTransform(*viewportCamera);
                m_cameraPosition = viewportCamera->position;
            }
            else XMStoreFloat3(&m_cameraPosition, position);
        }
    }

    XMMATRIX view{};
    float fovRadians = std::clamp(m_editorCameraFov, 20.0f, 110.0f) * Pi / 180.0f;
    float nearClip = 0.05f;
    float farClip = 1200.0f;
    if (viewportCamera)
    {
        // WM_INPUT adjusts yaw/pitch; write those changes into the selected camera component.
        viewportCamera->rotation.x = -m_cameraPitch;
        viewportCamera->rotation.y = m_cameraYaw;
        RebuildWorldTransform(*viewportCamera);
        const XMMATRIX cameraWorld = XMLoadFloat4x4(&viewportCamera->world);
        const XMVECTOR forward = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), cameraWorld));
        const XMVECTOR up = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), cameraWorld));
        view = XMMatrixLookToLH(XMLoadFloat3(&viewportCamera->position), forward, up);
        m_cameraPosition = viewportCamera->position;
        fovRadians = std::clamp(viewportCamera->cameraFieldOfView, 20.0f, 110.0f) * Pi / 180.0f;
        nearClip = std::clamp(viewportCamera->cameraNearClip, 0.01f, 100.0f);
        farClip = std::max(nearClip + 0.1f, std::clamp(viewportCamera->cameraFarClip, nearClip + 0.1f, 100000.0f));
        m_editorCameraFov = viewportCamera->cameraFieldOfView;
    }
    else
    {
        const XMVECTOR position = XMLoadFloat3(&m_cameraPosition);
        const XMVECTOR direction = XMVectorSet(forwardDirection.x, forwardDirection.y, forwardDirection.z, 0.0f);
        view = XMMatrixLookToLH(position, direction, XMVectorSet(0, 1, 0, 0));
    }

    const float sceneWidth = std::max(1.0f, m_sceneViewportWidth);
    const float sceneHeight = std::max(1.0f, m_sceneViewportHeight);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(fovRadians, sceneWidth / sceneHeight, nearClip, farClip);
    const XMMATRIX viewProjection = view * projection;
    const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, viewProjection);
    XMStoreFloat4x4(&m_view, view);
    XMStoreFloat4x4(&m_projection, projection);
    XMStoreFloat4x4(&m_viewProjection, viewProjection);
    XMStoreFloat4x4(&m_inverseViewProjection, inverseViewProjection);
}

bool ForgeEngine::BuildMouseRay(int mouseX, int mouseY, XMFLOAT3& origin, XMFLOAT3& direction) const
{
    const int sceneWidth = static_cast<int>(std::max(1.0f, m_sceneViewportWidth));
    const int sceneHeight = static_cast<int>(std::max(1.0f, m_sceneViewportHeight));
    if (sceneWidth <= 0 || sceneHeight <= 0)
        return false;

    // Pixel-center sampling removes half-pixel bias at high-DPI and fractional viewport sizes.
    const float ndcX = ((static_cast<float>(mouseX) + 0.5f) / static_cast<float>(sceneWidth)) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((static_cast<float>(mouseY) + 0.5f) / static_cast<float>(sceneHeight)) * 2.0f;
    const XMMATRIX inverseViewProjection = XMLoadFloat4x4(&m_inverseViewProjection);
    const XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverseViewProjection);
    const XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverseViewProjection);
    XMVECTOR cameraOrigin = XMLoadFloat3(&m_cameraPosition);
    // A near-to-far direction is more numerically stable than treating the unprojected
    // far position as a direction, but still starts at the actual camera eye for picking.
    XMVECTOR ray = XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint));
    // Perspective projection rays share the eye as origin; orthographic mode isn't used
    // by this viewport but keeping the origin explicit makes that contract clear.
    (void)nearPoint;
    XMStoreFloat3(&origin, cameraOrigin);
    XMStoreFloat3(&direction, ray);
    return true;
}

void ForgeEngine::PickObject(int mouseX, int mouseY)
{
    XMFLOAT3 rayOrigin{}, rayDirection{};
    if (!BuildMouseRay(mouseX, mouseY, rayOrigin, rayDirection))
    {
        SelectSceneObject(-1);
        return;
    }

    float closestDistance = FLT_MAX;
    int closestObject = -1;
    const XMVECTOR worldOrigin = XMLoadFloat3(&rayOrigin);
    const XMVECTOR worldDirection = XMLoadFloat3(&rayDirection);
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        const GameObject& object = m_objects[i];
        if (!object.meshRendererEnabled && !object.isCamera) continue;
        const XMMATRIX inverseWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&object.world));
        const XMVECTOR localOriginV = XMVector3TransformCoord(worldOrigin, inverseWorld);
        const XMVECTOR localDirectionV = XMVector3TransformNormal(worldDirection, inverseWorld);
        XMFLOAT3 localOrigin{}, localDirection{};
        XMStoreFloat3(&localOrigin, localOriginV);
        XMStoreFloat3(&localDirection, localDirectionV);
        float hitDistance = FLT_MAX;
        const std::vector<Vertex>* vertices = object.mesh ? &object.mesh->cpuVertices : nullptr;
        const std::vector<std::uint32_t>* indices = object.mesh ? &object.mesh->cpuIndices : nullptr;
        if (object.isTerrain && !object.terrainVertices.empty() && !object.terrainIndices.empty())
        { vertices = &object.terrainVertices; indices = &object.terrainIndices; }

        if (vertices && indices && !vertices->empty() && indices->size() >= 3)
        {
            // Exact two-sided Möller–Trumbore ray/triangle tests. Direction remains
            // unnormalized in local space, so hit t still orders hits by world distance.
            for (size_t tri = 0; tri + 2 < indices->size(); tri += 3)
            {
                const std::uint32_t ia = (*indices)[tri], ib = (*indices)[tri + 1], ic = (*indices)[tri + 2];
                if (ia >= vertices->size() || ib >= vertices->size() || ic >= vertices->size()) continue;
                const XMFLOAT3& a = (*vertices)[ia].position;
                const XMFLOAT3& b = (*vertices)[ib].position;
                const XMFLOAT3& c = (*vertices)[ic].position;
                const XMFLOAT3 e1{ b.x-a.x, b.y-a.y, b.z-a.z };
                const XMFLOAT3 e2{ c.x-a.x, c.y-a.y, c.z-a.z };
                const XMFLOAT3 pvec{ localDirection.y*e2.z-localDirection.z*e2.y,
                    localDirection.z*e2.x-localDirection.x*e2.z,
                    localDirection.x*e2.y-localDirection.y*e2.x };
                const float det = e1.x*pvec.x + e1.y*pvec.y + e1.z*pvec.z;
                if (std::abs(det) < 1.0e-8f) continue;
                const float invDet = 1.0f / det;
                const XMFLOAT3 tvec{ localOrigin.x-a.x, localOrigin.y-a.y, localOrigin.z-a.z };
                const float u = (tvec.x*pvec.x + tvec.y*pvec.y + tvec.z*pvec.z) * invDet;
                if (u < 0.0f || u > 1.0f) continue;
                const XMFLOAT3 qvec{ tvec.y*e1.z-tvec.z*e1.y,
                    tvec.z*e1.x-tvec.x*e1.z,
                    tvec.x*e1.y-tvec.y*e1.x };
                const float v = (localDirection.x*qvec.x + localDirection.y*qvec.y + localDirection.z*qvec.z) * invDet;
                if (v < 0.0f || u + v > 1.0f) continue;
                const float t = (e2.x*qvec.x + e2.y*qvec.y + e2.z*qvec.z) * invDet;
                if (t >= 0.0f && t < hitDistance) hitDistance = t;
            }
        }
        else
        {
            // Selection volume for cameras/editor helpers without triangle geometry.
            float tMin = 0.0f, tMax = FLT_MAX;
            const float o[3] = { localOrigin.x, localOrigin.y, localOrigin.z };
            const float d[3] = { localDirection.x, localDirection.y, localDirection.z };
            const float radius = object.isCamera ? 0.25f : 1.0f;
            const float bMin[3] = { -radius, -radius, -radius }, bMax[3] = { radius, radius, radius };
            bool hit = true;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::abs(d[axis]) < 1.0e-8f) { if (o[axis] < bMin[axis] || o[axis] > bMax[axis]) { hit = false; break; } continue; }
                float t0 = (bMin[axis] - o[axis]) / d[axis], t1 = (bMax[axis] - o[axis]) / d[axis];
                if (t0 > t1) std::swap(t0, t1);
                tMin = std::max(tMin, t0); tMax = std::min(tMax, t1);
                if (tMin > tMax) { hit = false; break; }
            }
            if (hit) hitDistance = tMin >= 0.0f ? tMin : tMax;
        }
        if (hitDistance >= 0.0f && hitDistance < closestDistance)
        { closestDistance = hitDistance; closestObject = static_cast<int>(i); }
    }
    SelectSceneObject(closestObject);
}

void ForgeEngine::SelectSceneObject(int index)
{
    if (index < -1 || index >= static_cast<int>(m_objects.size())) index = -1;
    const bool wasFollowingCamera = m_viewportCameraObjectIndex >= 0;
    m_selectedObject = index;
    // Unreal-style behavior: selecting a Camera immediately drives the Scene viewport
    // from that camera; selecting anything else returns to the free editor camera.
    if (index >= 0 && m_objects[static_cast<size_t>(index)].isCamera)
    {
        GameObject& camera = m_objects[static_cast<size_t>(index)];
        m_viewportCameraObjectIndex = index;
        m_cameraPosition = camera.position;
        m_cameraYaw = camera.rotation.y;
        m_cameraPitch = std::clamp(-camera.rotation.x, -1.50f, 1.50f);
        m_editorCameraFov = std::clamp(camera.cameraFieldOfView, 20.0f, 110.0f);
        RebuildWorldTransform(camera);
        UpdateCamera(0.0f);
    }
    else if (wasFollowingCamera)
    {
        m_viewportCameraObjectIndex = -1;
        m_editorCameraFov = 60.0f;
        UpdateCamera(0.0f);
    }
    UpdateEditorInspector();
}

void ForgeEngine::SetGizmoMode(TransformMode mode)
{
    m_gizmoMode = mode;
    if (m_editorHwnd)
    {
        const wchar_t* modeName = mode == TransformMode::Translate ? L"MOVE" :
            (mode == TransformMode::Rotate ? L"ROTATE" : L"SCALE");
        std::wstring title = L"Forge Engine Editor - ";
        title += modeName;
        SetWindowTextW(m_editorHwnd, title.c_str());
    }
}

void ForgeEngine::SetCursorHidden(bool hidden)
{
    if (hidden == m_cursorHidden) return;
    if (hidden)
    {
        m_cursorHideCallCount = 0;
        // ShowCursor uses a process-wide display counter. Balance every decrement on release.
        int displayCount = 0;
        do
        {
            displayCount = ShowCursor(FALSE);
            ++m_cursorHideCallCount;
        } while (displayCount >= 0 && m_cursorHideCallCount < 32);
        m_cursorHidden = true;
        SetCursor(nullptr);
    }
    else
    {
        for (int i = 0; i < m_cursorHideCallCount; ++i) ShowCursor(TRUE);
        m_cursorHideCallCount = 0;
        m_cursorHidden = false;
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
}

float ForgeEngine::GetGizmoLength() const
{
    if (m_selectedObject < 0 || m_selectedObject >= static_cast<int>(m_objects.size())) return 1.5f;
    const XMFLOAT3& p = m_objects[static_cast<size_t>(m_selectedObject)].position;
    const float dx = m_cameraPosition.x - p.x;
    const float dy = m_cameraPosition.y - p.y;
    const float dz = m_cameraPosition.z - p.z;
    const float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    return std::clamp(distance * 0.12f, 1.2f, 5.0f);
}

float ForgeEngine::RayAxisParameter(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDirection,
    const XMFLOAT3& axisOrigin, const XMFLOAT3& axisDirection, float segmentLength, float& separation) const
{
    const XMFLOAT3 w{ rayOrigin.x-axisOrigin.x, rayOrigin.y-axisOrigin.y, rayOrigin.z-axisOrigin.z };
    const float a = rayDirection.x*rayDirection.x + rayDirection.y*rayDirection.y + rayDirection.z*rayDirection.z;
    const float b = rayDirection.x*axisDirection.x + rayDirection.y*axisDirection.y + rayDirection.z*axisDirection.z;
    const float c = axisDirection.x*axisDirection.x + axisDirection.y*axisDirection.y + axisDirection.z*axisDirection.z;
    const float d = rayDirection.x*w.x + rayDirection.y*w.y + rayDirection.z*w.z;
    const float e = axisDirection.x*w.x + axisDirection.y*w.y + axisDirection.z*w.z;
    const float denominator = a*c - b*b;
    float axisT = denominator > 1.0e-6f ? (a*e - b*d) / denominator : 0.0f;
    axisT = std::clamp(axisT, 0.0f, segmentLength);
    const XMFLOAT3 axisPoint{ axisOrigin.x + axisDirection.x*axisT,
        axisOrigin.y + axisDirection.y*axisT, axisOrigin.z + axisDirection.z*axisT };
    const float rayT = std::max(0.0f,
        (axisPoint.x-rayOrigin.x)*rayDirection.x +
        (axisPoint.y-rayOrigin.y)*rayDirection.y +
        (axisPoint.z-rayOrigin.z)*rayDirection.z);
    const XMFLOAT3 rayPoint{ rayOrigin.x+rayDirection.x*rayT,
        rayOrigin.y+rayDirection.y*rayT, rayOrigin.z+rayDirection.z*rayT };
    const float dx = rayPoint.x-axisPoint.x;
    const float dy = rayPoint.y-axisPoint.y;
    const float dz = rayPoint.z-axisPoint.z;
    separation = std::sqrt(dx*dx + dy*dy + dz*dz);
    return axisT;
}

bool ForgeEngine::IntersectGizmoRing(int axis, const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDirection,
    XMFLOAT3& radial, float& angle) const
{
    if (m_selectedObject < 0 || m_selectedObject >= static_cast<int>(m_objects.size()) || axis < 0 || axis > 2)
        return false;
    const GameObject& object = m_objects[static_cast<size_t>(m_selectedObject)];
    const XMFLOAT3 normal = axis == 0 ? XMFLOAT3{1,0,0} : axis == 1 ? XMFLOAT3{0,1,0} : XMFLOAT3{0,0,1};
    const float denominator = rayDirection.x*normal.x + rayDirection.y*normal.y + rayDirection.z*normal.z;
    if (std::abs(denominator) < 1.0e-5f) return false;
    const XMFLOAT3 delta{ object.position.x-rayOrigin.x, object.position.y-rayOrigin.y, object.position.z-rayOrigin.z };
    const float t = (delta.x*normal.x + delta.y*normal.y + delta.z*normal.z) / denominator;
    if (t <= 0.0f) return false;
    const XMFLOAT3 hit{ rayOrigin.x+rayDirection.x*t, rayOrigin.y+rayDirection.y*t, rayOrigin.z+rayDirection.z*t };
    radial = { hit.x-object.position.x, hit.y-object.position.y, hit.z-object.position.z };
    if (axis == 0) angle = std::atan2(radial.z, radial.y);
    else if (axis == 1) angle = std::atan2(radial.z, radial.x);
    else angle = std::atan2(radial.y, radial.x);
    return true;
}

bool ForgeEngine::TryBeginGizmoDrag(int mouseX, int mouseY, HWND hwnd)
{
    if (m_selectedObject < 0 || m_selectedObject >= static_cast<int>(m_objects.size())) return false;
    XMFLOAT3 rayOrigin{}, rayDirection{};
    if (!BuildMouseRay(mouseX, mouseY, rayOrigin, rayDirection)) return false;

    GameObject& object = m_objects[static_cast<size_t>(m_selectedObject)];
    const XMFLOAT3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    const float length = GetGizmoLength();
    float bestScore = FLT_MAX;
    int bestAxis = -1;
    float bestParameter = 0.0f;
    float bestAngle = 0.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (m_gizmoMode == TransformMode::Rotate)
        {
            XMFLOAT3 radial{};
            float angle = 0.0f;
            if (!IntersectGizmoRing(axis, rayOrigin, rayDirection, radial, angle)) continue;
            const float radius = std::sqrt(radial.x*radial.x + radial.y*radial.y + radial.z*radial.z);
            const float error = std::abs(radius - length * 0.82f);
            if (error <= length * 0.105f && error < bestScore)
            {
                bestScore = error;
                bestAxis = axis;
                bestAngle = angle;
            }
        }
        else
        {
            float separation = FLT_MAX;
            const float parameter = RayAxisParameter(rayOrigin, rayDirection, object.position, axes[axis], length, separation);
            const float tolerance = length * 0.075f;
            const bool onHandle = m_gizmoMode == TransformMode::Translate
                ? (parameter > length * 0.10f && separation <= tolerance)
                : (parameter >= length * 0.79f && separation <= tolerance * 1.9f);
            if (onHandle && separation < bestScore)
            {
                bestScore = separation;
                bestAxis = axis;
                bestParameter = parameter;
            }
        }
    }

    if (bestAxis < 0) return false;
    m_gizmoDragging = true;
    m_gizmoAxis = bestAxis;
    m_dragStartPosition = object.position;
    m_dragStartRotation = object.rotation;
    m_dragStartScale = object.scale;
    m_gizmoLength = length;
    m_dragStartAngle = bestAngle;
    if (m_gizmoMode == TransformMode::Rotate)
    {
        m_dragStartAxisParameter = 0.0f;
    }
    else
    {
        // Extend the axis in both directions during the drag so motion can cross the origin.
        m_dragStartAxisParameter = 1000.0f + bestParameter;
    }
    SetCapture(hwnd);
    return true;
}

void ForgeEngine::UpdateGizmoDrag(int mouseX, int mouseY)
{
    if (!m_gizmoDragging || m_gizmoAxis < 0 || m_selectedObject < 0 ||
        m_selectedObject >= static_cast<int>(m_objects.size())) return;
    XMFLOAT3 rayOrigin{}, rayDirection{};
    if (!BuildMouseRay(mouseX, mouseY, rayOrigin, rayDirection)) return;
    GameObject& object = m_objects[static_cast<size_t>(m_selectedObject)];
    const XMFLOAT3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    const XMFLOAT3 axis = axes[m_gizmoAxis];

    if (m_gizmoMode == TransformMode::Rotate)
    {
        XMFLOAT3 radial{};
        float angle = 0.0f;
        if (!IntersectGizmoRing(m_gizmoAxis, rayOrigin, rayDirection, radial, angle)) return;
        float delta = angle - m_dragStartAngle;
        while (delta > Pi) delta -= 2.0f * Pi;
        while (delta < -Pi) delta += 2.0f * Pi;
        object.rotation = m_dragStartRotation;
        if (m_gizmoAxis == 0) object.rotation.x += delta;
        else if (m_gizmoAxis == 1) object.rotation.y += delta;
        else object.rotation.z += delta;
    }
    else
    {
        const XMFLOAT3 extendedOrigin{ m_dragStartPosition.x-axis.x*1000.0f,
            m_dragStartPosition.y-axis.y*1000.0f, m_dragStartPosition.z-axis.z*1000.0f };
        float separation = FLT_MAX;
        const float parameter = RayAxisParameter(rayOrigin, rayDirection, extendedOrigin, axis, 2000.0f, separation);
        const float delta = parameter - m_dragStartAxisParameter;
        if (m_gizmoMode == TransformMode::Translate)
        {
            object.position = { m_dragStartPosition.x + axis.x*delta,
                m_dragStartPosition.y + axis.y*delta, m_dragStartPosition.z + axis.z*delta };
        }
        else
        {
            object.scale = m_dragStartScale;
            const float factor = delta / std::max(m_gizmoLength, 0.001f);
            if (m_gizmoAxis == 0) object.scale.x = std::max(0.05f, m_dragStartScale.x + factor);
            else if (m_gizmoAxis == 1) object.scale.y = std::max(0.05f, m_dragStartScale.y + factor);
            else object.scale.z = std::max(0.05f, m_dragStartScale.z + factor);
        }
    }
    RebuildWorldTransform(object);
    UpdateEditorInspector();
}

void ForgeEngine::ProcessPendingSelection()
{
    if (!m_clickPending)
        return;
    PickObject(m_clickX, m_clickY);
    m_clickPending = false;
}

void ForgeEngine::UpdateConstantBuffers(float totalSeconds)
{
    const XMVECTOR cameraPos = XMLoadFloat3(&m_cameraPosition);
    const auto safeNormalize = [](const XMFLOAT3& value, const XMFLOAT3& fallback)
    {
        const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
        return lengthSquared > 0.0001f ? Normalize3(value) : fallback;
    };
    const auto smoothStep = [](float edge0, float edge1, float value)
    {
        const float t = std::clamp((value - edge0) / std::max(edge1 - edge0, 0.0001f), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    float hours = std::fmod(m_timeOfDayHours, 24.0f);
    if (hours < 0.0f) hours += 24.0f;

    // Pick the selected directional light when possible, otherwise use the latest one.
    GameObject* directionalObject = nullptr;
    bool hasPointObjects = false;
    bool hasSpotObjects = false;
    for (GameObject& object : m_objects)
    {
        if (object.lightType == LightType::Directional)
        {
            directionalObject = &object;
        }
        else if (object.lightType == LightType::Point) hasPointObjects = true;
        else if (object.lightType == LightType::Spot) hasSpotObjects = true;
    }
    if (m_selectedObject >= 0 && m_selectedObject < static_cast<int>(m_objects.size()) &&
        m_objects[static_cast<size_t>(m_selectedObject)].lightType == LightType::Directional)
        directionalObject = &m_objects[static_cast<size_t>(m_selectedObject)];

    XMFLOAT3 directionalColor = m_directionalLightColor;
    float directionalIntensity = m_directionalLightIntensity;
    bool directionalEnabled = m_directionalLightEnabled;
    if (directionalObject != nullptr)
    {
        directionalColor = directionalObject->lightColor;
        directionalIntensity = directionalObject->lightIntensity;
        directionalEnabled = directionalObject->lightEnabled;
    }

    XMFLOAT3 lightDirection = safeNormalize(m_directionalLightDirection,
        XMFLOAT3{ 0.42f, -1.0f, 0.28f });
    if (m_dayNightCycleEnabled || m_timeOfDayDrivesSun)
    {
        // 06:00 sunrise, 12:00 noon, 18:00 sunset, 00:00 midnight.
        const float angle = (hours - 6.0f) * Pi / 12.0f;
        const XMFLOAT3 sunDirection = safeNormalize(
            XMFLOAT3{ std::cos(angle), std::sin(angle), 0.12f }, XMFLOAT3{ 0.0f, 1.0f, 0.0f });
        lightDirection = { -sunDirection.x, -sunDirection.y, -sunDirection.z };
    }
    else if (m_directionalObjectTransformDrivesSun && directionalObject != nullptr)
    {
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
            directionalObject->rotation.x, directionalObject->rotation.y, directionalObject->rotation.z);
        const XMFLOAT3 localDown{ 0.0f, -1.0f, 0.0f };
        XMFLOAT3 transformed{};
        XMStoreFloat3(&transformed, XMVector3TransformNormal(XMLoadFloat3(&localDown), rotation));
        lightDirection = safeNormalize(transformed, XMFLOAT3{ 0.0f, -1.0f, 0.0f });
    }

    const float sunElevation = std::clamp(-lightDirection.y, -1.0f, 1.0f);
    const float geometricDaylight = smoothStep(-0.22f, 0.16f, sunElevation);
    const float brightnessDaylight = directionalEnabled ? std::clamp(directionalIntensity / 4.0f, 0.0f, 1.0f) : 0.0f;
    const float daylight = geometricDaylight * brightnessDaylight;
    const float effectiveDirectionalIntensity = directionalEnabled
        ? std::max(directionalIntensity, 0.0f) * smoothStep(-0.18f, 0.06f, sunElevation) : 0.0f;

    XMFLOAT3 lightFocus{ m_cameraPosition.x, 0.0f, m_cameraPosition.z };
    XMFLOAT3 lightPosition{
        lightFocus.x - lightDirection.x * 70.0f,
        lightFocus.y - lightDirection.y * 70.0f,
        lightFocus.z - lightDirection.z * 70.0f
    };
    const XMFLOAT3 lightUp = std::abs(lightDirection.y) > 0.98f
        ? XMFLOAT3{ 0.0f, 0.0f, 1.0f } : XMFLOAT3{ 0.0f, 1.0f, 0.0f };
    XMMATRIX lightView = XMMatrixLookAtLH(XMLoadFloat3(&lightPosition), XMLoadFloat3(&lightFocus), XMLoadFloat3(&lightUp));
    XMMATRIX lightProjection = XMMatrixOrthographicLH(115.0f, 115.0f, 1.0f, 180.0f);
    XMMATRIX lightViewProjection = lightView * lightProjection;

    FrameConstants frame{};
    frame.viewProjection = m_viewProjection;
    frame.inverseViewProjection = m_inverseViewProjection;
    XMStoreFloat4x4(&frame.lightViewProjection, lightViewProjection);
    XMStoreFloat3(&frame.cameraPosition, cameraPos);
    frame.timeSeconds = totalSeconds;
    frame.lightDirection = lightDirection;
    frame.viewportWidth = std::max(1.0f, m_sceneViewportWidth);
    frame.lightColor = directionalColor;
    frame.viewportHeight = std::max(1.0f, m_sceneViewportHeight);
    frame.viewportOrigin = { m_sceneViewportX, m_sceneViewportY };
    frame.renderTargetSize = { static_cast<float>(m_width), static_cast<float>(m_height) };
    frame.dayNightParameters = { daylight, sunElevation, hours, m_dayNightCycleEnabled ? 1.0f : 0.0f };
    frame.postProcessParameters = m_postProcessParameters;
    frame.postProcessOptions = m_postProcessOptions;
    if (!m_havePostProcessCameraPosition)
    {
        m_lastPostProcessCameraPosition = m_cameraPosition;
        m_havePostProcessCameraPosition = true;
    }
    const float motionX = std::clamp((m_cameraPosition.x - m_lastPostProcessCameraPosition.x) * 0.0015f, -0.006f, 0.006f);
    const float motionY = std::clamp((m_lastPostProcessCameraPosition.z - m_cameraPosition.z) * 0.0015f, -0.006f, 0.006f);
    frame.postProcessMotion = { motionX, motionY, m_postProcessMotion.z, m_postProcessMotion.w };
    frame.skyParameters = { std::clamp(m_skyCloudCoverage, 0.0f, 1.0f),
        std::clamp(m_skyCloudAltitude, 0.05f, 0.60f), std::max(m_skyWindSpeed, 0.0f),
        m_editorOverlayEnabled ? 1.0f : 0.0f };
    m_lastPostProcessCameraPosition = m_cameraPosition;

    int pointCount = 0;
    for (const GameObject& object : m_objects)
    {
        if (object.lightType != LightType::Point || !object.lightEnabled || pointCount >= 4) continue;
        const size_t index = static_cast<size_t>(pointCount++);
        frame.pointLightPositionRange[index] = { object.position.x, object.position.y, object.position.z,
            std::max(object.lightRange, 0.1f) };
        frame.pointLightColorIntensity[index] = { object.lightColor.x, object.lightColor.y, object.lightColor.z,
            std::max(object.lightIntensity, 0.0f) };
    }
    if (!hasPointObjects && m_pointLightEnabled)
    {
        pointCount = 1;
        frame.pointLightPositionRange[0] = { m_pointLightPosition.x, m_pointLightPosition.y, m_pointLightPosition.z,
            std::max(m_pointLightRange, 0.1f) };
        frame.pointLightColorIntensity[0] = { m_pointLightColor.x, m_pointLightColor.y, m_pointLightColor.z,
            std::max(m_pointLightIntensity, 0.0f) };
    }

    int spotCount = 0;
    for (const GameObject& object : m_objects)
    {
        if (object.lightType != LightType::Spot || !object.lightEnabled || spotCount >= 4) continue;
        const size_t index = static_cast<size_t>(spotCount++);
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(object.rotation.x, object.rotation.y, object.rotation.z);
        const XMFLOAT3 localDown{ 0.0f, -1.0f, 0.0f };
        XMFLOAT3 direction{};
        XMStoreFloat3(&direction, XMVector3TransformNormal(XMLoadFloat3(&localDown), rotation));
        direction = safeNormalize(direction, XMFLOAT3{ 0.0f, -1.0f, 0.0f });
        const float outerAngle = std::clamp(object.lightOuterAngle, 2.0f, 89.0f);
        const float outerCos = std::cos(outerAngle * Pi / 180.0f);
        const float innerAngle = std::clamp(object.lightInnerAngle, 1.0f, outerAngle - 1.0f);
        const float innerCos = std::cos(innerAngle * Pi / 180.0f);
        frame.spotLightPositionRange[index] = { object.position.x, object.position.y, object.position.z,
            std::max(object.lightRange, 0.1f) };
        frame.spotLightDirectionOuterCos[index] = { direction.x, direction.y, direction.z, outerCos };
        frame.spotLightColorIntensity[index] = { object.lightColor.x, object.lightColor.y, object.lightColor.z,
            std::max(object.lightIntensity, 0.0f) };
        frame.spotLightConeParameters[index] = { innerCos, 0.0f, 0.0f, 0.0f };
    }
    if (!hasSpotObjects && m_spotLightEnabled)
    {
        spotCount = 1;
        const XMFLOAT3 direction = safeNormalize(m_spotLightDirection, XMFLOAT3{ 0.0f, -1.0f, 0.0f });
        const float outerAngle = std::clamp(m_spotLightOuterAngle, 2.0f, 89.0f);
        const float outerCos = std::cos(outerAngle * Pi / 180.0f);
        const float innerAngle = std::clamp(m_spotLightInnerAngle, 1.0f, outerAngle - 1.0f);
        frame.spotLightPositionRange[0] = { m_spotLightPosition.x, m_spotLightPosition.y, m_spotLightPosition.z,
            std::max(m_spotLightRange, 0.1f) };
        frame.spotLightDirectionOuterCos[0] = { direction.x, direction.y, direction.z, outerCos };
        frame.spotLightColorIntensity[0] = { m_spotLightColor.x, m_spotLightColor.y, m_spotLightColor.z,
            std::max(m_spotLightIntensity, 0.0f) };
        frame.spotLightConeParameters[0] = { std::cos(innerAngle * Pi / 180.0f), 0.0f, 0.0f, 0.0f };
    }
    frame.lightingParameters = { effectiveDirectionalIntensity, std::max(m_ambientLightIntensity, 0.0f),
        static_cast<float>(pointCount), static_cast<float>(spotCount) };

    auto upload = [](ID3D12Resource* resource, const void* data, SIZE_T size)
    {
        void* mapped = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        HRESULT hr = resource->Map(0, &readRange, &mapped);
        if (FAILED(hr))
            ForgeEngine::ThrowIfFailed(hr, "Map constant buffer");
        std::memcpy(mapped, data, size);
        D3D12_RANGE writtenRange{ 0, size };
        resource->Unmap(0, &writtenRange);
    };

    upload(m_frameConstants[m_frameIndex].Get(), &frame, sizeof(frame));
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        GameObject& object = m_objects[i];
        RebuildWorldTransform(object);
        ObjectConstants constants{};
        constants.world = object.world;
        XMStoreFloat4x4(&constants.normalTransform, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&object.world))));
        constants.baseColor = object.lightType == LightType::None
            ? object.baseColor
            : XMFLOAT4{ object.lightColor.x, object.lightColor.y, object.lightColor.z, 1.0f };
        constants.material = { object.metallic, object.roughness,
            object.albedoTexture ? 1.0f : 0.0f,
            object.isDragPreviewEmissive ? 2.0f : (object.lightType != LightType::None ? 1.0f : 0.0f) };
        constants.uvTransform = { object.textureTiling.x, object.textureTiling.y,
            object.textureOffset.x, object.textureOffset.y };
        constants.materialMaps = { object.normalTexture ? 1.0f : 0.0f,
            object.metallicRoughnessTexture ? 1.0f : 0.0f, object.aoTexture ? 1.0f : 0.0f,
            std::max(object.normalMapScale, 0.0f) };
        constants.emissive = { object.emissiveColor.x, object.emissiveColor.y, object.emissiveColor.z,
            object.isDragPreviewEmissive ? 1.35f : std::max(object.emissiveIntensity, 0.0f) };
        constants.materialFlags = { object.emissiveTexture ? 1.0f : 0.0f,
            std::clamp(object.occlusionStrength, 0.0f, 1.0f), 0.0f, 0.0f };
        upload(object.objectConstants[m_frameIndex].Get(), &constants, sizeof(constants));

        ObjectConstants outline = constants;
        XMMATRIX expandedWorld = XMMatrixScaling(1.045f, 1.045f, 1.045f) * XMLoadFloat4x4(&object.world);
        XMStoreFloat4x4(&outline.world, expandedWorld);
        upload(object.outlineConstants[m_frameIndex].Get(), &outline, sizeof(outline));

        if (object.hasCollider && object.colliderConstants[m_frameIndex])
        {
            ObjectConstants collider{};
            const XMMATRIX colliderWorld = XMLoadFloat4x4(&object.world) *
                XMMatrixScaling(object.colliderSize.x, object.colliderSize.y, object.colliderSize.z);
            XMStoreFloat4x4(&collider.world, colliderWorld);
            XMStoreFloat4x4(&collider.normalTransform, XMMatrixTranspose(XMMatrixInverse(nullptr, colliderWorld)));
            collider.baseColor = static_cast<int>(i) == m_selectedObject
                ? XMFLOAT4{ 1.0f, 0.72f, 0.12f, 1.0f }
                : (object.isTrigger ? XMFLOAT4{ 0.25f, 0.65f, 1.0f, 1.0f } : XMFLOAT4{ 0.20f, 0.95f, 0.45f, 1.0f });
            collider.material = { 0.0f, 0.5f, 0.0f, 0.0f };
            upload(object.colliderConstants[m_frameIndex].Get(), &collider, sizeof(collider));
        }
    }

    const XMFLOAT4 axisColors[3] = {
        { 1.0f, 0.12f, 0.10f, 1.0f },
        { 0.10f, 1.0f, 0.30f, 1.0f },
        { 0.12f, 0.48f, 1.0f, 1.0f }
    };
    for (UINT axis = 0; axis < 3; ++axis)
    {
        ObjectConstants constants{};
        XMStoreFloat4x4(&constants.world, XMMatrixIdentity());
        XMStoreFloat4x4(&constants.normalTransform, XMMatrixIdentity());
        constants.baseColor = axisColors[axis];
        constants.material = { 0.0f, 0.5f, 0.0f, 0.0f };
        upload(m_axisConstants[axis][m_frameIndex].Get(), &constants, sizeof(constants));
    }

    m_gizmoLength = GetGizmoLength();
    for (UINT axis = 0; axis < 3; ++axis)
    {
        ObjectConstants constants{};
        XMFLOAT3 position = m_selectedObject >= 0 && m_selectedObject < static_cast<int>(m_objects.size())
            ? m_objects[static_cast<size_t>(m_selectedObject)].position : XMFLOAT3{0,0,0};
        const XMMATRIX gizmoWorld = XMMatrixScaling(m_gizmoLength, m_gizmoLength, m_gizmoLength) *
            XMMatrixTranslation(position.x, position.y, position.z);
        XMStoreFloat4x4(&constants.world, gizmoWorld);
        XMStoreFloat4x4(&constants.normalTransform, XMMatrixTranspose(XMMatrixInverse(nullptr, gizmoWorld)));
        constants.baseColor = axisColors[axis];
        constants.material = { 0.0f, 0.5f, 0.0f, 0.0f };
        upload(m_gizmoConstants[axis][m_frameIndex].Get(), &constants, sizeof(constants));
    }
}

void ForgeEngine::DrawMesh(const Mesh& mesh)
{
    m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexView);
    if (mesh.indexCount > 0)
    {
        m_commandList->IASetIndexBuffer(&mesh.indexView);
        m_commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }
    else
    {
        m_commandList->DrawInstanced(mesh.vertexCount, 1, 0, 0);
    }
}

void ForgeEngine::DrawShadowPass(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER toDepth = TransitionBarrier(m_shadowMap.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_commandList->ResourceBarrier(1, &toDepth);

    D3D12_VIEWPORT shadowViewport{ 0.0f, 0.0f, static_cast<float>(ShadowMapSize), static_cast<float>(ShadowMapSize), 0.0f, 1.0f };
    D3D12_RECT shadowScissor{ 0, 0, static_cast<LONG>(ShadowMapSize), static_cast<LONG>(ShadowMapSize) };
    m_commandList->RSSetViewports(1, &shadowViewport);
    m_commandList->RSSetScissorRects(1, &shadowScissor);
    m_commandList->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowDsvHandle);
    m_commandList->ClearDepthStencilView(m_shadowDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetPipelineState(m_shadowPipeline.Get());
    m_commandList->SetGraphicsRootConstantBufferView(0, m_frameConstants[frameIndex]->GetGPUVirtualAddress());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const GameObject& object : m_objects)
    {
        if (!object.meshRendererEnabled || !object.castShadows || object.mesh == nullptr ||
            object.lightType != LightType::None || object.isDragPreview) continue;
        m_commandList->SetGraphicsRootConstantBufferView(1, object.objectConstants[frameIndex]->GetGPUVirtualAddress());
        DrawMesh(*object.mesh);
    }

    D3D12_RESOURCE_BARRIER toShader = TransitionBarrier(m_shadowMap.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &toShader);
}

void ForgeEngine::DrawAxes(UINT frameIndex)
{
    m_commandList->SetPipelineState(m_axisPipeline.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_axisMesh.vertexView);
    // x, y and z axis colors are red, green and blue.
    const UINT vertexStarts[3] = { 0, 4, 2 };
    for (UINT axis = 0; axis < 3; ++axis)
    {
        m_commandList->SetGraphicsRootConstantBufferView(1, m_axisConstants[axis][frameIndex]->GetGPUVirtualAddress());
        m_commandList->DrawInstanced(2, 1, vertexStarts[axis], 0);
    }
}

void ForgeEngine::DrawGizmo(UINT frameIndex)
{
    if (m_selectedObject < 0 || m_selectedObject >= static_cast<int>(m_objects.size())) return;
    m_commandList->SetPipelineState(m_gizmoPipeline.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    for (UINT axis = 0; axis < 3; ++axis)
    {
        const Mesh& mesh = m_gizmoMeshes[axis];
        m_commandList->SetGraphicsRootConstantBufferView(1, m_gizmoConstants[axis][frameIndex]->GetGPUVirtualAddress());
        m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexView);
        VertexRange range{};
        if (m_gizmoMode == TransformMode::Translate) range = m_gizmoMoveRanges[axis];
        else if (m_gizmoMode == TransformMode::Rotate) range = m_gizmoRotateRanges[axis];
        else range = m_gizmoScaleRanges[axis];
        if (range.count > 0)
            m_commandList->DrawInstanced(range.count, 1, range.start, 0);
    }
}

void ForgeEngine::DrawColliderOverlay(UINT frameIndex)
{
    m_commandList->SetPipelineState(m_gizmoPipeline.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    for (const GameObject& object : m_objects)
    {
        if (!object.hasCollider || !object.colliderConstants[frameIndex]) continue;
        const int type = object.colliderType >= 0 && object.colliderType <= 2 ? object.colliderType : 0;
        const Mesh& mesh = m_colliderWireMeshes[type];
        m_commandList->SetGraphicsRootConstantBufferView(1, object.colliderConstants[frameIndex]->GetGPUVirtualAddress());
        m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexView);
        m_commandList->DrawInstanced(mesh.vertexCount, 1, 0, 0);
    }
}

void ForgeEngine::DrawObjects(UINT frameIndex)
{
    m_commandList->SetPipelineState(m_pbrPipeline.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        GameObject& object = m_objects[i];
        if (!object.meshRendererEnabled || object.mesh == nullptr) continue;
        // Do not draw the active camera's icon at the camera origin into its own view.
        if (object.isCamera && static_cast<int>(i) == m_viewportCameraObjectIndex) continue;

        // Unity-style culling mask: when a scene camera drives the viewport, objects
        // whose layer bit is cleared on that camera are skipped.
        if (m_viewportCameraObjectIndex >= 0 &&
            m_viewportCameraObjectIndex < static_cast<int>(m_objects.size()))
        {
            const GameObject& viewportCamera = m_objects[static_cast<size_t>(m_viewportCameraObjectIndex)];
            const int layerBit = object.layer & 31;
            if ((viewportCamera.cameraCullingMask & (1 << layerBit)) == 0) continue;
        }

        // Conservative view-space sphere culling. The old test compared projected Z
        // values to world-space radii, so it could discard visible objects. Selected
        // objects and deformable terrain bypass culling to preserve the outline and terrain.
        if (!object.isTerrain && static_cast<int>(i) != m_selectedObject && object.mesh != nullptr)
        {
            const XMFLOAT3 localCenter = object.mesh->boundsCenter;
            const float maxScale = (std::max)({ std::abs(object.scale.x), std::abs(object.scale.y), std::abs(object.scale.z) });
            // Use geometric mesh bounds (not the object origin) and a conservative sphere.
            const float radius = (std::max)(0.01f, object.mesh->boundsRadius * maxScale);
            const XMVECTOR centerWorld = XMVector3TransformCoord(XMLoadFloat3(&localCenter), XMLoadFloat4x4(&object.world));
            const XMVECTOR centerView = XMVector3TransformCoord(centerWorld, XMLoadFloat4x4(&m_view));
            const float vx = XMVectorGetX(centerView), vy = XMVectorGetY(centerView), vz = XMVectorGetZ(centerView);
            const bool hasViewportCamera = m_viewportCameraObjectIndex >= 0 &&
                m_viewportCameraObjectIndex < static_cast<int>(m_objects.size()) &&
                m_objects[static_cast<size_t>(m_viewportCameraObjectIndex)].isCamera;
            const float nearZ = hasViewportCamera
                ? (std::max)(0.01f, m_objects[static_cast<size_t>(m_viewportCameraObjectIndex)].cameraNearClip) : 0.05f;
            const float farZ = hasViewportCamera
                ? (std::max)(nearZ + 0.1f, m_objects[static_cast<size_t>(m_viewportCameraObjectIndex)].cameraFarClip) : 1200.0f;
            const float tanHalfV = 1.0f / (std::max)(0.0001f, std::abs(m_projection._22));
            const float tanHalfH = 1.0f / (std::max)(0.0001f, std::abs(m_projection._11));
            const float horizontalPlaneRadius = radius * std::sqrt(1.0f + tanHalfH * tanHalfH);
            const float verticalPlaneRadius = radius * std::sqrt(1.0f + tanHalfV * tanHalfV);
            const bool outsideDepth = vz + radius < nearZ || vz - radius > farZ;
            const bool outsideHorizontal = std::abs(vx) - vz * tanHalfH > horizontalPlaneRadius;
            const bool outsideVertical = std::abs(vy) - vz * tanHalfV > verticalPlaneRadius;
            if (outsideDepth || outsideHorizontal || outsideVertical) continue;
        }

        // Authored two-sided materials render without back-face culling so single-plane
        // geometry (foliage cards, cloth) is visible from both sides.
        m_commandList->SetPipelineState(object.doubleSided ? m_pbrDoubleSidedPipeline.Get() : m_pbrPipeline.Get());
        m_commandList->SetGraphicsRootConstantBufferView(1, object.objectConstants[frameIndex]->GetGPUVirtualAddress());
        D3D12_GPU_DESCRIPTOR_HANDLE albedoHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        albedoHandle.ptr += static_cast<UINT64>(object.albedoTexture ? object.albedoSrvIndex : (m_frameIndex == 0 ? 2u : 1u)) * m_srvDescriptorSize;
        m_commandList->SetGraphicsRootDescriptorTable(4, albedoHandle);
        auto textureHandle = [&](UINT index)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE h = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
            h.ptr += static_cast<UINT64>(index) * m_srvDescriptorSize;
            return h;
        };
        const UINT fallbackMaterialSrv = (m_frameIndex == 0 ? 2u : 1u);
        m_commandList->SetGraphicsRootDescriptorTable(6, textureHandle(object.normalTexture ? object.normalSrvIndex : fallbackMaterialSrv));
        m_commandList->SetGraphicsRootDescriptorTable(7, textureHandle(object.metallicRoughnessTexture ? object.metallicRoughnessSrvIndex : fallbackMaterialSrv));
        m_commandList->SetGraphicsRootDescriptorTable(8, textureHandle(object.aoTexture ? object.aoSrvIndex : fallbackMaterialSrv));
        m_commandList->SetGraphicsRootDescriptorTable(9, textureHandle(object.emissiveTexture ? object.emissiveSrvIndex : fallbackMaterialSrv));
        DrawMesh(*object.mesh);

        if (static_cast<int>(i) == m_selectedObject)
        {
            m_commandList->SetPipelineState(m_outlinePipeline.Get());
            m_commandList->SetGraphicsRootConstantBufferView(1, object.outlineConstants[frameIndex]->GetGPUVirtualAddress());
            DrawMesh(*object.mesh);
            m_commandList->SetPipelineState(m_pbrPipeline.Get());
        }
    }
}

void ForgeEngine::DrawScenePass(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER toRenderTarget = TransitionBarrier(m_hdrTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(FrameCount + frameIndex) * m_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    const float clearColor[] = { 0.025f, 0.035f, 0.055f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    D3D12_VIEWPORT sceneViewport{};
    sceneViewport.TopLeftX = std::clamp(m_sceneViewportX, 0.0f, static_cast<float>(m_width - 1));
    sceneViewport.TopLeftY = std::clamp(m_sceneViewportY, 0.0f, static_cast<float>(m_height - 1));
    sceneViewport.Width = std::max(1.0f, std::min(m_sceneViewportWidth, static_cast<float>(m_width) - sceneViewport.TopLeftX));
    sceneViewport.Height = std::max(1.0f, std::min(m_sceneViewportHeight, static_cast<float>(m_height) - sceneViewport.TopLeftY));
    sceneViewport.MinDepth = 0.0f;
    sceneViewport.MaxDepth = 1.0f;
    D3D12_RECT sceneScissor{
        static_cast<LONG>(sceneViewport.TopLeftX),
        static_cast<LONG>(sceneViewport.TopLeftY),
        static_cast<LONG>(sceneViewport.TopLeftX + sceneViewport.Width),
        static_cast<LONG>(sceneViewport.TopLeftY + sceneViewport.Height)
    };
    m_commandList->RSSetViewports(1, &sceneViewport);
    m_commandList->RSSetScissorRects(1, &sceneScissor);
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRootConstantBufferView(0, m_frameConstants[frameIndex]->GetGPUVirtualAddress());
    m_commandList->SetGraphicsRootDescriptorTable(2, m_shadowSrvGpuHandle);

    m_commandList->SetPipelineState(m_skyGridPipeline.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->DrawInstanced(3, 1, 0, 0);

    if (m_editorOverlayEnabled) DrawAxes(frameIndex);
    DrawObjects(frameIndex);
    // Collider markers use the gizmo pipeline (depth-always line list) so the collision
    // size of every object is readable in the editor viewport, even through geometry.
    if (m_editorOverlayEnabled) DrawColliderOverlay(frameIndex);
    // Transform handles are drawn and manipulated exclusively by ImGuizmo in the
    // editor overlay. Do not draw the legacy native DX12 gizmo here: it duplicates
    // axes, disagrees with ImGuizmo hit-testing, and makes selection feel broken.

    D3D12_RESOURCE_BARRIER toShader = TransitionBarrier(m_hdrTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &toShader);
}

void ForgeEngine::DrawPostProcessPass(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER depthToShader = TransitionBarrier(m_depthBuffer.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &depthToShader);

    D3D12_RESOURCE_BARRIER toRenderTarget = TransitionBarrier(m_renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(frameIndex) * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRootConstantBufferView(0, m_frameConstants[frameIndex]->GetGPUVirtualAddress());
    m_commandList->SetGraphicsRootDescriptorTable(3, m_sceneColorSrvGpuHandles[frameIndex]);
    m_commandList->SetGraphicsRootDescriptorTable(5, m_depthSrvGpuHandle);
    m_commandList->SetPipelineState(m_postProcessPipeline.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER depthToWrite = TransitionBarrier(m_depthBuffer.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_commandList->ResourceBarrier(1, &depthToWrite);

    D3D12_RESOURCE_BARRIER toPresent = TransitionBarrier(m_renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &toPresent);
}

void ForgeEngine::CaptureCameraPreviewFrame(UINT frameIndex)
{
    if (frameIndex >= FrameCount || !m_cameraPreviewTargets[frameIndex] || !m_renderTargets[frameIndex])
        return;
    // DrawPostProcessPass leaves the swap-chain image in PRESENT state and the preview
    // texture in PIXEL_SHADER_RESOURCE. Copy the final tone-mapped frame before ImGui
    // draws any editor windows over it, then restore both resources to their expected states.
    D3D12_RESOURCE_BARRIER toCopy[2] = {
        TransitionBarrier(m_renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE),
        TransitionBarrier(m_cameraPreviewTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
    };
    m_commandList->ResourceBarrier(2, toCopy);
    m_commandList->CopyResource(m_cameraPreviewTargets[frameIndex].Get(), m_renderTargets[frameIndex].Get());
    D3D12_RESOURCE_BARRIER toShader[2] = {
        TransitionBarrier(m_renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT),
        TransitionBarrier(m_cameraPreviewTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    };
    m_commandList->ResourceBarrier(2, toShader);
}

void ForgeEngine::EnsureObjectTexture(GameObject& object, int slot)
{
    std::string& texturePath = slot == 0 ? object.albedoTexturePath :
        slot == 1 ? object.normalTexturePath :
        slot == 2 ? object.metallicRoughnessTexturePath :
        slot == 3 ? object.aoTexturePath : object.emissiveTexturePath;
    std::string& loadedTexturePath = slot == 0 ? object.loadedAlbedoTexturePath :
        slot == 1 ? object.loadedNormalTexturePath :
        slot == 2 ? object.loadedMetallicRoughnessTexturePath :
        slot == 3 ? object.loadedAoTexturePath : object.loadedEmissiveTexturePath;
    ComPtr<ID3D12Resource>& texture = slot == 0 ? object.albedoTexture :
        slot == 1 ? object.normalTexture :
        slot == 2 ? object.metallicRoughnessTexture :
        slot == 3 ? object.aoTexture : object.emissiveTexture;
    UINT& srvIndex = slot == 0 ? object.albedoSrvIndex :
        slot == 1 ? object.normalSrvIndex :
        slot == 2 ? object.metallicRoughnessSrvIndex :
        slot == 3 ? object.aoSrvIndex : object.emissiveSrvIndex;
    if (texturePath == loadedTexturePath) return;

    m_lastTextureError.clear();
    // A previously submitted frame may still sample the old resource. Drain the queue
    // before releasing it; descriptors and resources are replaced only after that fence.
    if (texture)
        WaitForGpu();
    texture.Reset();
    srvIndex = 1;
    loadedTexturePath.clear();
    if (texturePath.empty()) return;

    if (m_nextTextureSrvIndex >= 1024) {
    m_lastTextureError = "Texture descriptor capacity exhausted (1024 entries).";
    OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
    return;
    }

    std::filesystem::path path = std::filesystem::u8path(texturePath);
    if (!path.is_absolute())
    {
        bool found = false;
        std::vector<std::filesystem::path> bases;
        std::error_code pathError;
        // The editor sets m_textureSearchRoot to the project root when a
        // project is opened, so relative asset paths resolve reliably even
        // when the executable lives outside the source tree.
        if (!m_textureSearchRoot.empty()) bases.push_back(m_textureSearchRoot);
        const auto workingDirectory = std::filesystem::current_path(pathError);
        if (!pathError) bases.push_back(workingDirectory);
        // Project paths are stored relative to <source-root>/Project, while the process
        // working directory is normally the build output. Search both layouts so assets
        // still resolve when Visual Studio launches from build/x64/Debug.
        std::filesystem::path executableDirectory;
        wchar_t executablePath[MAX_PATH]{};
        const DWORD executableLength = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
        if (executableLength > 0 && executableLength < MAX_PATH)
            executableDirectory = std::filesystem::path(executablePath).parent_path();
        if (!executableDirectory.empty()) bases.push_back(executableDirectory);

        for (const auto& initialBase : bases)
        {
            std::filesystem::path base = initialBase;
            for (int depth = 0; depth < 16 && !base.empty(); ++depth)
            {
                const std::filesystem::path candidates[] = { base / path, base / L"Project" / path };
                for (const auto& candidate : candidates)
                {
                    std::error_code existsError;
                    if (std::filesystem::is_regular_file(candidate, existsError))
                    {
                        path = candidate;
                        found = true;
                        break;
                    }
                }
                if (found) break;
                const auto parent = base.parent_path();
                if (parent == base) break;
                base = parent;
            }
            if (found) break;
        }
        if (!found)
        {
            m_lastTextureError = "Texture file not found. Check Project/Assets path: " + texturePath;
            OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
            return;
        }
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        m_lastTextureError = "Windows Imaging Component (WIC) could not start for: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        m_lastTextureError = "WIC could not decode texture: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        m_lastTextureError = "Could not read the first image frame: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    UINT width = 0, height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0 ||
        width > 16384 || height > 16384)
    {
        m_lastTextureError = "Texture dimensions are invalid or exceed 16384 x 16384: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) {
        m_lastTextureError = "Could not create a WIC pixel converter for: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        m_lastTextureError = "Texture pixel format could not be converted to RGBA: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    const UINT rowPitch = width * 4;
    const UINT imageBytes = rowPitch * height;
    std::vector<std::uint8_t> pixels(imageBytes);
    if (FAILED(converter->CopyPixels(nullptr, rowPitch, imageBytes, pixels.data()))) {
        m_lastTextureError = "Could not read texture pixels: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    const auto defaultHeap = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    hr = m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
    if (FAILED(hr)) {
        texture.Reset();
        m_lastTextureError = "DirectX 12 could not allocate texture memory: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows = 0;
    UINT64 rowSize = 0, uploadBytes = 0;
    m_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rows, &rowSize, &uploadBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    const auto uploadHeap = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const auto uploadDesc = BufferDescription(uploadBytes);
    hr = m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    if (FAILED(hr)) {
        texture.Reset();
        m_lastTextureError = "DirectX 12 could not allocate texture upload memory: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    std::uint8_t* mapped = nullptr;
    D3D12_RANGE readRange{ 0, 0 };
    if (FAILED(upload->Map(0, &readRange, reinterpret_cast<void**>(&mapped))) || mapped == nullptr) {
        texture.Reset();
        m_lastTextureError = "Could not map texture upload memory: " + path.string();
        OutputDebugStringA(("ForgeEngine: " + m_lastTextureError + "\n").c_str());
        return;
    }
    for (UINT y = 0; y < height; ++y)
        std::memcpy(mapped + footprint.Offset + static_cast<size_t>(y) * footprint.Footprint.RowPitch,
            pixels.data() + static_cast<size_t>(y) * rowPitch, rowPitch);
    upload->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = upload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    const auto barrier = TransitionBarrier(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrier);
    m_textureUploadResources.push_back(upload); // retain upload heap until shutdown, safely beyond GPU execution

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(m_nextTextureSrvIndex) * m_srvDescriptorSize;
    m_device->CreateShaderResourceView(texture.Get(), &srv, cpu);
    srvIndex = m_nextTextureSrvIndex++;
    loadedTexturePath = texturePath;
}

void ForgeEngine::Render(float deltaSeconds)
{
    if (!m_initialized || m_minimized)
        return;
    if (m_dayNightCycleEnabled)
    {
        const float safeDelta = std::clamp(deltaSeconds, 0.0f, 0.25f);
        m_timeOfDayHours = std::fmod(m_timeOfDayHours + safeDelta * std::max(m_dayNightCycleSpeed, 0.0f) / 60.0f, 24.0f);
        if (m_timeOfDayHours < 0.0f) m_timeOfDayHours += 24.0f;
        m_timeOfDayDrivesSun = true;
        m_directionalObjectTransformDrivesSun = false;
    }
    ResizeIfNeeded();
    if (m_standalonePlayer)
    {
        // Without the editor layout pass the whole window is the game viewport.
        m_sceneViewportX = 0.0f;
        m_sceneViewportY = 0.0f;
        m_sceneViewportWidth = static_cast<float>(m_width);
        m_sceneViewportHeight = static_cast<float>(m_height);
    }
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    WaitForFrame(m_frameIndex);

    UpdateCamera(deltaSeconds);
    if (m_editorUI)
    {
        if (!m_standalonePlayer)
        {
            m_editorUI->BeginFrame();
            m_editorUI->Draw(*this);
            // The editor updates the central dockspace rectangle; refresh camera
            // projection after layout and before writing this frame's GPU constants.
            UpdateCamera(0.0f);
        }
        m_editorUI->TickScripts(*this, deltaSeconds);
    }
    ProcessPendingSelection();
    const float totalSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_startTime).count();
    UpdateConstantBuffers(totalSeconds);

    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset(), "CommandAllocator::Reset");
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr), "CommandList::Reset");
    for (GameObject& object : m_objects)
    {
        EnsureObjectTexture(object, 0);
        EnsureObjectTexture(object, 1);
        EnsureObjectTexture(object, 2);
        EnsureObjectTexture(object, 3);
        EnsureObjectTexture(object, 4);
    }
    UpdateConstantBuffers(totalSeconds);
    DrawShadowPass(m_frameIndex);
    DrawScenePass(m_frameIndex);
    DrawPostProcessPass(m_frameIndex);
    if (m_selectedObject >= 0 && m_selectedObject < static_cast<int>(m_objects.size()) &&
        m_objects[static_cast<size_t>(m_selectedObject)].isCamera)
        CaptureCameraPreviewFrame(m_frameIndex);
    if (m_screenshotPending && m_renderTargets[m_frameIndex])
    {
        // Copy the tone-mapped back buffer before ImGui draws the editor overlay,
        // so saved screenshots contain the game view only.
        D3D12_RESOURCE_DESC desc = m_renderTargets[m_frameIndex]->GetDesc();
        m_screenshotWidth = static_cast<UINT>(desc.Width);
        m_screenshotHeight = desc.Height;
        UINT numRows = 0;
        UINT64 rowBytes = 0;
        m_device->GetCopyableFootprints(&desc, 0, 1, 0,
            &m_screenshotFootprint, &numRows, &rowBytes, nullptr);
        m_screenshotRowBytes = static_cast<UINT>(rowBytes);
        D3D12_RESOURCE_DESC stagingDesc{};
        stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        stagingDesc.Width = desc.Width;
        stagingDesc.Height = desc.Height;
        stagingDesc.DepthOrArraySize = 1;
        stagingDesc.MipLevels = 1;
        stagingDesc.Format = desc.Format;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto heap = HeapProperties(D3D12_HEAP_TYPE_READBACK);
        HRESULT stagingHr = m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &stagingDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_screenshotStaging));
        if (FAILED(stagingHr))
        {
            m_lastScreenshotResult = "ERROR: Could not create the screenshot readback texture.";
            m_screenshotPending = false;
        }
        else
        {
            D3D12_RESOURCE_BARRIER toCopy = TransitionBarrier(m_renderTargets[m_frameIndex].Get(),
                D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            m_commandList->ResourceBarrier(1, &toCopy);
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource = m_screenshotStaging.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint = m_screenshotFootprint;
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource = m_renderTargets[m_frameIndex].Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.SubresourceIndex = 0;
            m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            D3D12_RESOURCE_BARRIER toPresent = TransitionBarrier(m_renderTargets[m_frameIndex].Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
            m_commandList->ResourceBarrier(1, &toPresent);
        }
    }
    if (m_editorUI && !m_standalonePlayer) m_editorUI->RenderDrawData(*this);
    ThrowIfFailed(m_commandList->Close(), "CommandList::Close");

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);
    HRESULT presentResult = m_swapChain->Present(1, 0);
    if (presentResult == DXGI_ERROR_DEVICE_REMOVED || presentResult == DXGI_ERROR_DEVICE_RESET)
    {
        const HRESULT reason = m_device->GetDeviceRemovedReason();
        ThrowIfFailed(reason, "DirectX 12 device removed");
    }
    ThrowIfFailed(presentResult, "SwapChain::Present");

    const UINT64 fenceValue = m_nextFenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValue), "CommandQueue::Signal");
    m_frameFenceValues[m_frameIndex] = fenceValue;
    if (m_screenshotPending) EncodePendingScreenshot(fenceValue);
    UpdateWindowTitle(deltaSeconds);
}

void ForgeEngine::RequestScreenshot(const std::filesystem::path& path)
{
    if (m_screenshotPending) return;
    m_screenshotPath = path;
    m_screenshotPending = true;
}

void ForgeEngine::EncodePendingScreenshot(UINT64 fenceValue)
{
    // Wait until this frame's GPU work (including the back buffer copy) has finished.
    if (m_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent), "Screenshot fence");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    m_screenshotPending = false;
    if (!m_screenshotStaging)
    {
        m_lastScreenshotResult = "ERROR: Screenshot readback texture was not created.";
        return;
    }
    D3D12_RANGE readRange{ 0, m_screenshotFootprint.Footprint.RowPitch * m_screenshotHeight };
    void* pixels = nullptr;
    const HRESULT mapHr = m_screenshotStaging->Map(0, &readRange, &pixels);
    if (FAILED(mapHr) || pixels == nullptr)
    {
        m_lastScreenshotResult = "ERROR: Could not map the screenshot readback texture.";
        m_screenshotStaging.Reset();
        return;
    }
    std::error_code ec;
    if (m_screenshotPath.has_parent_path()) std::filesystem::create_directories(m_screenshotPath.parent_path(), ec);
    HRESULT writeHr = E_FAIL;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))))
    {
        // The back buffer is R8G8B8A8 (WIC RGBA). Wrap the mapped bytes, convert to
        // BGR (fully supported by the PNG encoder), then stream the result to disk.
        Microsoft::WRL::ComPtr<IWICBitmap> sourceBitmap;
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        Microsoft::WRL::ComPtr<IWICStream> stream;
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        GUID pngFormat = GUID_ContainerFormatPng;
        const UINT stride = m_screenshotFootprint.Footprint.RowPitch;
        if (SUCCEEDED(factory->CreateBitmapFromMemory(m_screenshotWidth, m_screenshotHeight,
            GUID_WICPixelFormat32bppRGBA, stride, stride * m_screenshotHeight,
            static_cast<BYTE*>(pixels), &sourceBitmap)) &&
            SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
            SUCCEEDED(converter->Initialize(sourceBitmap.Get(), GUID_WICPixelFormat24bppBGR,
                WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom)) &&
            SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(m_screenshotPath.c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(factory->CreateEncoder(pngFormat, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderCacheInMemory)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
            SUCCEEDED(frame->Initialize(nullptr)) &&
            SUCCEEDED(frame->SetSize(m_screenshotWidth, m_screenshotHeight)))
        {
            const UINT convertedPitch = (m_screenshotWidth * 3 + 3) & ~3u;
            std::vector<BYTE> converted(static_cast<size_t>(convertedPitch) * m_screenshotHeight);
            if (SUCCEEDED(converter->CopyPixels(nullptr, convertedPitch,
                static_cast<UINT>(converted.size()), converted.data())))
            {
                GUID bgrFormat = GUID_WICPixelFormat24bppBGR;
                frame->SetPixelFormat(&bgrFormat);
                writeHr = frame->WritePixels(m_screenshotHeight, convertedPitch,
                    static_cast<UINT>(converted.size()), converted.data());
                if (SUCCEEDED(writeHr)) writeHr = frame->Commit();
                if (SUCCEEDED(writeHr)) writeHr = encoder->Commit();
            }
        }
    }
    m_screenshotStaging->Unmap(0, nullptr);
    m_screenshotStaging.Reset();
    m_lastScreenshotResult = SUCCEEDED(writeHr)
        ? "Screenshot saved: " + m_screenshotPath.string()
        : "ERROR: Could not write the screenshot PNG: " + m_screenshotPath.string();
}

void ForgeEngine::WaitForFrame(UINT frameIndex)
{
    const UINT64 fenceValue = m_frameFenceValues[frameIndex];
    if (fenceValue != 0 && m_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent), "Fence::SetEventOnCompletion");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void ForgeEngine::WaitForGpu()
{
    if (!m_commandQueue || !m_fence)
        return;
    const UINT64 fenceValue = m_nextFenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValue), "WaitForGpu Signal");
    if (m_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent), "WaitForGpu SetEventOnCompletion");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    m_frameFenceValues.fill(0);
}

void ForgeEngine::UpdateWindowTitle(float deltaSeconds)
{
    m_titleTimer += deltaSeconds;
    if (m_titleTimer < 0.25f)
        return;
    m_titleTimer = 0.0f;
    if (m_standalonePlayer)
    {
        std::wstring title = m_gameTitle.empty() ? std::wstring(L"Forge Engine Game") : m_gameTitle;
        if (!m_gameCreator.empty()) title += L" | " + m_gameCreator;
        SetWindowTextW(m_hwnd, title.c_str());
        return;
    }
    const wchar_t* modeName = m_gizmoMode == TransformMode::Translate ? L"Move" :
        (m_gizmoMode == TransformMode::Rotate ? L"Rotate" : L"Scale");
    std::wstring title = L"Forge Engine | RMB Look | WASD/QE Move | LMB Select/Gizmo | 1 Move 2 Rotate 3 Scale | ";
    title += modeName;
    if (m_selectedObject >= 0 && static_cast<size_t>(m_selectedObject) < m_objects.size())
    {
        const std::string& name = m_objects[static_cast<size_t>(m_selectedObject)].name;
        std::wstring wideName(name.begin(), name.end());
        title += L" | Selecionado: " + wideName;
    }
    SetWindowTextW(m_hwnd, title.c_str());
}

int ForgeEngine::Run()
{
    MSG message{};
    auto previousTime = std::chrono::steady_clock::now();
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        else
        {
            const auto currentTime = std::chrono::steady_clock::now();
            float deltaSeconds = std::chrono::duration<float>(currentTime - previousTime).count();
            previousTime = currentTime;
            deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.05f);
            Render(deltaSeconds);
            if (m_minimized)
                WaitMessage();
        }
    }
    WaitForGpu();
    return static_cast<int>(message.wParam);
}
