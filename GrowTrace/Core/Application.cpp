#include "Application.h"
#include "../Event/EventManager.h"
#include "../Server/ENetServer.h"
#include "../Server/TCPServer.h"
#include "../UI/GTProxyPanel.h"
#include "../UI/UITheme.h"
#include "IO/Log.h"
#include "Math/Math.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

moodycamel::ConcurrentQueue<UICapturedPacket> gIncomingUIQueue;
moodycamel::ConcurrentQueue<OutgoingPacket> gOutgoingQueue;

static bool gIsDraggingTitleBar = false;
static double gDragStartX = 0.0;
static double gDragStartY = 0.0;
static bool gIsResizing = false;
static int gResizeEdge = 0;

void CaptureAndPushToUI(bool isC2S, ENetPacket* packet, bool isCustom, bool isBlocked)
{
    if (!packet || !packet->data || packet->dataLength < 4)
        return;

    UICapturedPacket pkt;
    pkt.isC2S = isC2S;
    pkt.isCustom = isCustom;
    pkt.isBlocked = isBlocked;
    pkt.rawData.assign(packet->data, packet->data + packet->dataLength);

    pkt.msgType = (eMessagePacketType)(*(uint32*)(packet->data));
    if (pkt.msgType == NET_MESSAGE_GAME_PACKET && packet->dataLength >= 5)
    {
        pkt.gameType = (eGamePacketType)(*(packet->data + 4));
    }

    gIncomingUIQueue.enqueue(std::move(pkt));
}

static void DispatchFormattedLog(const ImVec4& color, const char* fmt, va_list args)
{
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    GTProxyPanel* pPanel = GetApp()->GetProxyPanel();
    if (pPanel)
    {
        pPanel->AddLog(buffer, color);
    }

    printf("%s\n", buffer);
}

void LogInfo(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DispatchFormattedLog(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), fmt, args);
    va_end(args);
}

void LogWarn(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DispatchFormattedLog(ImVec4(1.00f, 0.75f, 0.20f, 1.00f), fmt, args);
    va_end(args);
}

void LogError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DispatchFormattedLog(ImVec4(1.00f, 0.35f, 0.35f, 1.00f), fmt, args);
    va_end(args);
}

void LogDebug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DispatchFormattedLog(ImVec4(0.75f, 0.45f, 0.90f, 1.00f), fmt, args);
    va_end(args);
}

Application::Application() : m_pWindow(nullptr), m_lastTime(0.0f), m_stopFlag(0), m_pCachedProxyPanel(nullptr) {}
Application::~Application() {}

bool Application::Init(int32 width, int32 height, const char* title)
{
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    m_pWindow = glfwCreateWindow(width, height, title, NULL, NULL);

    if (!m_pWindow)
    {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_pWindow);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ProxyUI::SetupDarkTheme();

    ImGui_ImplGlfw_InitForOpenGL(m_pWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig mainFontCfg;
    mainFontCfg.FontDataOwnedByAtlas = false;

    ImFont* pMainFont =
        io.Fonts->AddFontFromMemoryTTF((void*)gDroidSansFont, sizeof(gDroidSansFont), 15.0f, &mainFontCfg);

    ImFontConfig iconFontCfg;
    iconFontCfg.MergeMode = true;
    iconFontCfg.PixelSnapH = true;
    iconFontCfg.FontDataOwnedByAtlas = false;

    static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    io.Fonts->AddFontFromMemoryTTF((void*)gFontAwesomeIconFont, sizeof(gFontAwesomeIconFont), 14.0f, &iconFontCfg,
                                   iconRanges);

    io.Fonts->Build();

    GetEventManager()->RegisterEvents();

    m_panelManager.RegisterPanel(std::make_unique<GTProxyPanel>());
    m_pCachedProxyPanel = m_panelManager.GetPanel<GTProxyPanel>();

    m_stopFlag = 0;
    return true;
}

static void ForwardENetPacket(ENetPeer* pTargetPeer, const ENetEvent& event)
{
    if (!pTargetPeer || !event.packet || pTargetPeer->state != ENET_PEER_STATE_CONNECTED)
        return;

    ENetPacket* pPacket = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags);
    if (pPacket)
    {
        enet_peer_send(pTargetPeer, event.channelID, pPacket);
    }
}

void Application::ProcessNetworkEvents()
{
    ENetEvent clientEvent;
    while (gENetClient.PopEvent(clientEvent))
    {
        switch (clientEvent.type)
        {
            case ENET_EVENT_TYPE_RECEIVE:
            {
                bool allowForward = GetEventManager()->ProcessIncomingPacket(clientEvent.peer, clientEvent.packet);
                CaptureAndPushToUI(false, clientEvent.packet, false, !allowForward);

                if (allowForward && gENetServer.GetPeer() && gENetServer.GetPeer()->state == ENET_PEER_STATE_CONNECTED)
                {
                    ForwardENetPacket(gENetServer.GetPeer(), clientEvent);
                }

                enet_packet_destroy(clientEvent.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                LogWarn("[Client] Target server closed connection.");
                gENetClient.SetPeer(nullptr);

                if (!gENetServer.IsReconnecting())
                {
                    if (gENetServer.GetPeer())
                    {
                        LogWarn("[Server] Closing peer connection, disconnecting local client");
                        gENetServer.DisconnectPeer(gENetServer.GetPeer());
                        gENetServer.SetPeer(nullptr);
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    ENetEvent serverEvent;
    while (gENetServer.PopEvent(serverEvent))
    {
        switch (serverEvent.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                LogInfo("[Server] Client connected! ConnectionID: %u", serverEvent.peer->connectID);
                gENetServer.SetPeer(serverEvent.peer);

                enet_peer_timeout(serverEvent.peer, 0, ENET_PEER_TIMEOUT_MAXIMUM / 2, 0);

                if (gENetServer.IsReconnecting())
                {
                    const string& targetHost = gENetServer.GetReconnectHost();
                    uint16 targetPort = gENetServer.GetReconnectPort();

                    LogInfo("[Server] Client reconnected to proxy! Routing to sub-server: %s:%d", targetHost.c_str(),
                            targetPort);

                    if (gENetClient.GetPeer())
                    {
                        gENetClient.DisconnectPeer(gENetClient.GetPeer());
                        gENetClient.SetPeer(nullptr);
                    }

                    if (!gENetClient.Connect(targetHost, targetPort, 5000))
                    {
                        LogError("[Client] Failed to connect to sub-server %s:%d", targetHost.c_str(), targetPort);
                        gENetServer.DisconnectPeer(gENetServer.GetPeer());
                        gENetServer.SetPeer(nullptr);
                    }

                    gENetServer.SetReconnecting(false);
                }
                else
                {
                    const string& targetHost = gTCPServer.GetLastAddress();
                    uint16 targetPort = gTCPServer.GetLastPort();

                    if (targetHost.empty() || targetPort == 0)
                    {
                        LogError("[Server] Target address is invalid, disconnecting");
                        gENetServer.DisconnectPeer(gENetServer.GetPeer());
                        gENetServer.SetPeer(nullptr);
                        break;
                    }

                    if (!gENetClient.Connect(targetHost, targetPort, 5000))
                    {
                        LogError("[Client] Failed to connect to target address");
                        gENetServer.DisconnectPeer(gENetServer.GetPeer());
                        gENetServer.SetPeer(nullptr);
                    }
                }
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE:
            {
                CaptureAndPushToUI(true, serverEvent.packet, false, false);

                if (gENetClient.GetPeer() && gENetClient.GetPeer()->state == ENET_PEER_STATE_CONNECTED)
                {
                    ForwardENetPacket(gENetClient.GetPeer(), serverEvent);
                }
                else
                {
                    LogWarn("[Client -> Server] Target peer is disconnected, packet dropped.");
                }

                enet_packet_destroy(serverEvent.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                LogWarn("[Server] Peer disconnected (ID: %u)", serverEvent.peer->connectID);

                if (serverEvent.peer == gENetServer.GetPeer())
                {
                    gENetServer.SetPeer(nullptr);

                    if (!gENetServer.IsReconnecting() && gENetClient.GetPeer())
                    {
                        LogWarn("[Client] Closing connection to target server.");
                        gENetClient.DisconnectPeer(gENetClient.GetPeer());
                        gENetClient.SetPeer(nullptr);
                    }
                }
                else
                {
                    LogWarn("[Server] Ignored disconnect event from old peer.");
                }
                break;
            }

            default:
                break;
        }
    }
}

void Application::Run()
{
    while (m_stopFlag == 0 && !glfwWindowShouldClose(m_pWindow))
    {
        if (glfwGetWindowAttrib(m_pWindow, GLFW_ICONIFIED))
        {
            SleepMS(30);
            glfwPollEvents();
            continue;
        }

        glfwPollEvents();

        float time = (float)glfwGetTime();
        float dt = time - m_lastTime;
        m_lastTime = time;

        m_panelManager.UpdateAll(m_context, dt);

        BeginFrame();
        RenderRootWindow();
        EndFrame();
    }
}

void Application::BeginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::RenderRootWindow()
{
    HandleBorderlessResize();

    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(pViewport->WorkPos);
    ImGui::SetNextWindowSize(pViewport->WorkSize);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                   ImGuiWindowFlags_NoScrollbar;

    float windowRounding = 10.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.27f, 0.35f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("MainWindow", nullptr, windowFlags);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    ImGuiStyle& style = ImGui::GetStyle();

    float titleBarHeight = ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f;
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("CustomTitleBar", ImVec2(0.0f, titleBarHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 windowPos = ImGui::GetWindowPos();
    float windowWidth = ImGui::GetWindowWidth();

    ImGui::GetWindowDrawList()->AddRectFilled(windowPos,
                                              ImVec2(windowPos.x + windowWidth, windowPos.y + titleBarHeight),
                                              IM_COL32(18, 20, 26, 255), windowRounding, ImDrawFlags_RoundCornersTop);

    float textY = (titleBarHeight - ImGui::GetFontSize()) * 0.5f;

    ImGui::SetCursorPos(ImVec2(style.WindowPadding.x + 4.0f, textY));
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_PLAY);

    const char* titleText = "GrowTrace";
    float textWidth = ImGui::CalcTextSize(titleText).x;
    float centerX = (windowWidth - textWidth) * 0.5f;

    if (centerX > 70.0f)
    {
        ImGui::SetCursorPos(ImVec2(centerX, textY));
        ImGui::TextUnformatted(titleText);
    }

    float btnSize = ImGui::GetFrameHeight() * 0.85f;
    float btnY = (titleBarHeight - btnSize) * 0.5f;
    float rightMargin = style.WindowPadding.x + 4.0f;
    float btnSpacing = style.ItemSpacing.x;
    float startX = windowWidth - (btnSize * 2.0f + btnSpacing + rightMargin);

    ImGui::SetCursorPos(ImVec2(startX, btnY));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, btnSize * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.27f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.34f, 0.42f, 1.00f));

    if (ImGui::Button(ICON_FA_MINUS, ImVec2(btnSize, btnSize)))
    {
        glfwIconifyWindow(m_pWindow);
    }

    ImGui::SameLine(0, btnSpacing);

    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.00f));
    if (ImGui::Button(ICON_FA_XMARK, ImVec2(btnSize, btnSize)))
    {
        glfwSetWindowShouldClose(m_pWindow, GLFW_TRUE);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            gIsDraggingTitleBar = true;
            glfwGetCursorPos(m_pWindow, &gDragStartX, &gDragStartY);
        }
    }

    if (gIsDraggingTitleBar)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            double currentMouseX, currentMouseY;
            glfwGetCursorPos(m_pWindow, &currentMouseX, &currentMouseY);
            int windowX, windowY;
            glfwGetWindowPos(m_pWindow, &windowX, &windowY);

            glfwSetWindowPos(m_pWindow, windowX + (int)(currentMouseX - gDragStartX),
                             windowY + (int)(currentMouseY - gDragStartY));
        }
        else
        {
            gIsDraggingTitleBar = false;
        }
    }

    ImGui::EndChild();

    float padding = style.WindowPadding.x > 0 ? style.WindowPadding.x : 4.0f;
    ImGui::SetCursorPos(ImVec2(padding, titleBarHeight + padding));

    ImGui::BeginChild("MainContentArea",
                      ImVec2(ImGui::GetContentRegionAvail().x - padding, ImGui::GetContentRegionAvail().y - padding),
                      false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (m_pCachedProxyPanel)
    {
        m_pCachedProxyPanel->OnImGuiRender(m_context);
    }

    ImGui::EndChild();
    ImGui::End();
}

void Application::HandleBorderlessResize()
{
    if (gIsDraggingTitleBar)
        return;

    double mouseX, mouseY;
    glfwGetCursorPos(m_pWindow, &mouseX, &mouseY);

    int winWidth, winHeight, winX, winY;
    glfwGetWindowSize(m_pWindow, &winWidth, &winHeight);
    glfwGetWindowPos(m_pWindow, &winX, &winY);

    int32 borderThickness = 8;

    bool left = mouseX >= 0 && mouseX <= borderThickness;
    bool right = mouseX >= (winWidth - borderThickness) && mouseX <= winWidth;
    bool top = mouseY >= 0 && mouseY <= borderThickness;
    bool bottom = mouseY >= (winHeight - borderThickness) && mouseY <= winHeight;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (right && bottom)
            gResizeEdge = 3;
        else if (right)
            gResizeEdge = 1;
        else if (bottom)
            gResizeEdge = 2;
        else if (left)
            gResizeEdge = 4;
        else if (top)
            gResizeEdge = 5;

        if (gResizeEdge != 0)
            gIsResizing = true;
    }

    if (gIsResizing)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            int newWidth = winWidth;
            int newHeight = winHeight;

            if (gResizeEdge == 1 || gResizeEdge == 3)
                newWidth = Max(350, (int)(mouseX));
            if (gResizeEdge == 2 || gResizeEdge == 3)
                newHeight = Max(250, (int)(mouseY));

            glfwSetWindowSize(m_pWindow, newWidth, newHeight);
        }
        else
        {
            gIsResizing = false;
            gResizeEdge = 0;
        }
    }
}

void Application::EndFrame()
{
    ImGui::Render();

    int32 width, height;
    glfwGetFramebufferSize(m_pWindow, &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backupCurrentContext);
    }

    glfwSwapBuffers(m_pWindow);
}

void Application::Shutdown()
{
    m_stopFlag = 1;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}

Application* GetApp()
{
    return Application::GetInstance();
}