#include "EditorUI.h"
#include "../Engine/Engine.h"

#include <imgui_internal.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <ImGuizmo.h>

// Assimp model-import API (Importer, aiScene, aiMesh, aiFace and post-process flags).
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winreg.h>
#include <cctype>
#include <functional>
#include <ctime>

// The Win32 backend header may leave this declaration commented out because
// applications normally declare it themselves when forwarding window messages.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    std::wstring Lower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
        return value;
    }

    bool IsImageExtension(const std::filesystem::path& path)
    {
        const std::wstring ext = Lower(path.extension().wstring());
        return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" ||
            ext == L".tif" || ext == L".tiff" || ext == L".gif" || ext == L".ico";
    }

    bool IsVideoExtension(const std::filesystem::path& path)
    {
        const std::wstring ext = Lower(path.extension().wstring());
        return ext == L".mp4" || ext == L".mov" || ext == L".avi" || ext == L".mkv" ||
            ext == L".webm" || ext == L".wmv" || ext == L".m4v" || ext == L".mpeg" ||
            ext == L".mpg";
    }

    bool IsModelExtension(const std::filesystem::path& path)
    {
        const std::wstring ext = Lower(path.extension().wstring());
        return ext == L".obj" || ext == L".fbx" || ext == L".gltf" || ext == L".glb" ||
            ext == L".dae" || ext == L".stl" || ext == L".ply" || ext == L".3ds";
    }

    XMFLOAT3 Normalize3(const XMFLOAT3& value)
    {
        const XMVECTOR v = XMLoadFloat3(&value);
        const float length = XMVectorGetX(XMVector3Length(v));
        if (length < 1.0e-6f) return {0.0f, 1.0f, 0.0f};
        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVectorScale(v, 1.0f / length));
        return result;
    }

    bool RayTriangle(const XMFLOAT3& origin, const XMFLOAT3& direction,
        const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, float& t)
    {
        const XMVECTOR o = XMLoadFloat3(&origin), d = XMLoadFloat3(&direction);
        const XMVECTOR av = XMLoadFloat3(&a), bv = XMLoadFloat3(&b), cv = XMLoadFloat3(&c);
        const XMVECTOR e1 = XMVectorSubtract(bv, av), e2 = XMVectorSubtract(cv, av);
        const XMVECTOR p = XMVector3Cross(d, e2);
        const float det = XMVectorGetX(XMVector3Dot(e1, p));
        if (std::abs(det) < 1.0e-7f) return false;
        const float invDet = 1.0f / det;
        const XMVECTOR tv = XMVectorSubtract(o, av);
        const float u = XMVectorGetX(XMVector3Dot(tv, p)) * invDet;
        if (u < 0.0f || u > 1.0f) return false;
        const XMVECTOR q = XMVector3Cross(tv, e1);
        const float v = XMVectorGetX(XMVector3Dot(d, q)) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;
        t = XMVectorGetX(XMVector3Dot(e2, q)) * invDet;
        return t >= 0.0f && std::isfinite(t);
    }

    XMFLOAT3 QuaternionToEuler(const XMVECTOR& quaternion)
    {
        XMFLOAT4 q{};
        XMStoreFloat4(&q, quaternion);
        const float sinX = 2.0f * (q.w * q.x + q.y * q.z);
        const float cosX = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        const float x = std::atan2(sinX, cosX);
        const float sinY = 2.0f * (q.w * q.y - q.z * q.x);
        const float y = std::abs(sinY) >= 1.0f
            ? std::copysign(Pi * 0.5f, sinY) : std::asin(sinY);
        const float sinZ = 2.0f * (q.w * q.z + q.x * q.y);
        const float cosZ = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        const float z = std::atan2(sinZ, cosZ);
        return { x, y, z };
    }

    ImVec4 Accent() { return ImVec4(0.35f, 0.43f, 0.98f, 1.0f); }

    // Segoe MDL2 Assets private-use glyphs (UTF-8 bytes). Defined as macros so
    // they concatenate with adjacent string literals at compile time.
    #define ICON_PLAY     "\xEE\x9D\xA8"
    #define ICON_STOP     "\xEE\x9C\x9A"
    #define ICON_REFRESH  "\xEE\x9C\xAC"
    #define ICON_EDIT     "\xEE\x9C\x8F"
    #define ICON_ADD      "\xEE\x9C\x90"
    #define ICON_SETTINGS "\xEE\x9C\x93"
    #define ICON_SEARCH   "\xEE\x9C\xA1"
    #define ICON_CAMERA   "\xEE\x9C\xA2"
    #define ICON_LIGHT    "\xEE\x9C\x86"
    #define ICON_DELETE   "\xEE\x9D\x8D"
    #define ICON_SAVE     "\xEE\x9D\x8E"
    #define ICON_COPY     "\xEE\xA3\x88"
    #define ICON_PERSON   "\xEE\x9D\xBB"
    #define ICON_HELP     "\xEE\xA2\x97"
    #define ICON_FOLDER   "\xEE\xA2\xB7"
    #define ICON_DOCUMENT "\xEE\xA2\xA5"
    #define ICON_CLOSE    "\xEE\xA2\xBB"
    #define ICON_MINIMIZE "\xEE\xA4\xA1"
    #define ICON_MAXIMIZE "\xEE\xA4\xA2"
    #define ICON_RESTORE  "\xEE\xA4\xA3"
    #define ICON_PACKAGE  "\xEE\x9E\xB8"
    #define ICON_HOME     "\xEE\xA0\x8F"
    #define ICON_LIST     "\xEE\xA3\xBD"
    #define ICON_REPAIR   "\xEE\x9E\x90"
    #define ICON_CHART    "\xEE\xA8\x89"
    #define ICON_COMMAND  "\xEE\x9D\x96"

    std::filesystem::path GetAppDataDir()
    {
        PWSTR appDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath)))
        {
            std::filesystem::path result(appDataPath);
            CoTaskMemFree(appDataPath);
            return result / L"ForgeEngine";
        }
        return {};
    }

    std::filesystem::path GetRecentProjectsFile()
    {
        return GetAppDataDir() / L"RecentProjects.ini";
    }

    std::vector<std::filesystem::path> LoadRecentProjects()
    {
        std::vector<std::filesystem::path> result;
        const auto path = GetRecentProjectsFile();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) return result;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty()) continue;
            std::filesystem::path p = std::filesystem::u8path(line);
            if (std::filesystem::exists(p / L"ForgeEngine.project", ec))
                result.push_back(std::move(p));
        }
        if (result.size() > 12) result.resize(12);
        return result;
    }

    void SaveRecentProjects(const std::vector<std::filesystem::path>& projects)
    {
        const auto dir = GetAppDataDir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const auto path = dir / L"RecentProjects.ini";
        std::ofstream file(path, std::ios::binary);
        for (const auto& p : projects)
            file << p.u8string() << "\n";
    }

    bool PickFolderDialog(HWND owner, const wchar_t* title, std::wstring& outPath)
    {
        // IFileOpenDialog with FOS_PICKFOLDERS gives a modern Vista+ picker.
        ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog))))
            return false;
        DWORD options = 0;
        if (FAILED(dialog->GetOptions(&options))) return false;
        if (FAILED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST))) return false;
        if (title) dialog->SetTitle(title);
        if (dialog->Show(owner) != S_OK) return false;
        ComPtr<IShellItem> item;
        if (FAILED(dialog->GetResult(&item))) return false;
        PWSTR rawPath = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath))) return false;
        outPath = rawPath;
        CoTaskMemFree(rawPath);
        return true;
    }

    // Scope guard for Dear ImGui child windows. BeginChild() must always be
    // paired with EndChild(), including when BeginChild() returns false.
    // EndNow() closes the child at the intended location; the destructor is
    // a safety net for early returns added during future editor changes.
    class ImGuiChildScope final
    {
    public:
        ImGuiChildScope(const char* id, const ImVec2& size, bool border = false,
            ImGuiWindowFlags flags = 0)
            : m_visible(ImGui::BeginChild(id, size, border, flags)), m_active(true)
        {
        }

        ~ImGuiChildScope() { EndNow(); }

        ImGuiChildScope(const ImGuiChildScope&) = delete;
        ImGuiChildScope& operator=(const ImGuiChildScope&) = delete;

        bool Visible() const noexcept { return m_visible; }

        void EndNow() noexcept
        {
            if (m_active)
            {
                ImGui::EndChild();
                m_active = false;
            }
        }

    private:
        bool m_visible = false;
        bool m_active = false;
    };
}

ForgeEditorUI::~ForgeEditorUI()
{
    Shutdown();
}

bool ForgeEditorUI::Initialize(ForgeEngine& engine)
{
    if (m_contextInitialized)
        return true;

    std::array<wchar_t, 32768> executableBuffer{};
    const DWORD executableLength = GetModuleFileNameW(nullptr, executableBuffer.data(),
        static_cast<DWORD>(executableBuffer.size()));
    const std::filesystem::path executableDirectory = executableLength > 0
        ? std::filesystem::path(std::wstring(executableBuffer.data(), executableLength)).parent_path()
        : std::filesystem::current_path();
    // Exported games ship their data in a <Product>_Data folder next to the
    // executable (mirroring Unity). The editor walks up to the source tree instead.
    std::filesystem::path sourceRoot;
    if (engine.m_standalonePlayer)
    {
        sourceRoot = ResolveStandaloneDataRoot(executableDirectory, engine.m_gameProjectFolder);
        if (sourceRoot.empty()) sourceRoot = DetectSourceRoot(executableDirectory);
    }
    else
    {
        sourceRoot = DetectSourceRoot(executableDirectory);
    }
    m_sourceRoot = sourceRoot;
    m_projectRoot = sourceRoot / L"Project";
    m_scriptRuntime = std::make_unique<ForgeScriptRuntime>();
    m_assetsRoot = m_projectRoot / L"Assets";
    m_engine = &engine;
    if (engine.m_hwnd) DragAcceptFiles(engine.m_hwnd, TRUE);
    m_projectLocation = m_sourceRoot / L"Projects";
    std::error_code projectLocError;
    std::filesystem::create_directories(m_projectLocation, projectLocError);
    m_recentProjects.push_back(m_projectRoot);

    const std::array<std::filesystem::path, 12> folders{
        m_assetsRoot / L"Textures", m_assetsRoot / L"Models", m_assetsRoot / L"Audio",
        m_assetsRoot / L"Videos", m_assetsRoot / L"Materials", m_assetsRoot / L"Scenes",
        m_assetsRoot / L"Shaders", m_assetsRoot / L"Misc", m_projectRoot / L"Config",
        m_projectRoot / L"Scripts", m_projectRoot / L"Plugins", m_projectRoot / L"Presets"
    };
    std::error_code ec;
    for (const auto& folder : folders)
        std::filesystem::create_directories(folder, ec);

    const auto projectFile = m_projectRoot / L"ForgeEngine.project";
    if (!std::filesystem::exists(projectFile, ec))
    {
        std::ofstream project(projectFile, std::ios::binary);
        project << "FORGEENGINE_PROJECT 1\nName \"Forge Engine Project\"\nEngineVersion \"0.5.10\"\nAssetRoot \"Assets\"\n";
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_contextInitialized = true;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    m_iniPath = (m_projectRoot / L"Config" / L"EditorLayout.ini").u8string();
    io.IniFilename = m_iniPath.c_str();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = false;

    // Modern font stack: Segoe UI for text, Segoe MDL2 Assets as a merged
    // fallback so the ICON_* glyphs render inline in any label without
    // PushFont gymnastics. Falls back to ImGui's default when system fonts
    // are missing (e.g. headless / Wine).
    {
        ImFontConfig mainCfg{};
        mainCfg.OversampleH = 2;
        mainCfg.OversampleV = 1;
        ImFont* mainFont = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &mainCfg);
        if (!mainFont)
            mainFont = io.Fonts->AddFontDefault();

        static const ImWchar iconRanges[] = {
            0xE700, 0xE7C0,
            0xE7E0, 0xE7FF,
            0xE800, 0xE8FF,
            0xE900, 0xE9FF,
            0xEA00, 0xEA5F,
            0
        };
        ImFontConfig iconCfg{};
        iconCfg.MergeMode = true;
        iconCfg.PixelSnapH = true;
        iconCfg.GlyphMinAdvanceX = 16.0f;
        ImFont* icons = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segmdl2.ttf", 16.0f, &iconCfg, iconRanges);
        (void)icons;
    }

    ApplyStyle();

    // Restore persisted recent projects list from %APPDATA%\ForgeEngine.
    m_recentProjects = LoadRecentProjects();

    if (!ImGui_ImplWin32_Init(engine.m_hwnd))
    {
        OutputDebugStringA("Forge Engine ERROR: ImGui_ImplWin32_Init failed.\n");
        return false;
    }
    m_win32Initialized = true;

    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = static_cast<UINT>(m_srvSlots.size());
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDescription.NodeMask = 0;
    ForgeEngine::ThrowIfFailed(engine.m_device->CreateDescriptorHeap(
        &heapDescription, IID_PPV_ARGS(&m_srvHeap)), "Create Dear ImGui SRV descriptor heap");
    m_srvDescriptorIncrement = engine.m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = engine.m_device.Get();
    initInfo.CommandQueue = engine.m_commandQueue.Get();
    initInfo.NumFramesInFlight = static_cast<int>(ForgeEngine::FrameCount);
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = m_srvHeap.Get();
    initInfo.UserData = this;
    initInfo.SrvDescriptorAllocFn = AllocateSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeSrvDescriptor;
    if (!ImGui_ImplDX12_Init(&initInfo))
    {
        OutputDebugStringA("Forge Engine ERROR: ImGui_ImplDX12_Init failed. Check the DirectX 12 debug output.\n");
        return false;
    }
    m_dx12Initialized = true;

    m_activeAssetFolder = m_assetsRoot;
    m_currentScenePath = m_assetsRoot / L"Scenes" / L"Default.forgeScene";
    const auto defaultScene = m_currentScenePath;
    if (std::filesystem::exists(defaultScene, ec))
    {
        if (!LoadScene(engine, defaultScene))
            Log("A cena padrão não pôde ser carregada; mantendo a cena inicial.");
    }
    else
    {
        SaveScene(engine, defaultScene);
    }
    LoadProjectPostProcess(engine);

    m_lastScriptSourceSignature = CalculateScriptSourceSignature();
    if (engine.m_standalonePlayer)
    {
        // Exported games never show editor UI: load the scene and run scripts directly.
        m_showWelcome = false;
        m_showStats = false;
        m_statusText = "Running game";
        StartPlayMode(engine);
        Log("Standalone player started. Editor overlay is disabled (Ctrl+G re-enables it for debugging).");
        return true;
    }
    Log("Forge Engine Editor inicializado.");
    Log("C# scripting: Project/Scripts/ForgeGameScripts.csproj | Ctrl+S compiles | Run > Play starts scripts.");
    Log("Clique no centro da janela para selecionar; segure RMB no centro para olhar e use WASD + Q/E para mover.");
    Log("Gizmo: selecione um objeto na Hierarchy ou viewport; teclas 1/2/3 = mover/rotacionar/escalar. Arraste os eixos no centro da Scene.");
    return true;
}

void ForgeEditorUI::Shutdown()
{
    if (m_scriptRuntime)
        m_scriptRuntime->Stop();
    m_playPreview = false;
    if (m_dx12Initialized)
    {
        ImGui_ImplDX12_Shutdown();
        m_dx12Initialized = false;
    }
    if (m_win32Initialized)
    {
        ImGui_ImplWin32_Shutdown();
        m_win32Initialized = false;
    }
    if (m_contextInitialized)
    {
        ImGui::DestroyContext();
        m_contextInitialized = false;
    }
    m_srvHeap.Reset();
    m_srvSlots.fill(false);
}

void ForgeEditorUI::HandleWin32Message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DROPFILES) { HandleDroppedFiles(reinterpret_cast<HDROP>(wParam)); return; }
    if (m_win32Initialized)
    {
        (void)ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);
    }
}

bool ForgeEditorUI::WantsCaptureKeyboard() const
{
    return m_contextInitialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
}

bool ForgeEditorUI::WantsCaptureMouse() const
{
    return m_contextInitialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

void ForgeEditorUI::AllocateSrvDescriptor(ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
{
    if (!outCpu || !outGpu)
        return;

    outCpu->ptr = 0;
    outGpu->ptr = 0;

    auto* self = info ? static_cast<ForgeEditorUI*>(info->UserData) : nullptr;
    if (!self || !self->m_srvHeap || self->m_srvDescriptorIncrement == 0)
    {
        OutputDebugStringA("Forge Engine: ImGui SRV allocator called before heap initialization.\n");
        return;
    }

    for (UINT slot = 0; slot < static_cast<UINT>(self->m_srvSlots.size()); ++slot)
    {
        if (self->m_srvSlots[slot])
            continue;

        self->m_srvSlots[slot] = true;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = self->m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = self->m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        cpu.ptr += static_cast<SIZE_T>(slot) * self->m_srvDescriptorIncrement;
        gpu.ptr += static_cast<UINT64>(slot) * self->m_srvDescriptorIncrement;
        *outCpu = cpu;
        *outGpu = gpu;
        return;
    }

    // Keep this as a diagnostic instead of forcing an unconditional __debugbreak.
    // A 1024-entry heap should be ample for the editor UI and its dynamic font atlas.
    OutputDebugStringA("Forge Engine ERROR: ImGui SRV descriptor heap exhausted (1024 entries).\n");
}

void ForgeEditorUI::FreeSrvDescriptor(ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
{
    auto* self = info ? static_cast<ForgeEditorUI*>(info->UserData) : nullptr;
    if (!self || !self->m_srvHeap || self->m_srvDescriptorIncrement == 0 || cpu.ptr == 0)
        return;
    const D3D12_CPU_DESCRIPTOR_HANDLE base = self->m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    if (cpu.ptr < base.ptr) return;
    const SIZE_T offset = cpu.ptr - base.ptr;
    if (offset % self->m_srvDescriptorIncrement != 0) return;
    const SIZE_T index = offset / self->m_srvDescriptorIncrement;
    if (index < self->m_srvSlots.size()) self->m_srvSlots[index] = false;
}

void ForgeEditorUI::ApplyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 9.0f;
    style.ChildRounding = 7.0f;
    style.FrameRounding = 7.0f;
    style.PopupRounding = 9.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 7.0f;
    style.TabRounding = 7.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 10.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImVec4(0.89f, 0.92f, 0.97f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.43f, 0.49f, 0.59f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.067f, 0.090f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.045f, 0.055f, 0.075f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.065f, 0.078f, 0.105f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.14f, 0.17f, 0.22f, 0.88f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.15f);
    c[ImGuiCol_FrameBg] = ImVec4(0.095f, 0.115f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.18f, 0.25f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.23f, 0.33f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.045f, 0.055f, 0.075f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.075f, 0.09f, 0.13f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.073f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.025f, 0.03f, 0.042f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.24f, 0.31f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.33f, 0.43f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = Accent();
    c[ImGuiCol_CheckMark] = ImVec4(0.51f, 0.57f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.39f, 0.46f, 0.95f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.53f, 0.60f, 1.00f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.12f, 0.15f, 0.21f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.24f, 0.34f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.27f, 0.31f, 0.48f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.14f, 0.17f, 0.25f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.24f, 0.35f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.30f, 0.47f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.15f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.38f, 0.44f, 0.71f, 1.00f);
    c[ImGuiCol_SeparatorActive] = Accent();
    c[ImGuiCol_ResizeGrip] = ImVec4(0.32f, 0.39f, 0.75f, 0.35f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.48f, 0.93f, 0.75f);
    c[ImGuiCol_ResizeGripActive] = Accent();
    c[ImGuiCol_Tab] = ImVec4(0.075f, 0.092f, 0.13f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.21f, 0.26f, 0.39f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.13f, 0.16f, 0.23f, 1.00f);
    c[ImGuiCol_TabSelectedOverline] = Accent();
    c[ImGuiCol_TabDimmed] = ImVec4(0.06f, 0.073f, 0.10f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.10f, 0.12f, 0.17f, 1.00f);
    c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.28f, 0.34f, 0.55f, 0.60f);
    c[ImGuiCol_TextLink] = ImVec4(0.45f, 0.62f, 1.00f, 1.00f);
    c[ImGuiCol_InputTextCursor] = ImVec4(0.72f, 0.80f, 1.00f, 1.00f);
    c[ImGuiCol_DockingPreview] = ImVec4(0.39f, 0.46f, 0.98f, 0.55f);
    c[ImGuiCol_DockingEmptyBg] = ImVec4(0.04f, 0.05f, 0.07f, 1.00f);
    c[ImGuiCol_PlotLines] = ImVec4(0.43f, 0.55f, 0.96f, 1.00f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.78f, 0.63f, 1.00f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.19f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.12f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.35f, 0.70f, 0.50f);
    c[ImGuiCol_DragDropTarget] = ImVec4(0.55f, 0.75f, 1.0f, 0.95f);
    c[ImGuiCol_NavHighlight] = Accent();
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(0.85f, 0.88f, 1.0f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.02f, 0.025f, 0.04f, 0.55f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.01f, 0.015f, 0.025f, 0.65f);
}

std::filesystem::path ForgeEditorUI::DetectSourceRoot(const std::filesystem::path& executableDirectory) const
{
    std::filesystem::path current = executableDirectory;
    std::error_code ec;
    for (int depth = 0; depth < 10 && !current.empty(); ++depth)
    {
        if (std::filesystem::exists(current / L"CMakeLists.txt", ec) &&
            std::filesystem::exists(current / L"src" / L"Engine" / L"Engine.h", ec))
            return current;
        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return executableDirectory;
}

std::filesystem::path ForgeEditorUI::ResolveStandaloneDataRoot(const std::filesystem::path& executableDirectory,
    const std::wstring& dataFolder) const
{
    std::error_code ec;
    // Prefer the folder named in forge_game.ini, then fall back to the first *_Data folder
    // next to the executable (the layout produced by ExportGame).
    if (!dataFolder.empty())
    {
        const auto named = executableDirectory / dataFolder;
        if (std::filesystem::exists(named / L"Project", ec)) return named;
    }
    for (std::filesystem::directory_iterator it(executableDirectory, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code kindEc;
        if (!it->is_directory(kindEc)) continue;
        if (it->path().extension() != L"_Data") continue;
        if (std::filesystem::exists(it->path() / L"Project", ec)) return it->path();
    }
    return {};
}

void ForgeEditorUI::BeginFrame()
{
    if (!m_dx12Initialized || !m_win32Initialized) return;
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void ForgeEditorUI::Draw(ForgeEngine& engine)
{
    if (!m_contextInitialized) return;

    // Texture decoding/upload errors are reported into the editor console rather than
    // disappearing only into the debugger output stream.
    if (engine.m_lastTextureError != m_lastReportedTextureError)
    {
        if (!engine.m_lastTextureError.empty())
            Log("ERROR: " + engine.m_lastTextureError);
        m_lastReportedTextureError = engine.m_lastTextureError;
    }
    if (engine.m_lastScreenshotResult != m_lastScreenshotResult)
    {
        m_lastScreenshotResult = engine.m_lastScreenshotResult;
        if (!m_lastScreenshotResult.empty()) Log(m_lastScreenshotResult);
        if (m_statusText.rfind("Saving screenshot", 0) == 0) m_statusText = "Ready";
    }

    if (m_showWelcome)
    {
        // Every successful ImGui::NewFrame() must be paired with Render() or
        // EndFrame(), including this early-return path. Without this call the
        // next frame trips ImGui's "Forgot to call Render() or EndFrame()" assert.
        DrawWelcome(engine);
        ImGui::Render();
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const bool control = io.KeyCtrl;
    const bool shift = io.KeyShift;
    if (!io.WantTextInput && !ImGui::IsAnyItemActive())
    {
        if (control && ImGui::IsKeyPressed(ImGuiKey_N, false)) NewScene(engine);
        if (control && ImGui::IsKeyPressed(ImGuiKey_O, false)) OpenSceneDialog(engine);
        if (control && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (shift) SaveSceneDialog(engine);
            else CompileScripts();
        }
        if (control && ImGui::IsKeyPressed(ImGuiKey_I, false)) ImportAssetDialog();
        if (control && ImGui::IsKeyPressed(ImGuiKey_B, false)) m_openExportModal = true;
        if (control && ImGui::IsKeyPressed(ImGuiKey_D, false)) DuplicateSelected(engine);
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
        {
            if (shift) StopPlayMode(engine);
            else StartPlayMode(engine);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelected(engine);
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) m_showStats = !m_showStats;
        if (ImGui::IsKeyPressed(ImGuiKey_F12, false)) RequestViewportScreenshot(engine);
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##ForgeEngineDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    DrawWindowChrome(engine);
    DrawMainMenu(engine);

    const ImGuiID dockspaceId = ImGui::GetID("ForgeEngineDockspace");
    m_dockspaceId = dockspaceId;
    const ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();
    EnsureDefaultDockLayout(dockspaceId, dockspaceSize);
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

    // The center is a passthrough render surface, not an ImGui Scene window.
    // DirectX 12 renders into this rectangle, while the docking panels overlay it.
    ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId);
    if (centralNode != nullptr && centralNode->Size.x > 1.0f && centralNode->Size.y > 1.0f)
    {
        engine.m_sceneViewportX = std::max(0.0f, centralNode->Pos.x - viewport->Pos.x);
        engine.m_sceneViewportY = std::max(0.0f, centralNode->Pos.y - viewport->Pos.y);
        engine.m_sceneViewportWidth = std::max(1.0f, std::min(centralNode->Size.x,
            static_cast<float>(engine.m_width) - engine.m_sceneViewportX));
        engine.m_sceneViewportHeight = std::max(1.0f, std::min(centralNode->Size.y,
            static_cast<float>(engine.m_height) - engine.m_sceneViewportY));
    }
    else
    {
        engine.m_sceneViewportX = 0.0f;
        engine.m_sceneViewportY = 0.0f;
        engine.m_sceneViewportWidth = static_cast<float>(engine.m_width);
        engine.m_sceneViewportHeight = static_cast<float>(engine.m_height);
        centralNode = nullptr;
    }

    // Keep camera projection synchronized with the central region before drawing
    // ImGuizmo. There is no Scene child window: this gizmo overlays the passthrough
    // docking area directly.
    engine.UpdateCamera(0.0f);
    if (centralNode != nullptr && centralNode->Size.x > 1.0f && centralNode->Size.y > 1.0f &&
        engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
    {
        auto& object = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
        // IMPORTANT: ImGuizmo interprets the flat float[16] as column-major.
        // DirectXMath stores row-vector matrices row-major. The same 16 memory
        // values therefore already represent the transposed (column-vector) matrix
        // that ImGuizmo needs. Explicitly transposing here would transpose twice
        // semantically and moves translation out of elements 12..14, breaking the
        // gizmo's position, picking handles, and manipulation.
        XMFLOAT4X4 viewForGizmo = engine.m_view;
        XMFLOAT4X4 projectionForGizmo = engine.m_projection;
        XMFLOAT4X4 matrixForGizmo = object.world;

        const bool validMatrices = std::isfinite(viewForGizmo._11) && std::isfinite(viewForGizmo._22) &&
            std::isfinite(projectionForGizmo._11) && std::isfinite(projectionForGizmo._22) &&
            std::isfinite(matrixForGizmo._11) && std::isfinite(matrixForGizmo._22);
        if (validMatrices)
        {
            ImGuizmo::Enable(true);
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
            ImGuizmo::SetRect(centralNode->Pos.x, centralNode->Pos.y,
                centralNode->Size.x, centralNode->Size.y);
            // Constant clip-space size keeps the handles grabbable at any zoom level.
            ImGuizmo::SetGizmoSizeClipSpace(0.14f);
            ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
            if (engine.m_gizmoMode == ForgeEngine::TransformMode::Rotate)
                operation = ImGuizmo::ROTATE;
            else if (engine.m_gizmoMode == ForgeEngine::TransformMode::Scale)
                operation = ImGuizmo::SCALE;

            // Hold Ctrl for Unity-style snapping while dragging.
            float snapSteps[3];
            snapSteps[0] = snapSteps[1] = snapSteps[2] =
                operation == ImGuizmo::ROTATE ? 5.0f : (operation == ImGuizmo::SCALE ? 0.1f : 0.25f);
            const float* snap = ImGui::GetIO().KeyCtrl ? snapSteps : nullptr;

            const bool changed = ImGuizmo::Manipulate(
                reinterpret_cast<const float*>(&viewForGizmo),
                reinterpret_cast<const float*>(&projectionForGizmo),
                operation, ImGuizmo::LOCAL,
                reinterpret_cast<float*>(&matrixForGizmo), nullptr, snap);
            if (changed)
            {
                // The edited array is already laid out exactly like XMFLOAT4X4's
                // row-major storage for DirectXMath's row-vector world matrix.
                // object.world now includes the parent chain, so convert the edited
                // world matrix back to LOCAL space before decomposing; otherwise a
                // child would snap its local transform to the parent's world offset.
                const XMMATRIX updatedWorld = XMLoadFloat4x4(&matrixForGizmo);
                const XMFLOAT4X4 ancestorWorld = engine.GetAncestorWorld(object);
                const XMMATRIX inverseParent = XMMatrixInverse(nullptr, XMLoadFloat4x4(&ancestorWorld));
                const XMMATRIX localMatrix = updatedWorld * inverseParent;
                XMVECTOR scaleVector{}, rotationQuaternion{}, translationVector{};
                if (XMMatrixDecompose(&scaleVector, &rotationQuaternion, &translationVector, localMatrix))
                {
                    XMStoreFloat3(&object.position, translationVector);
                    XMStoreFloat3(&object.scale, scaleVector);
                    object.rotation = QuaternionToEuler(rotationQuaternion);
                }
                else
                {
                    // Singular drag result (flattened scale): keep rotation/scale and
                    // apply only the translation row to avoid writing NaNs.
                    const XMVECTOR translation = XMVector4Transform(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), localMatrix);
                    XMStoreFloat3(&object.position, translation);
                }
                if (object.lightType == ForgeEngine::LightType::Directional)
                {
                    engine.m_timeOfDayDrivesSun = false;
                    engine.m_directionalObjectTransformDrivesSun = true;
                }
                engine.RebuildWorldTransform(object);
            }
        }

        // Draw the selected camera's actual world-space FOV frustum as viewport lines.
        if (object.isCamera)
        {
            const XMMATRIX world = XMLoadFloat4x4(&object.world);
            const float aspect = std::max(0.1f, engine.m_sceneViewportWidth / std::max(1.0f, engine.m_sceneViewportHeight));
            const float distance = std::clamp(object.cameraFarClip * 0.025f, 3.0f, 12.0f);
            const float halfH = std::tan(object.cameraFieldOfView * Pi / 360.0f) * distance;
            const float halfW = halfH * aspect;
            XMFLOAT3 localCorners[4] = { {-halfW,-halfH,distance}, {-halfW,halfH,distance}, {halfW,halfH,distance}, {halfW,-halfH,distance} };
            XMFLOAT3 worldCorners[4]{};
            for (int k = 0; k < 4; ++k)
                XMStoreFloat3(&worldCorners[k], XMVector3TransformCoord(XMLoadFloat3(&localCorners[k]), world));
            auto projectToScreen = [&](const XMFLOAT3& point, ImVec2& screen) -> bool
            {
                XMVECTOR clip = XMVector4Transform(XMVectorSet(point.x, point.y, point.z, 1.0f), XMLoadFloat4x4(&engine.m_viewProjection));
                const float w = XMVectorGetW(clip);
                if (w <= 0.001f) return false;
                const float x = XMVectorGetX(clip) / w, y = XMVectorGetY(clip) / w;
                screen = ImVec2(centralNode->Pos.x + (x * 0.5f + 0.5f) * centralNode->Size.x,
                    centralNode->Pos.y + (0.5f - y * 0.5f) * centralNode->Size.y);
                return true;
            };
            ImVec2 originScreen{}, cornerScreen[4]{};
            if (projectToScreen(object.position, originScreen))
            {
                ImDrawList* frustumDraw = ImGui::GetForegroundDrawList();
                const ImU32 frustumColor = IM_COL32(75, 205, 255, 235);
                for (int k = 0; k < 4; ++k)
                {
                    if (projectToScreen(worldCorners[k], cornerScreen[k]))
                    {
                        frustumDraw->AddLine(originScreen, cornerScreen[k], frustumColor, 1.8f);
                        const int next = (k + 1) % 4;
                        ImVec2 nextScreen{};
                        if (projectToScreen(worldCorners[next], nextScreen))
                            frustumDraw->AddLine(cornerScreen[k], nextScreen, frustumColor, 1.5f);
                    }
                }
            }
        }

        // Pick in the central render region only. ImGuizmo owns clicks on its handles;
        // clicks elsewhere are converted to a ray in the same central-viewport space.
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool mouseInCenter = mouse.x >= centralNode->Pos.x && mouse.y >= centralNode->Pos.y &&
            mouse.x < centralNode->Pos.x + centralNode->Size.x &&
            mouse.y < centralNode->Pos.y + centralNode->Size.y;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseInCenter &&
            !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
        {
            engine.PickObject(static_cast<int>(mouse.x - centralNode->Pos.x),
                static_cast<int>(mouse.y - centralNode->Pos.y));
        }
    }
    else
    {
        // Still permit ray picking in the center region when there is no selection.
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (centralNode != nullptr && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            mouse.x >= centralNode->Pos.x && mouse.y >= centralNode->Pos.y &&
            mouse.x < centralNode->Pos.x + centralNode->Size.x &&
            mouse.y < centralNode->Pos.y + centralNode->Size.y &&
            !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
        {
            engine.PickObject(static_cast<int>(mouse.x - centralNode->Pos.x),
                static_cast<int>(mouse.y - centralNode->Pos.y));
        }
    }
    // Terrain sculpting is an editor tool, active only while a terrain is selected,
    // the left button is held inside the central render viewport, and ImGui isn't using it.
    if (centralNode != nullptr && engine.m_selectedObject >= 0 &&
        engine.m_selectedObject < static_cast<int>(engine.m_objects.size()) &&
        engine.m_objects[static_cast<size_t>(engine.m_selectedObject)].isTerrain &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (mouse.x >= centralNode->Pos.x && mouse.y >= centralNode->Pos.y &&
            mouse.x < centralNode->Pos.x + centralNode->Size.x &&
            mouse.y < centralNode->Pos.y + centralNode->Size.y)
        {
            XMFLOAT3 rayOrigin{}, rayDirection{};
            const int localX = static_cast<int>(mouse.x - centralNode->Pos.x);
            const int localY = static_cast<int>(mouse.y - centralNode->Pos.y);
            if (engine.BuildMouseRay(localX, localY, rayOrigin, rayDirection))
            {
                auto& terrain = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
                // Intersect the actual deformed height-field triangles, not the old flat Y=0 plane.
                // This prevents sculpting through the terrain or missing it on slopes.
                const XMMATRIX inverseWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&terrain.world));
                XMFLOAT3 localOrigin{}, localDirection{};
                XMStoreFloat3(&localOrigin, XMVector3TransformCoord(XMLoadFloat3(&rayOrigin), inverseWorld));
                XMStoreFloat3(&localDirection, XMVector3TransformNormal(XMLoadFloat3(&rayDirection), inverseWorld));
                float nearestT = FLT_MAX;
                bool hit = false;
                for (size_t tri = 0; tri + 2 < terrain.terrainIndices.size(); tri += 3)
                {
                    const auto& a = terrain.terrainVertices[terrain.terrainIndices[tri]].position;
                    const auto& b = terrain.terrainVertices[terrain.terrainIndices[tri + 1]].position;
                    const auto& c = terrain.terrainVertices[terrain.terrainIndices[tri + 2]].position;
                    float t = 0.0f;
                    if (RayTriangle(localOrigin, localDirection, a, b, c, t) && t < nearestT)
                    { nearestT = t; hit = true; }
                }
                if (hit)
                {
                    XMFLOAT3 localHit{ localOrigin.x + localDirection.x * nearestT,
                        localOrigin.y + localDirection.y * nearestT, localOrigin.z + localDirection.z * nearestT };
                    XMFLOAT3 worldHit{};
                    XMStoreFloat3(&worldHit, XMVector3TransformCoord(XMLoadFloat3(&localHit), XMLoadFloat4x4(&terrain.world)));
                    ApplyTerrainBrush(engine, worldHit.x, worldHit.z, 1.0f / 60.0f);
                }
            }
        }
    }
    else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (m_terrainNeedsUpload && engine.m_selectedObject >= 0 &&
            engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
        {
            auto& terrain = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
            if (terrain.isTerrain && terrain.mesh && !terrain.terrainVertices.empty())
            {
                constexpr int side = 65;
                for (int z = 0; z < side; ++z) for (int x = 0; x < side; ++x)
                {
                    const int left = (std::max)(0, x - 1), right = (std::min)(64, x + 1);
                    const int up = (std::max)(0, z - 1), down = (std::min)(64, z + 1);
                    const auto& a = terrain.terrainVertices[static_cast<size_t>(z * side + left)].position;
                    const auto& b = terrain.terrainVertices[static_cast<size_t>(z * side + right)].position;
                    const auto& c = terrain.terrainVertices[static_cast<size_t>(up * side + x)].position;
                    const auto& d = terrain.terrainVertices[static_cast<size_t>(down * side + x)].position;
                    const XMFLOAT3 tx{ b.x-a.x, b.y-a.y, b.z-a.z };
                    const XMFLOAT3 tz{ d.x-c.x, d.y-c.y, d.z-c.z };
                    XMFLOAT3 n{ tz.y*tx.z - tz.z*tx.y,
                                tz.z*tx.x - tz.x*tx.z,
                                tz.x*tx.y - tz.y*tx.x };
                    const float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
                    if (len > 1.0e-5f) { n.x/=len; n.y/=len; n.z/=len; } else n = {0,1,0};
                    if (n.y < 0.0f) { n.x = -n.x; n.y = -n.y; n.z = -n.z; }
                    terrain.terrainVertices[static_cast<size_t>(z * side + x)].normal = n;
                }
                engine.WaitForGpu();
                engine.CreateMesh(*terrain.mesh, terrain.terrainVertices, terrain.terrainIndices);
            }
        }
        m_terrainNeedsUpload = false;
        m_terrainLastStampX = m_terrainLastStampZ = 1.0e9f;
        m_terrainLastUpload = {};
    }

    // Close the dock host before submitting the docked windows below. This is the
    // matching End() for the host Begin() above, not an early exit from a panel.
    ImGui::End();

    if (m_showHierarchy) DrawHierarchy(engine);
    if (m_showInspector) DrawInspector(engine);
    if (m_showLighting) DrawLightingPanel(engine);
    if (m_showProject) DrawProjectBrowser(engine);
    if (m_showPostProcess) DrawPostProcessSettings(engine);
    if (m_showStats) DrawStatsOverlay(engine);
    DrawExportModal(engine);

    // Detect a normal mouse drag from an asset row and handle model placement
    // ourselves. This is a deliberate fallback for cases where ImGui's drag/drop
    // source/target state is not activated by a table Selectable item.
    if (m_assetPressCandidate && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !m_manualModelDragActive)
    {
        const ImVec2 mouseNow = ImGui::GetIO().MousePos;
        const float dx = mouseNow.x - m_assetPressPosition.x;
        const float dy = mouseNow.y - m_assetPressPosition.y;
        if (dx * dx + dy * dy >= 64.0f) // 8 px drag threshold
        {
            m_manualModelDragActive = true;
            m_manualModelDragPath = m_assetPressPath;
            Log("Manual asset drag started: " + m_manualModelDragPath.filename().string());
        }
    }
    const bool manualModelDragOwnsDropTarget = m_manualModelDragActive;
    if (m_manualModelDragActive && centralNode != nullptr)
    {
        const ImVec2 mouseNow = ImGui::GetIO().MousePos;
        const bool mouseOverScene = mouseNow.x >= centralNode->Pos.x && mouseNow.y >= centralNode->Pos.y &&
            mouseNow.x < centralNode->Pos.x + centralNode->Size.x &&
            mouseNow.y < centralNode->Pos.y + centralNode->Size.y;
        const bool isRelease = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        const bool isEscape = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (mouseOverScene)
        {
            const std::filesystem::path dropped = m_manualModelDragPath;
            if (IsImageExtension(dropped))
            {
                // The manual fallback does not depend on ImGui payload delivery.
                // On release, apply the image to the selected object or create a plane.
                if (isRelease && !isEscape)
                {
                    const std::string texturePath = RelativeAssetPath(dropped);
                    if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
                    {
                        engine.m_objects[static_cast<size_t>(engine.m_selectedObject)].albedoTexturePath = texturePath;
                        Log("Image dropped onto selected object: " + dropped.filename().string());
                    }
                    else
                    {
                        CreatePrimitiveKind(engine, 2);
                        if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
                        {
                            auto& imagePlane = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
                            imagePlane.name = "Image - " + dropped.stem().string();
                            imagePlane.albedoTexturePath = texturePath;
                            imagePlane.position = { engine.m_cameraPosition.x, 0.02f, engine.m_cameraPosition.z + 5.0f };
                            engine.RebuildWorldTransform(imagePlane);
                        }
                        Log("Created image plane from dropped file: " + dropped.filename().string());
                    }
                }
            }
            else if (IsVideoExtension(dropped))
            {
                if (isRelease && !isEscape)
                    Log("Video asset imported: " + dropped.filename().string() + ". Video playback surfaces are not implemented yet; the file is available in Project/Assets/Videos.");
            }
            else if (m_dragPreviewPath != dropped.wstring() || m_dragPreviewIsTexture ||
                (m_dragPreviewObjectIndex < 0 && !m_dragPreviewLoadFailed))
            {
                // There should normally be no other preview at this point, but restore
                // an old texture-preview assignment defensively before starting a model.
                if (m_dragPreviewIsTexture && m_dragPreviewTextureTarget >= 0 &&
                    m_dragPreviewTextureTarget < static_cast<int>(engine.m_objects.size()))
                {
                    engine.m_objects[static_cast<size_t>(m_dragPreviewTextureTarget)].albedoTexturePath =
                        m_dragPreviewPreviousTexture;
                }
                m_dragPreviewPreviousSelection = engine.m_selectedObject;
                m_dragPreviewObjectIndex = -1;
                m_dragPreviewTextureTarget = -1;
                m_dragPreviewPreviousTexture.clear();
                m_dragPreviewPath = dropped.wstring();
                m_dragPreviewIsTexture = false;
                m_dragPreviewLoadFailed = false;
                m_dragPreviewOwnsImportedMesh = false;

                if (InstantiateObj(engine, dropped))
                {
                    m_dragPreviewObjectIndex = static_cast<int>(engine.m_objects.size()) - 1;
                    m_dragPreviewOwnsImportedMesh = true;
                    auto& preview = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                    m_dragPreviewOriginalColor = preview.baseColor;
                    m_dragPreviewOriginalCastShadows = preview.castShadows;
                    preview.isDragPreview = true;
                    preview.isDragPreviewEmissive = true;
                    preview.castShadows = false;
                    preview.baseColor = { 0.22f, 0.78f, 1.0f, 1.0f };
                    Log("3D preview loaded; release mouse to place: " + dropped.filename().string());
                }
                else
                {
                    m_dragPreviewLoadFailed = true;
                    Log("ERROR: Could not create drag preview for " + dropped.filename().string());
                }
            }

            if (m_dragPreviewObjectIndex >= 0 &&
                m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
            {
                const int localMouseX = static_cast<int>(mouseNow.x - centralNode->Pos.x);
                const int localMouseY = static_cast<int>(mouseNow.y - centralNode->Pos.y);
                XMFLOAT3 rayOrigin{}, rayDirection{};
                XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
                if (engine.BuildMouseRay(localMouseX, localMouseY, rayOrigin, rayDirection))
                {
                    const float hitT = std::abs(rayDirection.y) > 0.0001f
                        ? -rayOrigin.y / rayDirection.y : -1.0f;
                    if (hitT > 0.0f && std::isfinite(hitT))
                    {
                        position.x = rayOrigin.x + rayDirection.x * hitT;
                        position.z = rayOrigin.z + rayDirection.z * hitT;
                    }
                    else
                    {
                        constexpr float previewDistance = 8.0f;
                        position.x = rayOrigin.x + rayDirection.x * previewDistance;
                        position.z = rayOrigin.z + rayDirection.z * previewDistance;
                    }
                }
                auto& preview = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                preview.meshRendererEnabled = true;
                preview.position = { position.x, 1.0f, position.z };
                engine.RebuildWorldTransform(preview);
            }
        }
        else if (m_dragPreviewObjectIndex >= 0 &&
                 m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
        {
            // Hide the ghost while the cursor is outside Scene instead of leaving
            // it visually stuck at its last position.
            engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)].meshRendererEnabled = false;
        }

        if (isRelease || isEscape)
        {
            if (!IsImageExtension(m_manualModelDragPath) && !IsVideoExtension(m_manualModelDragPath) &&
                isRelease && !isEscape && mouseOverScene && m_dragPreviewObjectIndex >= 0 &&
                m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
            {
                auto& placed = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                placed.isDragPreview = false;
                placed.isDragPreviewEmissive = false;
                placed.baseColor = m_dragPreviewOriginalColor;
                placed.castShadows = m_dragPreviewOriginalCastShadows;
                engine.RebuildWorldTransform(placed);
                engine.SelectSceneObject(m_dragPreviewObjectIndex);
                Log("Placed model in Scene: " + m_manualModelDragPath.filename().string());
                m_dragPreviewOwnsImportedMesh = false;
                m_dragPreviewObjectIndex = -1;
                m_dragPreviewTextureTarget = -1;
                m_dragPreviewPreviousTexture.clear();
                m_dragPreviewPath.clear();
                m_dragPreviewIsTexture = false;
                m_dragPreviewLoadFailed = false;
            }
            else if (!mouseOverScene)
            {
                Log("Asset drag cancelled: release over the Scene viewport to place/apply it.");
            }
            m_manualModelDragActive = false;
            m_manualModelDragPath.clear();
            m_assetPressCandidate = false;
            m_assetPressPath.clear();
        }
    }
    else if (m_assetPressCandidate && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_assetPressCandidate = false;
        m_assetPressPath.clear();
    }

    // Important: the Scene drop target is evaluated AFTER the Project panel has
    // submitted its drag source. A short-lived overlay window gives ImGui a real
    // hovered item under the cursor (the previous custom target ran too early in
    // the frame and often never received deliveries).
    if (centralNode != nullptr && centralNode->Size.x > 1.0f && centralNode->Size.y > 1.0f &&
        (ImGui::IsDragDropActive() || m_manualModelDragActive || manualModelDragOwnsDropTarget))
    {
        ImGui::SetNextWindowPos(centralNode->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(centralNode->Size, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        const ImGuiWindowFlags dropOverlayFlags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
        ImGui::Begin("##ForgeSceneAssetDropOverlay", nullptr, dropOverlayFlags);
        const ImVec2 dropMouse = ImGui::GetIO().MousePos;
        const bool sceneDropHovered = dropMouse.x >= centralNode->Pos.x &&
            dropMouse.y >= centralNode->Pos.y &&
            dropMouse.x < centralNode->Pos.x + centralNode->Size.x &&
            dropMouse.y < centralNode->Pos.y + centralNode->Size.y;

        if (sceneDropHovered)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 minimum = centralNode->Pos;
            const ImVec2 maximum(centralNode->Pos.x + centralNode->Size.x,
                                 centralNode->Pos.y + centralNode->Size.y);
            drawList->AddRect(minimum, maximum, IM_COL32(70, 205, 255, 235), 2.0f, 0, 3.0f);
            const char* hint = m_dragPreviewObjectIndex >= 0 && !m_dragPreviewIsTexture
                ? "PREVIEW 3D ATIVO  |  SOLTE PARA ADICIONAR"
                : (m_dragPreviewLoadFailed ? "FALHA AO CARREGAR MODELO - VEJA O CONSOLE"
                                           : "SOLTE PARA ADICIONAR A CENA");
            const ImVec2 hintSize = ImGui::CalcTextSize(hint);
            const ImVec2 center((minimum.x + maximum.x - hintSize.x) * 0.5f,
                                minimum.y + 22.0f);
            drawList->AddRectFilled(ImVec2(center.x - 12.0f, center.y - 7.0f),
                                    ImVec2(center.x + hintSize.x + 12.0f, center.y + hintSize.y + 7.0f),
                                    IM_COL32(10, 18, 31, 205), 6.0f);
            drawList->AddText(center, IM_COL32(145, 232, 255, 255), hint);
        }

        auto rollbackDragPreview = [&]()
        {
            const bool hadPreviewState = !m_dragPreviewPath.empty() || m_dragPreviewObjectIndex >= 0 ||
                m_dragPreviewTextureTarget >= 0;
            if (m_dragPreviewIsTexture && m_dragPreviewTextureTarget >= 0 &&
                m_dragPreviewTextureTarget < static_cast<int>(engine.m_objects.size()))
            {
                engine.m_objects[static_cast<size_t>(m_dragPreviewTextureTarget)].albedoTexturePath =
                    m_dragPreviewPreviousTexture;
            }
            ForgeEngine::Mesh* importedPreviewMesh = nullptr;
            if (m_dragPreviewOwnsImportedMesh && m_dragPreviewObjectIndex >= 0 &&
                m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
            {
                importedPreviewMesh = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)].mesh;
                engine.WaitForGpu();
            }
            if (m_dragPreviewObjectIndex >= 0 &&
                m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
            {
                if (m_dragPreviewObjectIndex == static_cast<int>(engine.m_objects.size()) - 1)
                {
                    engine.m_objects.pop_back();
                }
                else
                {
                    engine.m_objects.erase(engine.m_objects.begin() + m_dragPreviewObjectIndex);
                    if (engine.m_selectedObject > m_dragPreviewObjectIndex) --engine.m_selectedObject;
                }
            }
            if (importedPreviewMesh != nullptr && !engine.m_importedMeshes.empty() &&
                engine.m_importedMeshes.back().get() == importedPreviewMesh)
                engine.m_importedMeshes.pop_back();
            if (hadPreviewState)
                engine.m_selectedObject = m_dragPreviewPreviousSelection;
            m_dragPreviewOwnsImportedMesh = false;
            m_dragPreviewObjectIndex = -1;
            m_dragPreviewTextureTarget = -1;
            m_dragPreviewPreviousTexture.clear();
            m_dragPreviewPath.clear();
            m_dragPreviewIsTexture = false;
            m_dragPreviewLoadFailed = false;
        };

        const ImRect sceneDropRect(centralNode->Pos,
            ImVec2(centralNode->Pos.x + centralNode->Size.x, centralNode->Pos.y + centralNode->Size.y));
        if (!manualModelDragOwnsDropTarget &&
            ImGui::BeginDragDropTargetCustom(sceneDropRect, ImGui::GetID("##ForgeSceneAssetDropTarget")))
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                    "FORGE_ASSET_PATH", ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                if (payload->Data != nullptr && payload->DataSize >= static_cast<int>(sizeof(wchar_t)))
                {
                    const auto* widePath = static_cast<const wchar_t*>(payload->Data);
                    const size_t maxCharacters = static_cast<size_t>(payload->DataSize) / sizeof(wchar_t);
                    size_t pathLength = 0;
                    while (pathLength < maxCharacters && widePath[pathLength] != L'\0') ++pathLength;
                    if (pathLength > 0 && pathLength < maxCharacters)
                    {
                        const std::filesystem::path dropped(std::wstring(widePath, pathLength));
                        const std::wstring extension = Lower(dropped.extension().wstring());
                        const bool isModel = extension == L".obj" || extension == L".fbx" ||
                            extension == L".gltf" || extension == L".glb" || extension == L".dae" ||
                            extension == L".stl" || extension == L".ply" || extension == L".3ds";
                        const bool isTexture = IsImageExtension(dropped);
                        const bool delivery = payload->Delivery;
                        const ImVec2 mouse = ImGui::GetIO().MousePos;
                        const int localMouseX = static_cast<int>(mouse.x - centralNode->Pos.x);
                        const int localMouseY = static_cast<int>(mouse.y - centralNode->Pos.y);
                        XMFLOAT3 rayOrigin{}, rayDirection{};
                        XMFLOAT3 groundPosition{ engine.m_cameraPosition.x, 0.0f, engine.m_cameraPosition.z + 5.0f };
                        if (engine.BuildMouseRay(localMouseX, localMouseY, rayOrigin, rayDirection))
                        {
                            // First try the ground plane. If the cursor is above the horizon,
                            // the ray cannot hit y=0; use a forward-ray fallback so the model
                            // preview still follows the mouse instead of sticking at world origin.
                            const float hitT = std::abs(rayDirection.y) > 0.0001f
                                ? -rayOrigin.y / rayDirection.y : -1.0f;
                            if (hitT > 0.0f && std::isfinite(hitT))
                            {
                                groundPosition.x = rayOrigin.x + rayDirection.x * hitT;
                                groundPosition.z = rayOrigin.z + rayDirection.z * hitT;
                            }
                            else
                            {
                                constexpr float previewDistance = 8.0f;
                                groundPosition.x = rayOrigin.x + rayDirection.x * previewDistance;
                                groundPosition.z = rayOrigin.z + rayDirection.z * previewDistance;
                            }
                        }

                        if (isModel)
                        {
                            if (m_dragPreviewPath != dropped.wstring() || m_dragPreviewIsTexture ||
                                (m_dragPreviewObjectIndex < 0 && !m_dragPreviewLoadFailed))
                            {
                                rollbackDragPreview();
                                m_dragPreviewPreviousSelection = engine.m_selectedObject;
                                m_dragPreviewPath = dropped.wstring();
                                m_dragPreviewIsTexture = false;
                                m_dragPreviewLoadFailed = false;
                                if (InstantiateObj(engine, dropped))
                                {
                                    m_dragPreviewObjectIndex = static_cast<int>(engine.m_objects.size()) - 1;
                                    m_dragPreviewOwnsImportedMesh = true;
                                    auto& preview = engine.m_objects.back();
                                    m_dragPreviewOriginalColor = preview.baseColor;
                                    m_dragPreviewOriginalCastShadows = preview.castShadows;
                                    preview.isDragPreview = !delivery;
                                    preview.isDragPreviewEmissive = !delivery;
                                    preview.castShadows = false;
                                    if (!delivery)
                                        preview.baseColor = { 0.27f, 0.78f, 1.0f, 1.0f };
                                    preview.position = { groundPosition.x, 1.0f, groundPosition.z };
                                    engine.RebuildWorldTransform(preview);
                                    if (!delivery)
                                        Log("Model preview active; release to place: " + dropped.filename().string());
                                }
                                else
                                {
                                    m_dragPreviewLoadFailed = true;
                                    m_dragPreviewObjectIndex = -1;
                                }
                            }

                            if (m_dragPreviewObjectIndex >= 0 &&
                                m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
                            {
                                auto& preview = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                                preview.position = { groundPosition.x, 1.0f, groundPosition.z };
                                engine.RebuildWorldTransform(preview);
                                if (delivery)
                                {
                                    preview.isDragPreview = false;
                                    preview.isDragPreviewEmissive = false;
                                    preview.baseColor = m_dragPreviewOriginalColor;
                                    preview.castShadows = m_dragPreviewOriginalCastShadows;
                                    engine.SelectSceneObject(m_dragPreviewObjectIndex);
                                    Log("Model placed in Scene: " + dropped.filename().string());
                                    m_dragPreviewOwnsImportedMesh = false;
                                    m_dragPreviewObjectIndex = -1;
                                    m_dragPreviewPath.clear();
                                    m_dragPreviewIsTexture = false;
                                    m_dragPreviewLoadFailed = false;
                                }
                            }
                        }
                        else if (isTexture)
                        {
                            if (m_dragPreviewPath != dropped.wstring() || !m_dragPreviewIsTexture)
                            {
                                rollbackDragPreview();
                                m_dragPreviewPreviousSelection = engine.m_selectedObject;
                                m_dragPreviewPath = dropped.wstring();
                                m_dragPreviewIsTexture = true;
                                m_dragPreviewLoadFailed = false;
                                m_selectedAssetPath = dropped;
                                if (engine.m_selectedObject >= 0 &&
                                    engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
                                {
                                    m_dragPreviewTextureTarget = engine.m_selectedObject;
                                    auto& target = engine.m_objects[static_cast<size_t>(m_dragPreviewTextureTarget)];
                                    m_dragPreviewPreviousTexture = target.albedoTexturePath;
                                    target.albedoTexturePath = RelativeAssetPath(dropped);
                                }
                                else
                                {
                                    CreatePrimitiveKind(engine, 2);
                                    if (engine.m_selectedObject >= 0 &&
                                        engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
                                    {
                                        m_dragPreviewObjectIndex = engine.m_selectedObject;
                                        m_dragPreviewOwnsImportedMesh = false;
                                        auto& preview = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                                        preview.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                                        m_dragPreviewOriginalColor = preview.baseColor;
                                        m_dragPreviewOriginalCastShadows = preview.castShadows;
                                        preview.isDragPreview = !delivery;
                                        preview.castShadows = false;
                                        preview.position = { groundPosition.x, 0.015f, groundPosition.z };
                                        preview.albedoTexturePath = RelativeAssetPath(dropped);
                                        engine.RebuildWorldTransform(preview);
                                    }
                                }
                            }

                            if (m_dragPreviewIsTexture && m_dragPreviewObjectIndex >= 0 &&
                                m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
                            {
                                auto& preview = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                                preview.position = { groundPosition.x, 0.015f, groundPosition.z };
                                preview.albedoTexturePath = RelativeAssetPath(dropped);
                                engine.RebuildWorldTransform(preview);
                            }

                            if (delivery)
                            {
                                const std::string finalTexturePath = RelativeAssetPath(dropped);
                                if (m_dragPreviewTextureTarget >= 0 &&
                                    m_dragPreviewTextureTarget < static_cast<int>(engine.m_objects.size()))
                                {
                                    engine.m_objects[static_cast<size_t>(m_dragPreviewTextureTarget)].albedoTexturePath = finalTexturePath;
                                }
                                else if (m_dragPreviewObjectIndex >= 0 &&
                                         m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
                                {
                                    auto& placed = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)];
                                    placed.isDragPreview = false;
                                    placed.castShadows = m_dragPreviewOriginalCastShadows;
                                    placed.baseColor = m_dragPreviewOriginalColor;
                                    placed.albedoTexturePath = finalTexturePath;
                                    engine.SelectSceneObject(m_dragPreviewObjectIndex);
                                    m_dragPreviewOwnsImportedMesh = false;
                                }
                                else if (engine.m_selectedObject >= 0 &&
                                         engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
                                {
                                    engine.m_objects[static_cast<size_t>(engine.m_selectedObject)].albedoTexturePath = finalTexturePath;
                                }
                                Log("Texture applied: " + dropped.filename().string());
                                m_dragPreviewObjectIndex = -1;
                                m_dragPreviewTextureTarget = -1;
                                m_dragPreviewPreviousTexture.clear();
                                m_dragPreviewPath.clear();
                                m_dragPreviewIsTexture = false;
                                m_dragPreviewLoadFailed = false;
                            }
                        }
                        else if (delivery)
                        {
                            Log("Unsupported Scene drop. Drop a supported model or image texture.");
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    // If the user releases outside the viewport or presses Escape, restore texture
    // assignments and remove the temporary preview instead of leaving ghost objects.
    if (!ImGui::IsDragDropActive() && !m_manualModelDragActive && !manualModelDragOwnsDropTarget &&
        (!m_dragPreviewPath.empty() || m_dragPreviewObjectIndex >= 0 || m_dragPreviewTextureTarget >= 0))
    {
        if (m_dragPreviewIsTexture && m_dragPreviewTextureTarget >= 0 &&
            m_dragPreviewTextureTarget < static_cast<int>(engine.m_objects.size()))
        {
            engine.m_objects[static_cast<size_t>(m_dragPreviewTextureTarget)].albedoTexturePath =
                m_dragPreviewPreviousTexture;
        }
        ForgeEngine::Mesh* importedPreviewMesh = nullptr;
        if (m_dragPreviewOwnsImportedMesh && m_dragPreviewObjectIndex >= 0 &&
            m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
        {
            importedPreviewMesh = engine.m_objects[static_cast<size_t>(m_dragPreviewObjectIndex)].mesh;
            engine.WaitForGpu();
        }
        if (m_dragPreviewObjectIndex >= 0 &&
            m_dragPreviewObjectIndex < static_cast<int>(engine.m_objects.size()))
        {
            if (m_dragPreviewObjectIndex == static_cast<int>(engine.m_objects.size()) - 1)
                engine.m_objects.pop_back();
            else
                engine.m_objects.erase(engine.m_objects.begin() + m_dragPreviewObjectIndex);
        }
        if (importedPreviewMesh != nullptr && !engine.m_importedMeshes.empty() &&
            engine.m_importedMeshes.back().get() == importedPreviewMesh)
            engine.m_importedMeshes.pop_back();
        engine.m_selectedObject = m_dragPreviewPreviousSelection;
        m_dragPreviewOwnsImportedMesh = false;
        m_dragPreviewObjectIndex = -1;
        m_dragPreviewTextureTarget = -1;
        m_dragPreviewPreviousTexture.clear();
        m_dragPreviewPath.clear();
        m_dragPreviewIsTexture = false;
        m_dragPreviewLoadFailed = false;
        m_dragPreviewOwnsImportedMesh = false;
    }

    if (m_showConsole) DrawConsole();
    DrawAboutWindow();

    if (m_showShortcuts)
    {
        ImGui::SetNextWindowSize(ImVec2(460.0f, 360.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Keyboard Shortcuts", &m_showShortcuts))
        {
            if (ImGui::BeginTable("##ShortcutsTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableHeadersRow();
                const char* rows[][2] = {
                    {"Move camera", "Hold RMB + WASD"},
                    {"Camera vertical", "Hold RMB + Q / E"},
                    {"Select object", "Left mouse button"},
                    {"Translate gizmo", "1"},
                    {"Rotate gizmo", "2"},
                    {"Scale gizmo", "3"},
                    {"New scene", "Ctrl + N"},
                    {"Open scene", "Ctrl + O"},
                    {"Compile C# scripts", "Ctrl + S"},
                    {"Save scene as", "Ctrl + Shift + S"},
                    {"Play scripts", "F5"},
                    {"Stop play", "Shift + F5"},
                    {"Import asset", "Ctrl + I"},
                    {"Duplicate object", "Ctrl + D"},
                    {"Delete selected", "Delete"},
                    {"Save screenshot (PNG)", "F12"},
                    {"Toggle stats overlay", "F3"},
                    {"Camera view", "Select a Camera object"}
                };
                for (const auto& row : rows)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(row[0]);
                    ImGui::TableSetColumnIndex(1); ImGui::TextColored(Accent(), "%s", row[1]);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
}

void ForgeEditorUI::DrawWindowChrome(ForgeEngine& engine)
{
    const float height = 30.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##ForgeCustomCaption", ImVec2(0.0f, height), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Solid dark background matching Unreal Engine's title bar.
    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(30, 30, 30, 255));
    // Thin separator at the bottom.
    draw->AddLine(ImVec2(origin.x, origin.y + height - 1.0f),
        ImVec2(origin.x + width, origin.y + height - 1.0f), IM_COL32(55, 55, 55, 255), 1.0f);

    // Project name on the left.
    ImGui::SetCursorPos(ImVec2(12.0f, 6.0f));
    ImGui::TextColored(Accent(), "FORGE");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetCursorPosY(6.0f);
    ImGui::TextDisabled("DX12 ENGINE  |  %s", m_projectRoot.filename().string().c_str());

    // Window control buttons — small, flat, UE-style.
    const float btnW = 34.0f, btnH = 24.0f, gap = 2.0f;
    const float buttonsWidth = btnW * 3.0f + gap * 2.0f + 6.0f;
    ImGui::SetCursorPos(ImVec2((std::max)(6.0f, width - buttonsWidth), 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
    if (ImGui::Button(ICON_MINIMIZE, ImVec2(btnW, btnH))) ShowWindow(engine.m_hwnd, SW_MINIMIZE);
    ImGui::SameLine(0.0f, gap);
    if (ImGui::Button(IsZoomed(engine.m_hwnd) ? ICON_RESTORE : ICON_MAXIMIZE, ImVec2(btnW, btnH)))
        ShowWindow(engine.m_hwnd, IsZoomed(engine.m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    ImGui::SameLine(0.0f, gap);
    ImGui::PopStyleColor(3);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.10f, 0.10f, 1.0f));
    const bool closePressed = ImGui::Button(ICON_CLOSE, ImVec2(btnW, btnH));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    if (closePressed) PostMessageW(engine.m_hwnd, WM_CLOSE, 0, 0);

    // Drag only the blank left part; buttons remain ordinary ImGui hit targets.
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool dragRegion = mouse.x >= origin.x && mouse.x < origin.x + width - buttonsWidth &&
        mouse.y >= origin.y && mouse.y < origin.y + height;
    if (dragRegion && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        ShowWindow(engine.m_hwnd, IsZoomed(engine.m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    else if (dragRegion && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ReleaseCapture();
        POINT pt{}; GetCursorPos(&pt);
        SendMessageW(engine.m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void ForgeEditorUI::DrawMainMenu(ForgeEngine& engine)
{
    if (ImGui::BeginMenuBar())
    {
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem(ICON_ADD "  New Scene", "Ctrl+N")) NewScene(engine);
        if (ImGui::MenuItem(ICON_FOLDER "  Open Scene...", "Ctrl+O")) OpenSceneDialog(engine);
        if (ImGui::MenuItem(ICON_SAVE "  Save Scene"))
        {
            if (m_currentScenePath.empty()) SaveSceneDialog(engine);
            else SaveScene(engine, m_currentScenePath);
        }
        if (ImGui::MenuItem(ICON_SAVE "  Save Scene As...", "Ctrl+Shift+S")) SaveSceneDialog(engine);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_CAMERA "  Save Screenshot (PNG)", "F12")) RequestViewportScreenshot(engine);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_PACKAGE "  Export Game...", "Ctrl+B")) m_openExportModal = true;
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_ADD "  Import Asset...", "Ctrl+I")) ImportAssetDialog();
        if (ImGui::MenuItem(ICON_HOME "  Project Hub")) m_showWelcome = true;
        if (ImGui::MenuItem(ICON_FOLDER "  Open Project Folder"))
            ShellExecuteW(nullptr, L"open", m_projectRoot.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_CLOSE "  Exit")) PostMessageW(engine.m_hwnd, WM_CLOSE, 0, 0);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create Object"))
    {
        if (ImGui::MenuItem(ICON_ADD "  Cube")) CreatePrimitiveKind(engine, 0);
        if (ImGui::MenuItem(ICON_ADD "  Sphere")) CreatePrimitiveKind(engine, 1);
        if (ImGui::MenuItem(ICON_ADD "  Plane")) CreatePrimitiveKind(engine, 2);
        if (ImGui::MenuItem(ICON_ADD "  Cylinder")) CreatePrimitiveKind(engine, 3);
        if (ImGui::MenuItem(ICON_ADD "  Cone")) CreatePrimitiveKind(engine, 4);
        if (ImGui::MenuItem(ICON_ADD "  Torus")) CreatePrimitiveKind(engine, 5);
        if (ImGui::MenuItem(ICON_ADD "  Terrain")) CreateTerrainObject(engine);
        if (ImGui::MenuItem(ICON_CAMERA "  Camera")) CreateCameraObject(engine);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_SETTINGS "  Post Processing Settings...")) m_showPostProcess = true;
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_LIGHT "  Directional Light")) CreateLightObject(engine, 1);
        if (ImGui::MenuItem(ICON_LIGHT "  Point Light")) CreateLightObject(engine, 2);
        if (ImGui::MenuItem(ICON_LIGHT "  Spot Light")) CreateLightObject(engine, 3);
        ImGui::Separator();
        const bool hasSelection = engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size());
        if (ImGui::MenuItem(ICON_COPY "  Duplicate Selected", "Ctrl+D", false, hasSelection)) DuplicateSelected(engine);
        if (ImGui::MenuItem(ICON_DELETE "  Delete Selected", "Delete", false, hasSelection)) DeleteSelected(engine);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Assets"))
    {
        if (ImGui::MenuItem(ICON_ADD "  Import Asset...", "Ctrl+I")) ImportAssetDialog();
        if (ImGui::MenuItem(ICON_FOLDER "  Open Assets Folder"))
            ShellExecuteW(nullptr, L"open", m_assetsRoot.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Run"))
    {
        if (!m_playPreview)
        {
            if (ImGui::MenuItem(ICON_PLAY "  Play", "F5")) StartPlayMode(engine);
        }
        else if (ImGui::MenuItem(ICON_STOP "  Stop", "Shift+F5")) StopPlayMode(engine);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_REFRESH "  Compile C# Scripts", "Ctrl+S")) CompileScripts();
        if (ImGui::MenuItem(ICON_EDIT "  Open Scripts Project"))
            ShellExecuteW(nullptr, L"open", (m_projectRoot / L"Scripts" / L"ForgeGameScripts.csproj").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem(ICON_LIST "  Scene Hierarchy", nullptr, &m_showHierarchy);
        ImGui::MenuItem(ICON_EDIT "  Object Inspector", nullptr, &m_showInspector);
        ImGui::MenuItem(ICON_LIGHT "  Lighting", nullptr, &m_showLighting);
        ImGui::MenuItem(ICON_FOLDER "  Project Files", nullptr, &m_showProject);
        ImGui::MenuItem(ICON_SETTINGS "  Post Processing", nullptr, &m_showPostProcess);
        ImGui::MenuItem(ICON_COMMAND "  Console", nullptr, &m_showConsole);
        ImGui::MenuItem(ICON_CHART "  Stats Overlay", "F3", &m_showStats);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_REPAIR "  Reset Dock Layout")) ResetDockLayout();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem(ICON_COMMAND "  Keyboard Shortcuts")) m_showShortcuts = true;
        if (ImGui::MenuItem(ICON_HELP "  About Forge Engine")) m_showAbout = true;
        ImGui::EndMenu();
    }

    ImGui::SameLine();
    if (!m_playPreview)
    {
        if (ImGui::SmallButton(ICON_PLAY "  Play")) StartPlayMode(engine);
    }
    else
    {
        if (ImGui::SmallButton(ICON_STOP "  Stop")) StopPlayMode(engine);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_REFRESH "  Compile C#")) CompileScripts();
    ImGui::SameLine(ImGui::GetWindowWidth() - 188.0f);
    ImGui::TextColored(Accent(), "FORGE ENGINE");
    ImGui::SameLine();
    ImGui::TextDisabled("v0.5.10");
    ImGui::EndMenuBar();
    }
}

void ForgeEditorUI::EnsureDefaultDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
    if (m_dockLayoutInitialized)
        return;

    ImGuiDockNode* existingNode = ImGui::DockBuilderGetNode(dockspaceId);
    if (existingNode != nullptr && !m_forceDefaultDockLayout)
    {
        // Dear ImGui restored the user's saved docking layout from EditorLayout.ini.
        m_dockLayoutInitialized = true;
        return;
    }

    // Use the explicit output IDs supported by DockBuilderSplitNode. This avoids
    // relying on the function's return value when constructing the dock tree.
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);

    const ImVec2 safeSize(
        std::max(size.x, 640.0f),
        std::max(size.y, 420.0f));
    ImGui::DockBuilderSetNodeSize(dockspaceId, safeSize);

    ImGuiID center = dockspaceId;
    ImGuiID left = 0;
    ImGuiID right = 0;
    ImGuiID bottom = 0;

    ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, &left, &center);
    if (left == 0 || center == 0)
    {
        OutputDebugStringA("Forge Engine: could not create left dock node; keeping a single scene dockspace.\n");
        ImGui::DockBuilderFinish(dockspaceId);
        m_dockLayoutInitialized = true;
        m_forceDefaultDockLayout = false;
        return;
    }

    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.27f, &bottom, &center);

    if (left != 0) ImGui::DockBuilderDockWindow("Scene Hierarchy", left);
    if (right != 0) ImGui::DockBuilderDockWindow("Object Inspector", right);
    if (bottom != 0)
    {
        ImGui::DockBuilderDockWindow("Project Files", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
    }

    ImGui::DockBuilderFinish(dockspaceId);
    m_dockLayoutInitialized = true;
    m_forceDefaultDockLayout = false;
}

void ForgeEditorUI::DrawHierarchy(ForgeEngine& engine)
{
    const bool windowVisible = ImGui::Begin(ICON_LIST "  Scene Hierarchy", &m_showHierarchy);
    if (windowVisible)
    {
    ImGui::TextDisabled("SCENE OBJECTS");
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", static_cast<int>(engine.m_objects.size()));
    ImGui::Separator();

    if (ImGui::Button("+ Cube", ImVec2(-1.0f, 0.0f))) CreatePrimitive(engine, false);
    if (ImGui::Button("+ Sphere", ImVec2(-1.0f, 0.0f))) CreatePrimitive(engine, true);
    if (ImGui::Button("Model Library", ImVec2(-1.0f, 0.0f))) ImGui::OpenPopup("ForgeModelLibrary");
    if (ImGui::BeginPopup("ForgeModelLibrary")) {
        ImGui::TextDisabled("BUILT-IN MODELS");
        if (ImGui::MenuItem("Plane")) CreatePrimitiveKind(engine, 2);
        if (ImGui::MenuItem("Cylinder")) CreatePrimitiveKind(engine, 3);
        if (ImGui::MenuItem("Cone")) CreatePrimitiveKind(engine, 4);
        if (ImGui::MenuItem("Torus")) CreatePrimitiveKind(engine, 5);
        if (ImGui::MenuItem("Terrain")) CreateTerrainObject(engine);
        if (ImGui::MenuItem("Camera")) CreateCameraObject(engine);
        if (ImGui::MenuItem("Cube")) CreatePrimitiveKind(engine, 0);
        if (ImGui::MenuItem("Sphere")) CreatePrimitiveKind(engine, 1);
        ImGui::EndPopup();
    }
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##HierarchyFilter", m_hierarchyFilter, sizeof(m_hierarchyFilter));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter the hierarchy by object name. Matches include parents of matching children.");

    bool deleteRequested = false;
    int deleteIndex = -1;
    int duplicateIndex = -1;
    ImGuiChildScope sceneObjectList("##SceneObjectList", ImVec2(0.0f, 0.0f), false);
    if (sceneObjectList.Visible())
    {
        const int count = static_cast<int>(engine.m_objects.size());
        std::vector<std::vector<int>> children(static_cast<size_t>(count));
        std::vector<int> roots;
        for (int i = 0; i < count; ++i)
        {
            const int parent = engine.m_objects[static_cast<size_t>(i)].parentIndex;
            if (parent >= 0 && parent < count && parent != i) children[static_cast<size_t>(parent)].push_back(i);
            else roots.push_back(i);
        }
        auto toLowerAscii = [](std::string value)
        {
            for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return value;
        };
        const std::string filter = toLowerAscii(std::string(m_hierarchyFilter));
        const bool filtering = !filter.empty();
        auto nameMatches = [&](int idx) -> bool
        {
            if (!filtering) return true;
            return toLowerAscii(engine.m_objects[static_cast<size_t>(idx)].name).find(filter) != std::string::npos;
        };
        std::vector<int8_t> subtreeMatchCache(static_cast<size_t>(count), -1);
        std::function<bool(int)> subtreeMatches = [&](int i) -> bool
        {
            if (i < 0 || i >= count) return false;
            int8_t& memo = subtreeMatchCache[static_cast<size_t>(i)];
            if (memo >= 0) return memo != 0;
            memo = 0;
            bool ok = nameMatches(i);
            for (int child : children[static_cast<size_t>(i)])
                if (subtreeMatches(child)) ok = true;
            memo = ok ? 1 : 0;
            return ok;
        };
        std::vector<bool> visited(static_cast<size_t>(count), false);
        auto wouldCreateCycle = [&](int child, int parent)
        {
            if (child == parent) return true;
            int cursor = parent;
            for (int step = 0; step <= count && cursor >= 0 && cursor < count; ++step)
            {
                if (cursor == child) return true;
                cursor = engine.m_objects[static_cast<size_t>(cursor)].parentIndex;
            }
            return false;
        };
        std::function<void(int)> drawNode = [&](int i)
        {
            if (i < 0 || i >= count || visited[static_cast<size_t>(i)]) return;
            visited[static_cast<size_t>(i)] = true;
            if (filtering && !subtreeMatches(i)) return;
            auto& object = engine.m_objects[static_cast<size_t>(i)];
            ImGui::PushID(i + 1000);
            const bool hasChildren = !children[static_cast<size_t>(i)].empty();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
            if (engine.m_selectedObject == i) flags |= ImGuiTreeNodeFlags_Selected;
            if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            const char* icon = object.lightType == ForgeEngine::LightType::Directional ? "[SUN] " :
                object.lightType == ForgeEngine::LightType::Point ? "[PNT] " :
                object.lightType == ForgeEngine::LightType::Spot ? "[SPT] " :
                object.isCamera ? "[CAM] " : object.isTerrain ? "[TRN] " :
                object.mesh == &engine.m_sphereMesh ? "[SPH] " : "[OBJ] ";
            std::string label = std::string(icon) + object.name;
            if (!object.meshRendererEnabled) label = "[HID] " + object.name;
            const bool opened = ImGui::TreeNodeEx("##HierarchyNode", flags, "%s", label.c_str());
            if (ImGui::IsItemClicked()) engine.SelectSceneObject(i);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                // Frame the object at its world position, not its local one.
                const XMFLOAT3 worldPos{ object.world._41, object.world._42, object.world._43 };
                engine.m_cameraPosition = { worldPos.x, worldPos.y + 2.0f, worldPos.z - 5.0f };
                engine.m_cameraYaw = 0.0f; engine.m_cameraPitch = -0.15f;
            }
            if (ImGui::BeginDragDropSource())
            {
                const int payloadIndex = i;
                ImGui::SetDragDropPayload("FORGE_HIERARCHY_OBJECT", &payloadIndex, sizeof(payloadIndex));
                ImGui::Text("%s", object.name.c_str());
                ImGui::TextDisabled("Drop onto another object to parent it");
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FORGE_HIERARCHY_OBJECT"))
                {
                    const int childIndex = *static_cast<const int*>(payload->Data);
                    if (childIndex >= 0 && childIndex < count && !wouldCreateCycle(childIndex, i))
                    {
                        engine.m_objects[static_cast<size_t>(childIndex)].parentIndex = i;
                        engine.RebuildWorldTransform(engine.m_objects[static_cast<size_t>(childIndex)]);
                        Log("Set '" + engine.m_objects[static_cast<size_t>(childIndex)].name + "' as child of '" + object.name + "'.");
                    }
                    else Log("ERROR: A hierarchy parent cannot be its own child or descendant.");
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem("ObjectContext"))
            {
                if (ImGui::MenuItem("Select")) engine.SelectSceneObject(i);
                const int selected = engine.m_selectedObject;
                const bool canParent = selected >= 0 && selected < count && selected != i && !wouldCreateCycle(selected, i);
                if (ImGui::MenuItem("Parent selected object here", nullptr, false, canParent))
                {
                    auto& child = engine.m_objects[static_cast<size_t>(selected)];
                    child.parentIndex = i;
                    engine.RebuildWorldTransform(child);
                }
                if (ImGui::MenuItem("Unparent", nullptr, false, object.parentIndex >= 0))
                {
                    object.parentIndex = -1;
                    engine.RebuildWorldTransform(object);
                }
                if (ImGui::MenuItem("Duplicate")) duplicateIndex = i;
                if (ImGui::MenuItem("Delete")) { deleteRequested = true; deleteIndex = i; }
                ImGui::EndPopup();
            }
            if (opened && hasChildren)
            {
                for (int child : children[static_cast<size_t>(i)]) drawNode(child);
                ImGui::TreePop();
            }
            ImGui::PopID();
        };
        for (int rootIndex : roots) drawNode(rootIndex);
        // Broken/cyclic hierarchy references are still visible instead of disappearing.
        for (int i = 0; i < count; ++i) if (!visited[static_cast<size_t>(i)]) drawNode(i);
    }
    sceneObjectList.EndNow();
    if (duplicateIndex >= 0)
    {
        engine.SelectSceneObject(duplicateIndex);
        DuplicateSelected(engine);
    }
    if (deleteRequested)
    {
        engine.SelectSceneObject(deleteIndex);
        DeleteSelected(engine);
    }
    }
    ImGui::End();

}

void ForgeEditorUI::DrawInspector(ForgeEngine& engine)
{
    const bool windowVisible = ImGui::Begin(ICON_EDIT "  Object Inspector", &m_showInspector);
    if (windowVisible)
    {
    if (engine.m_selectedObject < 0 || engine.m_selectedObject >= static_cast<int>(engine.m_objects.size()))
    {
        ImGui::Spacing();
        ImGui::TextDisabled("NO OBJECT SELECTED");
        ImGui::Separator();
        ImGui::TextWrapped("Select an object in Scene Hierarchy or click a mesh in the central 3D view to edit its properties.");
        ImGui::Spacing();
        if (ImGui::Button("Create Cube", ImVec2(-1.0f, 0.0f))) CreatePrimitive(engine, false);
        if (ImGui::Button("Create Sphere", ImVec2(-1.0f, 0.0f))) CreatePrimitive(engine, true);
    }
    else
    {
    auto& object = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
    char nameBuffer[192]{};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object.name.c_str());
    if (ImGui::InputText("Object Name", nameBuffer, sizeof(nameBuffer))) object.name = nameBuffer;
    ImGui::TextDisabled("ID %03d   |   %s", engine.m_selectedObject,
        object.mesh == &engine.m_sphereMesh ? "Sphere Mesh" : "Cube Mesh");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float position[3]{ object.position.x, object.position.y, object.position.z };
        if (ImGui::InputFloat3("Position", position, "%.3f"))
            object.position = { position[0], position[1], position[2] };

        float rotation[3]{ object.rotation.x * 180.0f / Pi,
            object.rotation.y * 180.0f / Pi, object.rotation.z * 180.0f / Pi };
        if (ImGui::InputFloat3("Rotation", rotation, "%.1f deg"))
        {
            object.rotation = { rotation[0] * Pi / 180.0f, rotation[1] * Pi / 180.0f, rotation[2] * Pi / 180.0f };
            if (object.lightType == ForgeEngine::LightType::Directional)
            {
                engine.m_timeOfDayDrivesSun = false;
                engine.m_directionalObjectTransformDrivesSun = true;
            }
        }

        float scale[3]{ object.scale.x, object.scale.y, object.scale.z };
        if (ImGui::InputFloat3("Scale", scale, "%.3f"))
            object.scale = { std::max(0.01f, scale[0]), std::max(0.01f, scale[1]), std::max(0.01f, scale[2]) };

        if (ImGui::Button("Reset Transform", ImVec2(-1.0f, 0.0f)))
        {
            object.position = { 0.0f, 1.0f, 0.0f };
            object.rotation = { 0.0f, 0.0f, 0.0f };
            object.scale = { 1.0f, 1.0f, 1.0f };
        }
        engine.RebuildWorldTransform(object);
    }

    if (object.isTerrain && ImGui::CollapsingHeader("Terrain Sculpting", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* modes[] = { "Raise", "Lower", "Smooth", "Flatten" };
        ImGui::Combo("Brush", &m_terrainBrushMode, modes, IM_ARRAYSIZE(modes));
        ImGui::SliderFloat("Brush Radius", &m_terrainBrushRadius, 0.5f, 12.0f, "%.1f m");
        ImGui::SliderFloat("Brush Strength", &m_terrainBrushStrength, 0.1f, 8.0f, "%.1f");
        if (m_terrainBrushMode == 3) ImGui::DragFloat("Flatten Height", &m_terrainFlattenHeight, 0.1f);
        ImGui::TextWrapped("Drag with the left mouse button over the viewport to sculpt. Mesh uploads are throttled to reduce GPU stalls.");
    }

    if (object.isCamera && ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Orthographic", &object.cameraOrthographic);
        if (object.cameraOrthographic)
        {
            ImGui::DragFloat("Orthographic Size", &object.cameraOrthographicSize, 0.05f, 0.1f, 500.0f, "%.2f");
        }
        else
        {
            ImGui::SliderFloat("Field of View", &object.cameraFieldOfView, 20.0f, 110.0f, "%.1f deg");
        }
        ImGui::DragFloat("Near Clip", &object.cameraNearClip, 0.01f, 0.01f, 5.0f, "%.2f");
        ImGui::DragFloat("Far Clip", &object.cameraFarClip, 1.0f, 10.0f, 5000.0f, "%.0f");
        ImGui::DragInt("Depth", &object.cameraDepth, 1.0f, -100, 100);
        ImGui::ColorEdit4("Background", &object.cameraBackgroundColor.x);
        ImGui::Checkbox("Draw Skybox", &object.cameraDrawSkybox);
        ImGui::Checkbox("Draw Gizmos", &object.cameraDrawGizmos);
        ImGui::TextDisabled("Culling Mask");
        for (int layerIndex = 0; layerIndex < 8; ++layerIndex)
        {
            ImGui::PushID(layerIndex);
            bool layerEnabled = (object.cameraCullingMask & (1 << layerIndex)) != 0;
            if (ImGui::Checkbox("##Mask", &layerEnabled))
                object.cameraCullingMask = layerEnabled
                    ? (object.cameraCullingMask | (1 << layerIndex))
                    : (object.cameraCullingMask & ~(1 << layerIndex));
            ImGui::SameLine();
            ImGui::Text("Layer %d", layerIndex);
            ImGui::PopID();
        }
        ImGui::Separator();
        if (ImGui::Button("Look Through This Camera", ImVec2(-1.0f, 0.0f)))
        {
            engine.m_viewportCameraObjectIndex = engine.m_selectedObject;
            engine.m_cameraPosition = object.position;
            engine.m_cameraYaw = object.rotation.y;
            engine.m_cameraPitch = std::clamp(-object.rotation.x, -1.50f, 1.50f);
            engine.m_editorCameraFov = std::clamp(object.cameraFieldOfView, 20.0f, 110.0f);
            engine.RebuildWorldTransform(object);
            engine.UpdateCamera(0.0f);
            Log("Active camera now drives the Scene viewport.");
        }
        ImGui::TextDisabled(engine.m_viewportCameraObjectIndex == engine.m_selectedObject
            ? "This camera is currently rendering the Scene viewport."
            : "Use this to render the Scene viewport through the camera, Unity-style.");
    }

    if (object.lightType != ForgeEngine::LightType::None &&
        ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* typeName = object.lightType == ForgeEngine::LightType::Directional ? "Directional Light / Sun" :
            object.lightType == ForgeEngine::LightType::Point ? "Point Light" : "Spot Light";
        ImGui::TextColored(Accent(), "%s", typeName);
        ImGui::Checkbox("Light Enabled", &object.lightEnabled);
        ImGui::ColorEdit3("Light Color", &object.lightColor.x);
        ImGui::SliderFloat("Intensity##LightComponent", &object.lightIntensity, 0.0f,
            object.lightType == ForgeEngine::LightType::Directional ? 10.0f : 180.0f, "%.2f");
        if (object.lightType != ForgeEngine::LightType::Directional)
            ImGui::SliderFloat("Range##LightComponent", &object.lightRange, 0.5f, 80.0f, "%.1f m");
        if (object.lightType == ForgeEngine::LightType::Spot)
        {
            ImGui::SliderFloat("Inner Cone##LightComponent", &object.lightInnerAngle, 1.0f, 87.0f, "%.0f deg");
            ImGui::SliderFloat("Outer Cone##LightComponent", &object.lightOuterAngle, 2.0f, 89.0f, "%.0f deg");
            if (object.lightOuterAngle <= object.lightInnerAngle)
                object.lightOuterAngle = std::min(89.0f, object.lightInnerAngle + 1.0f);
        }
        if (object.lightType == ForgeEngine::LightType::Directional)
        {
            ImGui::TextWrapped("Rotate this object to steer the sun when the Time of Day control is not driving it.");
            ImGui::TextDisabled("The sky, sunlight and directional shadow follow this light.");
        }
        else if (object.lightType == ForgeEngine::LightType::Spot)
            ImGui::TextDisabled("Rotate the object to aim the spotlight cone.");
        engine.RebuildWorldTransform(object);
    }

    if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &object.meshRendererEnabled);
        ImGui::Checkbox("Cast Shadows", &object.castShadows);
        const char* meshLabel = "Cube / Imported Mesh";
        if (object.lightType == ForgeEngine::LightType::Directional) meshLabel = "Directional Light Gizmo";
        else if (object.lightType == ForgeEngine::LightType::Point) meshLabel = "Point Light Gizmo";
        else if (object.lightType == ForgeEngine::LightType::Spot) meshLabel = "Spot Light Gizmo";
        else if (object.mesh == &engine.m_sphereMesh) meshLabel = "Sphere";
        else if (object.mesh == &engine.m_primitiveMeshes[0]) meshLabel = "Plane";
        else if (object.mesh == &engine.m_primitiveMeshes[1]) meshLabel = "Cylinder";
        else if (object.mesh == &engine.m_primitiveMeshes[2]) meshLabel = "Cone";
        else if (object.mesh == &engine.m_primitiveMeshes[3]) meshLabel = "Torus";
        ImGui::TextDisabled("Mesh: %s", meshLabel);
        ImGui::TextDisabled("Renderer: DirectX 12 / PBR");
    }

    if (object.hasMaterial && ImGui::CollapsingHeader("Material / PBR", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit4("Base Color", &object.baseColor.x,
            ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Uint8);
        ImGui::SliderFloat("Metallic", &object.metallic, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Roughness", &object.roughness, 0.045f, 1.0f, "%.3f");
        ImGui::SliderFloat("Normal Map Scale", &object.normalMapScale, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("AO Strength", &object.occlusionStrength, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("Double Sided", &object.doubleSided);
        ImGui::TextColored(Accent(), "Emission");
        ImGui::ColorEdit3("Emissive Color", &object.emissiveColor.x,
            ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Uint8);
        ImGui::SliderFloat("Emissive Intensity", &object.emissiveIntensity, 0.0f, 12.0f, "%.2f");
        ImGui::Separator();
        ImGui::TextColored(Accent(), "Texture Coordinates (UV)");
        ImGui::DragFloat2("Tiling U / V", &object.textureTiling.x, 0.025f, -100.0f, 100.0f, "%.3f");
        if (std::abs(object.textureTiling.x) < 0.001f) object.textureTiling.x = 0.001f;
        if (std::abs(object.textureTiling.y) < 0.001f) object.textureTiling.y = 0.001f;
        ImGui::DragFloat2("Offset U / V", &object.textureOffset.x, 0.01f, -100.0f, 100.0f, "%.3f");
        if (ImGui::SmallButton("Reset UV")) { object.textureTiling = {1.0f, 1.0f}; object.textureOffset = {0.0f, 0.0f}; }
        ImGui::Separator();
        auto textureRow = [&](const char* label, std::string& path, TextureSlot slot)
        {
            ImGui::PushID(label);
            const ImVec2 slotMin = ImGui::GetCursorScreenPos();
            ImGui::TextDisabled("%s", label);
            ImGui::TextWrapped("%s", path.empty() ? "Not assigned — drop an image here" : path.c_str());
            // The whole label + current-path area is a drop target, even though TextWrapped
            // does not itself own a stable drag/drop ID.
            ImVec2 slotMax = ImGui::GetItemRectMax();
            const ImVec2 windowPosition = ImGui::GetWindowPos();
            const ImVec2 contentRegionMax = ImGui::GetWindowContentRegionMax();
            slotMax.x = (std::max)(slotMax.x, windowPosition.x + contentRegionMax.x);
            if (ImGui::BeginDragDropTargetCustom(ImRect(slotMin, slotMax), ImGui::GetID("##TextureSlotDrop"))) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FORGE_ASSET_PATH")) {
                    const auto* widePath = static_cast<const wchar_t*>(payload->Data);
                    const size_t characterCount = payload->DataSize > 0
                        ? static_cast<size_t>(payload->DataSize) / sizeof(wchar_t) : 0;
                    size_t pathLength = 0;
                    if (widePath != nullptr)
                        while (pathLength < characterCount && widePath[pathLength] != L'\0') ++pathLength;
                    if (widePath && pathLength > 0 && pathLength < characterCount) {
                        const std::filesystem::path source(std::wstring(widePath, pathLength));
                        if (IsImageExtension(source)) {
                            const auto imported = ImportFile(source, L"Textures");
                            if (!imported.empty())
                            {
                                path = RelativeAssetPath(imported);
                                m_selectedAssetPath = imported;
                                Log(std::string("Assigned texture reference to ") + label + ": " + imported.filename().string());
                            }
                            else Log(std::string("ERROR: Could not import texture for ") + label + ". Check the Console for details.");
                        } else Log("This material slot accepts image textures only.");
                    } else Log("ERROR: The dragged texture path was invalid.");
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::SmallButton("Browse / Import")) BrowseTexture(engine, slot);
            ImGui::SameLine();
            if (ImGui::SmallButton("Use Selected Asset")) AssignSelectedAsset(engine, slot);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) path.clear();
            ImGui::PopID();
            ImGui::Spacing();
        };
        textureRow("Albedo / Base Color Map", object.albedoTexturePath, TextureSlot::Albedo);
        textureRow("Normal Map", object.normalTexturePath, TextureSlot::Normal);
        textureRow("Metallic / Roughness Map", object.metallicRoughnessTexturePath, TextureSlot::MetallicRoughness);
        textureRow("Ambient Occlusion Map", object.aoTexturePath, TextureSlot::AmbientOcclusion);
        textureRow("Emissive Map", object.emissiveTexturePath, TextureSlot::Emissive);
        ImGui::TextWrapped("Albedo is sampled as base color. Normal maps perturb the surface normal (scale is adjustable above); packed Metallic/Roughness maps use B=metallic and G=roughness (glTF convention); AO maps use the red channel; emissive maps multiply the Emissive Color. UV tiling and offset affect every assigned map.");
    }

    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SetNextItemWidth(240.0f);
        char tagBuffer[128];
        std::snprintf(tagBuffer, sizeof(tagBuffer), "%s", object.tag.c_str());
        if (ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string trimmed = tagBuffer;
            const size_t first = trimmed.find_first_not_of(" \t");
            const size_t last = trimmed.find_last_not_of(" \t");
            trimmed = first == std::string::npos ? "Untagged" : trimmed.substr(first, last - first + 1);
            object.tag = trimmed;
        }
        static const char* layerNames[] = { "Default", "TransparentFX", "Ignore Raycast", "Water", "UI", "Player", "Enemy", "Prop", "Trigger", "Lighting" };
        ImGui::SetNextItemWidth(240.0f);
        int layer = std::clamp(object.layer, 0, 9);
        if (ImGui::Combo("Layer", &layer, layerNames, IM_ARRAYSIZE(layerNames))) object.layer = layer;

        ImGui::Checkbox("Collider component", &object.hasCollider);
        if (object.hasCollider)
        {
            const char* colliderTypes[] = { "Box", "Sphere", "Capsule" };
            ImGui::SetNextItemWidth(160.0f);
            int colliderType = std::clamp(object.colliderType, 0, 2);
            if (ImGui::Combo("Collider Type", &colliderType, colliderTypes, IM_ARRAYSIZE(colliderTypes)))
                object.colliderType = colliderType;
            ImGui::DragFloat3("Collider Size", &object.colliderSize.x, 0.05f, 0.01f, 100.0f, "%.2f");
            ImGui::DragFloat3("Collider Center", &object.colliderCenter.x, 0.05f, -100.0f, 100.0f, "%.2f");
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##ColliderCenter"))
                object.colliderCenter = { 0.0f, 0.0f, 0.0f };
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Local-space offset of the collision shape from the object origin.");
            ImGui::Checkbox("Is Trigger", &object.isTrigger);
        }
        ImGui::Checkbox("Rigidbody component", &object.hasRigidbody);
        if (object.hasRigidbody)
        {
            ImGui::DragFloat("Mass", &object.rigidbodyMass, 0.1f, 0.01f, 1000.0f, "%.2f");
            ImGui::Checkbox("Use Gravity", &object.rigidbodyUseGravity);
            ImGui::Checkbox("Is Kinematic", &object.rigidbodyIsKinematic);
        }
        ImGui::Checkbox("Audio Source component", &object.hasAudioSource);
        if (object.hasAudioSource)
        {
            ImGui::DragFloat("Volume", &object.audioSourceVolume, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::DragFloat("Pitch", &object.audioSourcePitch, 0.01f, 0.1f, 3.0f, "%.2f");
            ImGui::Checkbox("Loop", &object.audioSourceLoop);
            ImGui::Checkbox("Play On Awake", &object.audioSourcePlayOnAwake);
            ImGui::Checkbox("Spatial Blend (3D)", &object.audioSourceSpatial);
            if (object.audioSourceSpatial)
            {
                ImGui::DragFloat("Min Distance", &object.audioSourceMinDistance, 0.05f, 0.01f, 1000.0f, "%.2f");
                ImGui::DragFloat("Max Distance", &object.audioSourceMaxDistance, 1.0f, 0.1f, 10000.0f, "%.1f");
            }
            ImGui::TextDisabled("Clip: %s", object.audioSourceClipPath.empty() ? "(none)" : object.audioSourceClipPath.c_str());
        }

        ImGui::Checkbox("Audio Listener component", &object.hasAudioListener);
        if (object.hasAudioListener)
        {
            ImGui::DragFloat("Listener Volume", &object.audioListenerVolume, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::TextDisabled("Place exactly one listener; it defines the ear transform.");
        }

        ImGui::Checkbox("Character Controller component", &object.hasCharacterController);
        if (object.hasCharacterController)
        {
            ImGui::DragFloat("Height", &object.characterHeight, 0.05f, 0.2f, 20.0f, "%.2f");
            ImGui::DragFloat("Radius", &object.characterRadius, 0.02f, 0.05f, 10.0f, "%.3f");
            ImGui::DragFloat("Slope Limit", &object.characterSlopeLimit, 0.5f, 0.0f, 89.0f, "%.1f deg");
            ImGui::DragFloat("Step Offset", &object.characterStepOffset, 0.01f, 0.0f, 2.0f, "%.3f");
            ImGui::DragFloat("Skin Width", &object.characterSkinWidth, 0.001f, 0.0f, 0.5f, "%.3f");
            ImGui::DragFloat("Gravity", &object.characterGravity, 0.05f, -100.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Move Speed", &object.characterSpeed, 0.1f, 0.0f, 200.0f, "%.2f");
            ImGui::DragFloat("Jump Height", &object.characterJumpHeight, 0.05f, 0.0f, 50.0f, "%.2f");
            ImGui::Text("Grounded: %s", object.characterIsGrounded ? "yes" : "no");
            ImGui::DragFloat3("Velocity", &object.characterVelocity.x, 0.05f, -1000.0f, 1000.0f, "%.2f");
        }

        ImGui::Checkbox("Particle System component", &object.hasParticleSystem);
        if (object.hasParticleSystem)
        {
            ImGui::DragInt("Max Particles", &object.particleMaxCount, 8.0f, 1, 100000);
            ImGui::DragFloat("Lifetime", &object.particleLifetime, 0.05f, 0.05f, 120.0f, "%.2f");
            ImGui::DragFloat("Start Speed", &object.particleStartSpeed, 0.05f, 0.0f, 200.0f, "%.2f");
            ImGui::DragFloat("Start Size", &object.particleStartSize, 0.01f, 0.001f, 10.0f, "%.3f");
            ImGui::DragFloat("Gravity", &object.particleGravity, 0.05f, -100.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Emission Rate", &object.particleEmissionRate, 1.0f, 0.0f, 10000.0f, "%.1f /s");
            ImGui::ColorEdit3("Start Color", &object.particleStartColor.x);
            ImGui::DragFloat3("Direction", &object.particleDirection.x, 0.02f, -1.0f, 1.0f, "%.3f");
            ImGui::DragFloat("Spread", &object.particleSpread, 0.5f, 0.0f, 180.0f, "%.1f deg");
            ImGui::Checkbox("Looping", &object.particleLooping);
        }

        ImGui::Checkbox("Animator component", &object.hasAnimator);
        if (object.hasAnimator)
        {
            ImGui::TextDisabled("Clip: %s", object.animatorClipName.empty() ? "(none)" : object.animatorClipName.c_str());
            ImGui::DragFloat("Speed", &object.animatorSpeed, 0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::Checkbox("Loop", &object.animatorLoop);
            ImGui::Checkbox("Playing", &object.animatorPlaying);
            ImGui::DragFloat("Time", &object.animatorTime, 0.01f, 0.0f, 3600.0f, "%.3f s");
        }
        ImGui::Separator();
        if (ImGui::Button("+ Add Component", ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("Add Component Popup");
        if (ImGui::BeginPopup("Add Component Popup"))
        {
            if (ImGui::BeginMenu("Rendering"))
            {
                if (ImGui::MenuItem("Mesh Renderer", nullptr, object.meshRendererEnabled)) object.meshRendererEnabled = !object.meshRendererEnabled;
                if (ImGui::MenuItem("Material (PBR)", nullptr, object.hasMaterial)) object.hasMaterial = !object.hasMaterial;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Physics"))
            {
                if (ImGui::MenuItem("Collider", nullptr, object.hasCollider)) object.hasCollider = !object.hasCollider;
                if (ImGui::MenuItem("Rigidbody", nullptr, object.hasRigidbody)) object.hasRigidbody = !object.hasRigidbody;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera & Audio"))
            {
                if (ImGui::MenuItem("Camera", nullptr, object.isCamera))
                {
                    object.isCamera = !object.isCamera;
                    if (object.isCamera) engine.m_viewportCameraObjectIndex = -1;
                }
                if (ImGui::MenuItem("Audio Source", nullptr, object.hasAudioSource)) object.hasAudioSource = !object.hasAudioSource;
                if (ImGui::MenuItem("Audio Listener", nullptr, object.hasAudioListener)) object.hasAudioListener = !object.hasAudioListener;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Effects"))
            {
                if (ImGui::MenuItem("Particle System", nullptr, object.hasParticleSystem)) object.hasParticleSystem = !object.hasParticleSystem;
                if (ImGui::MenuItem("Animator", nullptr, object.hasAnimator)) object.hasAnimator = !object.hasAnimator;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Character"))
            {
                if (ImGui::MenuItem("Character Controller", nullptr, object.hasCharacterController)) object.hasCharacterController = !object.hasCharacterController;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Lighting"))
            {
                const bool isLight = object.lightType != ForgeEngine::LightType::None;
                if (ImGui::MenuItem("Directional Light", nullptr, object.lightType == ForgeEngine::LightType::Directional))
                    object.lightType = object.lightType == ForgeEngine::LightType::Directional ? ForgeEngine::LightType::None : ForgeEngine::LightType::Directional;
                if (ImGui::MenuItem("Point Light", nullptr, object.lightType == ForgeEngine::LightType::Point))
                    object.lightType = object.lightType == ForgeEngine::LightType::Point ? ForgeEngine::LightType::None : ForgeEngine::LightType::Point;
                if (ImGui::MenuItem("Spot Light", nullptr, object.lightType == ForgeEngine::LightType::Spot))
                    object.lightType = object.lightType == ForgeEngine::LightType::Spot ? ForgeEngine::LightType::None : ForgeEngine::LightType::Spot;
                if (isLight)
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem("Remove Light")) object.lightType = ForgeEngine::LightType::None;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Script C#")) RequestCreateScript(engine.m_selectedObject);
            ImGui::EndPopup();
        }
        if (!object.scriptComponents.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(Accent(), "C# Scripts (%d)", static_cast<int>(object.scriptComponents.size()));
            int removeAt = -1;
            for (size_t i = 0; i < object.scriptComponents.size(); ++i)
            {
                auto& script = object.scriptComponents[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Checkbox("##ScriptEnabled", &script.enabled);
                ImGui::SameLine();
                ImGui::TextWrapped("%s", script.className.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Open in VS"))
                    OpenScriptInVisualStudio(m_projectRoot / L"Scripts" / std::filesystem::u8path(script.sourceFile));
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) removeAt = static_cast<int>(i);
                ImGui::PopID();
            }
            if (removeAt >= 0)
            {
                Log("Removed script component: " + object.scriptComponents[static_cast<size_t>(removeAt)].className);
                object.scriptComponents.erase(object.scriptComponents.begin() + removeAt);
                object.hasScript = !object.scriptComponents.empty();
            }
        }
        else ImGui::TextDisabled("No scripts attached. Use Add Component → Script C#.");
        ImGui::TextWrapped("C# scripts use ForgeEngine.API. Saving a .cs file in Visual Studio triggers an automatic rebuild after the file stops changing.");
    }

    if (m_openCreateScriptModal)
    {
        ImGui::OpenPopup("Create C# Script");
        m_openCreateScriptModal = false;
    }
    if (ImGui::BeginPopupModal("Create C# Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create and attach a MonoBehaviour script.");
        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputText("Class Name", m_newScriptClassName, sizeof(m_newScriptClassName));
        if (ImGui::Button("Create & Attach", ImVec2(160.0f, 0.0f)))
        {
            if (CreateAndAttachScript(engine)) ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();
    if (ImGui::Button("Duplicate", ImVec2(-1.0f, 0.0f))) DuplicateSelected(engine);
    if (ImGui::Button("Delete Object", ImVec2(-1.0f, 0.0f))) DeleteSelected(engine);
    }
    }
    ImGui::End();

}

void ForgeEditorUI::DrawLightingPanel(ForgeEngine& engine)
{
    using LightType = ForgeEngine::LightType;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 380.0f, viewport->WorkPos.y + 64.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(370.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ICON_LIGHT "  Lighting", &m_showLighting))
    {
        ImGui::End();
        return;
    }

    auto findLight = [&](LightType type) -> ForgeEngine::GameObject*
    {
        if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
        {
            auto& selected = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
            if (selected.lightType == type) return &selected;
        }
        for (auto it = engine.m_objects.rbegin(); it != engine.m_objects.rend(); ++it)
            if (it->lightType == type) return &(*it);
        return nullptr;
    };

    ImGui::TextDisabled("Scene lighting and procedural sky");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Day / Night Cycle", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Enable automatic cycle", &engine.m_dayNightCycleEnabled) && engine.m_dayNightCycleEnabled)
        {
            engine.m_timeOfDayDrivesSun = true;
            engine.m_directionalObjectTransformDrivesSun = false;
        }
        if (ImGui::SliderFloat("Time of day", &engine.m_timeOfDayHours, 0.0f, 24.0f, "%.2f h"))
        {
            engine.m_timeOfDayHours = std::fmod(engine.m_timeOfDayHours, 24.0f);
            engine.m_timeOfDayDrivesSun = true;
            engine.m_directionalObjectTransformDrivesSun = false;
        }
        const int hour = static_cast<int>(engine.m_timeOfDayHours) % 24;
        const int minute = static_cast<int>((engine.m_timeOfDayHours - std::floor(engine.m_timeOfDayHours)) * 60.0f);
        ImGui::TextColored(Accent(), "Clock: %02d:%02d", hour, minute);
        ImGui::SliderFloat("Cycle speed (game hours / min)", &engine.m_dayNightCycleSpeed, 0.1f, 12.0f, "%.1f");
        ImGui::TextWrapped("06:00 sunrise  |  12:00 noon  |  18:00 sunset  |  00:00 night");
        if (ImGui::Button("Set Day", ImVec2((ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f, 0.0f)))
        {
            engine.m_timeOfDayHours = 12.0f;
            engine.m_dayNightCycleEnabled = false;
            engine.m_timeOfDayDrivesSun = true;
            engine.m_directionalObjectTransformDrivesSun = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Set Night", ImVec2(-1.0f, 0.0f)))
        {
            engine.m_timeOfDayHours = 0.0f;
            engine.m_dayNightCycleEnabled = false;
            engine.m_timeOfDayDrivesSun = true;
            engine.m_directionalObjectTransformDrivesSun = false;
        }
        if (engine.m_directionalObjectTransformDrivesSun && !engine.m_dayNightCycleEnabled)
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.28f, 1.0f), "Sun direction is currently controlled by the Directional Light object's rotation.");
        else
            ImGui::TextDisabled("Time of day drives sun height, sky color, stars and sunlight.");
    }

    if (ImGui::CollapsingHeader("Procedural Sky & Clouds", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Cloud coverage", &engine.m_skyCloudCoverage, 0.05f, 0.95f, "%.2f");
        ImGui::SliderFloat("Cloud altitude", &engine.m_skyCloudAltitude, 0.05f, 0.60f, "%.2f");
        ImGui::SliderFloat("Wind speed", &engine.m_skyWindSpeed, 0.0f, 8.0f, "%.1f x");
        ImGui::TextWrapped("The sky renders a blue zenith-to-horizon gradient with two procedural cloud layers (cumulus + cirrus). Changes apply live in the viewport.");
    }

    auto* directional = findLight(LightType::Directional);
    if (ImGui::CollapsingHeader("Directional Light / Sun", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool& enabled = directional ? directional->lightEnabled : engine.m_directionalLightEnabled;
        XMFLOAT3& color = directional ? directional->lightColor : engine.m_directionalLightColor;
        float& intensity = directional ? directional->lightIntensity : engine.m_directionalLightIntensity;
        ImGui::Checkbox("Enable directional light", &enabled);
        ImGui::ColorEdit3("Sun color", &color.x);
        ImGui::SliderFloat("Intensity##Directional", &intensity, 0.0f, 10.0f, "%.2f");
        if (directional)
        {
            ImGui::TextDisabled("Active object: %s", directional->name.c_str());
            ImGui::TextWrapped("Rotate this light in Object Inspector to override the time-of-day direction.");
        }
        else
        {
            if (ImGui::SliderFloat3("Direction", &engine.m_directionalLightDirection.x, -1.0f, 1.0f,
                "%.2f", ImGuiSliderFlags_NoRoundToFormat))
            {
                engine.m_timeOfDayDrivesSun = false;
                engine.m_directionalObjectTransformDrivesSun = false;
            }
            ImGui::TextDisabled("Create Object → Directional Light adds a transformable sun object.");
        }
    }

    auto* point = findLight(LightType::Point);
    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (point)
        {
            ImGui::Checkbox("Enable point light", &point->lightEnabled);
            ImGui::ColorEdit3("Point color", &point->lightColor.x);
            ImGui::SliderFloat("Intensity##Point", &point->lightIntensity, 0.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Range##Point", &point->lightRange, 0.5f, 80.0f, "%.1f m");
            ImGui::TextDisabled("Active object: %s", point->name.c_str());
            ImGui::TextDisabled("Move the Point Light object in the scene to position it.");
        }
        else
        {
            ImGui::Checkbox("Enable point light", &engine.m_pointLightEnabled);
            ImGui::ColorEdit3("Point color", &engine.m_pointLightColor.x);
            ImGui::SliderFloat3("Position##Point", &engine.m_pointLightPosition.x, -40.0f, 40.0f, "%.1f");
            ImGui::SliderFloat("Range##Point", &engine.m_pointLightRange, 0.5f, 80.0f, "%.1f m");
            ImGui::SliderFloat("Intensity##Point", &engine.m_pointLightIntensity, 0.0f, 180.0f, "%.1f");
        }
    }

    auto* spot = findLight(LightType::Spot);
    if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (spot)
        {
            ImGui::Checkbox("Enable spotlight", &spot->lightEnabled);
            ImGui::ColorEdit3("Spot color", &spot->lightColor.x);
            ImGui::SliderFloat("Intensity##Spot", &spot->lightIntensity, 0.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Range##Spot", &spot->lightRange, 0.5f, 80.0f, "%.1f m");
            ImGui::SliderFloat("Inner cone##Spot", &spot->lightInnerAngle, 1.0f, 87.0f, "%.0f deg");
            ImGui::SliderFloat("Outer cone##Spot", &spot->lightOuterAngle, 2.0f, 89.0f, "%.0f deg");
            if (spot->lightOuterAngle <= spot->lightInnerAngle)
                spot->lightOuterAngle = std::min(89.0f, spot->lightInnerAngle + 1.0f);
            ImGui::TextDisabled("Move / rotate the Spot Light object to aim the cone.");
        }
        else
        {
            ImGui::Checkbox("Enable spotlight", &engine.m_spotLightEnabled);
            ImGui::ColorEdit3("Spot color", &engine.m_spotLightColor.x);
            ImGui::SliderFloat3("Position##Spot", &engine.m_spotLightPosition.x, -40.0f, 40.0f, "%.1f");
            ImGui::SliderFloat3("Direction##Spot", &engine.m_spotLightDirection.x, -1.0f, 1.0f,
                "%.2f", ImGuiSliderFlags_NoRoundToFormat);
            ImGui::SliderFloat("Range##Spot", &engine.m_spotLightRange, 0.5f, 80.0f, "%.1f m");
            ImGui::SliderFloat("Intensity##Spot", &engine.m_spotLightIntensity, 0.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Inner cone##Spot", &engine.m_spotLightInnerAngle, 1.0f, 87.0f, "%.0f deg");
            ImGui::SliderFloat("Outer cone##Spot", &engine.m_spotLightOuterAngle, 2.0f, 89.0f, "%.0f deg");
            if (engine.m_spotLightOuterAngle <= engine.m_spotLightInnerAngle)
                engine.m_spotLightOuterAngle = std::min(89.0f, engine.m_spotLightInnerAngle + 1.0f);
        }
    }

    ImGui::Separator();
    ImGui::SliderFloat("Ambient / sky fill", &engine.m_ambientLightIntensity, 0.0f, 2.0f, "%.2f");
    ImGui::TextDisabled("Directional shadow map: 4096 x 4096 with 5x5 PCF.");
    ImGui::TextDisabled("Up to four Point Lights and four Spot Lights can be active at once.");
    ImGui::End();
}

void ForgeEditorUI::RefreshAssetCache()
{
    const auto now = std::chrono::steady_clock::now();
    const bool folderChanged = m_assetListingFolder != m_activeAssetFolder;
    if (!folderChanged && now < m_assetNextScanAt) return;
    m_assetNextScanAt = now + std::chrono::milliseconds(500);
    m_assetListingFolder = m_activeAssetFolder;

    std::error_code ec;
    std::filesystem::create_directories(m_activeAssetFolder, ec);

    std::vector<AssetListingEntry> listing;
    bool scanOk = true;
    for (std::filesystem::directory_iterator it(m_activeAssetFolder, ec), end; !ec && it != end; it.increment(ec))
    {
        const auto name = it->path().filename().wstring();
        if (!name.empty() && name[0] == L'.') continue;
        std::error_code kindEc;
        const bool isDirectory = it->is_directory(kindEc);
        if (kindEc) { scanOk = false; break; }
        AssetListingEntry entry;
        entry.path = it->path();
        entry.isDirectory = isDirectory;
        if (!isDirectory)
        {
            std::error_code sizeEc;
            const auto size = it->file_size(sizeEc);
            if (!sizeEc) { entry.size = static_cast<unsigned long long>(size); entry.sizeValid = true; }
        }
        listing.push_back(std::move(entry));
    }
    if (ec) scanOk = false;
    if (scanOk)
    {
        std::sort(listing.begin(), listing.end(), [](const AssetListingEntry& a, const AssetListingEntry& b)
        {
            if (a.isDirectory != b.isDirectory) return a.isDirectory;
            return Lower(a.path.filename().wstring()) < Lower(b.path.filename().wstring());
        });
        m_assetListingCache.swap(listing);
    }

    std::unordered_map<std::wstring, std::vector<std::filesystem::path>> tree;
    std::vector<std::filesystem::path> pending{ m_assetsRoot };
    bool treeOk = true;
    while (!pending.empty() && treeOk)
    {
        const auto folder = pending.back();
        pending.pop_back();
        if (tree.count(folder.wstring())) continue;
        std::vector<std::filesystem::path> children;
        std::error_code folderEc;
        for (std::filesystem::directory_iterator it(folder, folderEc), end; !folderEc && it != end; it.increment(folderEc))
        {
            std::error_code kindEc;
            if (!it->is_directory(kindEc)) { if (kindEc) { treeOk = false; break; } continue; }
            children.push_back(it->path());
            pending.push_back(it->path());
        }
        if (folderEc) treeOk = false;
        if (!treeOk) break;
        std::sort(children.begin(), children.end(), [](const auto& a, const auto& b)
            { return Lower(a.filename().wstring()) < Lower(b.filename().wstring()); });
        tree[folder.wstring()] = std::move(children);
    }
    if (treeOk) m_subfolderCache.swap(tree);
}

void ForgeEditorUI::DrawProjectBrowser(ForgeEngine& engine)
{
    const bool windowVisible = ImGui::Begin(ICON_FOLDER "  Project Files", &m_showProject);
    if (windowVisible)
    {
    RefreshAssetCache();
    ImGui::TextDisabled("PROJECT");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", m_projectRoot.filename().string().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Import Asset...")) ImportAssetDialog();
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Folder")) { std::snprintf(m_newFolderName, sizeof(m_newFolderName), "%s", "NewFolder"); m_openCreateFolderModal = true; }
    ImGui::SameLine();
    const bool canRenameAsset = !m_selectedAssetPath.empty() && std::filesystem::exists(m_selectedAssetPath);
    if (!canRenameAsset) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Rename") && canRenameAsset) { m_renameOriginalPath = m_selectedAssetPath; std::snprintf(m_renameAssetName, sizeof(m_renameAssetName), "%s", m_selectedAssetPath.filename().string().c_str()); m_openRenameAssetModal = true; }
    if (!canRenameAsset) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("Open Folder"))
        ShellExecuteW(nullptr, L"open", m_activeAssetFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    const std::wstring selectedAssetExtension = Lower(m_selectedAssetPath.extension().wstring());
    const bool selectedAssetIsModel = !m_selectedAssetPath.empty() &&
        (selectedAssetExtension == L".obj" || selectedAssetExtension == L".fbx" ||
         selectedAssetExtension == L".gltf" || selectedAssetExtension == L".glb" ||
         selectedAssetExtension == L".dae" || selectedAssetExtension == L".stl" ||
         selectedAssetExtension == L".ply" || selectedAssetExtension == L".3ds");
    ImGui::SameLine();
    if (!selectedAssetIsModel) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Add to Scene") && selectedAssetIsModel)
    {
        if (InstantiateObj(engine, m_selectedAssetPath))
        {
            const int placedIndex = static_cast<int>(engine.m_objects.size()) - 1;
            XMFLOAT3 spawn{ 0.0f, 0.0f, 0.0f };
            XMFLOAT3 rayOrigin{}, rayDirection{};
            const int centerX = static_cast<int>(engine.m_sceneViewportWidth * 0.5f);
            const int centerY = static_cast<int>(engine.m_sceneViewportHeight * 0.5f);
            if (engine.BuildMouseRay(centerX, centerY, rayOrigin, rayDirection))
            {
                const float hitT = std::abs(rayDirection.y) > 0.0001f ? -rayOrigin.y / rayDirection.y : -1.0f;
                if (hitT > 0.0f && std::isfinite(hitT))
                {
                    spawn.x = rayOrigin.x + rayDirection.x * hitT;
                    spawn.z = rayOrigin.z + rayDirection.z * hitT;
                }
            }
            auto& placed = engine.m_objects[static_cast<size_t>(placedIndex)];
            placed.position = { spawn.x, 1.0f, spawn.z };
            engine.RebuildWorldTransform(placed);
            engine.SelectSceneObject(placedIndex);
            Log("Added asset to Scene via button: " + m_selectedAssetPath.filename().string());
        }
    }
    if (!selectedAssetIsModel) ImGui::EndDisabled();
    ImGui::Separator();

    ImGuiChildScope projectFolderTree("##ProjectFolderTree", ImVec2(175.0f, 0.0f), true);
    if (projectFolderTree.Visible())
    {
        std::error_code treeEc;
        std::function<void(const std::filesystem::path&, int)> drawFolder = [&](const std::filesystem::path& folder, int depth)
        {
            std::vector<std::filesystem::path> subfolders;
            if (const auto cached = m_subfolderCache.find(folder.wstring()); cached != m_subfolderCache.end())
                subfolders = cached->second;
            const bool isRoot = folder == m_assetsRoot;
            const bool isSelected = m_activeAssetFolder.lexically_normal() == folder.lexically_normal();
            // u8string binds PushID to the const char* overload (content hash).
            // A wchar_t* seed would bind to PushID(const void*) and produce an unstable
            // per-frame ID, making the Assets tree node oscillate open/closed.
            const std::string folderIdSeed = folder.u8string();
            ImGui::PushID(folderIdSeed.c_str());
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
            if (subfolders.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            const std::string label = std::string(isRoot ? "[A] " : "[+] ") + folder.filename().string();
            bool opened = ImGui::TreeNodeEx("##folderNode", flags, "%s", label.c_str());
            if (ImGui::IsItemClicked()) m_activeAssetFolder = folder;
            if (ImGui::BeginPopupContextItem("FolderContext"))
            {
                if (ImGui::MenuItem("Open")) m_activeAssetFolder = folder;
                if (ImGui::MenuItem("New subfolder")) { m_activeAssetFolder = folder; std::snprintf(m_newFolderName, sizeof(m_newFolderName), "%s", "NewFolder"); m_openCreateFolderModal = true; }
                if (!isRoot && ImGui::MenuItem("Rename")) { m_renameOriginalPath = folder; std::snprintf(m_renameAssetName, sizeof(m_renameAssetName), "%s", folder.filename().string().c_str()); m_openRenameAssetModal = true; }
                ImGui::EndPopup();
            }
            if (opened && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            {
                for (const auto& child : subfolders) drawFolder(child, depth + 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        };
        drawFolder(m_assetsRoot, 0);
    }
    projectFolderTree.EndNow();
    ImGui::SameLine();

    ImGuiChildScope projectAssetGrid("##ProjectAssetGrid", ImVec2(0.0f, 0.0f), true);
    if (projectAssetGrid.Visible())
    {
    const auto relativeFolder = m_activeAssetFolder.lexically_relative(m_projectRoot).generic_string();
    ImGui::TextColored(Accent(), "%s", relativeFolder.empty() ? "Project" : relativeFolder.c_str());
    ImGui::Separator();
    ImGui::TextDisabled("Drop files from Windows Explorer anywhere in this editor to import them.");
    ImGui::TextDisabled("Click-drag a model row into Scene, or select it and press Add to Scene.");

    if (ImGui::BeginTable("##AssetTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableHeadersRow();

        for (const auto& listed : m_assetListingCache)
        {
            const auto path = listed.path;
            const bool directory = listed.isDirectory;
            const std::string filename = path.filename().string();
            const std::string extension = directory ? "DIR" : path.extension().string();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (directory) ImGui::TextColored(ImVec4(0.94f, 0.71f, 0.32f, 1.0f), "FOLDER");
            else if (IsImageExtension(path)) ImGui::TextColored(ImVec4(0.38f, 0.76f, 0.97f, 1.0f), "%s", extension.c_str());
            else if (CategoryForExtension(path).filename() == L"Models") ImGui::TextColored(ImVec4(0.73f, 0.55f, 0.98f, 1.0f), "%s", extension.c_str());
            else if (CategoryForExtension(path).filename() == L"Audio") ImGui::TextColored(ImVec4(0.35f, 0.81f, 0.63f, 1.0f), "%s", extension.c_str());
            else if (IsVideoExtension(path)) ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.28f, 1.0f), "%s", extension.c_str());
            else ImGui::TextDisabled("%s", extension.c_str());

            ImGui::TableSetColumnIndex(1);
            const bool isSelected = path == m_selectedAssetPath;
            const std::string assetIdSeed = path.u8string();
            ImGui::PushID(assetIdSeed.c_str());
            if (ImGui::Selectable(filename.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_selectedAssetPath = path;
                if (directory) m_activeAssetFolder = path;
                else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && Lower(path.extension().wstring()) == L".forgescene")
                    LoadScene(engine, path);
            }
            // Record the physical mouse-down on model rows. Unlike BeginDragDropSource,
            // this does not rely on ImGui creating an internal drag payload.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                const bool supportedDropAsset = !directory && (IsModelExtension(path) || IsImageExtension(path) || IsVideoExtension(path));
                m_assetPressCandidate = supportedDropAsset;
                m_assetPressPath = supportedDropAsset ? path : std::filesystem::path{};
                m_assetPressPosition = ImGui::GetIO().MousePos;
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const std::wstring assetPayload = path.wstring();
                ImGui::SetDragDropPayload("FORGE_ASSET_PATH", assetPayload.c_str(),
                    (assetPayload.size() + 1) * sizeof(wchar_t));
                ImGui::Text("%s", filename.c_str());
                ImGui::TextDisabled("Drop on Scene or a material slot");
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem("AssetContext"))
            {
                if (ImGui::MenuItem("Select Asset")) m_selectedAssetPath = path;
                if (!directory && ImGui::MenuItem("Reveal in Explorer"))
                {
                    const std::wstring args = L"/select,\"" + path.wstring() + L"\"";
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
                }
                if (ImGui::MenuItem("Open Folder"))
                    ShellExecuteW(nullptr, L"open", path.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::TableSetColumnIndex(2);
            if (directory) ImGui::TextDisabled("Folder");
            else if (listed.sizeValid) ImGui::TextDisabled("%.1f KB", static_cast<double>(listed.size) / 1024.0);
            else ImGui::TextDisabled("Asset");
        }
        ImGui::EndTable();
    }
    }
    projectAssetGrid.EndNow();
    }

    if (m_openCreateFolderModal && !ImGui::IsPopupOpen("CreateProjectFolder"))
    { ImGui::OpenPopup("CreateProjectFolder"); m_openCreateFolderModal = false; }
    if (m_openRenameAssetModal && !ImGui::IsPopupOpen("RenameProjectAsset"))
    { ImGui::OpenPopup("RenameProjectAsset"); m_openRenameAssetModal = false; }
    if (ImGui::BeginPopupModal("CreateProjectFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create a folder inside:");
        ImGui::TextDisabled("%s", m_activeAssetFolder.string().c_str());
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText("Folder name", m_newFolderName, sizeof(m_newFolderName));
        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::filesystem::path name = std::filesystem::u8path(m_newFolderName);
            if (!name.empty() && name.filename() == name && name != "." && name != "..")
            {
                std::error_code ec; const auto target = m_activeAssetFolder / name;
                std::filesystem::create_directories(target, ec);
                if (!ec) { m_activeAssetFolder = target; InvalidateAssetCache(); Log("Created asset folder: " + target.string()); }
                else Log("ERROR: Could not create folder: " + ec.message());
            }
            else Log("ERROR: Folder name must be a single valid directory name.");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("RenameProjectAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Rename selected file or folder");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText("New name", m_renameAssetName, sizeof(m_renameAssetName));
        if (ImGui::Button("Rename", ImVec2(120, 0)))
        {
            const std::filesystem::path newName = std::filesystem::u8path(m_renameAssetName);
            std::error_code ec;
            if (!newName.empty() && newName.filename() == newName && newName != "." && newName != "..")
            {
                const auto target = m_renameOriginalPath.parent_path() / newName;
                std::filesystem::rename(m_renameOriginalPath, target, ec);
                if (!ec)
                {
                    if (m_activeAssetFolder == m_renameOriginalPath) m_activeAssetFolder = target;
                    if (m_selectedAssetPath == m_renameOriginalPath) m_selectedAssetPath = target;
                    InvalidateAssetCache();
                    Log("Renamed asset: " + target.filename().string());
                }
                else Log("ERROR: Rename failed: " + ec.message());
            }
            else Log("ERROR: Enter a valid single file/folder name.");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();

    // Selecting a Camera opens a preview panel fed by a copy of the real post-processed
    // Scene output. When Camera View is active, this shows the scene from that camera.
    if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()) &&
        engine.m_objects[static_cast<size_t>(engine.m_selectedObject)].isCamera)
    {
        auto& camera = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(mainViewport->Pos.x + engine.m_sceneViewportX +
            (std::max)(20.0f, engine.m_sceneViewportWidth - 320.0f),
            mainViewport->Pos.y + engine.m_sceneViewportY + 38.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 330.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(ICON_CAMERA "  Camera Preview", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(Accent(), "%s", camera.name.c_str());
            ImGui::Text("FOV %.1f deg  |  Clip %.2f - %.0f", camera.cameraFieldOfView,
                camera.cameraNearClip, camera.cameraFarClip);
            const bool isLiveCamera = engine.m_viewportCameraObjectIndex == engine.m_selectedObject;
            if (!isLiveCamera && ImGui::Button("Look Through Camera", ImVec2(-1.0f, 0.0f)))
            {
                engine.m_viewportCameraObjectIndex = engine.m_selectedObject;
                engine.m_cameraPosition = camera.position;
                engine.m_cameraYaw = camera.rotation.y;
                engine.m_cameraPitch = std::clamp(-camera.rotation.x, -1.50f, 1.50f);
                engine.m_editorCameraFov = std::clamp(camera.cameraFieldOfView, 20.0f, 110.0f);
                // Apply the edited camera transform before the next render/update, so
                // the first frame never snaps back to the free editor camera.
                engine.RebuildWorldTransform(camera);
                engine.UpdateCamera(0.0f);
                Log("Active camera now drives the actual Scene viewport. Hold RMB to navigate and edit the camera.");
            }
            if (isLiveCamera)
            {
                ImGui::TextColored(Accent(), "LIVE CAMERA VIEW");
                if (ImGui::Button("Exit Camera View", ImVec2(-1.0f, 0.0f)))
                {
                    engine.m_viewportCameraObjectIndex = -1;
                    engine.m_editorCameraFov = 60.0f;
                    engine.UpdateCamera(0.0f);
                    Log("Returned to free editor camera.");
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled(isLiveCamera
                ? "LIVE — final viewport image from this camera."
                : "Preview shows the current Scene. Activate Camera View to see this camera's actual view.");
            const UINT frame = engine.m_frameIndex % 2u;
            UINT& previewSlot = m_cameraPreviewSrvSlots[frame];
            if (previewSlot >= m_srvSlots.size())
            {
                previewSlot = 0xFFFFFFFFu;
                for (UINT slot = 0; slot < static_cast<UINT>(m_srvSlots.size()); ++slot)
                {
                    if (!m_srvSlots[slot]) { m_srvSlots[slot] = true; previewSlot = slot; break; }
                }
            }
            if (previewSlot < m_srvSlots.size() && engine.m_srvHeap && m_srvHeap && engine.m_cameraPreviewSrvGpuHandles[frame].ptr != 0)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE source = engine.m_srvHeap->GetCPUDescriptorHandleForHeapStart();
                source.ptr += static_cast<SIZE_T>(4u + frame) * engine.m_srvDescriptorSize;
                D3D12_CPU_DESCRIPTOR_HANDLE destination = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
                destination.ptr += static_cast<SIZE_T>(previewSlot) * m_srvDescriptorIncrement;
                engine.m_device->CopyDescriptorsSimple(1, destination, source, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
                gpu.ptr += static_cast<UINT64>(previewSlot) * m_srvDescriptorIncrement;
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const float imageWidth = (std::max)(80.0f, available.x);
                const float imageHeight = (std::min)(available.y, imageWidth * 9.0f / 16.0f);
                ImGui::Image((ImTextureID)(intptr_t)gpu.ptr, ImVec2(imageWidth, (std::max)(64.0f, imageHeight)));
            }
            else
            {
                ImGui::TextWrapped("Camera preview descriptor allocation failed. Close unused image previews and restart the editor.");
            }
        }
        ImGui::End();
    }
}

void ForgeEditorUI::DrawPostProcessSettings(ForgeEngine& engine)
{
    if (!m_showPostProcess) return;
    ImGui::SetNextWindowSize(ImVec2(360, 470), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_SETTINGS "  Post Processing", &m_showPostProcess))
    {
        bool changed = false;
        ImGui::TextColored(Accent(), "RENDERING PIPELINE");
        ImGui::TextDisabled("HDR scene -> screen-space effects -> ACES tone mapping");
        ImGui::Separator();
        ImGui::Text("Bloom");
        changed |= ImGui::SliderFloat("Intensity##Bloom", &engine.m_postProcessParameters.x, 0.0f, 1.8f, "%.2f");
        ImGui::Text("Screen-space ambient occlusion");
        changed |= ImGui::SliderFloat("Strength##AO", &engine.m_postProcessParameters.y, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("AO radius (px)", &engine.m_postProcessMotion.z, 0.5f, 5.0f, "%.1f");
        ImGui::Text("Motion blur");
        changed |= ImGui::SliderFloat("Strength##Motion", &engine.m_postProcessParameters.z, 0.0f, 1.0f, "%.2f");
        ImGui::Text("Lens and grading");
        changed |= ImGui::SliderFloat("Vignette", &engine.m_postProcessParameters.w, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Chromatic aberration", &engine.m_postProcessOptions.x, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Sharpen", &engine.m_postProcessOptions.y, 0.0f, 1.5f, "%.2f");
        changed |= ImGui::SliderFloat("Saturation", &engine.m_postProcessOptions.z, 0.0f, 2.0f, "%.2f");
        changed |= ImGui::SliderFloat("Exposure", &engine.m_postProcessOptions.w, 0.1f, 2.5f, "%.2f");
        if (ImGui::Button("Cinematic preset", ImVec2(-1, 0)))
        {
            engine.m_postProcessParameters = { 0.55f, 0.35f, 0.12f, 0.22f };
            engine.m_postProcessOptions = { 0.10f, 0.22f, 1.08f, 1.0f };
            engine.m_postProcessMotion.z = 2.5f;
            changed = true;
        }
        if (ImGui::Button("Neutral / Reset", ImVec2(-1, 0)))
        {
            engine.m_postProcessParameters = { 0.0f, 0.0f, 0.0f, 0.0f };
            engine.m_postProcessOptions = { 0.0f, 0.0f, 1.0f, 1.0f };
            engine.m_postProcessMotion.z = 1.5f;
            changed = true;
        }
        if (changed) SaveProjectPostProcess(engine);
        ImGui::Separator();
        ImGui::TextDisabled("Changes are saved to Presets/ActivePostProcessing.forgePreset.");
        ImGui::TextWrapped("AO is a depth-neighborhood approximation. Motion blur follows camera movement and has no temporal per-object velocity buffer.");
    }
    ImGui::End();
}

void ForgeEditorUI::LoadProjectPostProcess(ForgeEngine& engine)
{
    const auto path = m_projectRoot / L"Presets" / L"ActivePostProcessing.forgePreset";
    std::ifstream in(path, std::ios::binary);
    if (in)
    {
        std::string signature; int version = 0;
        XMFLOAT4 parameters{}, options{}, motion{};
        in >> signature >> version
           >> parameters.x >> parameters.y >> parameters.z >> parameters.w
           >> options.x >> options.y >> options.z >> options.w
           >> motion.z >> motion.w;
        const auto finite = [](float value) { return std::isfinite(value); };
        if (in && (signature == "FORGEENGINE_POSTPROCESS" || signature == "DX12FORGE_POSTPROCESS") && version == 1 &&
            finite(parameters.x) && finite(parameters.y) && finite(parameters.z) && finite(parameters.w) &&
            finite(options.x) && finite(options.y) && finite(options.z) && finite(options.w) &&
            finite(motion.z) && finite(motion.w))
        {
            engine.m_postProcessParameters = {
                std::clamp(parameters.x, 0.0f, 1.8f), std::clamp(parameters.y, 0.0f, 1.0f),
                std::clamp(parameters.z, 0.0f, 1.0f), std::clamp(parameters.w, 0.0f, 1.0f) };
            engine.m_postProcessOptions = {
                std::clamp(options.x, 0.0f, 1.0f), std::clamp(options.y, 0.0f, 1.5f),
                std::clamp(options.z, 0.0f, 2.0f), std::clamp(options.w, 0.1f, 2.5f) };
            engine.m_postProcessMotion.z = std::clamp(motion.z, 0.5f, 5.0f);
            engine.m_postProcessMotion.w = motion.w;
            Log("Loaded project post-processing preset.");
            return;
        }
        Log("WARNING: Invalid post-processing preset; keeping default values.");
    }
    SaveProjectPostProcess(engine);
}

void ForgeEditorUI::SaveProjectPostProcess(const ForgeEngine& engine) const
{
    if (m_projectRoot.empty()) return;
    std::error_code ec;
    const auto directory = m_projectRoot / L"Presets";
    std::filesystem::create_directories(directory, ec);
    if (ec) return;
    std::ofstream out(directory / L"ActivePostProcessing.forgePreset", std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << std::setprecision(9)
        << "FORGEENGINE_POSTPROCESS 1\n"
        << engine.m_postProcessParameters.x << ' ' << engine.m_postProcessParameters.y << ' '
        << engine.m_postProcessParameters.z << ' ' << engine.m_postProcessParameters.w << '\n'
        << engine.m_postProcessOptions.x << ' ' << engine.m_postProcessOptions.y << ' '
        << engine.m_postProcessOptions.z << ' ' << engine.m_postProcessOptions.w << '\n'
        << engine.m_postProcessMotion.z << ' ' << engine.m_postProcessMotion.w << '\n';
}

bool ForgeEditorUI::OpenProject(ForgeEngine& engine, const std::filesystem::path& projectPath)
{
    std::filesystem::path root = projectPath;
    std::error_code ec;
    if (std::filesystem::is_regular_file(root, ec)) root = root.parent_path();
    if (root.empty()) { Log("ERROR: Project folder is empty."); return false; }
    std::filesystem::create_directories(root, ec);
    if (ec) { Log("ERROR: Cannot access project folder: " + ec.message()); return false; }

    const auto marker = root / L"ForgeEngine.project";
    if (!std::filesystem::exists(marker, ec))
    {
        std::ofstream project(marker, std::ios::binary | std::ios::trunc);
        if (!project) { Log("ERROR: Cannot create ForgeEngine.project in selected folder."); return false; }
        project << "FORGEENGINE_PROJECT 1\nName \"" << root.filename().string() << "\"\nEngineVersion \"0.5.10\"\nAssetRoot \"Assets\"\n";
    }

    if (m_scriptRuntime) m_scriptRuntime->Stop();
    engine.WaitForGpu();
    engine.CreateStartupSplash(engine.m_instance);
    engine.UpdateStartupSplash(12, L"Opening project and resolving folders...");
    m_projectRoot = root;
    engine.m_textureSearchRoot = root;
    m_assetsRoot = root / L"Assets";
    const std::array<std::filesystem::path, 12> folders{
        m_assetsRoot / L"Textures", m_assetsRoot / L"Models", m_assetsRoot / L"Audio",
        m_assetsRoot / L"Videos", m_assetsRoot / L"Materials", m_assetsRoot / L"Scenes",
        m_assetsRoot / L"Shaders", m_assetsRoot / L"Misc", m_projectRoot / L"Config",
        m_projectRoot / L"Scripts", m_projectRoot / L"Plugins", m_projectRoot / L"Presets"
    };
    for (const auto& folder : folders) { std::filesystem::create_directories(folder, ec); ec.clear(); }
    engine.UpdateStartupSplash(32, L"Preparing Assets, Config, Plugins and Presets...");

    // Every project gets the correct scripts project file. Its engine API reference is
    // rewritten relative to the actual engine checkout at build time by ScriptRuntime.
    const auto csproj = m_projectRoot / L"Scripts" / L"ForgeGameScripts.csproj";
    if (!std::filesystem::exists(csproj, ec))
    {
        const auto templateProject = m_sourceRoot / L"Project" / L"Scripts" / L"ForgeGameScripts.csproj";
        if (std::filesystem::exists(templateProject, ec)) std::filesystem::copy_file(templateProject, csproj, std::filesystem::copy_options::overwrite_existing, ec);
        else
        {
            std::ofstream out(csproj);
            out << "<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net8.0</TargetFramework><ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable><LangVersion>latest</LangVersion><OutputType>Library</OutputType><AssemblyName>ForgeGameScripts</AssemblyName></PropertyGroup><ItemGroup><ProjectReference Include=\"../../Scripting/ForgeEngine.API/ForgeEngine.API.csproj\" /></ItemGroup></Project>";
        }
    }
    if (!std::filesystem::exists(m_projectRoot / L"Scripts" / L"PlayerMovement.cs", ec))
    {
        const auto example = m_sourceRoot / L"Project" / L"Scripts" / L"PlayerMovement.cs.example";
        if (std::filesystem::exists(example, ec))
            std::filesystem::copy_file(example, m_projectRoot / L"Scripts" / L"PlayerMovement.cs", std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (!std::filesystem::exists(m_projectRoot / L"Scripts" / L"CoinPickup.cs", ec))
    {
        const auto example = m_sourceRoot / L"Project" / L"Scripts" / L"CoinPickup.cs.example";
        if (std::filesystem::exists(example, ec))
            std::filesystem::copy_file(example, m_projectRoot / L"Scripts" / L"CoinPickup.cs", std::filesystem::copy_options::overwrite_existing, ec);
    }

    // Every project ships its input mapping, so scripts read key names from Config/Inputs.json.
    const auto inputsTarget = m_projectRoot / L"Config" / L"Inputs.json";
    if (!std::filesystem::exists(inputsTarget, ec))
    {
        const auto inputsTemplate = m_sourceRoot / L"Project" / L"Config" / L"Inputs.json";
        if (std::filesystem::exists(inputsTemplate, ec))
            std::filesystem::copy_file(inputsTemplate, inputsTarget, std::filesystem::copy_options::overwrite_existing, ec);
        else
        {
            std::ofstream inputs(inputsTarget);
            inputs << "{\n  \"axes\": [\n"
                   << "    { \"name\": \"Horizontal\", \"source\": \"Keys\", \"negative\": [\"A\", \"LeftArrow\"], \"positive\": [\"D\", \"RightArrow\"], \"smoothing\": 0 },\n"
                   << "    { \"name\": \"Vertical\", \"source\": \"Keys\", \"negative\": [\"S\", \"DownArrow\"], \"positive\": [\"W\", \"UpArrow\"], \"smoothing\": 0 },\n"
                   << "    { \"name\": \"Turn\", \"source\": \"Keys\", \"negative\": [\"Q\"], \"positive\": [\"E\"], \"smoothing\": 0 },\n"
                   << "    { \"name\": \"Mouse X\", \"source\": \"mouseDeltaX\", \"smoothing\": 0 },\n"
                   << "    { \"name\": \"Mouse Y\", \"source\": \"mouseDeltaY\", \"smoothing\": 0 },\n"
                   << "    { \"name\": \"Mouse ScrollWheel\", \"source\": \"mouseScroll\", \"smoothing\": 0 }\n  ],\n"
                   << "  \"buttons\": [\n"
                   << "    { \"name\": \"Jump\", \"keys\": [\"Space\"] },\n"
                   << "    { \"name\": \"Fire1\", \"keys\": [\"MouseLeft\", \"LeftCtrl\"] },\n"
                   << "    { \"name\": \"Fire2\", \"keys\": [\"MouseRight\"] },\n"
                   << "    { \"name\": \"Fire3\", \"keys\": [\"MouseMiddle\"] },\n"
                   << "    { \"name\": \"Run\", \"keys\": [\"LeftShift\"] },\n"
                   << "    { \"name\": \"Submit\", \"keys\": [\"Enter\", \"Space\"] },\n"
                   << "    { \"name\": \"Cancel\", \"keys\": [\"Escape\"] }\n  ]\n}\n";
        }
    }
    if (!std::filesystem::exists(m_projectRoot / L"README.md", ec))
    {
        const auto readmeTemplate = m_sourceRoot / L"Project" / L"README.md";
        if (std::filesystem::exists(readmeTemplate, ec))
            std::filesystem::copy_file(readmeTemplate, m_projectRoot / L"README.md", std::filesystem::copy_options::overwrite_existing, ec);
    }
    engine.UpdateStartupSplash(50, L"Resolving C# API, project scripts and Inputs mapping...");

    m_activeAssetFolder = m_assetsRoot;
    m_currentScenePath = m_assetsRoot / L"Scenes" / L"Default.forgeScene";
    engine.UpdateStartupSplash(66, L"Loading scene objects, meshes and transforms...");
    if (std::filesystem::exists(m_currentScenePath, ec))
    {
        if (!LoadScene(engine, m_currentScenePath))
        {
            Log("ERROR: Project opened, but its default scene failed to load.");
            engine.DestroyStartupSplash();
            return false;
        }
    }
    else
    {
        engine.m_objects.clear(); engine.SelectSceneObject(-1);
        if (!SaveScene(engine, m_currentScenePath)) { engine.DestroyStartupSplash(); return false; }
    }

    engine.UpdateStartupSplash(82, L"Loading project rendering and post-processing presets...");
    LoadProjectPostProcess(engine);
    engine.UpdateStartupSplash(90, L"Restoring editor layout and resource paths...");
    m_iniPath = (m_projectRoot / L"Config" / L"EditorLayout.ini").u8string();
    if (ImGui::GetCurrentContext()) ImGui::GetIO().IniFilename = m_iniPath.c_str();
    bool alreadyListed = false;
    for (const auto& item : m_recentProjects) if (item.lexically_normal() == m_projectRoot.lexically_normal()) alreadyListed = true;
    if (!alreadyListed) m_recentProjects.insert(m_recentProjects.begin(), m_projectRoot);
    if (m_recentProjects.size() > 12) m_recentProjects.resize(12);
    SaveRecentProjects(m_recentProjects);
    m_lastScriptSourceSignature = CalculateScriptSourceSignature();
    m_showWelcome = false;
    m_statusText = "Project loaded";
    Log("Project loaded: " + m_projectRoot.string());
    Log("Loaded scene data, object transforms, component metadata, scripts project and resource folders.");
    engine.UpdateStartupSplash(100, L"Project loaded. Opening editor...");
    engine.DestroyStartupSplash();
    return true;
}

void ForgeEditorUI::OpenProjectDialog(ForgeEngine& engine)
{
    std::array<wchar_t, 32768> fileName{};
    std::wstring initial = m_projectRoot.empty() ? m_sourceRoot.wstring() : m_projectRoot.wstring();
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = engine.m_hwnd;
    ofn.lpstrFilter = L"Forge Engine Project (ForgeEngine.project)\0ForgeEngine.project\0Project files (*.project)\0*.project\0All files (*.*)\0*.*\0\0";
    ofn.lpstrFile = fileName.data(); ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.lpstrInitialDir = initial.c_str(); ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) OpenProject(engine, std::filesystem::path(fileName.data()));
}

void ForgeEditorUI::CreateProjectFromDialog(ForgeEngine& engine)
{
    const std::string nameText = m_newProjectName;
    const std::filesystem::path name = std::filesystem::u8path(nameText);
    if (name.empty() || name.filename() != name || name == "." || name == "..")
    {
        Log("ERROR: Project name must be a simple folder name."); return;
    }
    std::filesystem::path location = std::filesystem::u8path(m_newProjectLocation);
    if (location.is_relative()) location = m_sourceRoot / location;
    const auto root = location / name;
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) { Log("ERROR: Could not create project: " + ec.message()); return; }
    const auto projectFile = root / L"ForgeEngine.project";
    {
        std::ofstream f(projectFile, std::ios::binary | std::ios::trunc);
        if (!f) { Log("ERROR: Cannot write project metadata."); return; }
        f << "FORGEENGINE_PROJECT 1\nName \"" << name.string() << "\"\nEngineVersion \"0.5.10\"\nAssetRoot \"Assets\"\nCreatedUtc \"";
        const auto now = std::chrono::system_clock::now();
        const auto stamp = std::chrono::system_clock::to_time_t(now);
        std::tm tm{}; gmtime_s(&tm, &stamp); char date[32]{}; std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M UTC", &tm); f << date << "\"\n";
    }
    OpenProject(engine, root);
    ImGui::CloseCurrentPopup();
}

void ForgeEditorUI::DrawWelcome(ForgeEngine& engine)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.045f, 0.07f, 1.0f));
    ImGui::Begin("Forge Engine Project Hub##fullscreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(); ImGui::PopStyleVar(3);

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    DrawWindowChrome(engine);
    ImGui::SetCursorPos(ImVec2(30.0f, 48.0f));
    ImGui::TextColored(Accent(), "FORGE ENGINE");
    ImGui::SameLine(); ImGui::TextDisabled("/ PROJECT HUB");
    ImGui::Separator(); ImGui::Spacing();
    ImGui::Text("Welcome back.");
    ImGui::TextDisabled("Create a project or continue building an existing world.");
    ImGui::Spacing();
    const float fullWidth = ImGui::GetContentRegionAvail().x;
    const float leftWidth = std::clamp(fullWidth * 0.34f, 280.0f, 430.0f);
    ImGui::BeginChild("##ProjectHubActions", ImVec2(leftWidth, -1), true);
    ImGui::TextColored(Accent(), "START"); ImGui::Spacing();
    if (ImGui::Button("+  New Project", ImVec2(-1, 54))) m_showCreateProject = true;
    ImGui::Spacing();
    if (ImGui::Button("Open Project...", ImVec2(-1, 48))) OpenProjectDialog(engine);
    ImGui::Spacing();
    if (ImGui::Button("Continue current project", ImVec2(-1, 48))) m_showWelcome = false;
    ImGui::Separator();
    ImGui::TextDisabled("Engine resources are initialized before this hub appears.");
    ImGui::TextDisabled("Project assets, scene transforms and C# scripts load when you open a project.");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##RecentProjectCards", ImVec2(0, -1), true);
    ImGui::TextColored(Accent(), "RECENT PROJECTS"); ImGui::SameLine(); ImGui::TextDisabled("%d", static_cast<int>(m_recentProjects.size()));
    ImGui::Separator();
    std::vector<std::filesystem::path> cards = m_recentProjects;
    std::error_code scanEc;
    const auto projectsDir = m_sourceRoot / L"Projects";
    for (std::filesystem::directory_iterator it(projectsDir, scanEc), end; !scanEc && it != end; it.increment(scanEc))
    {
        if (!it->is_directory(scanEc)) continue;
        const auto marker = it->path() / L"ForgeEngine.project";
        if (!std::filesystem::exists(marker, scanEc)) continue;
        bool seen = false; for (const auto& c : cards) if (c.lexically_normal() == it->path().lexically_normal()) seen = true;
        if (!seen) cards.push_back(it->path());
    }
    if (cards.empty()) ImGui::TextDisabled("No projects found yet.");
    for (const auto& project : cards)
    {
        const std::string projectIdSeed = project.u8string();
        ImGui::PushID(projectIdSeed.c_str());
        ImGui::BeginChild("##ProjectCard", ImVec2(0, 118), true);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float thumbW = 128.0f, thumbH = 78.0f;
        dl->AddRectFilled(p, ImVec2(p.x+thumbW, p.y+thumbH), IM_COL32(13, 25, 49, 255), 6.0f);
        dl->AddRectFilledMultiColor(p, ImVec2(p.x+thumbW, p.y+thumbH), IM_COL32(21, 72, 132, 255), IM_COL32(52, 133, 202, 255), IM_COL32(9, 20, 39, 255), IM_COL32(13, 34, 61, 255));
        const ImVec2 c(p.x + thumbW*0.5f, p.y + thumbH*0.72f);
        dl->AddLine(ImVec2(p.x+8,p.y+thumbH*0.78f), ImVec2(p.x+thumbW-8,p.y+thumbH*0.78f), IM_COL32(86,143,198,255), 1.0f);
        dl->AddRectFilled(ImVec2(c.x-16,c.y-27), ImVec2(c.x+16,c.y+2), IM_COL32(82,105,143,255), 3.0f);
        ImGui::Dummy(ImVec2(thumbW, thumbH)); ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextWrapped("%s", project.filename().string().c_str());
        std::string createdDate = "Project folder";
        std::ifstream meta(project / L"ForgeEngine.project"); std::string line;
        while (std::getline(meta,line)) if (line.rfind("CreatedUtc ",0)==0) { createdDate = line.substr(11); if (createdDate.size()>=2 && createdDate.front()=='"' && createdDate.back()=='"') createdDate=createdDate.substr(1,createdDate.size()-2); break; }
        if (createdDate == "Project folder")
        {
            std::error_code timeEc; const auto ft = std::filesystem::last_write_time(project / L"ForgeEngine.project", timeEc);
            if (!timeEc) { const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft - decltype(ft)::clock::now() + std::chrono::system_clock::now()); const auto t = std::chrono::system_clock::to_time_t(sys); std::tm tm{}; localtime_s(&tm, &t); char date[40]{}; std::strftime(date,sizeof(date),"%Y-%m-%d",&tm); createdDate=date; }
        }
        ImGui::TextDisabled("Created: %s", createdDate.c_str());
        if (ImGui::Button("Open project", ImVec2(140, 0))) OpenProject(engine, project);
        ImGui::EndGroup();
        ImGui::EndChild();
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (m_showCreateProject && !ImGui::IsPopupOpen("CreateForgeProject")) ImGui::OpenPopup("CreateForgeProject");
    if (ImGui::BeginPopupModal("CreateForgeProject", &m_showCreateProject, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create a new Forge Engine project"); ImGui::Separator();
        ImGui::SetNextItemWidth(370); ImGui::InputText("Project name", m_newProjectName, sizeof(m_newProjectName));
        ImGui::SetNextItemWidth(330); ImGui::InputText("Location", m_newProjectLocation, sizeof(m_newProjectLocation));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FOLDER "##BrowseLocation"))
        {
            std::wstring picked;
            if (PickFolderDialog(engine.m_hwnd, L"Choose project location", picked))
            {
                const std::string u8 = std::filesystem::path(picked).u8string();
                strncpy_s(m_newProjectLocation, sizeof(m_newProjectLocation), u8.c_str(), _TRUNCATE);
            }
        }
        ImGui::TextDisabled("Will create Assets, Scripts, Config, Plugins and Presets folders.");
        if (ImGui::Button("Create and open", ImVec2(180, 0))) CreateProjectFromDialog(engine);
        ImGui::SameLine(); if (ImGui::Button("Cancel", ImVec2(120, 0))) { m_showCreateProject=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::End();
}

void ForgeEditorUI::DrawStatsOverlay(ForgeEngine& engine)
{
    ImGui::SetNextWindowSize(ImVec2(250.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ICON_CHART "  Stats", &m_showStats))
    {
        ImGui::End();
        return;
    }
    static double smoothedMs = 16.0;
    const double frameMs = ImGui::GetIO().DeltaTime * 1000.0;
    smoothedMs = smoothedMs * 0.93 + frameMs * 0.07;
    ImGui::Text("%.1f ms  (%.0f FPS)", smoothedMs, smoothedMs > 0.001 ? 1000.0 / smoothedMs : 0.0);
    size_t triangles = 0;
    size_t renderers = 0;
    for (const auto& object : engine.m_objects)
    {
        if (!object.meshRendererEnabled || object.mesh == nullptr) continue;
        ++renderers;
        if (object.mesh->indexCount > 0) triangles += object.mesh->indexCount / 3;
        else triangles += object.mesh->vertexCount / 3;
    }
    ImGui::Text("%d objects  |  %d renderers", static_cast<int>(engine.m_objects.size()), static_cast<int>(renderers));
    ImGui::Text("%llu triangles", static_cast<unsigned long long>(triangles));
    ImGui::Text("Viewport %.0f x %.0f", engine.m_sceneViewportWidth, engine.m_sceneViewportHeight);
    if (engine.m_viewportCameraObjectIndex >= 0 &&
        engine.m_viewportCameraObjectIndex < static_cast<int>(engine.m_objects.size()))
    {
        ImGui::TextColored(Accent(), "Camera: %s (LIVE)",
            engine.m_objects[static_cast<size_t>(engine.m_viewportCameraObjectIndex)].name.c_str());
    }
    else
    {
        ImGui::Text("Camera: free editor camera");
    }
    ImGui::Text("Sun: %02d:%02d h", static_cast<int>(engine.m_timeOfDayHours) % 24,
        static_cast<int>((engine.m_timeOfDayHours - std::floor(engine.m_timeOfDayHours)) * 60.0f));
    ImGui::End();
}

void ForgeEditorUI::RequestViewportScreenshot(ForgeEngine& engine)
{
    std::error_code ec;
    const auto baseDir = m_projectRoot.empty()
        ? std::filesystem::current_path() / "Screenshots"
        : m_projectRoot / L"Assets" / L"Misc" / L"Screenshots";
    SYSTEMTIME stamp{};
    GetLocalTime(&stamp);
    char name[96];
    std::snprintf(name, sizeof(name), "forge_%04d%02d%02d_%02d%02d%02d.png",
        stamp.wYear, stamp.wMonth, stamp.wDay, stamp.wHour, stamp.wMinute, stamp.wSecond);
    engine.RequestScreenshot(baseDir / std::filesystem::u8path(name));
    m_statusText = "Saving screenshot " + std::string(name);
    Log("Screenshot requested: " + std::string(name));
}

void ForgeEditorUI::DrawConsole()
{
    const bool windowVisible = ImGui::Begin(ICON_COMMAND "  Console", &m_showConsole);
    if (windowVisible)
    {
    if (ImGui::SmallButton("Clear")) m_logs.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScrollConsole);
    ImGui::Separator();
    ImGuiChildScope consoleOutput("##ConsoleOutput", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (consoleOutput.Visible())
    {
    for (const auto& line : m_logs)
    {
        if (line.find("ERROR") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.36f, 0.36f, 1.0f), "%s", line.c_str());
        else if (line.find("Imported") != std::string::npos || line.find("Saved") != std::string::npos)
            ImGui::TextColored(ImVec4(0.37f, 0.81f, 0.64f, 1.0f), "%s", line.c_str());
        else ImGui::TextUnformatted(line.c_str());
    }
    if (m_autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    }
    consoleOutput.EndNow();
    }
    ImGui::End();

}

void ForgeEditorUI::DrawAboutWindow()
{
    if (!m_showAbout) return;
    ImGui::SetNextWindowSize(ImVec2(440, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About Forge Engine", &m_showAbout))
    {
        ImGui::TextColored(Accent(), "FORGE ENGINE");
        ImGui::Text("Editor 0.5.10   |   C++17   |   DirectX 12");
        ImGui::Separator();
        ImGui::TextWrapped("A native 3D engine editor with a DirectX 12 render surface behind a dockable Dear ImGui interface and ImGuizmo transform manipulation and HDR post-processing.");
        ImGui::Spacing();
        ImGui::TextWrapped("Built-in primitives and Assimp-supported triangle meshes (OBJ, FBX, glTF/GLB, DAE, STL, PLY and 3DS where supported by the importer build) can be placed in the Scene. Albedo textures are uploaded through WIC and sampled by the PBR shader; normal, metallic/roughness and AO maps currently store their paths but are not sampled yet.");
    }
    ImGui::End();
}

void ForgeEditorUI::CreatePrimitive(ForgeEngine& engine, bool sphere)
{
    CreatePrimitiveKind(engine, sphere ? 1 : 0);
}

void ForgeEditorUI::CreatePrimitiveKind(ForgeEngine& engine, int kind)
{
    ForgeEngine::Mesh* mesh = &engine.m_cubeMesh;
    const char* type = "Cube";
    XMFLOAT4 color{0.54f,0.58f,0.72f,1.0f};
    float radius = 1.732f;
    switch (kind) {
    case 1: mesh=&engine.m_sphereMesh; type="Sphere"; color={0.30f,0.72f,0.95f,1}; radius=1.0f; break;
    case 2: mesh=&engine.m_primitiveMeshes[0]; type="Plane"; color={0.30f,0.62f,0.38f,1}; radius=1.414f; break;
    case 3: mesh=&engine.m_primitiveMeshes[1]; type="Cylinder"; color={0.80f,0.55f,0.24f,1}; radius=1.414f; break;
    case 4: mesh=&engine.m_primitiveMeshes[2]; type="Cone"; color={0.80f,0.35f,0.30f,1}; radius=1.414f; break;
    case 5: mesh=&engine.m_primitiveMeshes[3]; type="Torus"; color={0.55f,0.42f,0.90f,1}; radius=1.0f; break;
    default: break;
    }
    const int serial = static_cast<int>(engine.m_objects.size()) + 1;
    XMFLOAT3 position{0,1,0};
    if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size())) {
        const auto& selected=engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
        position=selected.position; position.x += 1.8f;
    }
    ForgeEngine::GameObject object;
    object.name=std::string(type)+" "+std::to_string(serial); object.mesh=mesh;
    object.position=position; object.scale={1,1,1}; object.baseColor=color;
    object.metallic=0.05f; object.roughness=0.48f; object.localBoundsRadius=radius;
    engine.RebuildWorldTransform(object);
    engine.m_objects.push_back(std::move(object));
    engine.CreateObjectConstantBuffers(engine.m_objects.back());
    engine.SelectSceneObject(static_cast<int>(engine.m_objects.size())-1);
    Log(std::string("Created ")+type+".");
}

void ForgeEditorUI::CreateTerrainObject(ForgeEngine& engine)
{
    constexpr int cells = 64;
    constexpr float extent = 32.0f;
    auto mesh = std::make_unique<ForgeEngine::Mesh>();
    std::vector<ForgeEngine::Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve((cells + 1) * (cells + 1));
    indices.reserve(cells * cells * 6);
    for (int z = 0; z <= cells; ++z)
    {
        for (int x = 0; x <= cells; ++x)
        {
            const float u = static_cast<float>(x) / cells;
            const float v = static_cast<float>(z) / cells;
            ForgeEngine::Vertex vertex{};
            vertex.position = { (u - 0.5f) * extent, 0.0f, (v - 0.5f) * extent };
            vertex.normal = { 0.0f, 1.0f, 0.0f };
            vertex.uv = { u * 8.0f, v * 8.0f };
            vertices.push_back(vertex);
        }
    }
    for (int z = 0; z < cells; ++z)
    {
        for (int x = 0; x < cells; ++x)
        {
            const std::uint32_t a = static_cast<std::uint32_t>(z * (cells + 1) + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(cells + 1);
            const std::uint32_t d = c + 1;
            indices.insert(indices.end(), { a, c, b, b, c, d });
        }
    }
    engine.CreateMesh(*mesh, vertices, indices);
    ForgeEngine::Mesh* meshPtr = mesh.get();
    engine.m_importedMeshes.push_back(std::move(mesh));

    ForgeEngine::GameObject object;
    object.name = "Terrain " + std::to_string(engine.m_objects.size() + 1);
    object.mesh = meshPtr;
    object.position = { 0.0f, 0.0f, 0.0f };
    object.baseColor = { 0.25f, 0.40f, 0.22f, 1.0f };
    object.roughness = 0.94f;
    object.localBoundsRadius = 24.0f;
    object.isTerrain = true;
    object.terrainVertices = std::move(vertices);
    object.terrainIndices = std::move(indices);
    engine.RebuildWorldTransform(object);
    engine.m_objects.push_back(std::move(object));
    engine.CreateObjectConstantBuffers(engine.m_objects.back());
    engine.SelectSceneObject(static_cast<int>(engine.m_objects.size()) - 1);
    Log("Created 64x64 editable terrain. Choose a brush in Inspector and drag in the viewport to sculpt.");
}

void ForgeEditorUI::CreateCameraObject(ForgeEngine& engine)
{
    ForgeEngine::GameObject object;
    object.name = "Camera " + std::to_string(engine.m_objects.size() + 1);
    object.mesh = &engine.m_primitiveMeshes[2]; // small cone marker, visible in the editor
    object.position = { engine.m_cameraPosition.x, engine.m_cameraPosition.y, engine.m_cameraPosition.z };
    object.rotation = { -engine.m_cameraPitch, engine.m_cameraYaw, 0.0f };
    object.scale = { 0.22f, 0.22f, 0.38f };
    object.baseColor = { 0.20f, 0.80f, 0.95f, 1.0f };
    object.roughness = 0.35f;
    object.localBoundsRadius = 0.6f;
    object.isCamera = true;
    object.castShadows = false;
    engine.RebuildWorldTransform(object);
    engine.m_objects.push_back(std::move(object));
    engine.CreateObjectConstantBuffers(engine.m_objects.back());
    engine.SelectSceneObject(static_cast<int>(engine.m_objects.size()) - 1);
    Log("Created Camera component. Select it and use Look Through Camera to switch the actual Scene viewport to this camera.");
}

void ForgeEditorUI::ApplyTerrainBrush(ForgeEngine& engine, float worldX, float worldZ, float deltaSeconds)
{
    if (engine.m_selectedObject < 0 || engine.m_selectedObject >= static_cast<int>(engine.m_objects.size())) return;
    auto& terrain = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
    if (!terrain.isTerrain || terrain.terrainVertices.empty() || terrain.mesh == nullptr) return;
    const float dx = worldX - m_terrainLastStampX;
    const float dz = worldZ - m_terrainLastStampZ;
    if (dx * dx + dz * dz < 0.04f) return;
    m_terrainLastStampX = worldX;
    m_terrainLastStampZ = worldZ;

    const float radius = (std::max)(0.25f, m_terrainBrushRadius);
    const float amount = m_terrainBrushStrength * (std::max)(deltaSeconds, 1.0f / 120.0f);
    m_terrainNeedsUpload = true;
    for (auto& vertex : terrain.terrainVertices)
    {
        const float wx = terrain.position.x + vertex.position.x * terrain.scale.x;
        const float wz = terrain.position.z + vertex.position.z * terrain.scale.z;
        const float distance = std::sqrt((wx - worldX) * (wx - worldX) + (wz - worldZ) * (wz - worldZ));
        if (distance > radius) continue;
        const float falloff = 1.0f - distance / radius;
        if (m_terrainBrushMode == 0) vertex.position.y += amount * falloff;
        else if (m_terrainBrushMode == 1) vertex.position.y -= amount * falloff;
        else if (m_terrainBrushMode == 3) vertex.position.y += (m_terrainFlattenHeight - vertex.position.y) * std::min(1.0f, amount * falloff);
        else
        {
            // A stable local smoothing approximation; full neighbourhood averaging is applied
            // by sampling the nearest grid vertex and blending toward its height.
            const int gridSide = 65;
            const int gx = std::clamp(static_cast<int>(std::round((vertex.position.x / 32.0f + 0.5f) * 64.0f)), 0, 64);
            const int gz = std::clamp(static_cast<int>(std::round((vertex.position.z / 32.0f + 0.5f) * 64.0f)), 0, 64);
            float sum = 0.0f; int count = 0;
            for (int oz = -1; oz <= 1; ++oz) for (int ox = -1; ox <= 1; ++ox)
            {
                const int sx = gx + ox, sz = gz + oz;
                if (sx < 0 || sx >= gridSide || sz < 0 || sz >= gridSide) continue;
                sum += terrain.terrainVertices[static_cast<size_t>(sz * gridSide + sx)].position.y; ++count;
            }
            if (count) vertex.position.y += ((sum / count) - vertex.position.y) * std::min(1.0f, amount * falloff);
        }
    }
    // Avoid a GPU upload stall on every frame. Rebuild the vertex/index buffers at most
    // 12.5 times per second while a stroke is active.
    const auto now = std::chrono::steady_clock::now();
    if (m_terrainLastUpload.time_since_epoch().count() == 0 ||
        std::chrono::duration<float>(now - m_terrainLastUpload).count() >= 0.08f)
    {
        // Recompute smooth normals from the height field so lighting follows sculpted slopes.
        constexpr int side = 65;
        for (int z = 0; z < side; ++z)
        {
            for (int x = 0; x < side; ++x)
            {
                const int left = (std::max)(0, x - 1), right = (std::min)(64, x + 1);
                const int up = (std::max)(0, z - 1), down = (std::min)(64, z + 1);
                const auto& vx0 = terrain.terrainVertices[static_cast<size_t>(z * side + left)].position;
                const auto& vx1 = terrain.terrainVertices[static_cast<size_t>(z * side + right)].position;
                const auto& vz0 = terrain.terrainVertices[static_cast<size_t>(up * side + x)].position;
                const auto& vz1 = terrain.terrainVertices[static_cast<size_t>(down * side + x)].position;
                const XMFLOAT3 tangentX{ vx1.x - vx0.x, vx1.y - vx0.y, vx1.z - vx0.z };
                const XMFLOAT3 tangentZ{ vz1.x - vz0.x, vz1.y - vz0.y, vz1.z - vz0.z };
                // Use tangentZ x tangentX so an unmodified flat grid points upward (+Y).
                XMFLOAT3 normal{
                    tangentZ.y * tangentX.z - tangentZ.z * tangentX.y,
                    tangentZ.z * tangentX.x - tangentZ.x * tangentX.z,
                    tangentZ.x * tangentX.y - tangentZ.y * tangentX.x
                };
                const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (length > 1.0e-5f) { normal.x /= length; normal.y /= length; normal.z /= length; }
                else normal = { 0.0f, 1.0f, 0.0f };
                terrain.terrainVertices[static_cast<size_t>(z * side + x)].normal = normal;
            }
        }
        engine.WaitForGpu();
        engine.CreateMesh(*terrain.mesh, terrain.terrainVertices, terrain.terrainIndices);
        m_terrainLastUpload = now;
        m_terrainNeedsUpload = false;
    }
}

void ForgeEditorUI::CreateLightObject(ForgeEngine& engine, int lightKind)
{
    using LightType = ForgeEngine::LightType;
    ForgeEngine::GameObject object;
    object.lightType = lightKind == 1 ? LightType::Directional :
        lightKind == 2 ? LightType::Point : LightType::Spot;
    const int serial = static_cast<int>(engine.m_objects.size()) + 1;

    if (object.lightType == LightType::Directional)
    {
        object.name = "Directional Light " + std::to_string(serial);
        object.mesh = &engine.m_primitiveMeshes[2]; // compact cone-shaped sun gizmo
        object.position = { engine.m_cameraPosition.x, engine.m_cameraPosition.y + 1.0f, engine.m_cameraPosition.z + 2.0f };
        object.scale = { 0.16f, 0.38f, 0.16f };
        object.lightColor = { 1.0f, 0.82f, 0.55f };
        object.lightIntensity = 4.0f;
        object.localBoundsRadius = 0.45f;
    }
    else if (object.lightType == LightType::Point)
    {
        object.name = "Point Light " + std::to_string(serial);
        object.mesh = &engine.m_sphereMesh;
        object.position = { engine.m_cameraPosition.x, engine.m_cameraPosition.y - 0.5f, engine.m_cameraPosition.z + 2.0f };
        object.scale = { 0.16f, 0.16f, 0.16f };
        object.lightColor = { 0.28f, 0.58f, 1.0f };
        object.lightIntensity = 35.0f;
        object.lightRange = 12.0f;
        object.localBoundsRadius = 1.0f;
    }
    else
    {
        object.name = "Spot Light " + std::to_string(serial);
        object.mesh = &engine.m_primitiveMeshes[2]; // cone-shaped light marker
        object.position = { engine.m_cameraPosition.x, engine.m_cameraPosition.y + 0.3f, engine.m_cameraPosition.z + 2.0f };
        object.scale = { 0.18f, 0.36f, 0.18f };
        object.lightColor = { 1.0f, 0.42f, 0.20f };
        object.lightIntensity = 45.0f;
        object.lightRange = 16.0f;
        object.lightInnerAngle = 18.0f;
        object.lightOuterAngle = 30.0f;
        object.localBoundsRadius = 0.45f;
    }

    if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
    {
        const auto& selected = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
        object.position = { selected.position.x + 1.25f, selected.position.y + 0.6f, selected.position.z };
    }
    object.baseColor = { object.lightColor.x, object.lightColor.y, object.lightColor.z, 1.0f };
    object.metallic = 0.0f;
    object.roughness = 0.22f;
    object.castShadows = false;
    object.meshRendererEnabled = true;
    engine.RebuildWorldTransform(object);
    engine.m_objects.push_back(std::move(object));
    engine.CreateObjectConstantBuffers(engine.m_objects.back());
    engine.SelectSceneObject(static_cast<int>(engine.m_objects.size()) - 1);
    Log(std::string("Created ") + (lightKind == 1 ? "Directional Light" : lightKind == 2 ? "Point Light" : "Spot Light") +
        ". Edit its Light Component in Object Inspector.");
}

static std::filesystem::path FirstMaterialTexturePath(const aiScene* scene, aiTextureType type,
    const std::filesystem::path& modelDirectory)
{
    if (!scene) return {};
    for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
    {
        const aiMaterial* material = scene->mMaterials[materialIndex];
        if (!material || material->GetTextureCount(type) == 0) continue;
        aiString textureName;
        if (material->GetTexture(type, 0, &textureName, nullptr, nullptr, nullptr, nullptr, nullptr) == aiReturn_SUCCESS && textureName.length > 0)
        {
            std::filesystem::path texture = std::filesystem::u8path(textureName.C_Str());
            if (texture.is_relative()) texture = modelDirectory / texture;
            std::error_code ec;
            if (std::filesystem::exists(texture, ec)) return texture;
        }
    }
    return {};
}

bool ForgeEditorUI::InstantiateObj(ForgeEngine& engine, const std::filesystem::path& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string(),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality | aiProcess_SortByPType | aiProcess_ValidateDataStructure |
        aiProcess_PreTransformVertices | aiProcess_FlipUVs | aiProcess_FixInfacingNormals);
    if (!scene || !scene->HasMeshes()) {
        Log(std::string("ERROR: Assimp could not import model: ") + importer.GetErrorString());
        return false;
    }

    auto mesh = std::make_unique<ForgeEngine::Mesh>();
    std::vector<ForgeEngine::Vertex> vertices;
    std::vector<std::uint32_t> indices;
    XMFLOAT3 boundsMin{ FLT_MAX, FLT_MAX, FLT_MAX };
    XMFLOAT3 boundsMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* source = scene->mMeshes[meshIndex];
        if (!source || !source->HasPositions()) continue;
        const std::uint32_t baseVertex = static_cast<std::uint32_t>(vertices.size());
        for (unsigned int v = 0; v < source->mNumVertices; ++v) {
            ForgeEngine::Vertex vertex{};
            const aiVector3D& pos = source->mVertices[v];
            vertex.position = { pos.x, pos.y, pos.z };
            boundsMin.x = (std::min)(boundsMin.x, pos.x); boundsMin.y = (std::min)(boundsMin.y, pos.y); boundsMin.z = (std::min)(boundsMin.z, pos.z);
            boundsMax.x = (std::max)(boundsMax.x, pos.x); boundsMax.y = (std::max)(boundsMax.y, pos.y); boundsMax.z = (std::max)(boundsMax.z, pos.z);
            if (source->HasNormals()) {
                const aiVector3D& normal = source->mNormals[v];
                vertex.normal = Normalize3({ normal.x, normal.y, normal.z });
            } else vertex.normal = { 0.0f, 1.0f, 0.0f };
            if (source->HasTextureCoords(0)) {
                const aiVector3D& uv = source->mTextureCoords[0][v];
                vertex.uv = { uv.x, uv.y };
            }
            vertices.push_back(vertex);
        }
        for (unsigned int f = 0; f < source->mNumFaces; ++f) {
            const aiFace& face = source->mFaces[f];
            if (face.mNumIndices < 3) continue;
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
                indices.push_back(baseVertex + face.mIndices[j]);
        }
    }
    if (vertices.empty() || indices.empty()) {
        Log("ERROR: Assimp model contains no supported triangle geometry.");
        return false;
    }

    // Normalize the imported mesh into a predictable editor-friendly size and origin.
    // This prevents tiny/huge assets and models whose pivot is kilometres away from the geometry.
    const XMFLOAT3 center{
        (boundsMin.x + boundsMax.x) * 0.5f,
        (boundsMin.y + boundsMax.y) * 0.5f,
        (boundsMin.z + boundsMax.z) * 0.5f
    };
    const float sizeX = boundsMax.x - boundsMin.x;
    const float sizeY = boundsMax.y - boundsMin.y;
    const float sizeZ = boundsMax.z - boundsMin.z;
    const float largestDimension = (std::max)(sizeX, (std::max)(sizeY, sizeZ));
    if (largestDimension > 1.0e-5f) {
        const float normalizeScale = 2.0f / largestDimension;
        for (auto& vertex : vertices) {
            vertex.position.x = (vertex.position.x - center.x) * normalizeScale;
            vertex.position.y = (vertex.position.y - center.y) * normalizeScale;
            vertex.position.z = (vertex.position.z - center.z) * normalizeScale;
        }
    }

    engine.CreateMesh(*mesh, vertices, indices);
    ForgeEngine::Mesh* meshPtr = mesh.get();
    engine.m_importedMeshes.push_back(std::move(mesh));

    ForgeEngine::GameObject object;
    object.name = path.stem().string();
    object.mesh = meshPtr;
    object.importedModelPath = RelativeAssetPath(path);
    // Pre-fill texture slots from the model's own material so imported FBX/OBJ glTF
    // assets arrive with their albedo and normal maps already assigned.
    const auto modelDirectory = path.parent_path();
    const auto embeddedAlbedo = FirstMaterialTexturePath(scene, aiTextureType_DIFFUSE, modelDirectory);
    if (!embeddedAlbedo.empty()) object.albedoTexturePath = RelativeAssetPath(embeddedAlbedo);
    auto embeddedNormal = FirstMaterialTexturePath(scene, aiTextureType_NORMALS, modelDirectory);
    if (embeddedNormal.empty()) embeddedNormal = FirstMaterialTexturePath(scene, aiTextureType_HEIGHT, modelDirectory);
    if (!embeddedNormal.empty()) object.normalTexturePath = RelativeAssetPath(embeddedNormal);
    object.position = { 0.0f, 1.0f, 0.0f };
    object.scale = { 1.0f, 1.0f, 1.0f };
    object.baseColor = { 0.82f, 0.82f, 0.82f, 1.0f };
    object.localBoundsRadius = 1.75f;
    engine.RebuildWorldTransform(object);
    engine.m_objects.push_back(std::move(object));
    engine.CreateObjectConstantBuffers(engine.m_objects.back());
    engine.SelectSceneObject(static_cast<int>(engine.m_objects.size()) - 1);
    Log("Assimp imported and normalized model into Scene: " + path.filename().string());
    return true;
}

void ForgeEditorUI::HandleDroppedFiles(HDROP drop)
{
    if (!drop) return;
    if (!m_engine) { DragFinish(drop); return; }
    ForgeEngine& engine = *m_engine;
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        std::array<wchar_t, 32768> buffer{};
        if (DragQueryFileW(drop, i, buffer.data(), static_cast<UINT>(buffer.size())) == 0) continue;
        const std::filesystem::path source(buffer.data());
        const std::wstring ext = Lower(source.extension().wstring());
        const auto imported = ImportFile(source);
        if (imported.empty()) continue;
        m_selectedAssetPath = imported;
        m_activeAssetFolder = imported.parent_path();

        if (IsImageExtension(imported)) {
            if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size())) {
                engine.m_objects[static_cast<size_t>(engine.m_selectedObject)].albedoTexturePath = RelativeAssetPath(imported);
                Log("Dropped image assigned to selected object's Albedo: " + imported.filename().string());
            } else {
                CreatePrimitiveKind(engine, 2);
                if (engine.m_selectedObject >= 0 && engine.m_selectedObject < static_cast<int>(engine.m_objects.size()))
                    engine.m_objects[static_cast<size_t>(engine.m_selectedObject)].albedoTexturePath = RelativeAssetPath(imported);
                Log("Dropped image created a textured preview plane: " + imported.filename().string());
            }
        } else if (IsModelExtension(imported)) {
            Log("Model imported into Project Files. Drag it into the Scene to place it: " + imported.filename().string());
        } else if (IsVideoExtension(imported)) {
            Log("Video imported into Project/Assets/Videos: " + imported.filename().string() + ". Playback is not yet implemented.");
        }
    }
    DragFinish(drop);
}

void ForgeEditorUI::DuplicateSelected(ForgeEngine& engine)
{
    if (engine.m_selectedObject < 0 || engine.m_selectedObject >= static_cast<int>(engine.m_objects.size())) return;
    const auto& source = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
    // Copy every component field (transform, tag/layer, collider, rigidbody,
    // camera, light, materials and scripts), then replace the GPU constant buffers.
    auto duplicate = std::make_unique<ForgeEngine::GameObject>(source);
    duplicate->name = source.name + " Copy";
    duplicate->position = DirectX::XMFLOAT3{ source.position.x + 1.0f, source.position.y, source.position.z + 0.5f };
    duplicate->isDragPreview = false;
    duplicate->isDragPreviewEmissive = false;
    for (auto& buffer : duplicate->objectConstants) buffer.Reset();
    for (auto& buffer : duplicate->outlineConstants) buffer.Reset();
    engine.m_objects.push_back(std::move(*duplicate));
    auto& placed = engine.m_objects.back();
    engine.CreateObjectConstantBuffers(placed);
    engine.RebuildWorldTransform(placed);
    engine.SelectSceneObject(static_cast<int>(engine.m_objects.size()) - 1);
    Log("Selected object duplicated.");
}

void ForgeEditorUI::DeleteSelected(ForgeEngine& engine)
{
    const int index = engine.m_selectedObject;
    if (index < 0 || index >= static_cast<int>(engine.m_objects.size())) return;
    engine.WaitForGpu();
    if (engine.m_viewportCameraObjectIndex == index)
        engine.m_viewportCameraObjectIndex = -1;
    else if (engine.m_viewportCameraObjectIndex > index)
        --engine.m_viewportCameraObjectIndex;
    int deletedParent = engine.m_objects[static_cast<size_t>(index)].parentIndex;
    if (deletedParent == index) deletedParent = -1;
    else if (deletedParent > index) --deletedParent;
    for (auto& object : engine.m_objects)
    {
        if (object.parentIndex == index) object.parentIndex = deletedParent;
        else if (object.parentIndex > index) --object.parentIndex;
    }
    engine.m_objects.erase(engine.m_objects.begin() + index);
    for (auto& object : engine.m_objects) engine.RebuildWorldTransform(object);
    if (engine.m_objects.empty()) engine.SelectSceneObject(-1);
    else engine.SelectSceneObject(std::min(index, static_cast<int>(engine.m_objects.size()) - 1));
    Log("Selected object deleted.");
}

void ForgeEditorUI::NewScene(ForgeEngine& engine)
{
    engine.WaitForGpu();
    engine.m_viewportCameraObjectIndex = -1;
    engine.m_objects.clear();
    engine.SelectSceneObject(-1);
    m_currentScenePath = m_assetsRoot / L"Scenes" / L"Untitled.forgeScene";
    m_statusText = "New scene";
    Log("New empty scene created.");
}

bool ForgeEditorUI::SaveScene(ForgeEngine& engine, const std::filesystem::path& path)
{
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        Log("ERROR: Could not save scene: " + path.string());
        return false;
    }
    out << "FORGEENGINE_SCENE 11\n";
    out << "OBJECT_COUNT " << engine.m_objects.size() << '\n';
    for (const auto& object : engine.m_objects)
    {
        const int meshKind = object.mesh == &engine.m_sphereMesh ? 1 :
            object.mesh == &engine.m_primitiveMeshes[0] ? 2 :
            object.mesh == &engine.m_primitiveMeshes[1] ? 3 :
            object.mesh == &engine.m_primitiveMeshes[2] ? 4 :
            object.mesh == &engine.m_primitiveMeshes[3] ? 5 : 0;
        out << "OBJECT " << std::quoted(object.name) << ' ' << meshKind << ' '
            << object.position.x << ' ' << object.position.y << ' ' << object.position.z << ' '
            << object.rotation.x << ' ' << object.rotation.y << ' ' << object.rotation.z << ' '
            << object.scale.x << ' ' << object.scale.y << ' ' << object.scale.z << ' '
            << object.baseColor.x << ' ' << object.baseColor.y << ' ' << object.baseColor.z << ' ' << object.baseColor.w << ' '
            << object.metallic << ' ' << object.roughness << ' '
            << object.meshRendererEnabled << ' ' << object.castShadows << ' '
            << object.hasCollider << ' ' << object.hasAudioSource << ' ' << object.hasScript << ' '
            << std::quoted(object.albedoTexturePath) << ' ' << std::quoted(object.normalTexturePath) << ' '
            << std::quoted(object.metallicRoughnessTexturePath) << ' ' << std::quoted(object.aoTexturePath) << ' '
            << static_cast<int>(object.lightType) << ' ' << object.lightEnabled << ' '
            << object.lightColor.x << ' ' << object.lightColor.y << ' ' << object.lightColor.z << ' '
            << object.lightIntensity << ' ' << object.lightRange << ' '
            << object.lightInnerAngle << ' ' << object.lightOuterAngle << ' '
            << object.isTerrain << ' ' << object.isCamera << ' '
            << object.cameraFieldOfView << ' ' << object.cameraNearClip << ' ' << object.cameraFarClip << ' '
            << std::quoted(object.importedModelPath) << ' '
            << object.scriptComponents.size() << ' ';
        for (const auto& script : object.scriptComponents)
            out << std::quoted(script.className) << ' ' << std::quoted(script.sourceFile) << ' ' << script.enabled << ' ';
        out << object.parentIndex << ' ';
        out << (object.isTerrain ? object.terrainVertices.size() : 0u) << ' ';
        if (object.isTerrain) for (const auto& v : object.terrainVertices) out << v.position.y << ' ';
        out << object.textureTiling.x << ' ' << object.textureTiling.y << ' '
            << object.textureOffset.x << ' ' << object.textureOffset.y << ' ';
        out << std::quoted(object.tag) << ' ' << object.layer << ' ' << object.colliderType << ' '
            << object.colliderSize.x << ' ' << object.colliderSize.y << ' ' << object.colliderSize.z << ' '
            << object.isTrigger << ' ' << object.hasRigidbody << ' ' << object.rigidbodyMass << ' '
            << object.rigidbodyUseGravity << ' ' << object.rigidbodyIsKinematic << ' ';
        out << std::quoted(object.emissiveTexturePath) << ' '
            << object.emissiveColor.x << ' ' << object.emissiveColor.y << ' ' << object.emissiveColor.z << ' '
            << object.emissiveIntensity << ' ' << object.normalMapScale << ' ' << object.occlusionStrength << ' '
            << object.doubleSided << ' ';
        out << object.cameraOrthographic << ' ' << object.cameraOrthographicSize << ' '
            << object.cameraDepth << ' '
            << object.cameraBackgroundColor.x << ' ' << object.cameraBackgroundColor.y << ' '
            << object.cameraBackgroundColor.z << ' ' << object.cameraBackgroundColor.w << ' '
            << object.cameraDrawSkybox << ' ' << object.cameraDrawGizmos << ' '
            << object.cameraCullingMask << ' '
            << object.hasMaterial << ' '
            << object.colliderCenter.x << ' ' << object.colliderCenter.y << ' ' << object.colliderCenter.z << ' '
            << object.hasCharacterController << ' ' << object.characterHeight << ' ' << object.characterRadius << ' '
            << object.characterSlopeLimit << ' ' << object.characterStepOffset << ' ' << object.characterSkinWidth << ' '
            << object.characterGravity << ' ' << object.characterSpeed << ' ' << object.characterJumpHeight << ' '
            << object.hasParticleSystem << ' ' << object.particleMaxCount << ' ' << object.particleLifetime << ' '
            << object.particleStartSpeed << ' ' << object.particleStartSize << ' ' << object.particleGravity << ' '
            << object.particleEmissionRate << ' '
            << object.particleStartColor.x << ' ' << object.particleStartColor.y << ' ' << object.particleStartColor.z << ' '
            << object.particleDirection.x << ' ' << object.particleDirection.y << ' ' << object.particleDirection.z << ' '
            << object.particleSpread << ' ' << object.particleLooping << ' '
            << object.hasAnimator << ' ' << std::quoted(object.animatorClipName) << ' '
            << object.animatorSpeed << ' ' << object.animatorLoop << ' ' << object.animatorPlaying << ' '
            << object.hasAudioListener << ' ' << object.audioListenerVolume << ' '
            << object.audioSourceVolume << ' ' << object.audioSourcePitch << ' '
            << object.audioSourceMinDistance << ' ' << object.audioSourceMaxDistance << ' '
            << object.audioSourceLoop << ' ' << object.audioSourcePlayOnAwake << ' '
            << std::quoted(object.audioSourceClipPath) << ' ' << object.audioSourceSpatial << ' ';
        out << '\n';
    }
    if (!out.good())
    {
        Log("ERROR: Write failed while saving scene.");
        return false;
    }
    m_currentScenePath = path;
    m_statusText = "Scene saved";
    Log("Saved scene: " + path.filename().string());
    return true;
}

bool ForgeEditorUI::LoadScene(ForgeEngine& engine, const std::filesystem::path& path)
{
    std::error_code ec;
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        Log("ERROR: Could not open scene: " + path.string());
        return false;
    }
    std::string signature, token;
    int version = 0;
    size_t objectCount = 0;
    in >> signature >> version;
    // Accept both the legacy DX12FORGE_SCENE tag and the current FORGEENGINE_SCENE tag
    // so scenes authored before the engine rename keep loading.
    if (!in || (signature != "FORGEENGINE_SCENE" && signature != "DX12FORGE_SCENE") || (version < 1 || version > 11))
    {
        Log("ERROR: Invalid or unsupported scene file.");
        return false;
    }
    in >> token >> objectCount;
    if (!in || token != "OBJECT_COUNT" || objectCount > 100000)
    {
        Log("ERROR: Invalid object count in scene.");
        return false;
    }

    std::vector<ForgeEngine::GameObject> loaded;
    loaded.reserve(objectCount);
    for (size_t i = 0; i < objectCount; ++i)
    {
        ForgeEngine::GameObject object;
        int meshKind = 0;
        in >> token >> std::quoted(object.name) >> meshKind
            >> object.position.x >> object.position.y >> object.position.z
            >> object.rotation.x >> object.rotation.y >> object.rotation.z
            >> object.scale.x >> object.scale.y >> object.scale.z
            >> object.baseColor.x >> object.baseColor.y >> object.baseColor.z >> object.baseColor.w
            >> object.metallic >> object.roughness
            >> object.meshRendererEnabled >> object.castShadows
            >> object.hasCollider >> object.hasAudioSource >> object.hasScript
            >> std::quoted(object.albedoTexturePath) >> std::quoted(object.normalTexturePath)
            >> std::quoted(object.metallicRoughnessTexturePath) >> std::quoted(object.aoTexturePath);
        if (in && version >= 2)
        {
            int lightKind = 0;
            in >> lightKind >> object.lightEnabled
                >> object.lightColor.x >> object.lightColor.y >> object.lightColor.z
                >> object.lightIntensity >> object.lightRange
                >> object.lightInnerAngle >> object.lightOuterAngle;
            object.lightType = (lightKind >= 0 && lightKind <= 3)
                ? static_cast<ForgeEngine::LightType>(lightKind) : ForgeEngine::LightType::None;
            if (version >= 3)
            {
                if (version >= 4)
                    in >> object.isTerrain >> object.isCamera >> object.cameraFieldOfView >> object.cameraNearClip >> object.cameraFarClip;
                if (version >= 5) in >> std::quoted(object.importedModelPath);
                size_t scriptCount = 0;
                in >> scriptCount;
                if (!in || scriptCount > 1024)
                {
                    Log("ERROR: Invalid C# script component count in scene.");
                    return false;
                }
                for (size_t scriptIndex = 0; scriptIndex < scriptCount; ++scriptIndex)
                {
                    ForgeEngine::ScriptComponentData script;
                    in >> std::quoted(script.className) >> std::quoted(script.sourceFile) >> script.enabled;
                    if (!in)
                    {
                        Log("ERROR: Could not read C# script component data from scene.");
                        return false;
                    }
                    object.scriptComponents.push_back(std::move(script));
                }
                object.hasScript = !object.scriptComponents.empty();
                if (version >= 5)
                {
                    in >> object.parentIndex;
                    size_t savedTerrainHeightCount = 0;
                    in >> savedTerrainHeightCount;
                    if (savedTerrainHeightCount > 65u * 65u) { Log("ERROR: Invalid terrain height count in scene."); return false; }
                    if (savedTerrainHeightCount > 0)
                    {
                        object.terrainVertices.resize(savedTerrainHeightCount);
                        for (auto& v : object.terrainVertices) in >> v.position.y;
                    }
                    if (version >= 6)
                        in >> object.textureTiling.x >> object.textureTiling.y >> object.textureOffset.x >> object.textureOffset.y;
                    if (version >= 7)
                    {
                        in >> std::quoted(object.tag) >> object.layer >> object.colliderType
                           >> object.colliderSize.x >> object.colliderSize.y >> object.colliderSize.z
                           >> object.isTrigger >> object.hasRigidbody >> object.rigidbodyMass
                           >> object.rigidbodyUseGravity >> object.rigidbodyIsKinematic;
                        if (object.tag.empty()) object.tag = "Untagged";
                        object.layer = std::clamp(object.layer, 0, 31);
                        object.colliderType = std::clamp(object.colliderType, 0, 2);
                        object.colliderSize.x = std::max(0.01f, object.colliderSize.x);
                        object.colliderSize.y = std::max(0.01f, object.colliderSize.y);
                        object.colliderSize.z = std::max(0.01f, object.colliderSize.z);
                        object.rigidbodyMass = std::max(0.01f, object.rigidbodyMass);
                    }
                    if (version >= 8)
                    {
                        in >> std::quoted(object.emissiveTexturePath)
                           >> object.emissiveColor.x >> object.emissiveColor.y >> object.emissiveColor.z
                           >> object.emissiveIntensity >> object.normalMapScale >> object.occlusionStrength
                           >> object.doubleSided;
                        object.emissiveIntensity = std::clamp(object.emissiveIntensity, 0.0f, 64.0f);
                        object.normalMapScale = std::clamp(object.normalMapScale, 0.0f, 8.0f);
                        object.occlusionStrength = std::clamp(object.occlusionStrength, 0.0f, 1.0f);
                    }
                    if (version >= 9)
                    {
                        in >> object.cameraOrthographic >> object.cameraOrthographicSize
                           >> object.cameraDepth
                           >> object.cameraBackgroundColor.x >> object.cameraBackgroundColor.y
                           >> object.cameraBackgroundColor.z >> object.cameraBackgroundColor.w
                           >> object.cameraDrawSkybox >> object.cameraDrawGizmos
                           >> object.cameraCullingMask;
                        object.cameraOrthographicSize = std::clamp(object.cameraOrthographicSize, 0.1f, 500.0f);
                        object.cameraDepth = std::clamp(object.cameraDepth, -100, 100);
                        in >> object.hasMaterial;
                    }
                    if (version >= 10)
                        in >> object.colliderCenter.x >> object.colliderCenter.y >> object.colliderCenter.z;
                    if (version >= 11)
                    {
                        in >> object.hasCharacterController >> object.characterHeight >> object.characterRadius
                           >> object.characterSlopeLimit >> object.characterStepOffset >> object.characterSkinWidth
                           >> object.characterGravity >> object.characterSpeed >> object.characterJumpHeight
                           >> object.hasParticleSystem >> object.particleMaxCount >> object.particleLifetime
                           >> object.particleStartSpeed >> object.particleStartSize >> object.particleGravity
                           >> object.particleEmissionRate
                           >> object.particleStartColor.x >> object.particleStartColor.y >> object.particleStartColor.z
                           >> object.particleDirection.x >> object.particleDirection.y >> object.particleDirection.z
                           >> object.particleSpread >> object.particleLooping
                           >> object.hasAnimator >> std::quoted(object.animatorClipName)
                           >> object.animatorSpeed >> object.animatorLoop >> object.animatorPlaying
                           >> object.hasAudioListener >> object.audioListenerVolume
                           >> object.audioSourceVolume >> object.audioSourcePitch
                           >> object.audioSourceMinDistance >> object.audioSourceMaxDistance
                           >> object.audioSourceLoop >> object.audioSourcePlayOnAwake
                           >> std::quoted(object.audioSourceClipPath) >> object.audioSourceSpatial;
                        object.characterHeight = std::clamp(object.characterHeight, 0.2f, 20.0f);
                        object.characterRadius = std::clamp(object.characterRadius, 0.05f, 10.0f);
                        object.particleMaxCount = std::clamp(object.particleMaxCount, 1, 100000);
                        object.particleLifetime = std::clamp(object.particleLifetime, 0.05f, 120.0f);
                        object.particleStartSize = std::max(0.001f, object.particleStartSize);
                        object.animatorSpeed = std::clamp(object.animatorSpeed, -10.0f, 10.0f);
                    }
                }
            }
        }
        if (!in || token != "OBJECT")
        {
            Log("ERROR: Scene parsing failed. Existing scene was preserved.");
            return false;
        }
        if (std::abs(object.textureTiling.x) < 0.001f) object.textureTiling.x = 1.0f;
        if (std::abs(object.textureTiling.y) < 0.001f) object.textureTiling.y = 1.0f;
        switch (meshKind)
        {
        case 1: object.mesh = &engine.m_sphereMesh; object.localBoundsRadius = 1.0f; break;
        case 2: object.mesh = &engine.m_primitiveMeshes[0]; object.localBoundsRadius = 1.414f; break;
        case 3: object.mesh = &engine.m_primitiveMeshes[1]; object.localBoundsRadius = 1.414f; break;
        case 4: object.mesh = &engine.m_primitiveMeshes[2]; object.localBoundsRadius = 1.414f; break;
        case 5: object.mesh = &engine.m_primitiveMeshes[3]; object.localBoundsRadius = 1.0f; break;
        default: object.mesh = &engine.m_cubeMesh; object.localBoundsRadius = 1.732f; break;
        }
        if (version >= 5 && !object.importedModelPath.empty() && !object.isTerrain && !object.isCamera)
        {
            auto modelPath = ResolveAssetPath(object.importedModelPath);
            if (std::filesystem::exists(modelPath, ec))
            {
                Assimp::Importer importer;
                const aiScene* modelScene = importer.ReadFile(modelPath.string(), aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                    aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType |
                    aiProcess_ValidateDataStructure | aiProcess_PreTransformVertices | aiProcess_FlipUVs | aiProcess_FixInfacingNormals);
                std::vector<ForgeEngine::Vertex> modelVertices;
                std::vector<std::uint32_t> modelIndices;
                if (modelScene && modelScene->HasMeshes())
                {
                    XMFLOAT3 lo{ FLT_MAX, FLT_MAX, FLT_MAX }, hi{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    for (unsigned int mi=0; mi<modelScene->mNumMeshes; ++mi)
                    {
                        const aiMesh* src=modelScene->mMeshes[mi]; if (!src || !src->HasPositions()) continue;
                        const auto base=static_cast<std::uint32_t>(modelVertices.size());
                        for (unsigned int vi=0; vi<src->mNumVertices; ++vi)
                        {
                            ForgeEngine::Vertex v{}; const auto& pos=src->mVertices[vi]; v.position={pos.x,pos.y,pos.z};
                            lo.x=(std::min)(lo.x,pos.x);lo.y=(std::min)(lo.y,pos.y);lo.z=(std::min)(lo.z,pos.z);
                            hi.x=(std::max)(hi.x,pos.x);hi.y=(std::max)(hi.y,pos.y);hi.z=(std::max)(hi.z,pos.z);
                            if(src->HasNormals()) v.normal=Normalize3({src->mNormals[vi].x,src->mNormals[vi].y,src->mNormals[vi].z}); else v.normal={0,1,0};
                            if(src->HasTextureCoords(0)) v.uv={src->mTextureCoords[0][vi].x,src->mTextureCoords[0][vi].y};
                            modelVertices.push_back(v);
                        }
                        for(unsigned int fi=0;fi<src->mNumFaces;++fi) { const auto& face=src->mFaces[fi]; for(unsigned int j=0;j<face.mNumIndices;++j) modelIndices.push_back(base+face.mIndices[j]); }
                    }
                    if (!modelVertices.empty() && !modelIndices.empty())
                    {
                        const XMFLOAT3 center{(lo.x+hi.x)*0.5f,(lo.y+hi.y)*0.5f,(lo.z+hi.z)*0.5f};
                        const float largest=(std::max)({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
                        if(largest>1.0e-5f) for(auto& v:modelVertices) { v.position.x=(v.position.x-center.x)*2.0f/largest; v.position.y=(v.position.y-center.y)*2.0f/largest; v.position.z=(v.position.z-center.z)*2.0f/largest; }
                        auto mesh=std::make_unique<ForgeEngine::Mesh>(); engine.CreateMesh(*mesh,modelVertices,modelIndices);
                        object.localBoundsRadius=1.75f; object.mesh=mesh.get(); engine.m_importedMeshes.push_back(std::move(mesh));
                    }
                }
                else Log("WARNING: Imported model could not be reloaded: " + modelPath.string());
            }
            else Log("WARNING: Missing imported model asset: " + modelPath.string());
        }
        if (object.isTerrain)
        {
            // Reconstruct a flat editable terrain grid on older scenes. v5 stores heights below.
            constexpr int side = 65;
            std::vector<ForgeEngine::Vertex> terrainVertices;
            std::vector<std::uint32_t> terrainIndices;
            terrainVertices.reserve(side * side);
            const auto savedHeights = std::move(object.terrainVertices);
            const bool hasSavedHeights = savedHeights.size() == static_cast<size_t>(side * side);
            for (int z = 0; z < side; ++z) for (int x = 0; x < side; ++x)
            {
                ForgeEngine::Vertex v{};
                const size_t vertexIndex = static_cast<size_t>(z * side + x);
                const float savedHeight = hasSavedHeights ? savedHeights[vertexIndex].position.y : 0.0f;
                v.position = { (static_cast<float>(x) / 64.0f - 0.5f) * 32.0f, savedHeight,
                               (static_cast<float>(z) / 64.0f - 0.5f) * 32.0f };
                v.normal = { 0.0f, 1.0f, 0.0f };
                v.uv = { static_cast<float>(x) / 8.0f, static_cast<float>(z) / 8.0f };
                terrainVertices.push_back(v);
            }
            for (int z = 0; z < 64; ++z) for (int x = 0; x < 64; ++x)
            {
                const std::uint32_t a = static_cast<std::uint32_t>(z * side + x), b = a + 1, c = a + side, d = c + 1;
                terrainIndices.insert(terrainIndices.end(), { a, c, b, b, c, d });
            }
            auto mesh = std::make_unique<ForgeEngine::Mesh>();
            engine.CreateMesh(*mesh, terrainVertices, terrainIndices);
            object.mesh = mesh.get();
            object.terrainVertices = std::move(terrainVertices);
            object.terrainIndices = std::move(terrainIndices);
            engine.m_importedMeshes.push_back(std::move(mesh));
        }
        if (object.isCamera)
        {
            object.mesh = &engine.m_primitiveMeshes[2];
            object.castShadows = false;
        }
        if (object.lightType != ForgeEngine::LightType::None)
        {
            object.castShadows = false;
            object.lightIntensity = std::clamp(object.lightIntensity, 0.0f, 180.0f);
            object.lightRange = std::clamp(object.lightRange, 0.1f, 1000.0f);
            object.lightInnerAngle = std::clamp(object.lightInnerAngle, 1.0f, 87.0f);
            object.lightOuterAngle = std::clamp(object.lightOuterAngle, 2.0f, 89.0f);
        }
        object.scale.x = std::max(0.01f, object.scale.x);
        object.scale.y = std::max(0.01f, object.scale.y);
        object.scale.z = std::max(0.01f, object.scale.z);
        object.metallic = std::clamp(object.metallic, 0.0f, 1.0f);
        object.roughness = std::clamp(object.roughness, 0.045f, 1.0f);
        engine.RebuildWorldTransform(object);
        loaded.push_back(std::move(object));
    }

    // Reject self-links and cyclic references in serialized hierarchies.
    for (size_t i = 0; i < loaded.size(); ++i)
    {
        int parent = loaded[i].parentIndex;
        if (parent < 0 || parent >= static_cast<int>(loaded.size()) || parent == static_cast<int>(i))
        { loaded[i].parentIndex = -1; continue; }
        int cursor = parent; bool cycle = false;
        for (size_t step = 0; step <= loaded.size() && cursor >= 0 && cursor < static_cast<int>(loaded.size()); ++step)
        {
            if (cursor == static_cast<int>(i)) { cycle = true; break; }
            cursor = loaded[static_cast<size_t>(cursor)].parentIndex;
        }
        if (cycle) loaded[i].parentIndex = -1;
    }
    engine.WaitForGpu();
    // Do not carry an active camera index into a different scene; the same index
    // could refer to an unrelated object and make the viewport jump unexpectedly.
    engine.m_viewportCameraObjectIndex = -1;
    engine.m_objects.clear();
    engine.m_objects = std::move(loaded);
    for (auto& object : engine.m_objects) engine.CreateObjectConstantBuffers(object);
    // Rebuild worlds after the vector move so parent chains resolve correctly.
    for (auto& object : engine.m_objects) engine.RebuildWorldTransform(object);
    engine.SelectSceneObject(-1);
    m_currentScenePath = path;
    m_activeAssetFolder = path.parent_path();
    m_statusText = "Scene loaded";
    Log("Loaded scene: " + path.filename().string());
    return true;
}

bool ForgeEditorUI::SaveSceneDialog(ForgeEngine& engine)
{
    std::array<wchar_t, 32768> fileName{};
    const auto initial = m_currentScenePath.empty() ? m_assetsRoot / L"Scenes" / L"Untitled.forgeScene" : m_currentScenePath;
    const std::wstring initialString = initial.native();
    const size_t copyLength = (std::min)(initialString.size(), fileName.size() - 1);
    std::copy_n(initialString.data(), copyLength, fileName.data());
    fileName[copyLength] = L'\0';
    static const wchar_t filter[] = L"Forge Scene (*.forgeScene)\0*.forgeScene\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = engine.m_hwnd;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrDefExt = L"forgeScene";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return false;
    return SaveScene(engine, std::filesystem::path(fileName.data()));
}

bool ForgeEditorUI::OpenSceneDialog(ForgeEngine& engine)
{
    std::array<wchar_t, 32768> fileName{};
    static const wchar_t filter[] = L"Forge Scene (*.forgeScene)\0*.forgeScene\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = engine.m_hwnd;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    const std::filesystem::path initialDirectory = m_assetsRoot / L"Scenes";
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return false;
    return LoadScene(engine, std::filesystem::path(fileName.data()));
}

void ForgeEditorUI::ImportAssetDialog()
{
    std::array<wchar_t, 32768> fileName{};
    static const wchar_t filter[] = L"Supported assets (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.tga;*.webp;*.obj;*.fbx;*.gltf;*.glb;*.dae;*.stl;*.ply;*.3ds;*.mp4;*.mov;*.avi;*.mkv;*.webm;*.wmv;*.m4v;*.mp3;*.wav;*.ogg;*.forgeScene)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.tga;*.webp;*.obj;*.fbx;*.gltf;*.glb;*.dae;*.stl;*.ply;*.3ds;*.mp4;*.mov;*.avi;*.mkv;*.webm;*.wmv;*.m4v;*.mp3;*.wav;*.ogg;*.forgeScene\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&dialog)) return;
    const auto imported = ImportFile(std::filesystem::path(fileName.data()));
    if (!imported.empty())
    {
        m_selectedAssetPath = imported;
        m_activeAssetFolder = imported.parent_path();
    }
}

std::filesystem::path ForgeEditorUI::CategoryForExtension(const std::filesystem::path& path) const
{
    const std::wstring ext = Lower(path.extension().wstring());
    if (IsImageExtension(path)) return L"Textures";
    if (IsModelExtension(path)) return L"Models";
    if (IsVideoExtension(path)) return L"Videos";
    if (ext == L".mp3" || ext == L".wav" || ext == L".ogg" || ext == L".flac" || ext == L".m4a") return L"Audio";
    if (ext == L".forgeScene") return L"Scenes";
    if (ext == L".hlsl" || ext == L".fx" || ext == L".shader") return L"Shaders";
    if (ext == L".mat") return L"Materials";
    return L"Misc";
}

std::filesystem::path ForgeEditorUI::ImportFile(const std::filesystem::path& source, const std::wstring& forcedCategory)
{
    if (source.empty() || !std::filesystem::exists(source))
    {
        Log("ERROR: Selected asset does not exist.");
        return {};
    }
    const std::filesystem::path category = forcedCategory.empty() ? CategoryForExtension(source) : std::filesystem::path(forcedCategory);
    const auto destinationFolder = m_assetsRoot / category;
    std::error_code ec;
    std::filesystem::create_directories(destinationFolder, ec);
    if (ec)
    {
        Log("ERROR: Could not create asset directory.");
        return {};
    }

    auto destination = destinationFolder / source.filename();
    const auto normalizedSource = source.lexically_normal();
    if (normalizedSource == destination.lexically_normal())
        return destination;
    // Dropping a texture that is already in Project/Assets must be a no-op even if
    // one path was relative and the other absolute, otherwise the editor creates copies.
    ec.clear();
    if (std::filesystem::exists(destination, ec) && !ec)
    {
        std::error_code sameFileError;
        if (std::filesystem::equivalent(source, destination, sameFileError) && !sameFileError)
            return destination;
    }
    ec.clear();

    for (int suffix = 1; std::filesystem::exists(destination, ec); ++suffix)
    {
        destination = destinationFolder / (source.stem().wstring() + L"_" + std::to_wstring(suffix) + source.extension().wstring());
        if (suffix > 10000)
        {
            Log("ERROR: Too many duplicate asset names.");
            return {};
        }
    }
    if (!std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, ec) || ec)
    {
        Log("ERROR: Failed to import asset: " + source.filename().string());
        return {};
    }
    Log("Imported asset: " + destination.filename().string());
    InvalidateAssetCache();
    m_statusText = "Asset imported";
    return destination;
}

std::string ForgeEditorUI::RelativeAssetPath(const std::filesystem::path& path) const
{
    if (path.empty()) return {};
    std::error_code ec;
    const auto relative = std::filesystem::relative(path, m_projectRoot, ec);
    return (ec ? path : relative).generic_string();
}

std::filesystem::path ForgeEditorUI::ResolveAssetPath(const std::string& relativePath) const
{
    if (relativePath.empty()) return {};
    return (m_projectRoot / std::filesystem::path(relativePath)).lexically_normal();
}

void ForgeEditorUI::BrowseTexture(ForgeEngine& engine, TextureSlot slot)
{
    if (engine.m_selectedObject < 0 || engine.m_selectedObject >= static_cast<int>(engine.m_objects.size()))
    {
        Log("Select an object before assigning a texture.");
        return;
    }
    std::array<wchar_t, 32768> fileName{};
    static const wchar_t filter[] = L"Texture files (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.gif;*.ico)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.gif;*.ico\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = engine.m_hwnd;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return;
    const auto imported = ImportFile(std::filesystem::path(fileName.data()), L"Textures");
    if (imported.empty()) return;
    auto& object = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
    const std::string relative = RelativeAssetPath(imported);
    switch (slot)
    {
    case TextureSlot::Albedo: object.albedoTexturePath = relative; break;
    case TextureSlot::Normal: object.normalTexturePath = relative; break;
    case TextureSlot::MetallicRoughness: object.metallicRoughnessTexturePath = relative; break;
    case TextureSlot::AmbientOcclusion: object.aoTexturePath = relative; break;
    case TextureSlot::Emissive: object.emissiveTexturePath = relative; break;
    }
    Log("Texture reference assigned: " + relative);
}

void ForgeEditorUI::AssignSelectedAsset(ForgeEngine& engine, TextureSlot slot)
{
    if (engine.m_selectedObject < 0 || engine.m_selectedObject >= static_cast<int>(engine.m_objects.size()) || m_selectedAssetPath.empty()) return;
    const auto path = m_selectedAssetPath;
    if (!std::filesystem::exists(path) || !IsImageExtension(path))
    {
        Log("Choose a texture file in Project Files first.");
        return;
    }
    auto& object = engine.m_objects[static_cast<size_t>(engine.m_selectedObject)];
    const std::string relative = RelativeAssetPath(path);
    switch (slot)
    {
    case TextureSlot::Albedo: object.albedoTexturePath = relative; break;
    case TextureSlot::Normal: object.normalTexturePath = relative; break;
    case TextureSlot::MetallicRoughness: object.metallicRoughnessTexturePath = relative; break;
    case TextureSlot::AmbientOcclusion: object.aoTexturePath = relative; break;
    case TextureSlot::Emissive: object.emissiveTexturePath = relative; break;
    }
    Log("Texture reference assigned: " + relative);
}

void ForgeEditorUI::Log(const std::string& message)
{
    m_logs.push_back(message);
    if (m_logs.size() > 800) m_logs.erase(m_logs.begin(), m_logs.begin() + 100);
}

void ForgeEditorUI::ResetDockLayout()
{
    // Defer docking tree destruction until Draw() finishes the menu and reaches
    // EnsureDefaultDockLayout(). Removing the tree inside BeginMenu() can leave
    // windows referencing nodes being destroyed in the middle of the frame.
    m_dockLayoutInitialized = false;
    m_forceDefaultDockLayout = true;
}

void ForgeEditorUI::RenderDrawData(ForgeEngine& engine)
{
    if (!m_dx12Initialized || !m_contextInitialized || !ImGui::GetCurrentContext() || !ImGui::GetDrawData())
        return;
    if (engine.m_frameIndex >= ForgeEngine::FrameCount ||
        !engine.m_commandList || !engine.m_renderTargets[engine.m_frameIndex] || !m_srvHeap)
    {
        OutputDebugStringA("Forge Engine: skipped ImGui draw because a frame resource is unavailable.\n");
        return;
    }
    D3D12_RESOURCE_BARRIER toRenderTarget = ForgeEngine::TransitionBarrier(
        engine.m_renderTargets[engine.m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    engine.m_commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = engine.m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(engine.m_frameIndex) * engine.m_rtvDescriptorSize;
    engine.m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    engine.m_commandList->RSSetViewports(1, &engine.m_viewport);
    engine.m_commandList->RSSetScissorRects(1, &engine.m_scissorRect);
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    engine.m_commandList->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), engine.m_commandList.Get());

    D3D12_RESOURCE_BARRIER toPresent = ForgeEngine::TransitionBarrier(
        engine.m_renderTargets[engine.m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    engine.m_commandList->ResourceBarrier(1, &toPresent);
}

bool ForgeEditorUI::CompileScripts(bool logSuccess)
{
    if (m_scriptBuildInProgress)
    {
        Log("C# build is already running.");
        return false;
    }
    if (!m_scriptRuntime)
        m_scriptRuntime = std::make_unique<ForgeScriptRuntime>();

    m_scriptBuildInProgress = true;
    std::vector<std::string> buildLogs;
    const bool success = m_scriptRuntime->Build(m_sourceRoot, m_projectRoot, buildLogs);
    for (const auto& line : buildLogs) Log(line);
    if (success && logSuccess) Log("C# scripts are ready. Assembly: ForgeGameScripts.dll");
    if (success) m_statusText = "C# scripts compiled";
    else m_statusText = "C# script compilation failed — see Console";
    m_lastScriptSourceSignature = CalculateScriptSourceSignature();
    m_scriptFilesChanged = false;
    m_scriptBuildInProgress = false;
    return success;
}

void ForgeEditorUI::StartPlayMode(ForgeEngine& engine)
{
    if (m_playPreview) return;
    // Exported games ship precompiled DLLs; invoking dotnet here would be slow and useless.
    if (!engine.m_standalonePlayer && !CompileScripts(false))
    {
        Log("Play cancelled because the C# scripts did not compile. Fix the errors in Visual Studio and save again.");
        return;
    }

    m_playTransformSnapshots.clear();
    m_playTransformSnapshots.reserve(engine.m_objects.size());
    for (const auto& object : engine.m_objects)
    {
        PlayTransformSnapshot snapshot{};
        snapshot.name = object.name;
        snapshot.scriptEnabled.reserve(object.scriptComponents.size());
        for (const auto& script : object.scriptComponents) snapshot.scriptEnabled.push_back(script.enabled);
        snapshot.position = object.position;
        snapshot.rotation = object.rotation;
        snapshot.scale = object.scale;
        snapshot.lightColor = object.lightColor;
        snapshot.lightIntensity = object.lightIntensity;
        snapshot.lightRange = object.lightRange;
        snapshot.lightInnerAngle = object.lightInnerAngle;
        snapshot.lightOuterAngle = object.lightOuterAngle;
        snapshot.meshRendererEnabled = object.meshRendererEnabled;
        snapshot.lightEnabled = object.lightEnabled;
        m_playTransformSnapshots.push_back(snapshot);
    }

    std::vector<std::string> hostLogs;
    if (!m_scriptRuntime->Start(m_sourceRoot, m_projectRoot, hostLogs))
    {
        for (const auto& line : hostLogs) Log(line);
        m_playTransformSnapshots.clear();
        return;
    }
    for (const auto& line : hostLogs) Log(line);
    m_playPreview = true;
    m_statusText = "Playing C# scripts";
    Log("▶ Play mode started. MonoBehaviour.Start/Update/LateUpdate are active.");
}

void ForgeEditorUI::StopPlayMode(ForgeEngine& engine)
{
    if (m_scriptRuntime) m_scriptRuntime->Stop(&m_logs);
    m_playPreview = false;
    const size_t restoreCount = (std::min)(engine.m_objects.size(), m_playTransformSnapshots.size());
    for (size_t i = 0; i < restoreCount; ++i)
    {
        auto& object = engine.m_objects[i];
        const auto& snapshot = m_playTransformSnapshots[i];
        object.name = snapshot.name;
        for (size_t scriptIndex = 0; scriptIndex < object.scriptComponents.size() && scriptIndex < snapshot.scriptEnabled.size(); ++scriptIndex)
            object.scriptComponents[scriptIndex].enabled = snapshot.scriptEnabled[scriptIndex];
        object.position = snapshot.position;
        object.rotation = snapshot.rotation;
        object.scale = snapshot.scale;
        object.lightColor = snapshot.lightColor;
        object.lightIntensity = snapshot.lightIntensity;
        object.lightRange = snapshot.lightRange;
        object.lightInnerAngle = snapshot.lightInnerAngle;
        object.lightOuterAngle = snapshot.lightOuterAngle;
        object.meshRendererEnabled = snapshot.meshRendererEnabled;
        object.lightEnabled = snapshot.lightEnabled;
        engine.RebuildWorldTransform(object);
    }
    m_playTransformSnapshots.clear();
    m_statusText = "Stopped";
    Log("Play mode stopped. Original transforms restored.");
    if (engine.m_standalonePlayer && engine.m_hwnd) PostMessageW(engine.m_hwnd, WM_CLOSE, 0, 0);
}

namespace
{
    bool CopyTreeSkipping(const std::filesystem::path& source, const std::filesystem::path& destination, std::error_code& ec)
    {
        std::filesystem::create_directories(destination, ec);
        if (ec) return false;
        for (std::filesystem::directory_iterator it(source, ec), end; !ec && it != end; it.increment(ec))
        {
            const auto name = it->path().filename();
            std::error_code kindEc;
            if (it->is_directory(kindEc))
            {
                if (name == L"obj" || name == L".vs" || name == L"Logs") continue;
                if (!CopyTreeSkipping(it->path(), destination / name, ec)) return false;
            }
            else
            {
                std::filesystem::copy_file(it->path(), destination / name,
                    std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) return false;
            }
        }
        return true;
    }

    // Recursively copies files but drops anything that is part of the engine's own
    // project layout (Bin, source tree, projects registry). This is what keeps an
    // exported game from containing a second copy of the engine.
    bool CopyGameAssets(const std::filesystem::path& source, const std::filesystem::path& destination,
        std::error_code& ec)
    {
        std::filesystem::create_directories(destination, ec);
        if (ec) return false;
        static const std::wstring skippedDirectories[] = {
            L"obj", L"bin", L".vs", L"Logs", L"Intermediate", L"Binaries", L"Saved", L"Build",
            L"Exports", L"Scripting", L"Plugins"
        };
        for (std::filesystem::directory_iterator it(source, ec), end; !ec && it != end; it.increment(ec))
        {
            const auto name = it->path().filename();
            std::error_code kindEc;
            if (it->is_directory(kindEc))
            {
                bool skip = false;
                for (const auto& skipped : skippedDirectories) if (name == skipped) { skip = true; break; }
                if (skip) continue;
                if (!CopyGameAssets(it->path(), destination / name, ec)) return false;
            }
            else
            {
                // Never ship engine source or the editor's own project file.
                if (name == L"CMakeLists.txt" || name == L"ForgeEngine.project") continue;
                const auto ext = Lower(it->path().extension().wstring());
                if (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".cs" || ext == L".csproj")
                    continue;
                std::filesystem::copy_file(it->path(), destination / name,
                    std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) return false;
            }
        }
        return true;
    }

    std::wstring SanitizeFileName(std::wstring value)
    {
        const size_t first = value.find_first_not_of(L" \t.");
        const size_t last = value.find_last_not_of(L" \t.");
        if (first == std::wstring::npos) return L"ForgeGame";
        value = value.substr(first, last - first + 1);
        for (wchar_t& ch : value)
        {
            if (ch < 32 || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*') ch = L'_';
        }
        return value.empty() ? std::wstring(L"ForgeGame") : value;
    }
}

void ForgeEditorUI::ExportGame(ForgeEngine& engine)
{
    if (m_projectRoot.empty() || m_sourceRoot.empty())
    {
        Log("ERROR: Open a project before exporting a game.");
        return;
    }
    std::wstring product = SanitizeFileName(std::filesystem::u8path(m_exportProductName).wstring());
    const std::string productNameUtf8 = product.empty() ? "ForgeGame" : std::filesystem::path(product).u8string();

    std::error_code ec;
    std::filesystem::path outputRoot = std::filesystem::u8path(m_exportOutputDir);
    if (outputRoot.empty()) outputRoot = m_sourceRoot / L"Exports";
    else if (outputRoot.is_relative()) outputRoot = std::filesystem::current_path() / outputRoot;
    const auto gameDir = outputRoot / product;
    std::filesystem::remove_all(gameDir, ec); // replaces the previous export of the same game
    std::filesystem::create_directories(gameDir, ec);
    if (ec)
    {
        Log("ERROR: Could not create the export folder: " + gameDir.string());
        return;
    }

    // Save the current scene so the exported build contains what is on screen.
    SaveScene(engine, m_assetsRoot / L"Scenes" / L"Default.forgeScene");

    // 1) Engine runtime files that live next to the editor exe (DLLs + Shaders.hlsl).
    std::array<wchar_t, 32768> executableBuffer{};
    const DWORD executableLength = GetModuleFileNameW(nullptr, executableBuffer.data(),
        static_cast<DWORD>(executableBuffer.size()));
    if (executableLength == 0)
    {
        Log("ERROR: Could not locate the engine executable.");
        return;
    }
    const std::filesystem::path engineExecutable(executableBuffer.data());
    const auto exeDirectory = engineExecutable.parent_path();
    // Unity-style data folder: everything the game needs at runtime lives here, so the
    // game folder only exposes the executable, the data folder and the readme.
    const std::filesystem::path dataFolder = gameDir / (product + L"_Data");
    std::filesystem::create_directories(dataFolder, ec);
    int copiedRuntimeFiles = 0;
    for (std::filesystem::directory_iterator it(exeDirectory, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code kindEc;
        if (it->is_directory(kindEc)) continue;
        const auto ext = Lower(it->path().extension().wstring());
        if (ext == L".exe")
        {
            std::filesystem::copy_file(it->path(), gameDir / (product + L".exe"),
                std::filesystem::copy_options::overwrite_existing, ec);
        }
        else if (ext == L".dll")
        {
            // Native runtime DLLs must stay next to the executable so Windows resolves them.
            std::filesystem::copy_file(it->path(), gameDir / it->path().filename(),
                std::filesystem::copy_options::overwrite_existing, ec);
        }
        else if (it->path().filename() == L"Shaders.hlsl")
        {
            std::filesystem::copy_file(it->path(), dataFolder / L"Shaders.hlsl",
                std::filesystem::copy_options::overwrite_existing, ec);
        }
        else continue;
        if (!ec) ++copiedRuntimeFiles;
    }

    // 2) The precompiled C# script host, laid out the way ForgeScriptRuntime::Start
    //    expects it (<root>/Scripting/ForgeScriptHost/bin/Debug/net8.0).
    const auto hostBin = m_sourceRoot / L"Scripting" / L"ForgeScriptHost" / L"bin" / L"Debug" / L"net8.0";
    if (!std::filesystem::exists(hostBin, ec))
    {
        Log("ERROR: ForgeScriptHost is not built. Press Play once in the editor (or build the solution) before exporting.");
        return;
    }
    if (!CopyTreeSkipping(hostBin, dataFolder / L"Scripting" / L"ForgeScriptHost" / L"bin" / L"Debug" / L"net8.0", ec))
    {
        Log("ERROR: Could not copy the C# script host into the export.");
        return;
    }

    // 3) Game assets and configuration. CopyGameAssets skips the engine's own project
    //    layout (source, Bin, Scripting, Exports) so the game never nests the engine.
    if (!CopyGameAssets(m_projectRoot, dataFolder / L"Project", ec))
    {
        Log("ERROR: Could not copy the project files into the export.");
        return;
    }

    // 4) Optional custom icon shipped next to the exe.
    std::string iconIniValue;
    const std::filesystem::path iconSource = std::filesystem::u8path(m_exportIconPath);
    if (!iconSource.empty() && std::filesystem::exists(iconSource, ec))
    {
        const auto finalIcon = gameDir / (std::wstring(L"GameIcon") + iconSource.extension().wstring());
        std::filesystem::copy_file(iconSource, finalIcon, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) iconIniValue = "GameIcon" + finalIcon.extension().u8string();
        else Log("WARNING: Icon copy failed; the game will use the default icon.");
    }

    // 5) forge_game.ini next to the exe switches the engine into standalone player mode
    //    and points it at the <Product>_Data folder.
    const std::string dataFolderRelative = productNameUtf8 + "_Data";
    const auto iniPath = gameDir / L"forge_game.ini";
    std::ofstream ini(iniPath, std::ios::binary | std::ios::trunc);
    ini << "; Forge Engine standalone game configuration\n";
    ini << "product=" << productNameUtf8 << "\n";
    ini << "company=" << m_exportCompanyName << "\n";
    ini << "creator=" << m_exportCreatorName << "\n";
    ini << "dataFolder=" << dataFolderRelative << "\n";
    if (!iconIniValue.empty()) ini << "icon=" << iconIniValue << "\n";
    static const int resWidths[] = { 640, 1280, 1600, 1920, 2560 };
    static const int resHeights[] = { 480, 720, 900, 1080, 1440 };
    const int ri = std::clamp(m_exportResolutionIndex, 0, 4);
    ini << "width=" << resWidths[ri] << "\n";
    ini << "height=" << resHeights[ri] << "\n";
    ini << "fullscreen=" << (m_exportFullscreen ? "1" : "0") << "\n";
    ini.close();

    std::ofstream readme(gameDir / L"LEIA-ME.txt", std::ios::binary | std::ios::trunc);
    readme << productNameUtf8 << "\n\nExecute \"" << productNameUtf8 << ".exe\" para jogar.\n"
        << "Empresa: " << m_exportCompanyName << "  Criador: " << m_exportCreatorName << "\n"
        << "O overlay de editor (grade, eixos, gizmo e paineis) esta desativado na versao final.\n"
        << "Pressione Ctrl+G apenas para depurar a grade durante testes.\n";
    readme.close();

    Log("Export complete: " + gameDir.string());
    Log("The build ships scripts, scenes, models, textures and audio without the editor debug overlay.");
    m_statusText = "Game exported";
    ShellExecuteW(nullptr, L"open", gameDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (m_exportBuildAndRun)
    {
        const auto exePath = gameDir / (product + L".exe");
        ShellExecuteW(nullptr, L"open", exePath.c_str(), nullptr, gameDir.c_str(), SW_SHOWNORMAL);
    }
    (void)copiedRuntimeFiles;
}

void ForgeEditorUI::DrawExportModal(ForgeEngine& engine)
{
    if (m_openExportModal)
    {
        ImGui::OpenPopup("ExportGameModal");
        m_openExportModal = false;
        if (m_exportOutputDir[0] == '\0')
        {
            const auto defaultDir = (m_sourceRoot / L"Exports").u8string();
            std::snprintf(m_exportOutputDir, sizeof(m_exportOutputDir), "%s", defaultDir.c_str());
        }
    }
    if (ImGui::BeginPopupModal("ExportGameModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Export a standalone Windows game from this project.");
        ImGui::SetNextItemWidth(430.0f);
        ImGui::InputText("Game name##ExportProduct", m_exportProductName, sizeof(m_exportProductName));
        ImGui::SetNextItemWidth(430.0f);
        ImGui::InputText("Company##ExportCompany", m_exportCompanyName, sizeof(m_exportCompanyName));
        ImGui::SetNextItemWidth(430.0f);
        ImGui::InputText("Creator##ExportCreator", m_exportCreatorName, sizeof(m_exportCreatorName));
        ImGui::SetNextItemWidth(430.0f);
        ImGui::InputText("Icon (.ico or .png)##ExportIcon", m_exportIconPath, sizeof(m_exportIconPath));
        ImGui::SameLine();
        if (ImGui::SmallButton("Select...##ExportIconButton"))
        {
            std::array<wchar_t, 4096> fileName{};
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = engine.m_hwnd;
            dialog.lpstrFilter = L"Icon files\0*.ico;*.png;*.bmp\0All files\0*.*\0";
            dialog.lpstrFile = fileName.data();
            dialog.nMaxFile = static_cast<DWORD>(fileName.size());
            dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&dialog))
            {
                const auto utf8 = std::filesystem::path(fileName.data()).u8string();
                std::snprintf(m_exportIconPath, sizeof(m_exportIconPath), "%s", utf8.c_str());
            }
        }
        ImGui::SetNextItemWidth(430.0f);
        ImGui::InputText("Export folder##ExportDir", m_exportOutputDir, sizeof(m_exportOutputDir));
        ImGui::SameLine();
        if (ImGui::SmallButton("Browse...##ExportDirButton"))
        {
            BROWSEINFOW browse{};
            browse.hwndOwner = engine.m_hwnd;
            browse.lpszTitle = L"Choose the export folder";
            browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            if (const PIDLIST_ABSOLUTE picked = SHBrowseForFolderW(&browse))
            {
                std::array<wchar_t, 4096> pathBuffer{};
                if (SHGetPathFromIDListW(picked, pathBuffer.data()))
                {
                    const auto utf8 = std::filesystem::path(pathBuffer.data()).u8string();
                    std::snprintf(m_exportOutputDir, sizeof(m_exportOutputDir), "%s", utf8.c_str());
                }
                CoTaskMemFree(picked);
            }
        }
        ImGui::TextDisabled("A subfolder with the game name is created inside the export folder.");
        ImGui::SeparatorText("Build Settings");
        static const char* resolutionLabels[] = { "640 x 480", "1280 x 720", "1600 x 900", "1920 x 1080", "2560 x 1440" };
        ImGui::SetNextItemWidth(200.0f);
        ImGui::Combo("Resolution##ExportRes", &m_exportResolutionIndex, resolutionLabels, IM_ARRAYSIZE(resolutionLabels));
        ImGui::SameLine();
        ImGui::Checkbox("Fullscreen", &m_exportFullscreen);
        ImGui::Checkbox("Build and Run", &m_exportBuildAndRun);
        ImGui::SeparatorText("Included in the build");
        ImGui::TextDisabled("C# scripts, scenes, 3D models, textures, audio, presets and the engine runtime.");
        ImGui::SeparatorText("Excluded from the build");
        ImGui::TextDisabled("The editor debug viewport: gizmo, ground grid, axes, panels, console and Play controls.");
        ImGui::Spacing();
        if (ImGui::Button("Export Game", ImVec2(180.0f, 0.0f)))
        {
            ExportGame(engine);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void ForgeEditorUI::RequestCreateScript(int objectIndex)
{
    m_scriptCreateTargetIndex = objectIndex;
    std::snprintf(m_newScriptClassName, sizeof(m_newScriptClassName), "%s", "NewBehaviourScript");
    if (m_projectRoot.empty())
    {
        Log("ERROR: Project root has not been initialized.");
        return;
    }
    // Open the modal after closing the Add Component popup so both use the same ImGui ID stack.
    m_openCreateScriptModal = true;
}

bool ForgeEditorUI::CreateAndAttachScript(ForgeEngine& engine)
{
    if (m_scriptCreateTargetIndex < 0 || m_scriptCreateTargetIndex >= static_cast<int>(engine.m_objects.size()))
    {
        Log("ERROR: Select an object before adding a C# script.");
        return false;
    }
    std::string className(m_newScriptClassName);
    const size_t first = className.find_first_not_of(" \t\r\n");
    const size_t last = className.find_last_not_of(" \t\r\n");
    className = first == std::string::npos ? std::string{} : className.substr(first, last - first + 1);
    auto isStart = [](unsigned char ch) { return std::isalpha(ch) != 0 || ch == '_'; };
    auto isPart = [](unsigned char ch) { return std::isalnum(ch) != 0 || ch == '_'; };
    if (className.empty() || !isStart(static_cast<unsigned char>(className[0])) ||
        !std::all_of(className.begin(), className.end(), [&](char ch) { return isPart(static_cast<unsigned char>(ch)); }))
    {
        Log("ERROR: C# class names must start with a letter or underscore and contain only letters, numbers, or underscores.");
        return false;
    }

    const auto scriptsFolder = m_projectRoot / L"Scripts";
    std::error_code ec;
    std::filesystem::create_directories(scriptsFolder, ec);
    const auto scriptFile = scriptsFolder / std::filesystem::u8path(className + ".cs");
    if (std::filesystem::exists(scriptFile, ec))
    {
        Log("ERROR: Script file already exists: " + scriptFile.filename().string() + ". Choose another class name.");
        return false;
    }

    std::ofstream file(scriptFile, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        Log("ERROR: Could not create C# script: " + scriptFile.string());
        return false;
    }
    file << "using ForgeEngine;\n\n"
         << "public sealed class " << className << " : MonoBehaviour\n{\n"
         << "    // Start runs once when Play begins. Use Debug.Log to write to the Forge Console.\n"
         << "    public override void Start()\n    {\n"
         << "        Debug.Log($\"{gameObject.name}: " << className << " started\");\n"
         << "    }\n\n"
         << "    // Update runs every frame while this component is enabled.\n"
         << "    public override void Update()\n    {\n"
         << "        // Example: transform.Rotate(0f, 45f * Time.deltaTime, 0f);\n"
         << "        // Example: transform.position += Vector3.right * Time.deltaTime;\n"
         << "    }\n"
         << "}\n";
    file.close();
    if (!file)
    {
        Log("ERROR: Failed while writing C# script: " + scriptFile.string());
        return false;
    }

    auto& object = engine.m_objects[static_cast<size_t>(m_scriptCreateTargetIndex)];
    object.scriptComponents.push_back({ className, className + ".cs", true });
    object.hasScript = true;
    Log("Created and attached C# script '" + className + "' to '" + object.name + "'.");
    CompileScripts(false);
    OpenScriptInVisualStudio(scriptFile);
    m_lastScriptSourceSignature = CalculateScriptSourceSignature();
    m_scriptFilesChanged = false;
    m_scriptCreateTargetIndex = -1;
    return true;
}

std::filesystem::path ForgeEditorUI::FindVisualStudioExecutable() const
{
    const wchar_t* keyPaths[] = {
        L"SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VS7"
    };
    const wchar_t* versions[] = { L"18.0", L"17.0", L"16.0", L"15.0" };
    for (const wchar_t* keyPath : keyPaths)
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
            continue;
        for (const wchar_t* version : versions)
        {
            std::array<wchar_t, 2048> installPath{};
            DWORD type = 0, bytes = static_cast<DWORD>(installPath.size() * sizeof(wchar_t));
            if (RegQueryValueExW(key, version, nullptr, &type, reinterpret_cast<BYTE*>(installPath.data()), &bytes) != ERROR_SUCCESS ||
                (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
            const auto candidate = std::filesystem::path(installPath.data()) / L"Common7" / L"IDE" / L"devenv.exe";
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) { RegCloseKey(key); return candidate; }
        }
        RegCloseKey(key);
    }

    std::array<wchar_t, 32768> programFiles{};
    DWORD length = GetEnvironmentVariableW(L"ProgramFiles", programFiles.data(), static_cast<DWORD>(programFiles.size()));
    if (length > 0 && length < programFiles.size())
    {
        const auto base = std::filesystem::path(programFiles.data());
        const std::filesystem::path candidates[] = {
            base / L"Microsoft Visual Studio" / L"18" / L"Community" / L"Common7" / L"IDE" / L"devenv.exe",
            base / L"Microsoft Visual Studio" / L"18" / L"Professional" / L"Common7" / L"IDE" / L"devenv.exe",
            base / L"Microsoft Visual Studio" / L"2026" / L"Community" / L"Common7" / L"IDE" / L"devenv.exe",
            base / L"Microsoft Visual Studio" / L"2022" / L"Community" / L"Common7" / L"IDE" / L"devenv.exe",
            base / L"Microsoft Visual Studio" / L"2022" / L"Professional" / L"Common7" / L"IDE" / L"devenv.exe",
            base / L"Microsoft Visual Studio" / L"2022" / L"Enterprise" / L"Common7" / L"IDE" / L"devenv.exe"
        };
        for (const auto& candidate : candidates)
        {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) return candidate;
        }
    }
    return {};
}

void ForgeEditorUI::OpenScriptInVisualStudio(const std::filesystem::path& scriptFile)
{
    if (!std::filesystem::exists(scriptFile))
    {
        Log("ERROR: Script file does not exist: " + scriptFile.string());
        return;
    }
    const auto devenv = FindVisualStudioExecutable();
    if (!devenv.empty())
    {
        const auto projectFile = m_projectRoot / L"Scripts" / L"ForgeGameScripts.csproj";
        const std::wstring projectArgs = L"\"" + projectFile.wstring() + L"\"";
        HINSTANCE openProjectResult = ShellExecuteW(nullptr, L"open", devenv.c_str(), projectArgs.c_str(),
            m_sourceRoot.c_str(), SW_SHOWNORMAL);
        const std::wstring editArgs = L"/Edit \"" + scriptFile.wstring() + L"\"";
        HINSTANCE openFileResult = ShellExecuteW(nullptr, L"open", devenv.c_str(), editArgs.c_str(),
            m_sourceRoot.c_str(), SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(openProjectResult) <= 32 && reinterpret_cast<INT_PTR>(openFileResult) <= 32)
            Log("ERROR: Visual Studio could not be opened. Open this file manually: " + scriptFile.string());
        else Log("Opened script with Visual Studio: " + scriptFile.filename().string());
        return;
    }

    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", scriptFile.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
        Log("ERROR: Visual Studio was not found. Install VS 2022/2026 or open the script manually: " + scriptFile.string());
    else
        Log("Opened script using the Windows .cs file association: " + scriptFile.filename().string());
}

std::string ForgeEditorUI::CalculateScriptSourceSignature() const
{
    const auto folder = m_projectRoot / L"Scripts";
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec)) return {};
    std::vector<std::string> entries;
    for (std::filesystem::recursive_directory_iterator it(folder,
        std::filesystem::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
    {
        if (ec) { ec.clear(); continue; }
        if (it->is_directory(ec))
        {
            const auto name = it->path().filename().wstring();
            if (name == L"bin" || name == L"obj") it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        const auto ext = Lower(it->path().extension().wstring());
        if (ext != L".cs" && ext != L".csproj") continue;
        const auto relative = std::filesystem::relative(it->path(), folder, ec).generic_string();
        const auto stamp = std::filesystem::last_write_time(it->path(), ec).time_since_epoch().count();
        const auto size = std::filesystem::file_size(it->path(), ec);
        entries.push_back(relative + "|" + std::to_string(stamp) + "|" + std::to_string(size));
    }
    std::sort(entries.begin(), entries.end());
    std::string signature;
    for (const auto& entry : entries) { signature += entry; signature.push_back('\n'); }
    return signature;
}

void ForgeEditorUI::PollScriptFileChanges(ForgeEngine&)
{
    const auto now = std::chrono::steady_clock::now();
    if (m_lastScriptPollAt.time_since_epoch().count() != 0 &&
        now - m_lastScriptPollAt < std::chrono::milliseconds(200)) return;
    m_lastScriptPollAt = now;

    const std::string signature = CalculateScriptSourceSignature();
    if (signature != m_lastScriptSourceSignature)
    {
        m_lastScriptSourceSignature = signature;
        m_scriptFilesChanged = true;
        m_scriptChangeObservedAt = now;
        Log("C# source change detected. Automatic build queued...");
        return;
    }
    if (!m_scriptFilesChanged || m_scriptBuildInProgress || now - m_scriptChangeObservedAt < std::chrono::milliseconds(850))
        return;

    m_scriptFilesChanged = false;
    const bool wasPlaying = m_playPreview;
    if (CompileScripts(false))
    {
        if (wasPlaying)
        {
            std::vector<std::string> hostLogs;
            m_scriptRuntime->Stop();
            if (m_scriptRuntime->Start(m_sourceRoot, m_projectRoot, hostLogs))
            {
                for (const auto& line : hostLogs) Log(line);
                Log("Scripts reloaded after file save. Start() will run again for the new session.");
            }
            else
            {
                for (const auto& line : hostLogs) Log(line);
                m_playPreview = false;
                Log("ERROR: Could not restart scripts after automatic compilation.");
            }
        }
        else Log("Automatic C# build completed after save.");
    }
}

void ForgeEditorUI::TickScripts(ForgeEngine& engine, float deltaSeconds)
{
    if (!engine.m_standalonePlayer) PollScriptFileChanges(engine);
    if (!m_playPreview) return;
    if (!m_scriptRuntime || !m_scriptRuntime->IsRunning())
    {
        StopPlayMode(engine);
        return;
    }

    // Apply Instantiate/Destroy requests the host queued during the previous frame.
    if (m_scriptRuntime)
    {
        constexpr float DegToRadLocal = 0.017453292519943295f;
        const auto commands = m_scriptRuntime->PendingCommands();
        for (const auto& command : commands)
        {
            if (command.kind == 1)
            {
                if (engine.DestroyObjectForScript(command.targetId))
                    Log("Destroy removed object ID " + std::to_string(command.targetId) + " at end of frame.");
                continue;
            }
            if (command.sourceId < 0 || command.sourceId >= static_cast<int>(engine.m_objects.size())) continue;
            const auto& source = engine.m_objects[static_cast<size_t>(command.sourceId)];
            const std::string cloneName = command.name.empty() ? source.name + " (Clone)" : command.name;
            const int cloneIndex = engine.CloneObjectForScript(source, cloneName);
            auto& clone = engine.m_objects[static_cast<size_t>(cloneIndex)];
            clone.position = { command.position.x, command.position.y, command.position.z };
            clone.rotation = { command.rotationDegrees.x * DegToRadLocal, command.rotationDegrees.y * DegToRadLocal, command.rotationDegrees.z * DegToRadLocal };
            clone.scale = { std::max(0.001f, command.scale.x), std::max(0.001f, command.scale.y), std::max(0.001f, command.scale.z) };
            engine.RebuildWorldTransform(clone);
            m_scriptRuntime->AddIdMapping(command.tempId, cloneIndex);
            Log("Instantiate created '" + clone.name + "' (ID " + std::to_string(cloneIndex) + ").");
        }
    }

    ForgeScriptInputState input{};
    const bool appFocused = GetForegroundWindow() == engine.m_hwnd;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float mouseScreenX = 0.0f, mouseScreenY = 0.0f;
    if (engine.m_standalonePlayer)
    {
        // No ImGui frames run in the exported player, so Win32 supplies the mouse.
        POINT cursor{}; GetCursorPos(&cursor);
        static POINT previousCursor = cursor;
        ScreenToClient(engine.m_hwnd, &cursor);
        mouseScreenX = static_cast<float>(cursor.x);
        mouseScreenY = static_cast<float>(cursor.y);
        input.mousePositionX = mouseScreenX - engine.m_sceneViewportX;
        input.mousePositionY = mouseScreenY - engine.m_sceneViewportY;
        if (appFocused)
        {
            input.mouseX = static_cast<float>(cursor.x - previousCursor.x);
            input.mouseY = -static_cast<float>(cursor.y - previousCursor.y);
            if (GetAsyncKeyState(VK_LBUTTON) < 0) input.mouseButtons |= 1;
            if (GetAsyncKeyState(VK_RBUTTON) < 0) input.mouseButtons |= 2;
            if (GetAsyncKeyState(VK_MBUTTON) < 0) input.mouseButtons |= 4;
        }
        previousCursor = cursor;
    }
    else
    {
        mouseScreenX = io.MousePos.x;
        mouseScreenY = io.MousePos.y;
        const float sceneLeft = viewport->Pos.x + engine.m_sceneViewportX;
        const float sceneTop = viewport->Pos.y + engine.m_sceneViewportY;
        const bool mouseInScene = mouseScreenX >= sceneLeft && mouseScreenY >= sceneTop &&
            mouseScreenX < sceneLeft + engine.m_sceneViewportWidth && mouseScreenY < sceneTop + engine.m_sceneViewportHeight;
        if (mouseInScene)
        {
            input.mousePositionX = mouseScreenX - sceneLeft;
            input.mousePositionY = mouseScreenY - sceneTop;
        }
        if (appFocused && mouseInScene)
        {
            input.mouseX = io.MouseDelta.x;
            input.mouseY = -io.MouseDelta.y;
            if (io.MouseDown[0]) input.mouseButtons |= 1;
            if (io.MouseDown[1]) input.mouseButtons |= 2;
            if (io.MouseDown[2]) input.mouseButtons |= 4;
        }
    }
    if (appFocused)
    {
        const int keys[] = {
            'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
            '0','1','2','3','4','5','6','7','8','9', VK_SPACE, VK_RETURN, VK_ESCAPE,
            VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
            VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_TAB, VK_BACK, VK_DELETE,
            VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12
        };
        for (int key : keys) if ((GetAsyncKeyState(key) & 0x8000) != 0) input.keysDown.push_back(key);
    }

    constexpr float RadToDeg = 57.29577951308232f;
    constexpr float DegToRad = 0.017453292519943295f;
    std::vector<ForgeScriptObjectState> states;
    states.reserve(engine.m_objects.size());
    for (size_t i = 0; i < engine.m_objects.size(); ++i)
    {
        const auto& object = engine.m_objects[i];
        ForgeScriptObjectState state{};
        state.id = static_cast<int>(i);
        state.name = object.name;
        state.position = { object.position.x, object.position.y, object.position.z };
        state.rotationDegrees = { object.rotation.x * RadToDeg, object.rotation.y * RadToDeg, object.rotation.z * RadToDeg };
        state.scale = { object.scale.x, object.scale.y, object.scale.z };
        state.hasMeshRenderer = object.mesh != nullptr && object.lightType == ForgeEngine::LightType::None;
        state.meshRendererEnabled = object.meshRendererEnabled;
        state.hasCollider = object.hasCollider;
        state.hasAudioSource = object.hasAudioSource;
        state.hasCamera = object.isCamera;
        state.lightType = static_cast<int>(object.lightType);
        state.lightEnabled = object.lightEnabled;
        state.lightColor = { object.lightColor.x, object.lightColor.y, object.lightColor.z };
        state.lightIntensity = object.lightIntensity;
        state.lightRange = object.lightRange;
        state.lightInnerAngle = object.lightInnerAngle;
        state.lightOuterAngle = object.lightOuterAngle;
        state.tag = object.tag;
        state.layer = object.layer;
        state.colliderType = object.colliderType;
        state.colliderSize = { object.colliderSize.x, object.colliderSize.y, object.colliderSize.z };
        state.colliderCenter = { object.colliderCenter.x, object.colliderCenter.y, object.colliderCenter.z };
        state.parentIndex = object.parentIndex;
        state.hasCharacterController = object.hasCharacterController;
        state.characterHeight = object.characterHeight;
        state.characterRadius = object.characterRadius;
        state.characterSlopeLimit = object.characterSlopeLimit;
        state.characterStepOffset = object.characterStepOffset;
        state.characterSkinWidth = object.characterSkinWidth;
        state.characterGravity = object.characterGravity;
        state.characterSpeed = object.characterSpeed;
        state.characterJumpHeight = object.characterJumpHeight;
        state.characterVelocity = { object.characterVelocity.x, object.characterVelocity.y, object.characterVelocity.z };
        state.characterIsGrounded = object.characterIsGrounded;
        state.hasParticleSystem = object.hasParticleSystem;
        state.particleMaxCount = object.particleMaxCount;
        state.particleLifetime = object.particleLifetime;
        state.particleStartSpeed = object.particleStartSpeed;
        state.particleStartSize = object.particleStartSize;
        state.particleGravity = object.particleGravity;
        state.particleEmissionRate = object.particleEmissionRate;
        state.particleStartColor = { object.particleStartColor.x, object.particleStartColor.y, object.particleStartColor.z };
        state.particleDirection = { object.particleDirection.x, object.particleDirection.y, object.particleDirection.z };
        state.particleSpread = object.particleSpread;
        state.particleLooping = object.particleLooping;
        state.hasAnimator = object.hasAnimator;
        state.animatorClipName = object.animatorClipName;
        state.animatorSpeed = object.animatorSpeed;
        state.animatorLoop = object.animatorLoop;
        state.animatorPlaying = object.animatorPlaying;
        state.animatorTime = object.animatorTime;
        state.hasAudioListener = object.hasAudioListener;
        state.audioListenerVolume = object.audioListenerVolume;
        state.audioSourceVolume = object.audioSourceVolume;
        state.audioSourcePitch = object.audioSourcePitch;
        state.audioSourceMinDistance = object.audioSourceMinDistance;
        state.audioSourceMaxDistance = object.audioSourceMaxDistance;
        state.audioSourceLoop = object.audioSourceLoop;
        state.audioSourcePlayOnAwake = object.audioSourcePlayOnAwake;
        state.audioSourceClipPath = object.audioSourceClipPath;
        state.audioSourceSpatial = object.audioSourceSpatial;
        state.isTrigger = object.isTrigger;
        state.hasRigidbody = object.hasRigidbody;
        state.mass = object.rigidbodyMass;
        state.useGravity = object.rigidbodyUseGravity;
        state.isKinematic = object.rigidbodyIsKinematic;
        state.baseColor = { object.baseColor.x, object.baseColor.y, object.baseColor.z };
        state.metallic = object.metallic;
        state.roughness = object.roughness;
        state.emissiveColor = { object.emissiveColor.x, object.emissiveColor.y, object.emissiveColor.z };
        state.emissiveIntensity = object.emissiveIntensity;
        state.normalMapScale = object.normalMapScale;
        state.doubleSided = object.doubleSided;
        state.cameraFieldOfView = object.cameraFieldOfView;
        state.cameraNearClip = object.cameraNearClip;
        state.cameraFarClip = object.cameraFarClip;
        state.cameraOrthographic = object.cameraOrthographic;
        state.cameraOrthographicSize = object.cameraOrthographicSize;
        state.cameraDepth = object.cameraDepth;
        state.cameraBackgroundColor = { object.cameraBackgroundColor.x, object.cameraBackgroundColor.y,
            object.cameraBackgroundColor.z, object.cameraBackgroundColor.w };
        state.cameraDrawSkybox = object.cameraDrawSkybox;
        state.cameraDrawGizmos = object.cameraDrawGizmos;
        state.cameraCullingMask = object.cameraCullingMask;
        for (const auto& script : object.scriptComponents)
            state.scripts.push_back({ script.className, script.enabled });
        states.push_back(std::move(state));
    }

    std::vector<std::string> runtimeLogs;
    if (!m_scriptRuntime->Tick(deltaSeconds, input, states, runtimeLogs))
    {
        for (const auto& line : runtimeLogs) Log(line);
        StopPlayMode(engine);
        return;
    }
    for (const auto& line : runtimeLogs) Log(line);
    for (const auto& state : states)
    {
        if (state.id < 0 || state.id >= static_cast<int>(engine.m_objects.size())) continue;
        auto& object = engine.m_objects[static_cast<size_t>(state.id)];
        object.name = state.name;
        object.position = { state.position.x, state.position.y, state.position.z };
        object.rotation = { state.rotationDegrees.x * DegToRad, state.rotationDegrees.y * DegToRad, state.rotationDegrees.z * DegToRad };
        object.scale = { std::max(0.001f,state.scale.x), std::max(0.001f,state.scale.y), std::max(0.001f,state.scale.z) };
        object.meshRendererEnabled = state.meshRendererEnabled;
        object.lightEnabled = state.lightEnabled;
        object.lightColor = { state.lightColor.x, state.lightColor.y, state.lightColor.z };
        object.lightIntensity = std::clamp(state.lightIntensity,0.0f,180.0f);
        object.lightRange = std::clamp(state.lightRange,0.1f,1000.0f);
        object.lightInnerAngle = std::clamp(state.lightInnerAngle,1.0f,87.0f);
        object.lightOuterAngle = std::clamp(state.lightOuterAngle,2.0f,89.0f);
        if (!state.tag.empty()) object.tag = state.tag;
        object.layer = std::clamp(state.layer, 0, 31);
        object.colliderType = std::clamp(state.colliderType, 0, 2);
        object.colliderSize = { std::max(0.01f,state.colliderSize.x), std::max(0.01f,state.colliderSize.y), std::max(0.01f,state.colliderSize.z) };
        object.colliderCenter = { state.colliderCenter.x, state.colliderCenter.y, state.colliderCenter.z };
        if (state.parentIndex >= -1 && state.parentIndex < static_cast<int>(engine.m_objects.size()))
            object.parentIndex = state.parentIndex;
        object.hasCharacterController = state.hasCharacterController;
        object.characterHeight = std::clamp(state.characterHeight, 0.2f, 20.0f);
        object.characterRadius = std::clamp(state.characterRadius, 0.05f, 10.0f);
        object.characterSlopeLimit = std::clamp(state.characterSlopeLimit, 0.0f, 89.0f);
        object.characterStepOffset = std::clamp(state.characterStepOffset, 0.0f, 2.0f);
        object.characterSkinWidth = std::clamp(state.characterSkinWidth, 0.0f, 0.5f);
        object.characterGravity = state.characterGravity;
        object.characterSpeed = std::max(0.0f, state.characterSpeed);
        object.characterJumpHeight = std::max(0.0f, state.characterJumpHeight);
        object.characterVelocity = { state.characterVelocity.x, state.characterVelocity.y, state.characterVelocity.z };
        object.characterIsGrounded = state.characterIsGrounded;
        object.hasParticleSystem = state.hasParticleSystem;
        object.particleMaxCount = std::clamp(state.particleMaxCount, 1, 100000);
        object.particleLifetime = std::clamp(state.particleLifetime, 0.05f, 120.0f);
        object.particleStartSpeed = std::max(0.0f, state.particleStartSpeed);
        object.particleStartSize = std::max(0.001f, state.particleStartSize);
        object.particleGravity = state.particleGravity;
        object.particleEmissionRate = std::max(0.0f, state.particleEmissionRate);
        object.particleStartColor = { state.particleStartColor.x, state.particleStartColor.y, state.particleStartColor.z };
        object.particleDirection = { state.particleDirection.x, state.particleDirection.y, state.particleDirection.z };
        object.particleSpread = std::clamp(state.particleSpread, 0.0f, 180.0f);
        object.particleLooping = state.particleLooping;
        object.hasAnimator = state.hasAnimator;
        object.animatorClipName = state.animatorClipName;
        object.animatorSpeed = std::clamp(state.animatorSpeed, -10.0f, 10.0f);
        object.animatorLoop = state.animatorLoop;
        object.animatorPlaying = state.animatorPlaying;
        object.animatorTime = std::max(0.0f, state.animatorTime);
        object.hasAudioListener = state.hasAudioListener;
        object.audioListenerVolume = std::clamp(state.audioListenerVolume, 0.0f, 4.0f);
        object.audioSourceVolume = std::clamp(state.audioSourceVolume, 0.0f, 4.0f);
        object.audioSourcePitch = std::clamp(state.audioSourcePitch, 0.1f, 3.0f);
        object.audioSourceMinDistance = std::max(0.01f, state.audioSourceMinDistance);
        object.audioSourceMaxDistance = std::max(object.audioSourceMinDistance, state.audioSourceMaxDistance);
        object.audioSourceLoop = state.audioSourceLoop;
        object.audioSourcePlayOnAwake = state.audioSourcePlayOnAwake;
        object.audioSourceClipPath = state.audioSourceClipPath;
        object.audioSourceSpatial = state.audioSourceSpatial;
        object.isTrigger = state.isTrigger;
        object.hasCollider = state.hasCollider;
        object.hasRigidbody = state.hasRigidbody;
        object.rigidbodyMass = std::max(0.01f, state.mass);
        object.rigidbodyUseGravity = state.useGravity;
        object.rigidbodyIsKinematic = state.isKinematic;
        object.baseColor = { state.baseColor.x, state.baseColor.y, state.baseColor.z, object.baseColor.w };
        object.metallic = std::clamp(state.metallic, 0.0f, 1.0f);
        object.roughness = std::clamp(state.roughness, 0.045f, 1.0f);
        object.emissiveColor = { state.emissiveColor.x, state.emissiveColor.y, state.emissiveColor.z };
        object.emissiveIntensity = std::clamp(state.emissiveIntensity, 0.0f, 64.0f);
        object.normalMapScale = std::clamp(state.normalMapScale, 0.0f, 8.0f);
        object.doubleSided = state.doubleSided;
        object.cameraFieldOfView = std::clamp(state.cameraFieldOfView, 1.0f, 179.0f);
        object.cameraNearClip = std::max(0.001f, state.cameraNearClip);
        object.cameraFarClip = std::max(object.cameraNearClip + 0.01f, state.cameraFarClip);
        object.cameraOrthographic = state.cameraOrthographic;
        object.cameraOrthographicSize = std::clamp(state.cameraOrthographicSize, 0.01f, 500.0f);
        object.cameraDepth = std::clamp(state.cameraDepth, -100, 100);
        object.cameraBackgroundColor = { state.cameraBackgroundColor.x, state.cameraBackgroundColor.y,
            state.cameraBackgroundColor.z, state.cameraBackgroundColor.w };
        object.cameraDrawSkybox = state.cameraDrawSkybox;
        object.cameraDrawGizmos = state.cameraDrawGizmos;
        object.cameraCullingMask = state.cameraCullingMask;
        for (const auto& scriptState : state.scripts)
        {
            for (auto& scriptComponent : object.scriptComponents)
            {
                if (scriptComponent.className == scriptState.className)
                {
                    scriptComponent.enabled = scriptState.enabled;
                    break;
                }
            }
        }
        engine.RebuildWorldTransform(object);
    }
}
