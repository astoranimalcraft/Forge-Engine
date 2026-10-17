#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

struct ForgeScriptVec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct ForgeScriptAttachment { std::string className; bool enabled = true; };
struct ForgeScriptObjectState
{
    int id = -1;
    std::string name;
    ForgeScriptVec3 position{}, rotationDegrees{}, scale{1.0f,1.0f,1.0f};
    bool hasMeshRenderer = true, meshRendererEnabled = true;
    bool hasCollider = false, hasAudioSource = false, hasCamera = false;
    int lightType = 0;
    bool lightEnabled = true;
    ForgeScriptVec3 lightColor{1.0f,1.0f,1.0f};
    float lightIntensity = 1.0f, lightRange = 10.0f, lightInnerAngle = 18.0f, lightOuterAngle = 30.0f;
    std::string tag = "Untagged";
    int layer = 0;
    int colliderType = 0; // 0=Box, 1=Sphere, 2=Capsule
    ForgeScriptVec3 colliderSize{1.0f,1.0f,1.0f};
    bool isTrigger = false;
    bool hasRigidbody = false;
    float mass = 1.0f;
    bool useGravity = true;
    bool isKinematic = false;
    int parentId = -1; // stable id of the parent object, -1 = root
    bool activeSelf = true;
    float cameraFieldOfView = 60.0f, cameraNearClip = 0.05f, cameraFarClip = 1000.0f;
    std::vector<ForgeScriptAttachment> scripts;
};
struct ForgeScriptCommand
{
    int kind = 0;      // 0 = Instantiate, 1 = Destroy
    int tempId = -1;   // id the scripts already hold for the new clone (always < -1)
    int sourceId = -1; // stable id of the object to clone (may be the temp id of a clone made this frame)
    int targetId = -1; // stable id of the object to destroy (may be a temp id)
    std::string name;
    ForgeScriptVec3 position{}, rotationDegrees{}, scale{1.0f,1.0f,1.0f};
};
struct ForgeScriptInputState
{
    float mouseX = 0.0f, mouseY = 0.0f; // Mouse-axis delta
    float mousePositionX = 0.0f, mousePositionY = 0.0f; // Viewport-local position
    int mouseButtons = 0;
    float mouseScroll = 0.0f; // wheel notches this frame, positive = away from the user
    int screenWidth = 0, screenHeight = 0; // game viewport size in pixels
    std::vector<int> keysDown;
};

/// Runs managed scripts in a separate .NET process so script failures do not corrupt the native renderer.
class ForgeScriptRuntime final
{
public:
    ForgeScriptRuntime() = default;
    ~ForgeScriptRuntime();
    ForgeScriptRuntime(const ForgeScriptRuntime&) = delete;
    ForgeScriptRuntime& operator=(const ForgeScriptRuntime&) = delete;
    bool Build(const std::filesystem::path& sourceRoot, const std::filesystem::path& projectRoot,
        std::vector<std::string>& logs);
    bool Start(const std::filesystem::path& sourceRoot, const std::filesystem::path& projectRoot,
        std::vector<std::string>& logs);
    void Stop(std::vector<std::string>* logs = nullptr);
    bool Tick(float deltaSeconds, const ForgeScriptInputState& input,
        std::vector<ForgeScriptObjectState>& objects, std::vector<std::string>& logs);
    const std::vector<ForgeScriptCommand>& PendingCommands() const noexcept { return m_pendingCommands; }
    void AddIdMapping(int tempId, int realId) { m_idMappings.emplace_back(tempId, realId); }
    bool IsRunning() const noexcept
    {
        return m_process.hProcess != nullptr && WaitForSingleObject(m_process.hProcess, 0) == WAIT_TIMEOUT;
    }
private:
    bool RunDotnetBuild(const std::filesystem::path& dotnetPath, const std::filesystem::path& projectFile,
        const std::filesystem::path& workingDirectory, const std::filesystem::path& logFile,
        std::vector<std::string>& logs);
    bool SendText(const std::string& text);
    bool ReadProtocolLine(std::string& line, DWORD timeoutMilliseconds);
    void AppendHostLog(const std::string& line, std::vector<std::string>& logs);
    void CloseHandles();
    PROCESS_INFORMATION m_process{};
    HANDLE m_childInputWrite = nullptr;
    HANDLE m_childOutputRead = nullptr;
    std::string m_readBuffer;
    std::vector<ForgeScriptCommand> m_pendingCommands;
    std::vector<std::pair<int, int>> m_idMappings;
};
