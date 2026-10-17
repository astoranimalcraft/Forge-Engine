#pragma once

#include <windows.h>
#include <shellapi.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <imgui.h>
#include <wrl.h>
#include "ScriptRuntime.h"
#include <memory>
#include <chrono>
#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class DX12ForgeEngine;
struct ImGui_ImplDX12_InitInfo;

class DX12ForgeEditorUI final
{
public:
    DX12ForgeEditorUI() = default;
    ~DX12ForgeEditorUI();

    bool Initialize(DX12ForgeEngine& engine);
    void Shutdown();
    void BeginFrame();
    void Draw(DX12ForgeEngine& engine);
    void TickScripts(DX12ForgeEngine& engine, float deltaSeconds);
    void RenderDrawData(DX12ForgeEngine& engine);
    void HandleWin32Message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    bool WantsCaptureKeyboard() const;
    bool WantsCaptureMouse() const;

private:
    enum class TextureSlot { Albedo, Normal, MetallicRoughness, AmbientOcclusion };

    static void AllocateSrvDescriptor(ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu);
    static void FreeSrvDescriptor(ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    void ApplyStyle();
    void DrawWindowChrome(DX12ForgeEngine& engine);
    void DrawMainMenu(DX12ForgeEngine& engine);
    void DrawHierarchy(DX12ForgeEngine& engine);
    void DrawInspector(DX12ForgeEngine& engine);
    void DrawLightingPanel(DX12ForgeEngine& engine);
    void DrawProjectBrowser(DX12ForgeEngine& engine);
    void DrawPostProcessSettings(DX12ForgeEngine& engine);
    void DrawWelcome(DX12ForgeEngine& engine);
    bool OpenProject(DX12ForgeEngine& engine, const std::filesystem::path& projectPath);
    void LoadProjectPostProcess(DX12ForgeEngine& engine);
    void SaveProjectPostProcess(const DX12ForgeEngine& engine) const;
    void OpenProjectDialog(DX12ForgeEngine& engine);
    void CreateProjectFromDialog(DX12ForgeEngine& engine);
    void DrawConsole();
    void DrawStatsOverlay(DX12ForgeEngine& engine);
    void RequestViewportScreenshot(DX12ForgeEngine& engine);
    void DrawAboutWindow();
    void EnsureDefaultDockLayout(ImGuiID dockspaceId, const ImVec2& size);

    void CreatePrimitive(DX12ForgeEngine& engine, bool sphere);
    void CreatePrimitiveKind(DX12ForgeEngine& engine, int kind);
    void CreateLightObject(DX12ForgeEngine& engine, int lightKind);
    void CreateTerrainObject(DX12ForgeEngine& engine);
    void CreateCameraObject(DX12ForgeEngine& engine);
    void ApplyTerrainBrush(DX12ForgeEngine& engine, float worldX, float worldZ, float deltaSeconds);
    bool InstantiateObj(DX12ForgeEngine& engine, const std::filesystem::path& path);
    void HandleDroppedFiles(HDROP drop);
    DX12ForgeEngine* m_engine = nullptr;
    int m_dragPreviewObjectIndex = -1;
    int m_dragPreviewPreviousSelection = -1;
    std::wstring m_dragPreviewPath;
    std::string m_dragPreviewPreviousTexture;
    int m_dragPreviewTextureTarget = -1;
    bool m_dragPreviewIsTexture = false;
    bool m_dragPreviewLoadFailed = false;
    bool m_dragPreviewOwnsImportedMesh = false;
    // Manual mouse-drag fallback for model assets. This intentionally does not
    // depend on Dear ImGui's internal drag/drop payload state for Scene placement.
    std::filesystem::path m_assetPressPath;
    std::filesystem::path m_manualModelDragPath;
    ImVec2 m_assetPressPosition{ 0.0f, 0.0f };
    bool m_assetPressCandidate = false;
    bool m_manualModelDragActive = false;
    std::string m_lastReportedTextureError;
    DirectX::XMFLOAT4 m_dragPreviewOriginalColor{ 0.82f, 0.82f, 0.82f, 1.0f };
    bool m_dragPreviewOriginalCastShadows = true;
    void DuplicateSelected(DX12ForgeEngine& engine);
    void DeleteSelected(DX12ForgeEngine& engine);
    void NewScene(DX12ForgeEngine& engine);
    bool SaveScene(DX12ForgeEngine& engine, const std::filesystem::path& path);
    bool LoadScene(DX12ForgeEngine& engine, const std::filesystem::path& path);
    bool SaveSceneDialog(DX12ForgeEngine& engine);
    bool OpenSceneDialog(DX12ForgeEngine& engine);
    void ImportAssetDialog();
    std::filesystem::path ImportFile(const std::filesystem::path& source, const std::wstring& forcedCategory = L"");
    void BrowseTexture(DX12ForgeEngine& engine, TextureSlot slot);
    void AssignSelectedAsset(DX12ForgeEngine& engine, TextureSlot slot);
    void Log(const std::string& message);
    std::filesystem::path DetectSourceRoot(const std::filesystem::path& executableDirectory) const;
    std::filesystem::path CategoryForExtension(const std::filesystem::path& path) const;
    std::string RelativeAssetPath(const std::filesystem::path& path) const;
    std::filesystem::path ResolveAssetPath(const std::string& relativePath) const;
    void ResetDockLayout();
    bool CompileScripts(bool logSuccess = true);
    void StartPlayMode(DX12ForgeEngine& engine);
    void StopPlayMode(DX12ForgeEngine& engine);
    void ExportGame(DX12ForgeEngine& engine);
    void DrawExportModal(DX12ForgeEngine& engine);
    void RequestCreateScript(int objectIndex);
    bool CreateAndAttachScript(DX12ForgeEngine& engine);
    struct AssetListingEntry
    {
        std::filesystem::path path;
        bool isDirectory = false;
        unsigned long long size = 0;
        bool sizeValid = false;
    };
    void RefreshAssetCache();
    void InvalidateAssetCache() { m_assetNextScanAt = {}; }
    void OpenScriptInVisualStudio(const std::filesystem::path& scriptFile);
    std::filesystem::path FindVisualStudioExecutable() const;
    std::string CalculateScriptSourceSignature() const;
    void PollScriptFileChanges(DX12ForgeEngine& engine);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    std::array<bool, 1024> m_srvSlots{};
    UINT m_srvDescriptorIncrement = 0;
    std::array<UINT, 2> m_cameraPreviewSrvSlots{ 0xFFFFFFFFu, 0xFFFFFFFFu };
    bool m_contextInitialized = false;
    bool m_win32Initialized = false;
    bool m_dx12Initialized = false;
    bool m_dockLayoutInitialized = false;
    bool m_forceDefaultDockLayout = false;
    ImGuiID m_dockspaceId = 0;

    std::filesystem::path m_sourceRoot;
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_assetsRoot;
    std::unique_ptr<ForgeScriptRuntime> m_scriptRuntime;
    std::string m_lastScriptSourceSignature;
    std::chrono::steady_clock::time_point m_scriptChangeObservedAt{};
    std::chrono::steady_clock::time_point m_lastScriptPollAt{};
    bool m_scriptFilesChanged = false;
    bool m_scriptBuildInProgress = false;
    bool m_openCreateScriptModal = false;
    int m_scriptCreateTargetIndex = -1;
    char m_newScriptClassName[128] = "NewBehaviourScript";
    int m_nextScriptObjectId = 0;
    float m_pendingMouseWheel = 0.0f;
    std::filesystem::path m_activeAssetFolder;
    std::filesystem::path m_selectedAssetPath;
    // Project browser listings are cached and rescanned at most twice per second.
    // Scanning the filesystem every frame made the panel flicker whenever a
    // transient directory error returned a partial listing.
    std::vector<AssetListingEntry> m_assetListingCache;
    std::filesystem::path m_assetListingFolder;
    std::unordered_map<std::wstring, std::vector<std::filesystem::path>> m_subfolderCache;
    std::chrono::steady_clock::time_point m_assetNextScanAt{};
    std::filesystem::path m_currentScenePath;
    std::filesystem::path m_projectLocation;
    std::vector<std::filesystem::path> m_recentProjects;
    std::string m_iniPath;
    char m_newProjectName[128] = "MyGame";
    char m_newProjectLocation[512] = "Projects";
    char m_newFolderName[128] = "NewFolder";
    char m_renameAssetName[260] = "";
    bool m_openCreateFolderModal = false;
    bool m_openRenameAssetModal = false;
    std::filesystem::path m_renameOriginalPath;
    bool m_openPostProcessFromMenu = false;
    bool m_openExportModal = false;
    char m_exportProductName[256] = "MyForgeGame";
    char m_exportCompanyName[256] = "DX12 Forge";
    char m_exportCreatorName[256] = "";
    char m_exportIconPath[512] = "";
    char m_exportOutputDir[512] = "";
    int m_exportResolutionIndex = 3; // 0=640x480 1=1280x720 2=1600x900 3=1920x1080 4=2560x1440
    bool m_exportFullscreen = false;
    bool m_exportBuildAndRun = false;
    std::vector<std::string> m_logs;
    bool m_autoScrollConsole = true;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showLighting = true;
    bool m_showProject = true;
    bool m_showPostProcess = false;
    bool m_showWelcome = true;
    bool m_showCreateProject = false;
    bool m_showConsole = true;
    bool m_showAbout = false;
    bool m_showShortcuts = false;
    bool m_showStats = false;
    char m_hierarchyFilter[256] = "";
    std::string m_lastScreenshotResult;
    bool m_playPreview = false;
    int m_assetFilter = 0;
    std::string m_statusText = "Ready";
    int m_terrainBrushMode = 0; // 0 raise, 1 lower, 2 smooth, 3 flatten
    float m_terrainBrushRadius = 3.0f;
    float m_terrainBrushStrength = 2.0f;
    float m_terrainFlattenHeight = 0.0f;
    float m_terrainLastStampX = 1.0e9f;
    float m_terrainLastStampZ = 1.0e9f;
    std::chrono::steady_clock::time_point m_terrainLastUpload{};
    bool m_terrainNeedsUpload = false;
};
