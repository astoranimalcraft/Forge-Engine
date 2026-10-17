#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

class DX12ForgeEditorUI;

class DX12ForgeEngine final
{
    friend class DX12ForgeEditorUI;
public:
    DX12ForgeEngine();
    ~DX12ForgeEngine();

    bool Initialize(HINSTANCE instance, int showCommand);
    int Run();
    bool HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
    void RequestScreenshot(const std::filesystem::path& path);
    bool IsStandalonePlayer() const { return m_standalonePlayer; }

private:
    static constexpr UINT FrameCount = 2;
    static constexpr UINT ShadowMapSize = 4096; // high-resolution directional shadow map
    static constexpr UINT ConstantBufferAlignment = 256;

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT2 uv{ 0.0f, 0.0f };
    };

    struct Mesh
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexView{};
        D3D12_INDEX_BUFFER_VIEW indexView{};
        UINT vertexCount = 0;
        UINT indexCount = 0;
        // Retain CPU geometry for exact scene picking and conservative bounds tests.
        std::vector<Vertex> cpuVertices;
        std::vector<std::uint32_t> cpuIndices;
        XMFLOAT3 boundsCenter{ 0.0f, 0.0f, 0.0f };
        float boundsRadius = 1.7320508f;
    };

    struct FrameConstants
    {
        XMFLOAT4X4 viewProjection;
        XMFLOAT4X4 inverseViewProjection;
        XMFLOAT4X4 lightViewProjection;
        XMFLOAT3 cameraPosition;
        float timeSeconds = 0.0f;
        XMFLOAT3 lightDirection;
        float viewportWidth = 1.0f;
        XMFLOAT3 lightColor;
        float viewportHeight = 1.0f;
        XMFLOAT2 viewportOrigin{ 0.0f, 0.0f };
        XMFLOAT2 renderTargetSize{ 1440.0f, 900.0f };
        // Must match FrameData in Shaders.hlsl exactly. Counts are stored in z/w.
        XMFLOAT4 lightingParameters{ 4.0f, 0.65f, 1.0f, 1.0f };
        XMFLOAT4 dayNightParameters{ 1.0f, 0.7f, 14.0f, 0.0f }; // daylight, sun elevation, hour, reserved
        XMFLOAT4 postProcessParameters{ 0.35f, 0.25f, 0.0f, 0.08f }; // bloom, AO, motion blur, vignette
        XMFLOAT4 postProcessOptions{ 0.0f, 0.0f, 1.0f, 1.0f }; // chromatic aberration, sharpen, saturation, exposure
        XMFLOAT4 postProcessMotion{ 0.0f, 0.0f, 1.5f, 0.0f }; // screen motion vector, AO radius, reserved
        XMFLOAT4 skyParameters{ 0.35f, 0.20f, 1.0f, 1.0f }; // cloud coverage, cloud altitude, wind speed, grid overlay (1=on)
        std::array<XMFLOAT4, 4> pointLightPositionRange{};
        std::array<XMFLOAT4, 4> pointLightColorIntensity{};
        std::array<XMFLOAT4, 4> spotLightPositionRange{};
        std::array<XMFLOAT4, 4> spotLightDirectionOuterCos{};
        std::array<XMFLOAT4, 4> spotLightColorIntensity{};
        std::array<XMFLOAT4, 4> spotLightConeParameters{};
    };

    struct ObjectConstants
    {
        XMFLOAT4X4 world;
        XMFLOAT4X4 normalTransform;
        XMFLOAT4 baseColor;
        XMFLOAT4 material; // x = metallic, y = roughness, z = has albedo texture, w = light/drag-preview emissive mode
        XMFLOAT4 uvTransform; // xy = tiling, zw = offset
        XMFLOAT4 materialMaps; // x = normal map, y = metallic/roughness map, z = AO map, w = normal scale
    };

    struct VertexRange
    {
        UINT start = 0;
        UINT count = 0;
    };

    enum class TransformMode { Translate, Rotate, Scale };
    enum class LightType : int { None = 0, Directional = 1, Point = 2, Spot = 3 };

    struct ScriptComponentData
    {
        std::string className;
        std::string sourceFile;
        bool enabled = true;
    };

    struct GameObject
    {
        std::string name;
        Mesh* mesh = nullptr;
        XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        XMFLOAT3 rotation{ 0.0f, 0.0f, 0.0f }; // radians
        XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
        XMFLOAT4X4 world{};
        XMFLOAT4 baseColor{ 0.7f, 0.7f, 0.7f, 1.0f };
        float metallic = 0.0f;
        float roughness = 0.6f;
        float localBoundsRadius = 1.7320508f;
        XMFLOAT2 textureTiling{ 1.0f, 1.0f };
        XMFLOAT2 textureOffset{ 0.0f, 0.0f };
        bool meshRendererEnabled = true;
        bool isDragPreview = false; // temporary object shown while dragging an asset over the Scene
        bool isDragPreviewEmissive = false; // model preview remains legible against dark or bright sky
        bool isTerrain = false;
        bool isCamera = false;
        int parentIndex = -1; // Editor hierarchy relation; -1 means a root object.
        bool activeSelf = true; // GameObject.SetActive; inactive objects and their children are skipped by the renderer.
        int scriptObjectId = -1; // Stable C# runtime id for the current Play session (vector indices shift on Destroy).
        std::string tag = "Untagged";
        int layer = 0;
        // World matrix is now composed from the parent chain, so children follow
        // their parents. Editing any ancestor refreshes the whole subtree.
        float cameraFieldOfView = 60.0f;
        float cameraNearClip = 0.05f;
        float cameraFarClip = 1000.0f;
        std::vector<Vertex> terrainVertices;
        std::vector<std::uint32_t> terrainIndices;
        LightType lightType = LightType::None;
        bool lightEnabled = true;
        XMFLOAT3 lightColor{ 1.0f, 0.85f, 0.55f };
        float lightIntensity = 4.0f;
        float lightRange = 12.0f;
        float lightInnerAngle = 18.0f;
        float lightOuterAngle = 30.0f;
        bool castShadows = true;
        bool hasCollider = false;
        int colliderType = 0; // 0=Box, 1=Sphere, 2=Capsule
        XMFLOAT3 colliderSize{ 1.0f, 1.0f, 1.0f };
        bool isTrigger = false;
        bool hasRigidbody = false;
        float rigidbodyMass = 1.0f;
        bool rigidbodyUseGravity = true;
        bool rigidbodyIsKinematic = false;
        bool hasAudioSource = false;
        bool hasScript = false;
        std::vector<ScriptComponentData> scriptComponents;
        std::string importedModelPath;
        std::string albedoTexturePath;
        std::string normalTexturePath;
        std::string metallicRoughnessTexturePath;
        std::string aoTexturePath;
        std::string loadedAlbedoTexturePath;
        std::string loadedNormalTexturePath;
        std::string loadedMetallicRoughnessTexturePath;
        std::string loadedAoTexturePath;
        Microsoft::WRL::ComPtr<ID3D12Resource> albedoTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> normalTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> metallicRoughnessTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> aoTexture;
        UINT albedoSrvIndex = 1; // HDR SRV is a valid fallback when no albedo map is assigned.
        UINT normalSrvIndex = 1;
        UINT metallicRoughnessSrvIndex = 1;
        UINT aoSrvIndex = 1;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> objectConstants;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> outlineConstants;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> colliderConstants;
    };

private:
    void CreateWindowAndDevice(HINSTANCE instance, int showCommand);
    void CreateStartupSplash(HINSTANCE instance);
    void UpdateStartupSplash(int percent, const wchar_t* message);
    void DestroyStartupSplash();
    static LRESULT CALLBACK SplashWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void CreateDeviceAndCommandObjects();
    void CreateSwapChain();
    void CreateDescriptorHeaps();
    void CreateSizeDependentResources();
    void CreateShadowMap();
    void CreateRootSignature();
    void CreateShadersAndPipelines();
    void CreateScene();
    void CreateGizmoMeshes();
    void CreateColliderWireMeshes();
    void CreateEditorWindow(HINSTANCE instance);
    bool HandleEditorMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
    void UpdateEditorInspector();
    void SelectSceneObject(int index);
    void SetGizmoMode(TransformMode mode);
    void SetCursorHidden(bool hidden);
    void RebuildWorldTransform(GameObject& object);
    int IndexOfObject(const GameObject* object) const;
    XMFLOAT4X4 GetAncestorWorld(const GameObject& object) const;
    int CloneObjectForScript(const GameObject& source, const std::string& newName);
    bool DestroyObjectForScript(int index);
    bool IsActiveInHierarchy(const GameObject& object) const;
    void LoadGameConfig();
    void ApplyGameIcon();
    void DrawGizmo(UINT frameIndex);
    void DrawPostProcessPass(UINT frameIndex);
    void CaptureCameraPreviewFrame(UINT frameIndex);
    bool BuildMouseRay(int mouseX, int mouseY, XMFLOAT3& origin, XMFLOAT3& direction) const;
    float GetGizmoLength() const;
    float RayAxisParameter(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDirection,
        const XMFLOAT3& axisOrigin, const XMFLOAT3& axisDirection, float segmentLength, float& separation) const;
    bool IntersectGizmoRing(int axis, const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDirection,
        XMFLOAT3& radial, float& angle) const;
    bool TryBeginGizmoDrag(int mouseX, int mouseY, HWND hwnd);
    void UpdateGizmoDrag(int mouseX, int mouseY);
    void CreateMesh(Mesh& mesh, const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
    void CreateVertexOnlyMesh(Mesh& mesh, const std::vector<Vertex>& vertices);
    void CreateUploadResource(UINT64 size, Microsoft::WRL::ComPtr<ID3D12Resource>& resource);
    void CreateConstantBuffers();
    void CreateObjectConstantBuffers(GameObject& object);
    void EnsureObjectTexture(GameObject& object, int slot);

    void Render(float deltaSeconds);
    void UpdateCamera(float deltaSeconds);
    void UpdateConstantBuffers(float totalSeconds);
    void ProcessPendingSelection();
    void PickObject(int mouseX, int mouseY);
    void ResizeIfNeeded();
    void WaitForGpu();
    void WaitForFrame(UINT frameIndex);

    void DrawShadowPass(UINT frameIndex);
    void DrawScenePass(UINT frameIndex);
    void DrawMesh(const Mesh& mesh);
    void DrawObjects(UINT frameIndex);
    void DrawAxes(UINT frameIndex);
    void DrawColliderOverlay(UINT frameIndex);

    void OnResize(UINT width, UINT height);
    void UpdateWindowTitle(float deltaSeconds);
    std::filesystem::path GetShaderPath() const;
    void EncodePendingScreenshot(UINT64 fenceValue);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditorWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void ThrowIfFailed(HRESULT hr, const char* operation);
    static D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    static UINT AlignConstantBufferSize(UINT size);

private:
    HINSTANCE m_instance = nullptr;
    std::unique_ptr<DX12ForgeEditorUI> m_editorUI;
    HWND m_hwnd = nullptr;
    HWND m_splashHwnd = nullptr;
    std::wstring m_splashMessage = L"Initializing DirectX 12...";
    int m_splashProgress = 0;
    HWND m_editorHwnd = nullptr;
    HWND m_objectListHwnd = nullptr;
    std::array<HWND, 3> m_positionEdits{};
    std::array<HWND, 3> m_rotationEdits{};
    std::array<HWND, 3> m_scaleEdits{};
    HWND m_metallicEdit = nullptr;
    HWND m_roughnessEdit = nullptr;
    HBRUSH m_editorBackgroundBrush = nullptr;
    HBRUSH m_editorEditBrush = nullptr;
    HFONT m_editorFont = nullptr;
    UINT m_width = 1440;
    UINT m_height = 900;
    UINT m_pendingWidth = 1440;
    UINT m_pendingHeight = 900;
    bool m_resizePending = false;
    bool m_minimized = false;
    bool m_rightMouseDown = false;
    bool m_cursorHidden = false;
    int m_cursorHideCallCount = 0;
    bool m_clickPending = false;
    bool m_gizmoDragging = false;
    bool m_editorSyncing = false;
    int m_gizmoAxis = -1;
    TransformMode m_gizmoMode = TransformMode::Translate;
    float m_gizmoLength = 1.5f;
    float m_dragStartAxisParameter = 0.0f;
    float m_dragStartAngle = 0.0f;
    XMFLOAT3 m_dragStartPosition{};
    XMFLOAT3 m_dragStartRotation{};
    XMFLOAT3 m_dragStartScale{ 1.0f, 1.0f, 1.0f };
    int m_clickX = 0;
    int m_clickY = 0;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    float m_cameraYaw = -0.48f;
    float m_editorCameraFov = 60.0f;
    float m_cameraPitch = -0.22f;
    float m_cameraMoveSpeed = 7.0f;
    DirectX::XMFLOAT3 m_cameraPosition{ 5.0f, 4.0f, -10.0f };
    bool m_directionalLightEnabled = true;
    DirectX::XMFLOAT3 m_directionalLightDirection{ 0.42f, -1.0f, 0.28f };
    DirectX::XMFLOAT3 m_directionalLightColor{ 1.0f, 0.94f, 0.82f };
    float m_directionalLightIntensity = 4.0f;
    float m_ambientLightIntensity = 0.65f;
    bool m_dayNightCycleEnabled = false;
    std::string m_lastTextureError;
    std::filesystem::path m_textureSearchRoot;
    float m_timeOfDayHours = 14.0f;
    float m_dayNightCycleSpeed = 4.0f; // game hours per real minute
    bool m_timeOfDayDrivesSun = true;
    bool m_directionalObjectTransformDrivesSun = false;
    float m_skyCloudCoverage = 0.30f;
    float m_skyCloudAltitude = 0.20f;
    float m_skyWindSpeed = 1.0f;
    bool m_pointLightEnabled = true;
    DirectX::XMFLOAT3 m_pointLightPosition{ 2.0f, 3.0f, -2.0f };
    DirectX::XMFLOAT3 m_pointLightColor{ 0.20f, 0.55f, 1.0f };
    float m_pointLightRange = 12.0f;
    float m_pointLightIntensity = 35.0f;
    bool m_spotLightEnabled = true;
    DirectX::XMFLOAT3 m_spotLightPosition{ -3.0f, 6.0f, -3.0f };
    DirectX::XMFLOAT3 m_spotLightDirection{ 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT3 m_spotLightColor{ 1.0f, 0.36f, 0.14f };
    float m_spotLightRange = 16.0f;
    float m_spotLightIntensity = 45.0f;
    float m_spotLightInnerAngle = 18.0f;
    float m_spotLightOuterAngle = 30.0f;
    DirectX::XMFLOAT4X4 m_view{};
    DirectX::XMFLOAT4X4 m_projection{};
    DirectX::XMFLOAT4X4 m_viewProjection{};
    DirectX::XMFLOAT4X4 m_inverseViewProjection{};
    int m_selectedObject = -1;
    std::vector<GameObject> m_playSceneSnapshot; // full scene copy restored when Play mode stops
    bool m_hasPlaySceneSnapshot = false;
    int m_playSelectedObject = -1;
    int m_playViewportCameraObjectIndex = -1;
    bool m_standalonePlayer = false; // forge_game.ini next to the exe switches the engine into game mode
    bool m_editorOverlayEnabled = true; // grid, axes and gizmo; Ctrl+G toggles, always off in player mode
    bool m_transformCascadeActive = false; // guards against recursion while refreshing child transforms
    std::wstring m_gameTitle;
    std::wstring m_gameIconPath;
    std::wstring m_gameCreator;
    float m_sceneViewportX = 0.0f;
    float m_sceneViewportY = 0.0f;
    float m_sceneViewportWidth = 1440.0f;
    float m_sceneViewportHeight = 900.0f;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FrameCount> m_commandAllocators;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_nextFenceValue = 1;
    std::array<UINT64, FrameCount> m_frameFenceValues{};
    UINT m_frameIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    UINT m_rtvDescriptorSize = 0;
    UINT m_dsvDescriptorSize = 0;
    UINT m_srvDescriptorSize = 0;
    UINT m_nextTextureSrvIndex = 6; // 4..5 reserved for camera-preview textures
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureUploadResources;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> m_renderTargets;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> m_hdrTargets;
    // Post-processed copy of the current viewport used by the selected camera preview.
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> m_cameraPreviewTargets;
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, FrameCount> m_cameraPreviewSrvGpuHandles{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, FrameCount> m_sceneColorSrvGpuHandles{};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12_GPU_DESCRIPTOR_HANDLE m_depthSrvGpuHandle{};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowMap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_shadowDsvHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_shadowSrvGpuHandle{};
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pbrPipeline;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_shadowPipeline;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_skyGridPipeline;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_outlinePipeline;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_axisPipeline;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_gizmoPipeline;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_postProcessPipeline;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> m_frameConstants;
    std::array<std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount>, 3> m_axisConstants;
    std::array<std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount>, 3> m_gizmoConstants;
    std::array<Mesh, 3> m_gizmoMeshes;
    std::array<Mesh, 3> m_colliderWireMeshes; // 0=Box, 1=Sphere, 2=Capsule unit wireframes
    std::array<VertexRange, 3> m_gizmoMoveRanges{};
    std::array<VertexRange, 3> m_gizmoScaleRanges{};
    std::array<VertexRange, 3> m_gizmoRotateRanges{};
    Mesh m_cubeMesh;
    Mesh m_sphereMesh;
    std::array<Mesh, 4> m_primitiveMeshes; // plane, cylinder, cone, torus
    std::vector<std::unique_ptr<Mesh>> m_importedMeshes;
    Mesh m_axisMesh;
    std::vector<GameObject> m_objects;

    std::chrono::steady_clock::time_point m_startTime{};
    float m_titleTimer = 0.0f;
    bool m_initialized = false;
    bool m_comInitialized = false;

    // Post-processing values are editable from Create Object > Post Processing.
    XMFLOAT4 m_postProcessParameters{ 0.42f, 0.28f, 0.0f, 0.08f };
    XMFLOAT4 m_postProcessOptions{ 0.0f, 0.0f, 1.0f, 0.70f };
    XMFLOAT4 m_postProcessMotion{ 0.0f, 0.0f, 1.5f, 0.0f }; // x/y camera motion (computed per frame), z SSAO radius, w reserved
    XMFLOAT3 m_lastPostProcessCameraPosition{ 5.0f, 4.0f, -10.0f };
    bool m_havePostProcessCameraPosition = false;
    int m_viewportCameraObjectIndex = -1; // camera currently driving the real Scene viewport (-1 = free editor camera)

    // One-shot viewport capture requested from the editor (File > Save Screenshot).
    bool m_screenshotPending = false;
    std::filesystem::path m_screenshotPath;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_screenshotStaging;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_screenshotFootprint{};
    UINT m_screenshotRowBytes = 0;
    UINT m_screenshotWidth = 0;
    UINT m_screenshotHeight = 0;
    std::string m_lastScreenshotResult;
};
