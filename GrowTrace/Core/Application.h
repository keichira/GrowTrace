#pragma once
#include "../Editor/EditorContext.h"
#include "../Editor/PanelManager.h"
#include "Precompiled.h"
#include <GLFW/glfw3.h>
#include <concurrentqueue.h>
#include <enet/enet.h>
#include <signal.h>

class GTProxyPanel;
struct UICapturedPacket;

struct OutgoingPacket
{
    ENetPeer* targetPeer;
    uint8 channelID;
    std::vector<uint8> data;
    uint32 flags;
};

extern moodycamel::ConcurrentQueue<UICapturedPacket> gIncomingUIQueue;
extern moodycamel::ConcurrentQueue<OutgoingPacket> gOutgoingQueue;

void CaptureAndPushToUI(bool isC2S, ENetPacket* packet, bool isCustom = false, bool isBlocked = false);
void LogInfo(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
void LogError(const char* fmt, ...);
void LogDebug(const char* fmt, ...);

class Application
{
public:
    Application();
    ~Application();

public:
    static Application* GetInstance()
    {
        static Application instance;
        return &instance;
    }

public:
    bool Init(int32 width, int32 height, const char* title);
    void Run();
    void Shutdown();

    void ProcessNetworkEvents();

    bool IsRunning() { return m_stopFlag == 0; }
    GTProxyPanel* GetProxyPanel() { return m_pCachedProxyPanel; }

private:
    void BeginFrame();
    void EndFrame();
    void RenderRootWindow();
    void HandleBorderlessResize();

private:
    GLFWwindow* m_pWindow;
    EditorContext m_context;
    PanelManager m_panelManager;
    float m_lastTime;
    volatile sig_atomic_t m_stopFlag;

    GTProxyPanel* m_pCachedProxyPanel;
};

Application* GetApp();