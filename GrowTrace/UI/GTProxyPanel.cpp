#include "GTProxyPanel.h"
#include "../Core/Application.h"
#include "../Server/ENetServer.h"
#include "../Server/TCPServer.h"
#include "Math/Math.h"
#include "UITheme.h"
#include "Utils/StringUtils.h"

static const char gHexByteTable[256][3] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F", "10", "11", "12",
    "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F", "20", "21", "22", "23", "24", "25",
    "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F", "30", "31", "32", "33", "34", "35", "36", "37", "38",
    "39", "3A", "3B", "3C", "3D", "3E", "3F", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B",
    "4C", "4D", "4E", "4F", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E",
    "5F", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F", "70", "71",
    "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F", "80", "81", "82", "83", "84",
    "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F", "90", "91", "92", "93", "94", "95", "96", "97",
    "98", "99", "9A", "9B", "9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA",
    "AB", "AC", "AD", "AE", "AF", "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD",
    "BE", "BF", "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF", "D0",
    "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF", "E0", "E1", "E2", "E3",
    "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF", "F0", "F1", "F2", "F3", "F4", "F5", "F6",
    "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF"};

static ImVec4 GetRainbowColor(float speed = 0.2f, float saturation = 0.85f, float value = 1.0f)
{
    float hue = fmodf((float)ImGui::GetTime() * speed, 1.0f);
    ImVec4 col;
    ImGui::ColorConvertHSVtoRGB(hue, saturation, value, col.x, col.y, col.z);
    col.w = 1.0f;
    return col;
}

GTProxyPanel::GTProxyPanel()
    : Panel("GTopia Helper Toolkit"), m_currentTab(UI_TAB_DASHBOARD), m_directionFilter(UI_PACKET_FILTER_DIR_ALL),
      m_detailViewMode(UI_PACKET_VIEW_RAWHEX), m_packetCounter(0), m_isPaused(false), m_autoScrollPackets(false),
      m_autoScrollLogs(true), m_selectedPacketId(0), m_selectedPacketIndex(0), m_filterDirty(true),
      m_poppedFilteredCount(0), m_selectedItemId(0), m_selectedItemType(0), m_itemFilterDirty(true), m_lastItemCount(0),
      m_playerFilterDirty(true), m_worldObjFilterDirty(true)
{
    LogInfo("GrowTrace Panel initialized");
}

eGamePacketType GTProxyPanel::ParseGameTypeFromBuffer(const uint8* pData, usize length, eGamePacketType fallback)
{
    if (pData && length >= 4 + sizeof(GameUpdatePacket))
    {
        return (eGamePacketType)(((GameUpdatePacket*)(pData + 4))->type);
    }
    else if (pData && length >= sizeof(GameUpdatePacket))
    {
        (eGamePacketType)(((GameUpdatePacket*)(pData))->type);
    }
    return fallback;
}

bool GTProxyPanel::ShouldCapturePacket(bool isC2S, eMessagePacketType msgType, eGamePacketType gameType) const
{
    if (isC2S && !m_captureFilter.c2s)
        return false;
    if (!isC2S && !m_captureFilter.s2c)
        return false;

    if (!m_captureFilter.IsMessageAllowed(msgType))
        return false;

    if (msgType == NET_MESSAGE_GAME_PACKET)
    {
        if (!m_captureFilter.IsGameTypeAllowed(gameType))
            return false;
    }

    return true;
}

void GTProxyPanel::OnUpdate(EditorContext& ctx, float dt)
{
    UICapturedPacket capturedPacket;
    bool newPacketsAdded = false;

    while (gIncomingUIQueue.try_dequeue(capturedPacket))
    {
        if (m_isPaused)
            continue;

        if (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET && !capturedPacket.rawData.empty())
        {
            capturedPacket.gameType = ParseGameTypeFromBuffer(capturedPacket.rawData.data(),
                                                              capturedPacket.rawData.size(), capturedPacket.gameType);
        }

        if (!ShouldCapturePacket(capturedPacket.isC2S, capturedPacket.msgType, capturedPacket.gameType))
        {
            continue;
        }

        capturedPacket.id = ++m_packetCounter;
        capturedPacket.timestamp = GetTimeAsStr();
        m_packets.push_back(std::move(capturedPacket));

        if (m_packets.size() > 10000)
        {
            auto& frontCapturedPacket = m_packets.front();
            if (PacketMatchesFilter(frontCapturedPacket))
            {
                m_poppedFilteredCount++;
            }
            m_cachedFunctionNames.erase(frontCapturedPacket.id);
            m_packets.pop_front();
        }

        newPacketsAdded = true;
    }

    if (newPacketsAdded)
    {
        m_filterDirty = true;
    }
}

void GTProxyPanel::OnImGuiRender(EditorContext& ctx)
{
    RenderTopStatus();
    RenderTabBar();

    float totalAvailHeight = ImGui::GetContentRegionAvail().y;
    float logPanelHeight = Max(120.0f, totalAvailHeight * 0.22f);
    float contentHeight = totalAvailHeight - logPanelHeight - ImGui::GetStyle().ItemSpacing.y;

    if (contentHeight < 100.0f)
        contentHeight = 100.0f;

    ImGui::BeginChild("MainContentArea", ImVec2(0, contentHeight), false);
    {
        switch (m_currentTab)
        {
            case UI_TAB_DASHBOARD:
                RenderDashboardTab();
                break;
            case UI_TAB_PACKETSNIFFER:
                RenderPacketSnifferTab();
                break;
            case UI_TAB_WORLD:
                RenderWorldTab();
                break;
            case UI_TAB_ITEMDATA:
                RenderItemDataTab();
                break;
            case UI_TAB_PLAYER:
                RenderPlayerTab();
                break;
        }
    }
    ImGui::EndChild();

    RenderBottomLogPanel(logPanelHeight);
}

void GTProxyPanel::PushPacket(bool isC2S, eMessagePacketType msgType, const uint8* pData, usize length,
                              eGamePacketType gameType)
{
    if (m_isPaused)
        return;

    if (msgType == NET_MESSAGE_GAME_PACKET)
    {
        gameType = ParseGameTypeFromBuffer(pData, length, gameType);
    }

    if (!ShouldCapturePacket(isC2S, msgType, gameType))
        return;

    UICapturedPacket capturedPacket;
    capturedPacket.timestamp = GetTimeAsStr();
    capturedPacket.isC2S = isC2S;
    capturedPacket.msgType = msgType;
    capturedPacket.gameType = gameType;

    if (pData && length > 0)
    {
        capturedPacket.rawData.assign(pData, pData + length);
    }

    gIncomingUIQueue.enqueue(std::move(capturedPacket));
}

void GTProxyPanel::AddLog(const string& msg, const ImVec4& color)
{
    string timeStr = GetTimeAsStr();

    if (msg.empty())
    {
        m_logs.push_back({timeStr, timeStr + " ", color});
        if (m_logs.size() > 1000)
            m_logs.pop_front();
        return;
    }

    usize start = 0;
    while (start <= msg.size())
    {
        usize pos = msg.find('\n', start);
        string line = msg.substr(start, pos == string::npos ? string::npos : pos - start);

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        m_logs.push_back({timeStr, timeStr + " " + line, color});
        if (m_logs.size() > 1000)
            m_logs.pop_front();

        if (pos == string::npos)
            break;
        start = pos + 1;
    }
}

void GTProxyPanel::RenderTopStatus()
{
    float itemSpacingX = ImGui::GetStyle().ItemSpacing.x * 2.5f;

    bool httpReady = gTCPServer.ISHTTPReady();
    ImGui::TextColored(httpReady ? ImVec4(0.10f, 0.88f, 0.45f, 1.00f) : ImVec4(1.00f, 0.30f, 0.30f, 1.00f),
                       ICON_FA_CIRCLE);
    ImGui::SameLine();
    if (httpReady)
    {
        uint64 lastHeartbeatSec = gTCPServer.GetHTTPLastHeartbeatElapsedMS() / 1000;
        ImGui::Text("HTTP Server (%ds ago)", lastHeartbeatSec);
    }
    else
    {
        ImGui::Text("HTTP Server");
    }

    ImGui::SameLine(0.0f, itemSpacingX);
    if (httpReady)
    {
        if (gTCPServer.IsTogglePending())
        {
            ImGui::TextColored(ImVec4(0.90f, 0.65f, 0.20f, 1.00f), ICON_FA_SPINNER);
        }
        else if (gTCPServer.IsHTTPServerActive())
        {
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), ICON_FA_NETWORK_WIRED);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.90f, 0.60f, 0.20f, 1.00f), ICON_FA_PLUG_CIRCLE_XMARK);
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.90f, 0.60f, 0.20f, 1.00f), ICON_FA_PLUG_CIRCLE_XMARK);
    }

    ImGui::SameLine();
    ImGui::Text(" HTTP Proxy");

    ImGui::SameLine(0.0f, itemSpacingX);
    ImGui::TextColored(gENetServer.GetPeer() ? ImVec4(0.10f, 0.88f, 0.45f, 1.00f) : ImVec4(1.00f, 0.30f, 0.30f, 1.00f),
                       ICON_FA_CIRCLE);
    ImGui::SameLine();
    ImGui::Text("Server Connection");

    ImGui::SameLine(0.0f, itemSpacingX);
    ImGui::TextColored(gENetClient.GetPeer() ? ImVec4(0.10f, 0.88f, 0.45f, 1.00f) : ImVec4(1.00f, 0.30f, 0.30f, 1.00f),
                       ICON_FA_CIRCLE);
    ImGui::SameLine();
    ImGui::Text("Client Connection");

    ImGui::SameLine(0.0f, itemSpacingX);
    ImGui::TextDisabled("Packets Captured: %u", m_packetCounter);

    ImGui::SameLine(0.0f, itemSpacingX);
    ImVec4 rainbowCol = GetRainbowColor(0.2f, 0.85f, 1.0f);
    ImGui::TextColored(rainbowCol, "Made by keichira for GTopia - github.com/keichira");
    ImGui::Spacing();
}

void GTProxyPanel::RenderTabBar()
{
    ImGuiStyle& style = ImGui::GetStyle();
    float availWidth = ImGui::GetContentRegionAvail().x;
    float totalSpacing = style.ItemSpacing.x * (float)(UI_TAB_COUNT - 1);
    float baseButtonWidth = Round((availWidth - totalSpacing) / (float)(UI_TAB_COUNT));
    float buttonHeight = ImGui::GetFrameHeight();

    auto DrawTab = [this, baseButtonWidth, availWidth, buttonHeight](const char* label, eUITab tab, bool isLast)
    {
        bool isActive = (m_currentTab == tab);
        ImGui::PushStyleColor(ImGuiCol_Button,
                              isActive ? ImVec4(0.12f, 0.38f, 0.68f, 1.00f) : ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
        if (isActive)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        float currentButtonWidth = isLast ? ImGui::GetContentRegionAvail().x : baseButtonWidth;

        if (ImGui::Button(label, ImVec2(currentButtonWidth, buttonHeight)))
        {
            m_currentTab = tab;
        }

        if (isActive)
            ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        if (!isLast)
            ImGui::SameLine();
    };

    DrawTab(ICON_FA_GAUGE " Dashboard", UI_TAB_DASHBOARD, false);
    DrawTab(ICON_FA_BUG " Sniffer", UI_TAB_PACKETSNIFFER, false);
    DrawTab(ICON_FA_PERSON " Player", UI_TAB_PLAYER, false);
    DrawTab(ICON_FA_EARTH_AMERICAS " World", UI_TAB_WORLD, false);
    DrawTab(ICON_FA_BOXES_PACKING " Item Data", UI_TAB_ITEMDATA, true);

    ImGui::NewLine();
    ImGui::Spacing();
}

void GTProxyPanel::RenderBottomLogPanel(float height)
{
    ImGui::BeginChild("BottomLogArea", ImVec2(0.0f, height), true);
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_TERMINAL " Console Logs");

    float checkboxWidth = ImGui::GetFrameHeight() + ImGui::CalcTextSize("Auto-scroll").x + style.ItemInnerSpacing.x;
    float clearBtnWidth = ImGui::CalcTextSize(ICON_FA_TRASH " Clear").x + style.FramePadding.x * 2.0f;
    float rightControlsWidth = checkboxWidth + style.ItemSpacing.x + clearBtnWidth;
    float rightPosX = ImGui::GetContentRegionAvail().x - rightControlsWidth;

    if (rightPosX > ImGui::CalcTextSize(ICON_FA_TERMINAL " Console Logs").x + style.ItemSpacing.x * 2.0f)
        ImGui::SameLine(rightPosX);
    else
        ImGui::SameLine();

    ImGui::Checkbox("Auto-scroll", &m_autoScrollLogs);
    ImGui::SameLine(0.0f, style.ItemSpacing.x);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.18f, 0.18f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.12f, 0.12f, 1.00f));

    if (ImGui::Button(ICON_FA_TRASH " Clear"))
        m_logs.clear();

    ImGui::PopStyleColor(3);
    ImGui::Separator();

    ImGui::BeginChild("LogScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper logClipper;
    logClipper.Begin(m_logs.size());

    while (logClipper.Step())
    {
        for (int32 i = logClipper.DisplayStart; i < logClipper.DisplayEnd; ++i)
        {
            if (i < 0 || i >= m_logs.size())
                continue;

            ImGui::TextColored(m_logs[i].color, "%s", m_logs[i].message.c_str());
        }
    }

    if (m_autoScrollLogs && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::EndChild();
}

void GTProxyPanel::RenderDashboardTab()
{
    ImGui::BeginChild("DashboardView", ImVec2(0, 0), true);

    ImGuiStyle& style = ImGui::GetStyle();
    float availWidth = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.17f, 0.22f, 1.00f));
    ImGui::BeginChild("DashboardHeader", ImVec2(0, 70.0f), true);
    {
        ImGui::SetCursorPosY(8.0f);
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_GAUGE " GrowTrace - GTopia Helper Toolkit");
        ImGui::TextDisabled("Helper Toolkit for GTopia Private Server Users");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180.0f);
        ImGui::SetCursorPosY(18.0f);
        if (m_isPaused)
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), ICON_FA_PAUSE " STATUS: PAUSED");
        else
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), ICON_FA_PLAY " STATUS: MONITORING");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    float colWidth = (availWidth - style.ItemSpacing.x * 2.0f) / 3.0f;

    ImGui::BeginChild("CardNetworkStatus", ImVec2(colWidth, 140.0f), true);
    {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_NETWORK_WIRED " Network Status");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("HTTP Server:");
        ImGui::SameLine(130.0f);
        if (gTCPServer.IsHTTPConnected())
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "ONLINE");
        else
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "OFFLINE");

        ImGui::Text("Real Client:");
        ImGui::SameLine(130.0f);
        if (gENetServer.GetPeer())
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "CONNECTED");
        else
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "DISCONNECTED");

        ImGui::Text("Real Server:");
        ImGui::SameLine(130.0f);
        if (gENetClient.GetPeer())
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "CONNECTED");
        else
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "DISCONNECTED");
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CardTrafficMetrics", ImVec2(colWidth, 140.0f), true);
    {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.00f), ICON_FA_BUG " Traffic Overview");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Total Packets:");
        ImGui::SameLine(140.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%u", m_packetCounter);

        ImGui::Text("In Memory:");
        ImGui::SameLine(140.0f);
        ImGui::Text("%zu / 10000", m_packets.size());

        ImGui::Text("Active Logs:");
        ImGui::SameLine(140.0f);
        ImGui::Text("%zu entries", m_logs.size());
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CardQuickActions", ImVec2(colWidth, 140.0f), true);
    {
        ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), ICON_FA_SLIDERS " Quick Controls");
        ImGui::Separator();
        ImGui::Spacing();

        float btnWidth = ImGui::GetContentRegionAvail().x;

        bool isPending = gTCPServer.IsTogglePending();
        bool isActive = gTCPServer.IsHTTPServerActive();
        bool hasError = gTCPServer.HasToggleError();

        if (isPending)
        {
            ImGui::BeginDisabled(true);

            const char* spinnerIcons[] = {ICON_FA_SPINNER, ICON_FA_CIRCLE_NOTCH};
            int32 iconIdx = (int32)(ImGui::GetTime() * 8.0f) % 2;

            string pendingText = string(spinnerIcons[iconIdx]) + "  Processing Request...";
            ImGui::Button(pendingText.c_str(), ImVec2(btnWidth, 26.0f));

            ImGui::EndDisabled();
        }
        else if (isActive)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.22f, 0.22f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.15f, 0.15f, 1.00f));

            if (ImGui::Button(ICON_FA_POWER_OFF "  Stop HTTP Proxy (Free Ports 80/443)", ImVec2(btnWidth, 26.0f)))
            {
                gTCPServer.SendToggleHTTPData(0);
            }
            ImGui::PopStyleColor(3);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.28f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.75f, 0.35f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.55f, 0.20f, 1.00f));

            if (ImGui::Button(ICON_FA_PLAY "  Start HTTP Proxy (Bind Ports 80/443)", ImVec2(btnWidth, 26.0f)))
            {
                gTCPServer.SendToggleHTTPData(1);
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();

        if (gProxyData.proxySettings.localServerMode)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.22f, 0.22f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
            if (ImGui::Button(ICON_FA_POWER_OFF "  Disable Local Server Mode", ImVec2(btnWidth, 26.0f)))
            {
                gProxyData.proxySettings.localServerMode = false;
            }
            ImGui::PopStyleColor(3);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.28f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.75f, 0.35f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.55f, 0.20f, 1.00f));

            if (ImGui::Button(ICON_FA_PLAY "  Enable Local Server Mode", ImVec2(btnWidth, 26.0f)))
            {
                gProxyData.proxySettings.localServerMode = true;
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();

        if (m_isPaused)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.50f, 0.25f, 1.00f));
            if (ImGui::Button(ICON_FA_PLAY " Resume Capture", ImVec2(btnWidth, 26.0f)))
                m_isPaused = false;
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.20f, 0.20f, 1.00f));
            if (ImGui::Button(ICON_FA_PAUSE " Pause Capture", ImVec2(btnWidth, 26.0f)))
                m_isPaused = true;
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        if (ImGui::Button(ICON_FA_TRASH " Clear Captured Packets", ImVec2(btnWidth, 26.0f)))
        {
            m_packets.clear();
            m_filteredIndices.clear();
            m_cachedFunctionNames.clear();
            m_selectedPacketId = 0;
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float colWidth2 = (availWidth - style.ItemSpacing.x) * 0.5f;

    ImGui::BeginChild("SectionCommunity", ImVec2(colWidth2, 250.0f), true);
    {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_EARTH_AMERICAS " GTopia Links & Community");
        ImGui::Separator();
        ImGui::Spacing();

        float bigButtonWidth = ImGui::GetContentRegionAvail().x;
        float bigButtonHeight = 42.0f;

        ImVec4 rainbowCol = GetRainbowColor(0.2f, 0.85f, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.5f);
        ImGui::PushStyleColor(ImGuiCol_Border, rainbowCol);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.33f, 0.80f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.41f, 0.90f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.26f, 0.70f, 1.00f));

        if (ImGui::Button(ICON_FA_USERS "  Join GTopia Discord Community", ImVec2(bigButtonWidth, bigButtonHeight)))
        {
            LaunchBrowserURL("https://discord.gg/5XjTQm3kRh");
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.30f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.65f, 0.38f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.38f, 0.22f, 1.00f));
        if (ImGui::Button(ICON_FA_SERVER "  GTopia Server Source (GitHub)", ImVec2(bigButtonWidth, bigButtonHeight)))
        {
            if (!LaunchBrowserURL("https://github.com/keichira/GTopia"))
                LogError("Failed to open GTopia GitHub link in browser");
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.29f, 0.36f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
        if (ImGui::Button(ICON_FA_CODE "  GrowTrace Source (GitHub)", ImVec2(bigButtonWidth, bigButtonHeight)))
        {
            if (!LaunchBrowserURL("https://github.com/keichira/GrowTrace"))
                LogError("Failed to open GrowTrace GitHub link in browser");
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("SectionItemSysStatus", ImVec2(colWidth2, 220.0f), true);
    {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_BOXES_PACKING " Database Status");
        ImGui::Separator();
        ImGui::Spacing();

        ItemInfoManager* pItemMgr = GetItemInfoManager();
        bool isLoaded = (pItemMgr->GetItemCount() > 0);

        ImGui::Text("Item Database:");
        ImGui::SameLine(140.0f);
        if (isLoaded)
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "LOADED (v%u)", pItemMgr->GetVersion());
        else
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "NOT LOADED");

        ImGui::Text("Total Items:");
        ImGui::SameLine(140.0f);
        ImGui::Text("%u items", isLoaded ? pItemMgr->GetItemCount() : 0);

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Text("World Database:");
        ImGui::SameLine(140.0f);
        if (gProxyData.hasWorldLoaded)
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "LOADED (v%u)",
                               gProxyData.worldInfo.GetWorldVersion());
        else
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "NOT LOADED");

        if (gProxyData.hasWorldLoaded)
        {
            ImGui::Text("World Size:");
            ImGui::SameLine(140.0f);

            WorldTileManager* pTileMgr = gProxyData.worldInfo.GetTileManager();
            Vector2Int& vWorldSize = pTileMgr->GetSize();
            ImGui::Text("W:%u H:%u (%u tiles)", vWorldSize.x, vWorldSize.y, vWorldSize.x * vWorldSize.y);
        }

        ImGui::Spacing();
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

void GTProxyPanel::RenderPacketSnifferTab()
{
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float leftWidth = (totalWidth - spacing) * 0.55f;
    float rightWidth = totalWidth - leftWidth - spacing;

    RenderPacketTable(leftWidth);
    ImGui::SameLine();
    RenderHexView(rightWidth);
}

void GTProxyPanel::UpdateItemFilterCache()
{
    m_filteredItemIndices.clear();

    ItemInfoManager* pItemMgr = GetItemInfoManager();

    uint32 itemCount = pItemMgr->GetItemCount();
    m_filteredItemIndices.reserve(itemCount);

    string searchLower = ToLower(m_itemSearchBuf);
    bool hasSearch = !searchLower.empty();

    int32 selectedTypeFilter = -1;
    if (m_selectedItemType >= 0 && m_selectedItemType < gItemInfoTypeCount)
    {
        selectedTypeFilter = gItemInfoTypes[m_selectedItemType].first;
    }

    for (uint32 i = 0; i < pItemMgr->GetItemCount(); ++i)
    {
        ItemInfo* pItem = pItemMgr->GetItemByID(i);
        if (!pItem)
            continue;

        int32 itemId = (pItem->id == 0 && i > 0) ? i : pItem->id;

        if (selectedTypeFilter != -1 && pItem->type != selectedTypeFilter)
            continue;

        if (hasSearch)
        {
            string nameLower = ToLower(pItem->name);
            string idStr = ToString(itemId);

            if (nameLower.find(searchLower) == string::npos && idStr.find(searchLower) == string::npos)
                continue;
        }

        m_filteredItemIndices.push_back(i);
    }

    if ((m_selectedItemId <= 0) && !m_filteredItemIndices.empty())
    {
        m_selectedItemId = m_filteredItemIndices[0];
    }

    m_itemFilterDirty = false;
}

void GTProxyPanel::RenderPlayerTab()
{
    static int32 sPacketTargetDest = 0;
    static int32 sPacketMsgTypeIndex = 0;
    static char sPacketTextPayload[2048] = "";
    static GameUpdatePacket sPacketGamePacket = {};
    static std::vector<CustomVariantField> sPacketVariantFields = {};
    static string sPacketSenderStatusMsg = "";
    static ImVec4 sPacketSenderStatusColor = ImVec4(0.55f, 0.60f, 0.68f, 1.00f);

    float totalWidth = ImGui::GetContentRegionAvail().x;
    float totalHeight = ImGui::GetContentRegionAvail().y;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    float leftWidth = (totalWidth - spacing) * 0.42f;
    float rightWidth = totalWidth - leftWidth - spacing;

    ImGui::BeginChild("PlayerLeftColumn", ImVec2(leftWidth, totalHeight), false);
    {
        float infoCardHeight = 210.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
        ImGui::BeginChild("PlayerInfoCard", ImVec2(0, infoCardHeight), true);
        {
            ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_PERSON " Player Information");
            ImGui::Separator();
            ImGui::Spacing();

            ENetPeer* serverPeer = gENetClient.GetPeer();
            ENetPeer* clientPeer = gENetServer.GetPeer();

            bool isConn = (serverPeer && serverPeer->state == ENET_PEER_STATE_CONNECTED);
            const char* statusStr = isConn ? "Connected" : "Disconnected";
            ImVec4 statusColor = isConn ? ImVec4(0.10f, 0.88f, 0.45f, 1.00f) : ImVec4(1.00f, 0.30f, 0.30f, 1.00f);

            ImGui::TextColored(statusColor, ICON_FA_CIRCLE " %s", statusStr);
            ImGui::Spacing();

            if (ImGui::BeginTable("PlayerInfoTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                auto DrawRow = [](const char* prop, const char* fmt, ...)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(prop);
                    ImGui::TableSetColumnIndex(1);
                    va_list args;
                    va_start(args, fmt);
                    ImGui::TextV(fmt, args);
                    va_end(args);
                };

                string playerName = "N/A";
                int32 userID = 0;
                int32 netID = -1;

                if (gProxyData.HasActiveLocalPlayer())
                {
                    playerName = gProxyData.playerSession.name;
                    userID = gProxyData.playerSession.userID;
                    netID = gProxyData.playerSession.netID;

                    auto it = gProxyData.players.find(netID);
                    if (it != gProxyData.players.end())
                    {
                        if (!it->second.name.empty())
                            playerName = it->second.name;
                        if (it->second.userID != 0)
                            userID = it->second.userID;
                    }
                }

                DrawRow("Name", "%s", playerName.c_str());
                DrawRow("User ID", "%d", userID);
                DrawRow("Net ID", "%d", netID);

                uint32 serverPing = serverPeer ? serverPeer->roundTripTime : 0;
                uint32 clientPing = clientPeer ? clientPeer->roundTripTime : 0;
                DrawRow("Ping", "C: %u ms | S: %u ms", serverPing, clientPing);

                string worldName = gProxyData.hasWorldLoaded ? gProxyData.worldInfo.GetWorlName() : "None";
                DrawRow("World", "%s", worldName.empty() ? "None" : worldName.c_str());

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
    ImGui::BeginChild("PacketSenderPanel", ImVec2(rightWidth, totalHeight), true);
    {
        auto TransmitPacket = [&]()
        {
            bool sendToRealServer = (sPacketTargetDest == 0);
            eMessagePacketType selectedMsgType = NET_MESSAGE_GENERIC_TEXT;
            if (sPacketMsgTypeIndex == 1)
                selectedMsgType = NET_MESSAGE_GAME_MESSAGE;
            else if (sPacketMsgTypeIndex == 2)
                selectedMsgType = NET_MESSAGE_GAME_PACKET;

            if (selectedMsgType == NET_MESSAGE_GENERIC_TEXT || selectedMsgType == NET_MESSAGE_GAME_MESSAGE)
            {
                if (SendENetPacket(sendToRealServer, selectedMsgType, sPacketTextPayload))
                {
                    sPacketSenderStatusMsg = "Text packet sent to " + string(sendToRealServer ? "Server" : "Client");
                    sPacketSenderStatusColor = ImVec4(0.10f, 0.88f, 0.45f, 1.00f);
                }
                else
                {
                    sPacketSenderStatusMsg = "Failed to send packet (Peer disconnected)";
                    sPacketSenderStatusColor = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);
                }
                AddLog(sPacketSenderStatusMsg, sPacketSenderStatusColor);
            }
            else if (selectedMsgType == NET_MESSAGE_GAME_PACKET)
            {
                if (sPacketGamePacket.type == NET_GAME_PACKET_CALL_FUNCTION)
                {
                    VariantVector varVec;
                    for (const auto& vf : sPacketVariantFields)
                    {
                        switch (vf.type)
                        {
                            case 0:
                                varVec.push_back(Variant(string(vf.strVal)));
                                break;
                            case 1:
                                varVec.push_back(Variant((int32)vf.intVal));
                                break;
                            case 2:
                                varVec.push_back(Variant((uint32)vf.uintVal));
                                break;
                            case 3:
                                varVec.push_back(Variant((float)vf.floatVal));
                                break;
                            case 4:
                                varVec.push_back(Variant(Vector2Float(vf.vec2Val[0], vf.vec2Val[1])));
                                break;
                            case 5:
                                varVec.push_back(Variant(Vector3Float(vf.vec3Val[0], vf.vec3Val[1], vf.vec3Val[2])));
                                break;
                        }
                    }

                    uint32 newMemSize = 0;
                    uint8* pNewMem = Proton::SerializeToMem(varVec, &newMemSize, nullptr);

                    GameUpdatePacket packet = sPacketGamePacket;
                    packet.flags |= GAME_PACKET_FLAG_EXTENDED_DATA;
                    packet.extraDataSize = newMemSize;

                    if (SendENetPacketRaw(sendToRealServer, NET_MESSAGE_GAME_PACKET, &packet, sizeof(GameUpdatePacket),
                                          pNewMem))
                    {
                        sPacketSenderStatusMsg = "CallFunction packet (" + ToString(newMemSize) +
                                                 " variant bytes) sent to " + (sendToRealServer ? "Server" : "Client");
                        sPacketSenderStatusColor = ImVec4(0.10f, 0.88f, 0.45f, 1.00f);
                    }
                    else
                    {
                        sPacketSenderStatusMsg = "Failed to send CallFunction packet (Peer disconnected)";
                        sPacketSenderStatusColor = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);
                    }

                    if (pNewMem)
                        SAFE_DELETE_ARRAY(pNewMem);
                }
                else
                {
                    GameUpdatePacket packet = sPacketGamePacket;
                    packet.extraDataSize = 0;

                    if (SendENetPacketRaw(sendToRealServer, NET_MESSAGE_GAME_PACKET, &packet, sizeof(GameUpdatePacket),
                                          nullptr))
                    {
                        sPacketSenderStatusMsg =
                            "GameUpdatePacket sent to " + string(sendToRealServer ? "Server" : "Client");
                        sPacketSenderStatusColor = ImVec4(0.10f, 0.88f, 0.45f, 1.00f);
                    }
                    else
                    {
                        sPacketSenderStatusMsg = "Failed to send GameUpdatePacket (Peer disconnected)";
                        sPacketSenderStatusColor = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);
                    }
                }
                AddLog(sPacketSenderStatusMsg, sPacketSenderStatusColor);
            }
        };

        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_PAPER_PLANE " Packet Sender Panel");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.00f), "Target:");
        ImGui::SameLine();
        ImGui::RadioButton("Server", &sPacketTargetDest, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Client", &sPacketTargetDest, 1);

        float transmitBtnWidth = 140.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - transmitBtnWidth);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.50f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.65f, 0.32f, 1.00f));
        if (ImGui::Button(ICON_FA_PAPER_PLANE " Send Packet", ImVec2(transmitBtnWidth, 24.0f)))
        {
            TransmitPacket();
        }
        ImGui::PopStyleColor(2);

        if (!sPacketSenderStatusMsg.empty())
        {
            ImGui::TextColored(sPacketSenderStatusColor, "%s", sPacketSenderStatusMsg.c_str());
        }

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.00f), "Net Message Packet Type:");
        const char* msgTypeNames[] = {"2: NET_MESSAGE_GENERIC_TEXT", "3: NET_MESSAGE_GAME_MESSAGE",
                                      "4: NET_MESSAGE_GAME_PACKET"};
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::Combo("##MsgTypeCombo", &sPacketMsgTypeIndex, msgTypeNames, IM_ARRAYSIZE(msgTypeNames));

        ImGui::Separator();
        ImGui::Spacing();

        if (sPacketMsgTypeIndex == 0 || sPacketMsgTypeIndex == 1)
        {
            ImGui::Text("Text Payload:");
            float textHeight = totalHeight - 160.0f;
            if (textHeight < 120.0f)
                textHeight = 120.0f;

            ImGui::InputTextMultiline("##TextPayload", sPacketTextPayload, sizeof(sPacketTextPayload),
                                      ImVec2(-1, textHeight), ImGuiInputTextFlags_AllowTabInput);
        }
        else if (sPacketMsgTypeIndex == 2)
        {
            ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), "GameUpdatePacket Fields:");

            int32 currentPktType = sPacketGamePacket.type;
            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::InputInt("type (field_0)", &currentPktType))
            {
                sPacketGamePacket.type = Clamp(currentPktType, 0, 255);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", GetGamePacketTypeName((eGamePacketType)(sPacketGamePacket.type)));

            ImGui::Spacing();

            if (ImGui::BeginTable("StructGridOrdered", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                int32 f1 = sPacketGamePacket.field_1;
                if (ImGui::InputInt("field_1", &f1))
                    sPacketGamePacket.field_1 = (uint8)Clamp(f1, 0, 255);

                ImGui::TableSetColumnIndex(1);
                int32 f2 = sPacketGamePacket.field_2;
                if (ImGui::InputInt("field_2", &f2))
                    sPacketGamePacket.field_2 = (uint8)Clamp(f2, 0, 255);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                int32 f3 = sPacketGamePacket.field_3;
                if (ImGui::InputInt("field_3", &f3))
                    sPacketGamePacket.field_3 = (uint8)Clamp(f3, 0, 255);

                ImGui::TableSetColumnIndex(1);
                ImGui::InputInt("field_4", &sPacketGamePacket.field_4);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::InputInt("field_5", &sPacketGamePacket.field_5);

                ImGui::TableSetColumnIndex(1);
                ImGui::InputFloat("field_6", &sPacketGamePacket.field_6);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::InputInt("field_7", &sPacketGamePacket.field_7);

                ImGui::TableSetColumnIndex(1);

                char flagsPreview[64];
                snprintf(flagsPreview, sizeof(flagsPreview), "0x%08X", sPacketGamePacket.flags);

                if (ImGui::BeginCombo("flags", flagsPreview, ImGuiComboFlags_HeightLarge))
                {
                    for (auto item : gGamePacketFlagMap)
                    {
                        bool isSet = (sPacketGamePacket.flags & item.first) != 0;
                        if (ImGui::Checkbox(item.second, &isSet))
                        {
                            if (isSet)
                                sPacketGamePacket.flags |= item.first;
                            else
                                sPacketGamePacket.flags &= ~item.first;
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::InputFloat("field_8.x", &sPacketGamePacket.field_8.x);

                ImGui::TableSetColumnIndex(1);
                ImGui::InputFloat("field_8.y", &sPacketGamePacket.field_8.y);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::InputFloat("field_9.x", &sPacketGamePacket.field_9.x);

                ImGui::TableSetColumnIndex(1);
                ImGui::InputFloat("field_9.y", &sPacketGamePacket.field_9.y);

                ImGui::EndTable();
            }

            if (sPacketGamePacket.type == NET_GAME_PACKET_CALL_FUNCTION)
            {
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f),
                                   ICON_FA_CODE " CallFunction VariantVector Payload");

                float addBtnWidth = 110.0f;
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - addBtnWidth);
                if (ImGui::Button(ICON_FA_PLUS " Add Variant", ImVec2(addBtnWidth, 22.0f)))
                {
                    sPacketVariantFields.push_back(CustomVariantField{});
                }

                float varRegionHeight = totalHeight - 380.0f;
                if (varRegionHeight < 100.0f)
                    varRegionHeight = 100.0f;

                ImGui::BeginChild("VariantVectorList", ImVec2(0, varRegionHeight), true);
                {
                    const char* varTypeNames[] = {"String", "int32", "UINT", "FLOAT", "Vector2Float", "Vector3Float"};

                    for (int32 i = 0; i < sPacketVariantFields.size(); ++i)
                    {
                        ImGui::PushID(i);
                        auto& field = sPacketVariantFields[i];

                        ImGui::Text("[%zu]", i);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::Combo("##VarType", &field.type, varTypeNames, IM_ARRAYSIZE(varTypeNames));
                        ImGui::SameLine();

                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 35.0f);
                        switch (field.type)
                        {
                            case 0:
                                ImGui::InputText("##ValStr", field.strVal, sizeof(field.strVal));
                                break;
                            case 1:
                                ImGui::InputInt("##ValInt", &field.intVal);
                                break;
                            case 2:
                                ImGui::InputScalar("##ValUInt", ImGuiDataType_U32, &field.uintVal);
                                break;
                            case 3:
                                ImGui::InputFloat("##ValFlt", &field.floatVal);
                                break;
                            case 4:
                                ImGui::InputFloat2("##ValVec2", field.vec2Val);
                                break;
                            case 5:
                                ImGui::InputFloat3("##ValVec3", field.vec3Val);
                                break;
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("X"))
                        {
                            sPacketVariantFields.erase(sPacketVariantFields.begin() + i);
                            ImGui::PopID();
                            break;
                        }

                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void GTProxyPanel::UpdatePlayerFilterCache()
{
    m_filteredPlayerIndices.clear();
    m_filteredPlayerIndices.reserve(gProxyData.players.size());

    string searchLower = ToLower(m_playerSearchBuf);
    bool hasSearch = !searchLower.empty();

    for (auto& kv : gProxyData.players)
    {
        auto& player = kv.second;
        if (hasSearch)
        {
            string nameLower = ToLower(player.name);
            string idStr = ToString(player.userID);
            string netStr = ToString(player.netID);

            if (nameLower.find(searchLower) == string::npos && idStr.find(searchLower) == string::npos &&
                netStr.find(searchLower) == string::npos)
            {
                continue;
            }
        }
        m_filteredPlayerIndices.push_back(player.netID);
    }

    m_playerFilterDirty = false;
}

void GTProxyPanel::UpdateWorldObjFilterCache()
{
    m_filteredWorldObjIndices.clear();

    if (!gProxyData.hasWorldLoaded || !gProxyData.worldInfo.GetObjectManager())
    {
        m_worldObjFilterDirty = false;
        return;
    }

    std::vector<WorldObject>& objects = gProxyData.worldInfo.GetObjectManager()->GetObjects();

    m_filteredWorldObjIndices.reserve(objects.size());

    string searchLower = ToLower(m_worldObjSearchBuf);
    bool hasSearch = !searchLower.empty();

    ItemInfoManager* pItemMgr = GetItemInfoManager();

    for (auto& obj : objects)
    {
        if (obj.itemID == 0)
            continue;

        if (hasSearch)
        {
            string objIdStr = ToString(obj.objectID);
            string itemIdStr = ToString(obj.itemID);
            string itemName = "";
            ItemInfo* pItem = pItemMgr->GetItemByID(obj.itemID);
            if (pItem)
                itemName = ToLower(pItem->name);

            if (objIdStr.find(searchLower) == string::npos && itemIdStr.find(searchLower) == string::npos &&
                itemName.find(searchLower) == string::npos)
            {
                continue;
            }
        }
        m_filteredWorldObjIndices.push_back(obj.objectID);
    }

    m_worldObjFilterDirty = false;
}

void GTProxyPanel::RenderWorldInfoSection()
{
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_EARTH_AMERICAS " World Information");
    ImGui::Separator();
    ImGui::Spacing();

    if (!gProxyData.hasWorldLoaded)
    {
        ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), ICON_FA_CIRCLE " No World Loaded");
        ImGui::TextDisabled("Enter a world in Growtopia to load map data.");
        return;
    }

    ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), ICON_FA_CIRCLE " World Active");
    ImGui::Spacing();

    if (ImGui::BeginTable("WorldInfoTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto DrawRow = [](const char* prop, const char* fmt, ...)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(prop);
            ImGui::TableSetColumnIndex(1);
            va_list args;
            va_start(args, fmt);
            ImGui::TextV(fmt, args);
            va_end(args);
        };

        DrawRow("Name", "%s", gProxyData.worldInfo.GetWorlName().c_str());
        DrawRow("Version", "v%u", gProxyData.worldInfo.GetWorldVersion());

        Vector2Int& vWorldSize = gProxyData.worldInfo.GetTileManager()->GetSize();
        DrawRow("Size", "%d x %d", vWorldSize.x, vWorldSize.y);

        DrawRow("Current Weather", "%u", gProxyData.worldInfo.GetCurrentWeather());
        DrawRow("Default Weather", "%u", gProxyData.worldInfo.GetDefaultWeather());

        ImGui::EndTable();
    }
}

void GTProxyPanel::RenderWorldActionsSection()
{
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_FLOPPY_DISK " World Actions");
    ImGui::Separator();
    ImGui::Spacing();

    bool hasWorld = gProxyData.hasWorldLoaded;
    float btnWidth = ImGui::GetContentRegionAvail().x;

    ImGui::BeginDisabled(!hasWorld);

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save World Original", ImVec2(btnWidth, 30.0f)))
    {
        string path;
        if (gProxyData.SaveWorld(false, &path))
            AddLog("Saved world to " + path, ImVec4(0.10f, 0.88f, 0.45f, 1.00f));
        else
            AddLog("Failed to save world.", ImVec4(1.00f, 0.30f, 0.30f, 1.00f));
    }

    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save World for GTopia", ImVec2(btnWidth, 30.0f)))
    {
        string path;
        if (gProxyData.SaveWorld(true, &path))
            AddLog("Saved world to " + path, ImVec4(0.10f, 0.88f, 0.45f, 1.00f));
        else
            AddLog("Failed to save world.", ImVec4(1.00f, 0.30f, 0.30f, 1.00f));
    }

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.18f, 0.18f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.12f, 0.12f, 1.00f));

    if (ImGui::Button(ICON_FA_TRASH " Unload World Data", ImVec2(btnWidth, 30.0f)))
    {
        gProxyData.UnloadWorld();
        m_worldObjFilterDirty = true;
        AddLog("World data unloaded.", ImVec4(0.95f, 0.75f, 0.20f, 1.00f));
    }

    ImGui::PopStyleColor(3);

    ImGui::EndDisabled();
}

void GTProxyPanel::RenderPlayersSection(float height)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
    ImGui::BeginChild("PlayersCard", ImVec2(0, height), true);

    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_USERS " Players (%zu)", gProxyData.players.size());
    ImGui::Separator();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
    if (ImGui::InputTextWithHint("##PlayerSearch", "Search player by name, User ID, Net ID...", m_playerSearchBuf,
                                 sizeof(m_playerSearchBuf)))
    {
        m_playerFilterDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##Player"))
    {
        m_playerSearchBuf[0] = '\0';
        m_playerFilterDirty = true;
    }

    ImGui::Separator();

    if (ImGui::BeginTable("PlayersTable", 5,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable,
                          ImVec2(0, 0)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("User ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Net ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(m_filteredPlayerIndices.size());

        while (clipper.Step())
        {
            for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                int32 netID = m_filteredPlayerIndices[row];
                auto it = gProxyData.players.find(netID);
                if (it == gProxyData.players.end())
                    continue;

                auto& player = it->second;

                ImGui::PushID(player.netID);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (player.netID == gProxyData.playerSession.netID)
                {
                    ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "%s (You)", player.name.c_str());
                }
                else
                {
                    ImGui::TextUnformatted(player.name.c_str());
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", player.userID);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", player.netID);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("X: %d  Y: %d", player.posX, player.posY);

                ImGui::TableSetColumnIndex(4);
                if (ImGui::Button("Kick"))
                {
                }
                ImGui::SameLine();
                if (ImGui::Button("Ban"))
                {
                }
                ImGui::SameLine();
                if (ImGui::Button("Pull"))
                {
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void GTProxyPanel::RenderDroppedObjectsSection(float height)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
    ImGui::BeginChild("DroppedObjectsCard", ImVec2(0, 0), true);

    uint32 totalObjCount = 0;
    if (gProxyData.hasWorldLoaded && gProxyData.worldInfo.GetObjectManager())
    {
        RectFloat fullMapRect(-1000.0f, -1000.0f, 100000.0f, 100000.0f);
        totalObjCount = gProxyData.worldInfo.GetObjectManager()->GetCounfOfObjestsInRect(fullMapRect);
    }

    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_OBJECT_GROUP " Dropped Objects (%u)", totalObjCount);
    ImGui::Separator();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
    if (ImGui::InputTextWithHint("##ObjSearch", "Search object by ID, item name...", m_worldObjSearchBuf,
                                 sizeof(m_worldObjSearchBuf)))
    {
        m_worldObjFilterDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##Obj"))
    {
        m_worldObjSearchBuf[0] = '\0';
        m_worldObjFilterDirty = true;
    }

    ImGui::Separator();

    if (ImGui::BeginTable("DroppedObjectsTable", 5,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable,
                          ImVec2(0, 0)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        if (gProxyData.hasWorldLoaded && gProxyData.worldInfo.GetObjectManager())
        {
            ItemInfoManager* pItemMgr = GetItemInfoManager();
            ImGuiListClipper clipper;
            clipper.Begin(m_filteredWorldObjIndices.size());

            while (clipper.Step())
            {
                for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    uint32 objID = m_filteredWorldObjIndices[row];
                    WorldObject* pObj = gProxyData.worldInfo.GetObjectManager()->GetObjectByID(objID);
                    if (!pObj)
                        continue;

                    ImGui::PushID(pObj->objectID);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%u", pObj->objectID);

                    ImGui::TableSetColumnIndex(1);
                    string itemName = "Item " + ToString(pObj->itemID);
                    if (pItemMgr)
                    {
                        ItemInfo* item = pItemMgr->GetItemByID(pObj->itemID);
                        if (item && !item->name.empty())
                        {
                            itemName = item->name;
                        }
                    }
                    ImGui::TextUnformatted(itemName.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", pObj->count);

                    ImGui::TableSetColumnIndex(3);
                    int32 posX = (int32)(pObj->pos.x / 32.0f);
                    int32 posY = (int32)(pObj->pos.y / 32.0f);
                    ImGui::Text("%d, %d", posX, posY);

                    ImGui::TableSetColumnIndex(4);
                    ImGui::BeginDisabled(!gProxyData.HasActiveLocalPlayer());
                    if (ImGui::Button("Show"))
                    {
                    }
                    ImGui::EndDisabled();

                    ImGui::PopID();
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void GTProxyPanel::RenderWorldTab()
{
    if (m_playerFilterDirty || m_filteredPlayerIndices.size() != gProxyData.players.size())
        UpdatePlayerFilterCache();

    if (m_worldObjFilterDirty)
        UpdateWorldObjFilterCache();

    float totalWidth = ImGui::GetContentRegionAvail().x;
    float totalHeight = ImGui::GetContentRegionAvail().y;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    float leftWidth = (totalWidth - spacing) * 0.32f;
    float rightWidth = totalWidth - leftWidth - spacing;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
    ImGui::BeginChild("WorldLeftPanel", ImVec2(leftWidth, 0), true);
    {
        RenderWorldInfoSection();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        RenderWorldActionsSection();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::BeginChild("WorldRightPanel", ImVec2(rightWidth, 0), false);
    {
        float sectionHeight = (totalHeight - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

        RenderPlayersSection(sectionHeight);
        ImGui::Spacing();
        RenderDroppedObjectsSection(sectionHeight);
    }
    ImGui::EndChild();
}

void GTProxyPanel::RenderItemFlagsView(const ItemInfo& item)
{
    if (ImGui::CollapsingHeader("Active Item Flags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool anyFlag = false;
        for (int32 i = 0; i < gItemInfoFlagCount; ++i)
        {
            if (item.flags & gItemInfoFlagMap[i].first)
            {
                ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "  [X] %s (0x%04X)", gItemInfoFlagMap[i].second,
                                   gItemInfoFlagMap[i].first);
                anyFlag = true;
            }
        }
        if (!anyFlag)
        {
            ImGui::TextDisabled("  (No active item flags)");
        }
    }
}

void GTProxyPanel::RenderItemDataTab()
{
    ItemInfoManager* pItemMgr = GetItemInfoManager();
    bool isLoaded = (pItemMgr->GetItemCount() > 0);

    if (pItemMgr->GetItemCount() != m_lastItemCount)
    {
        m_lastItemCount = pItemMgr->GetItemCount();
        m_itemFilterDirty = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
    ImGui::BeginChild("ItemDataHeaderBar", ImVec2(0, 36.0f), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (isLoaded)
            ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), ICON_FA_CIRCLE);
        else
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), ICON_FA_CIRCLE);

        ImGui::SameLine();
        ImGui::TextUnformatted(isLoaded ? "Item Data Loaded" : "Item Data Not Loaded");

        if (isLoaded)
        {
            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_FILE_CODE " Version: v%u",
                               pItemMgr->GetVersion());

            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextDisabled(ICON_FA_BOOK " Hash: %u", pItemMgr->GetHash());

            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.00f), ICON_FA_CUBES " Total Items: %u",
                               pItemMgr->GetItemCount());

            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 25.0f);
            ImGui::TextDisabled(ICON_FA_FILTER " Filtered: %zu", m_filteredItemIndices.size());
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Spacing();

    if (m_itemFilterDirty || (isLoaded && m_filteredItemIndices.empty()))
    {
        UpdateItemFilterCache();
    }

    float totalWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float leftWidth = (totalWidth - spacing) * 0.42f;
    float rightWidth = totalWidth - leftWidth - spacing;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
    ImGui::BeginChild("ItemDataListPanel", ImVec2(leftWidth, 0), true);
    {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_BOXES_PACKING " Item Database Search");
        ImGui::Separator();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
        if (ImGui::InputTextWithHint("##ItemSearch", "Search by name or ID...", m_itemSearchBuf,
                                     sizeof(m_itemSearchBuf)))
        {
            m_itemFilterDirty = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            m_itemSearchBuf[0] = '\0';
            m_itemFilterDirty = true;
        }

        if (m_selectedItemType < 0 || m_selectedItemType >= gItemInfoTypeCount)
            m_selectedItemType = 0;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##ItemCategoryFilter", gItemInfoTypes[m_selectedItemType].second))
        {
            for (int32 i = 0; i < gItemInfoTypeCount; ++i)
            {
                bool isSelected = (m_selectedItemType == i);
                if (ImGui::Selectable(gItemInfoTypes[i].second, isSelected))
                {
                    m_selectedItemType = i;
                    m_itemFilterDirty = true;
                }
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        if (ImGui::BeginTable("ItemDatabaseTable", 3,
                              ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable,
                              ImVec2(0, 0)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableHeadersRow();

            if (isLoaded && !m_filteredItemIndices.empty())
            {
                ImGuiListClipper clipper;
                clipper.Begin(m_filteredItemIndices.size());

                while (clipper.Step())
                {
                    for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        if (row < 0 || row >= m_filteredItemIndices.size())
                            continue;

                        int32 itemId = m_filteredItemIndices[row];
                        ItemInfo* pItem = pItemMgr->GetItemByID(itemId);
                        if (!pItem)
                            continue;

                        ImGui::PushID(row);

                        ImGui::TableNextRow();
                        bool isSelected = (m_selectedItemId == pItem->id);

                        ImGui::TableSetColumnIndex(0);
                        char idBuf[32];
                        snprintf(idBuf, sizeof(idBuf), "%d##item_%d", pItem->id, row);
                        if (ImGui::Selectable(idBuf, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            m_selectedItemId = pItem->id;
                        }

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(pItem->name.empty() ? "(NAME EMPTY)" : pItem->name.c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("%s", GetItemInfoTypeName(pItem->type));

                        ImGui::PopID();
                    }
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::BeginChild("ItemInspectorPanel", ImVec2(rightWidth, 0), true);
    {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_MAGNIFYING_GLASS " Item Detail Viewer");
        ImGui::Separator();

        ItemInfo* pItem = isLoaded ? pItemMgr->GetItemByID(m_selectedItemId) : nullptr;

        if (pItem != nullptr)
        {
            auto DrawPropRow = [](const char* prop, const char* fmt, ...)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(prop);
                ImGui::TableSetColumnIndex(1);
                va_list args;
                va_start(args, fmt);
                ImGui::TextV(fmt, args);
                va_end(args);
            };

            ImGui::Text("ID: %d | Name: %s", pItem->id, pItem->name.c_str());
            ImGui::TextWrapped("Description: %s", pItem->description.c_str());
            ImGui::Separator();

            if (ImGui::CollapsingHeader("General Properties", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginTable("GenPropsTable", 2,
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    DrawPropRow("Type", "%s", GetItemInfoTypeName(pItem->type));
                    DrawPropRow("Material", "%s (%u)", GetItemMaterialRawStr(pItem->material), pItem->material);
                    DrawPropRow("Rarity", "%d", pItem->rarity);
                    DrawPropRow("Max Can Hold", "%u %s", pItem->maxCanHold,
                                pItem->IsUnlimited() ? "(Unlimited (0))" : "");
                    DrawPropRow("Storage Type", "%s (%u)", GetItemStorageTypeRawStr(pItem->storage), pItem->storage);
                    DrawPropRow("Collision Type", "%s (%u)", GetItemCollisionTypeRawStr(pItem->collisionType),
                                pItem->collisionType);
                    DrawPropRow("Clothing Body Part", "%s (%u)", GetItemBodyPartRawStr(pItem->bodyPart),
                                pItem->bodyPart);
                    DrawPropRow("HP", "%u", pItem->hp);
                    DrawPropRow("Restore Time", "%d s", pItem->restoreTime);
                    DrawPropRow("Cooking Time", "%d", pItem->cookingTime);

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Visual & Rendering Data", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginTable("VisPropsTable", 2,
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    DrawPropRow("Texture File", "%s", pItem->textureFile.c_str());
                    DrawPropRow("Texture Hash", "%u", pItem->textureHash);
                    DrawPropRow("Texture Coords", "X: %u, Y: %u", pItem->textureX, pItem->textureY);
                    DrawPropRow("Visual Effect", "%s (%u)", GetItemVisualEffectRawStr(pItem->visualEffect),
                                pItem->visualEffect);
                    DrawPropRow("Render Layer", "%d", pItem->layer);
                    DrawPropRow("Anim MS / Weather ID", "%d", pItem->animMS);
                    DrawPropRow("Overlay Texture", "%s", pItem->overlayTextureFile.c_str());
                    DrawPropRow("Multi Anim 1", "%s", pItem->multiAnim1.c_str());
                    DrawPropRow("Multi Anim 2", "%s", pItem->multiAnim2.c_str());
                    DrawPropRow("Dual Anim Layer", "X: %d, Y: %d", pItem->dualAnimLayer.x, pItem->dualAnimLayer.y);
                    DrawPropRow("Light Source Range", "%u", pItem->lightSourceRange);
                    DrawPropRow("Hit Particle ID", "%d", pItem->otherPlayerHitParticle);

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Seed Data"))
            {
                if (ImGui::BeginTable("SeedPropsTable", 2,
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    DrawPropRow("Grow Time", "%u", pItem->growTime);
                    DrawPropRow("Seed BG / FG", "%u / %u", pItem->seedBg, pItem->seedFg);
                    DrawPropRow("Tree BG / FG", "%u / %u", pItem->treeBg, pItem->treeFg);
                    DrawPropRow("Seed Base Colors", "Seed 1: %u, Seed 2: %u", pItem->seed1, pItem->seed2);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Seed BG Color");
                    ImGui::TableSetColumnIndex(1);
                    ImVec4 bgCol(pItem->seedBgColor.r / 255.0f, pItem->seedBgColor.g / 255.0f,
                                 pItem->seedBgColor.b / 255.0f, pItem->seedBgColor.a / 255.0f);
                    ImGui::ColorButton("##SeedBGColor", bgCol, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
                    ImGui::SameLine();
                    ImGui::Text("R: %u, G: %u, B: %u, A: %u", pItem->seedBgColor.r, pItem->seedBgColor.g,
                                pItem->seedBgColor.b, pItem->seedBgColor.a);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Seed FG Color");
                    ImGui::TableSetColumnIndex(1);
                    ImVec4 fgCol(pItem->seedFgColor.r / 255.0f, pItem->seedFgColor.g / 255.0f,
                                 pItem->seedFgColor.b / 255.0f, pItem->seedFgColor.a / 255.0f);
                    ImGui::ColorButton("##SeedFGColor", fgCol, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
                    ImGui::SameLine();
                    ImGui::Text("R: %u, G: %u, B: %u, A: %u", pItem->seedFgColor.r, pItem->seedFgColor.g,
                                pItem->seedFgColor.b, pItem->seedFgColor.a);

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Pet Properties"))
            {
                if (ImGui::BeginTable("PetPropsTable", 2,
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    DrawPropRow("Pet Name", "%s", pItem->petName.c_str());
                    DrawPropRow("Pet Sub Name", "%s", pItem->petSubName.c_str());
                    DrawPropRow("Pet End Name", "%s", pItem->petEndName.c_str());
                    DrawPropRow("Pet Power Name", "%s", pItem->petPowerName.c_str());

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Chair & Sprite Replacement Info"))
            {
                if (ImGui::BeginTable("ChairPropsTable", 2,
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    DrawPropRow("Chair Enabled", "%u", pItem->chairInfo.enabled);
                    DrawPropRow("Player Offset", "X: %d, Y: %d", pItem->chairInfo.playerOffset.x,
                                pItem->chairInfo.playerOffset.y);
                    DrawPropRow("Arm Position", "X: %d, Y: %d", pItem->chairInfo.armPos.x, pItem->chairInfo.armPos.y);
                    DrawPropRow("Arm Offset", "X: %d, Y: %d", pItem->chairInfo.armOffset.x,
                                pItem->chairInfo.armOffset.y);
                    DrawPropRow("Arm Texture", "%s", pItem->chairInfo.armTexture.c_str());

                    DrawPropRow("Random Sprite Enabled", "%u", pItem->randomSpriteInfo.enabled);
                    DrawPropRow("Random Sprite Offset Mod", "%d", pItem->randomSpriteInfo.offsetMod);
                    DrawPropRow("Random Sprite Chance", "%.2f", pItem->randomSpriteInfo.chance);

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Extra Parameters"))
            {
                if (ImGui::BeginTable("AdvPropsTable", 2,
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    DrawPropRow("Extra String", "%s", pItem->extraString.c_str());
                    DrawPropRow("Extra String Hash", "0x%08X", pItem->extraStringHash);
                    DrawPropRow("Tile Range / Pile Size", "%u / %u", pItem->tileRange, pItem->pileSize);
                    DrawPropRow("Custom Punch Params", "%s", pItem->customizedPunchParameters.c_str());
                    DrawPropRow("Config Name", "%s", pItem->configName.c_str());
                    DrawPropRow("Config Name Hash", "0x%08X", pItem->configNameHash);
                    DrawPropRow("Variant Version Item", "%u", pItem->variantVersionItem);
                    DrawPropRow("Hidden Parts Flags", "0x%02X", pItem->hiddenPartsFlags);
                    DrawPropRow("Can Transform", "%u", pItem->canTransform);
                    DrawPropRow("Slippery Type", "%s (%u)", GetItemSlipperyType(pItem->slipperyType),
                                pItem->slipperyType);

                    DrawPropRow("Extra Slot Counter", "%u", pItem->extraSlotCounter);
                    char extraPartsBuf[128] = {0};
                    snprintf(extraPartsBuf, sizeof(extraPartsBuf), "%u, %u, %u, %u, %u, %u, %u, %u, %u",
                             pItem->extraSlotBodyParts[0], pItem->extraSlotBodyParts[1], pItem->extraSlotBodyParts[2],
                             pItem->extraSlotBodyParts[3], pItem->extraSlotBodyParts[4], pItem->extraSlotBodyParts[5],
                             pItem->extraSlotBodyParts[6], pItem->extraSlotBodyParts[7], pItem->extraSlotBodyParts[8]);
                    DrawPropRow("Extra Body Parts", "[%s]", extraPartsBuf);

                    ImGui::EndTable();
                }

                if (ImGui::TreeNode("Client Data Array (15 Ints)"))
                {
                    for (int32 c = 0; c < 15; ++c)
                    {
                        ImGui::Text("clientData[%2d]: %d (0x%08X)", c, pItem->clientData[c], pItem->clientData[c]);
                    }
                    ImGui::TreePop();
                }
            }

            RenderItemFlagsView(*pItem);

            ImGui::Separator();

            if (ImGui::Button(ICON_FA_COPY " Copy Definition (GTopia format)"))
            {
                string itemDef = BuildItemTextDefinition(pItem);
                ImGui::SetClipboardText(itemDef.c_str());
            }
        }
        else
        {
            if (!isLoaded)
            {
                ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "No Item Data file loaded.");
            }
            else
            {
                ImGui::TextDisabled("Select an item from the list to view detailed information.");
            }
        }
    }
    ImGui::EndChild();
}

void GTProxyPanel::RenderFilterSubSet(UIPacketFilterSet& filterSet, const char* idPrefix)
{
    ImGui::PushID(idPrefix);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.25f, 1.00f), "Direction Filters:");
    if (ImGui::Checkbox("Client -> Server (C2S)", &filterSet.c2s))
        m_filterDirty = true;

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Checkbox("Server -> Client (S2C)", &filterSet.s2c))
        m_filterDirty = true;

    ImGui::Spacing();
    if (ImGui::Button("Select All"))
    {
        filterSet.SetAll(true);
        m_filterDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Deselect All"))
    {
        filterSet.SetAll(false);
        m_filterDirty = true;
    }

    ImGui::Separator();

    float childHeight = ImGui::GetContentRegionAvail().y - 30.0f;
    if (childHeight < 150.0f)
        childHeight = 150.0f;

    ImGui::BeginChild("FilterRulesScrollArea", ImVec2(0, childHeight), true);

    if (ImGui::CollapsingHeader("Message Packet Types", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("MsgTypesTable", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthStretch);

            int32 col = 0;
            for (auto msgType : gMessagePacketTypes)
            {
                if (col % 2 == 0)
                    ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(col % 2);

                int32 typeKey = (int32)(msgType);
                bool enabled = filterSet.messageTypes[typeKey];

                char buf[128];
                snprintf(buf, sizeof(buf), "%s (0x%02X)", GetMessageTypeName(msgType), msgType);

                if (ImGui::Checkbox(buf, &enabled))
                {
                    filterSet.messageTypes[typeKey] = enabled;
                    m_filterDirty = true;
                }
                col++;
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Game Packet Types", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("GameTypesTable", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthStretch);

            int32 col = 0;
            for (auto gameType : gGamePacketTypes)
            {
                if (col % 2 == 0)
                    ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(col % 2);

                int32 typeKey = (int32)(gameType);
                bool enabled = filterSet.gameTypes[typeKey];
                const char* name = GetGamePacketTypeName(gameType);

                char buf[128];
                snprintf(buf, sizeof(buf), "%s (0x%02X)", name, gameType);

                if (ImGui::Checkbox(buf, &enabled))
                {
                    filterSet.gameTypes[typeKey] = enabled;
                    m_filterDirty = true;
                }
                col++;
            }
            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
    ImGui::PopID();
}

void GTProxyPanel::RenderCaptureFilterModal()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    float width = Clamp(viewport->WorkSize.x * 0.75f, 480.0f, 750.0f);
    float height = Clamp(viewport->WorkSize.y * 0.75f, 400.0f, 650.0f);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("CaptureFilterPopup", nullptr, ImGuiWindowFlags_NoMove))
    {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), ICON_FA_FILTER " Packet Filtering Rules");
        ImGui::Separator();

        if (ImGui::BeginTabBar("RulesTabBar"))
        {
            if (ImGui::BeginTabItem(ICON_FA_DOWNLOAD " Incoming Rules (Capture)"))
            {
                RenderFilterSubSet(m_captureFilter, "capture");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(ICON_FA_EYE " Existing Rules (Display)"))
            {
                RenderFilterSubSet(m_displayFilter, "display");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();

        float btnWidth = 120.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnWidth) * 0.5f);
        if (ImGui::Button("Close", ImVec2(btnWidth, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void GTProxyPanel::RenderPacketTable(float width)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.00f));
    ImGui::BeginChild("PacketTablePanel", ImVec2(width, 0), true);

    ImGuiStyle& style = ImGui::GetStyle();
    float btnHeight = ImGui::GetFrameHeight();

    float allBtnW = ImGui::CalcTextSize("All").x + style.FramePadding.x * 2.5f;
    float c2sBtnW = ImGui::CalcTextSize("C -> S").x + style.FramePadding.x * 2.5f;
    float s2cBtnW = ImGui::CalcTextSize("S -> C").x + style.FramePadding.x * 2.5f;
    float pauseBtnW = ImGui::CalcTextSize(m_isPaused ? "Resume" : "Pause").x + style.FramePadding.x * 2.5f;

    bool isAllPushed = (m_directionFilter == UI_PACKET_FILTER_DIR_ALL);
    if (isAllPushed)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.48f, 0.35f, 1.00f));

    if (ImGui::Button("All", ImVec2(allBtnW, btnHeight)))
    {
        m_directionFilter = UI_PACKET_FILTER_DIR_ALL;
        m_filterDirty = true;
    }

    if (isAllPushed)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    bool isC2SPushed = (m_directionFilter == UI_PACKET_FILTER_DIR_C2S);
    if (isC2SPushed)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.48f, 0.35f, 1.00f));

    if (ImGui::Button("C -> S", ImVec2(c2sBtnW, btnHeight)))
    {
        m_directionFilter = UI_PACKET_FILTER_DIR_C2S;
        m_filterDirty = true;
    }

    if (isC2SPushed)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    bool isS2CPushed = (m_directionFilter == UI_PACKET_FILTER_DIR_S2C);
    if (isS2CPushed)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.48f, 0.35f, 1.00f));

    if (ImGui::Button("S -> C", ImVec2(s2cBtnW, btnHeight)))
    {
        m_directionFilter = UI_PACKET_FILTER_DIR_S2C;
        m_filterDirty = true;
    }

    if (isS2CPushed)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##Search", "Search packets...", m_searchBuf, sizeof(m_searchBuf)))
    {
        m_filterDirty = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_SLIDERS " Rules"))
    {
        ImGui::OpenPopup("CaptureFilterPopup");
    }
    RenderCaptureFilterModal();

    ImGui::SameLine();

    bool isPausePushed = m_isPaused;
    if (isPausePushed)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.20f, 1.00f));

    if (ImGui::Button(m_isPaused ? "Resume" : "Pause", ImVec2(pauseBtnW, btnHeight)))
    {
        m_isPaused = !m_isPaused;
    }

    if (isPausePushed)
        ImGui::PopStyleColor(); // this gonna kill me dawg

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScrollPackets);

    if (m_filterDirty)
    {
        UpdateFilterCache();
    }

    float fontSize = ImGui::GetFontSize();
    if (ImGui::BeginTable("PacketTable", 7,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(0, 0)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, fontSize * 3.0f);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, fontSize * 5.5f);
        ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed, fontSize * 3.5f);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, fontSize * 4.5f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, fontSize * 7.5f);
        ImGui::TableSetupColumn("Sub Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Len", ImGuiTableColumnFlags_WidthFixed, fontSize * 3.0f);
        ImGui::TableHeadersRow();

        if (m_poppedFilteredCount > 0)
        {
            if (!m_autoScrollPackets)
            {
                float rowHeight = ImGui::GetTextLineHeightWithSpacing();
                float currentScrollY = ImGui::GetScrollY();
                float adjustment = (float)(m_poppedFilteredCount)*rowHeight;
                float newScrollY = (currentScrollY > adjustment) ? (currentScrollY - adjustment) : 0.0f;
                ImGui::SetScrollY(newScrollY);
            }
            m_poppedFilteredCount = 0;
        }

        auto RenderCellText = [](const char* text)
        {
            ImGui::TextUnformatted(text);
            if (ImGui::CalcTextSize(text).x > ImGui::GetContentRegionAvail().x && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", text);
            }
        };

        auto RenderCellTextColored = [](const ImVec4& color, const char* text)
        {
            ImGui::TextColored(color, "%s", text);
            if (ImGui::CalcTextSize(text).x > ImGui::GetContentRegionAvail().x && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", text);
            }
        };

        ImGuiListClipper clipper;
        clipper.Begin(m_filteredIndices.size());

        while (clipper.Step())
        {
            for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                if (row < 0 || row >= m_filteredIndices.size())
                    continue;

                usize packetktIndex = m_filteredIndices[row];
                if (packetktIndex >= m_packets.size())
                    continue;

                auto& capturedPacket = m_packets[packetktIndex];

                ImGui::PushID(capturedPacket.id);

                const char* msgName = GetMessageTypeName(capturedPacket.msgType);
                string subTypeName = GetSubTypeName(capturedPacket);

                ImGui::TableNextRow();
                bool isSelected = (capturedPacket.id == m_selectedPacketId);

                ImGui::TableSetColumnIndex(0);
                char idBuf[16];
                snprintf(idBuf, sizeof(idBuf), "%u", capturedPacket.id);
                if (ImGui::Selectable(idBuf, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_selectedPacketId = capturedPacket.id;
                    m_selectedPacketIndex = packetktIndex;
                }

                ImGui::TableSetColumnIndex(1);
                RenderCellText(capturedPacket.timestamp.c_str());

                ImGui::TableSetColumnIndex(2);
                RenderCellTextColored(capturedPacket.isC2S ? ImVec4(0.00f, 0.85f, 1.00f, 1.00f)
                                                           : ImVec4(0.88f, 0.35f, 0.98f, 1.00f),
                                      capturedPacket.isC2S ? "C->S" : "S->C");

                ImGui::TableSetColumnIndex(3);
                if (capturedPacket.isCustom)
                    RenderCellTextColored(ImVec4(1.00f, 0.60f, 0.10f, 1.00f), "[CUSTOM]");
                else if (capturedPacket.isBlocked)
                    RenderCellTextColored(ImVec4(1.00f, 0.25f, 0.30f, 1.00f), "[BLOCKED]");
                else
                    RenderCellTextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f), "NORMAL");

                ImGui::TableSetColumnIndex(4);
                RenderCellText(msgName);

                ImGui::TableSetColumnIndex(5);
                RenderCellText(subTypeName.c_str());

                ImGui::TableSetColumnIndex(6);
                char lenBuf[16];
                snprintf(lenBuf, sizeof(lenBuf), "%zu", capturedPacket.rawData.size());
                RenderCellText(lenBuf);

                ImGui::PopID();
            }
        }

        if (m_autoScrollPackets)
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void GTProxyPanel::RenderHexView(float width)
{
    ImGui::BeginChild("HexViewPanel", ImVec2(width, 0), true);
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), "Packet Detail View");
    ImGui::Separator();

    float actionButtonHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f;

    UICapturedPacket* pSelectedPacket = nullptr;
    if (m_selectedPacketId != 0)
    {
        for (auto& capturedPacket : m_packets)
        {
            if (capturedPacket.id == m_selectedPacketId)
            {
                pSelectedPacket = &capturedPacket;
                break;
            }
        }
    }

    if (pSelectedPacket)
    {
        auto& capturedPacket = *pSelectedPacket;

        ImGui::Text("#%u | Size: %zu bytes", capturedPacket.id, capturedPacket.rawData.size());
        ImGui::Text("Type: %s", GetMessageTypeName(capturedPacket.msgType));
        if (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET)
        {
            string subTypeName = GetSubTypeName(capturedPacket);
            ImGui::Text("Game Type: %s (0x%X)", subTypeName.c_str(), capturedPacket.gameType);
        }
        ImGui::Separator();

        if (capturedPacket.msgType == NET_MESSAGE_GENERIC_TEXT || capturedPacket.msgType == NET_MESSAGE_GAME_MESSAGE)
        {
            bool isHexViewActive = (m_detailViewMode == UI_PACKET_VIEW_RAWHEX);
            if (isHexViewActive)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.38f, 0.68f, 1.00f));

            if (ImGui::RadioButton("Raw Hex View", m_detailViewMode == UI_PACKET_VIEW_RAWHEX))
                m_detailViewMode = UI_PACKET_VIEW_RAWHEX;

            if (isHexViewActive)
                ImGui::PopStyleColor();

            ImGui::SameLine();

            bool isTextViewActive = (m_detailViewMode == UI_PACKET_VIEW_TEXT);
            if (isTextViewActive)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.38f, 0.68f, 1.00f));

            if (ImGui::RadioButton("View Text", m_detailViewMode == UI_PACKET_VIEW_TEXT))
                m_detailViewMode = UI_PACKET_VIEW_TEXT;

            if (isTextViewActive)
                ImGui::PopStyleColor();

            ImGui::Separator();

            if (m_detailViewMode == UI_PACKET_VIEW_TEXT)
            {
                if (capturedPacket.rawData.size() > 4)
                {
                    usize textLen = capturedPacket.rawData.size() - 4;
                    if (textLen > 0 && capturedPacket.rawData.back() == '\0')
                    {
                        textLen--;
                    }
                    string textContent((const char*)(capturedPacket.rawData.data() + 4), textLen);
                    float inputHeight = ImGui::GetContentRegionAvail().y - actionButtonHeight;

                    ImGui::BeginChild("TextContentViewScroll", ImVec2(0, Max(40.0f, inputHeight)), true,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    ImGui::TextWrapped("%s", textContent.c_str());
                    ImGui::EndChild();
                }
                else
                {
                    ImGui::TextDisabled("Packet text buffer is empty.");
                }
            }
            else
            {
                RenderRawHexView(capturedPacket, actionButtonHeight);
            }
        }
        else if (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET)
        {
            if (ImGui::RadioButton("Struct View", m_detailViewMode == UI_PACKET_VIEW_STRUCT))
                m_detailViewMode = UI_PACKET_VIEW_STRUCT;

            ImGui::SameLine();
            if (ImGui::RadioButton("Raw Hex View", m_detailViewMode == UI_PACKET_VIEW_RAWHEX))
                m_detailViewMode = UI_PACKET_VIEW_RAWHEX;

            if (capturedPacket.gameType == NET_GAME_PACKET_CALL_FUNCTION)
            {
                ImGui::SameLine();
                if (ImGui::RadioButton("Variant View", m_detailViewMode == UI_PACKET_VIEW_VARIANT))
                    m_detailViewMode = UI_PACKET_VIEW_VARIANT;
            }
            else if (m_detailViewMode == UI_PACKET_VIEW_VARIANT)
            {
                m_detailViewMode = UI_PACKET_VIEW_STRUCT;
            }
            ImGui::Separator();

            if (m_detailViewMode == UI_PACKET_VIEW_STRUCT)
            {
                RenderGameUpdatePacketStructView(capturedPacket);
            }
            else if (m_detailViewMode == UI_PACKET_VIEW_VARIANT &&
                     capturedPacket.gameType == NET_GAME_PACKET_CALL_FUNCTION)
            {
                RenderVariantView(capturedPacket);
            }
            else
            {
                RenderRawHexView(capturedPacket, actionButtonHeight);
            }
        }
        else
        {
            RenderRawHexView(capturedPacket, actionButtonHeight);
        }

        ImGui::Separator();

        if (ImGui::Button("Copy Hex"))
        {
            string hexOnly;
            if (!capturedPacket.rawData.empty())
            {
                hexOnly.reserve(capturedPacket.rawData.size() * 3);
                for (usize i = 0; i < capturedPacket.rawData.size(); ++i)
                {
                    uint8 b = capturedPacket.rawData[i];
                    hexOnly.push_back(gHexByteTable[b][0]);
                    hexOnly.push_back(gHexByteTable[b][1]);

                    if (i + 1 < capturedPacket.rawData.size())
                        hexOnly.push_back(' ');
                }
            }
            ImGui::SetClipboardText(hexOnly.c_str());
        }

        if (capturedPacket.msgType == NET_MESSAGE_GENERIC_TEXT || capturedPacket.msgType == NET_MESSAGE_GAME_MESSAGE)
        {
            ImGui::SameLine();
            if (ImGui::Button("Copy Text"))
            {
                if (capturedPacket.rawData.size() > 4)
                {
                    usize textLen = capturedPacket.rawData.size() - 4;
                    if (textLen > 0 && capturedPacket.rawData.back() == '\0')
                        textLen--;

                    string textContent((const char*)(capturedPacket.rawData.data() + 4), textLen);
                    ImGui::SetClipboardText(textContent.c_str());
                }
                else
                {
                    ImGui::SetClipboardText("");
                }
            }
        }

        if (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET &&
            capturedPacket.rawData.size() >= 4 + sizeof(GameUpdatePacket))
        {
            ImGui::SameLine();
            if (ImGui::Button("Copy Struct"))
            {
                GameUpdatePacket* pGamePacket = (GameUpdatePacket*)(capturedPacket.rawData.data() + 4);

                char structBuf[1024];
                snprintf(structBuf, sizeof(structBuf),
                         "type: %u (0x%02X)\nfield_1: %u\nfield_2: %u\nfield_3: %u\nfield_4: %d\nfield_5: %d\nflags: "
                         "0x%08X\nfield_6: %.4f\nfield_7: %d\nfield_8: X: %.2f, Y: %.2f\nfield_9: X: %.2f, Y: "
                         "%.2f\nfield_10: %.4f\nfield_11: %d\nfield_12: %d\nextraDataSize: %d",
                         pGamePacket->type, pGamePacket->type, pGamePacket->field_1, pGamePacket->field_2,
                         pGamePacket->field_3, pGamePacket->field_4, pGamePacket->field_5, pGamePacket->flags,
                         pGamePacket->field_6, pGamePacket->field_7, pGamePacket->field_8.x, pGamePacket->field_8.y,
                         pGamePacket->field_9.x, pGamePacket->field_9.y, pGamePacket->field_10, pGamePacket->field_11,
                         pGamePacket->field_12, pGamePacket->extraDataSize);
                ImGui::SetClipboardText(structBuf);
            }
        }

        int32 structOffset = 4 + sizeof(GameUpdatePacket);

        if (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET &&
            capturedPacket.gameType == NET_GAME_PACKET_CALL_FUNCTION && capturedPacket.rawData.size() > structOffset)
        {
            ImGui::SameLine();
            if (ImGui::Button("Copy Variant"))
            {
                GameUpdatePacket* pGamePacket = (GameUpdatePacket*)(capturedPacket.rawData.data() + 4);

                int32 availableExtraBytes = capturedPacket.rawData.size() - structOffset;
                if (pGamePacket->extraDataSize == 0 || pGamePacket->extraDataSize > availableExtraBytes)
                    pGamePacket->extraDataSize = availableExtraBytes;

                uint8* pExtra = (uint8*)(capturedPacket.rawData.data() + structOffset);
                VariantVector varVec;
                int32 bytesRead = 0;
                if (Proton::SerializeFromMem(pExtra, pGamePacket->extraDataSize, varVec, &bytesRead))
                {
                    string varStr;
                    for (usize i = 0; i < varVec.size(); ++i)
                    {
                        auto& var = varVec[i];
                        char lineBuf[512];
                        snprintf(lineBuf, sizeof(lineBuf), "(%s) %s\n", i, UIUtils::GetVariantTypeName(var.GetType()),
                                 UIUtils::GetVariantValueString(var).c_str());
                        varStr += lineBuf;
                    }
                    ImGui::SetClipboardText(varStr.c_str());
                }
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Select a packet to inspect.");
    }

    ImGui::EndChild();
}

void GTProxyPanel::RenderRawHexView(const UICapturedPacket& capturedPacket, float reservedBottomHeight)
{
    usize totalBytes = capturedPacket.rawData.size();
    if (totalBytes == 0)
    {
        ImGui::TextDisabled("Packet buffer is empty.");
        return;
    }

    float availHeight = ImGui::GetContentRegionAvail().y - reservedBottomHeight;
    ImGui::BeginChild("RawHexDumpScroll", ImVec2(0, Max(40.0f, availHeight)), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    int32 bytesPerLine = 16;
    int32 lineCount = ((totalBytes + bytesPerLine - 1) / bytesPerLine);

    ImGuiListClipper clipper;
    clipper.Begin(lineCount);

    char hexStr[16 * 3 + 1];
    char asciiStr[16 + 1];

    while (clipper.Step())
    {
        for (int32 line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
        {
            usize offset = (usize)(line)*bytesPerLine;
            if (offset >= totalBytes)
                break;

            ImGui::Text("%08zX:", offset);
            ImGui::SameLine();

            usize hexPos = 0;
            usize asciiPos = 0;

            for (usize j = 0; j < bytesPerLine; ++j)
            {
                usize idx = offset + j;
                if (idx < totalBytes)
                {
                    uint8 b = capturedPacket.rawData[idx];
                    hexStr[hexPos++] = gHexByteTable[b][0];
                    hexStr[hexPos++] = gHexByteTable[b][1];
                    hexStr[hexPos++] = ' ';
                    asciiStr[asciiPos++] = (b >= 32 && b <= 126) ? (char)(b) : '.';
                }
                else
                {
                    hexStr[hexPos++] = ' ';
                    hexStr[hexPos++] = ' ';
                    hexStr[hexPos++] = ' ';
                }
            }

            hexStr[hexPos] = '\0';
            asciiStr[asciiPos] = '\0';

            ImGui::TextUnformatted(hexStr);
            ImGui::SameLine();
            ImGui::Text("| %s", asciiStr);
        }
    }

    ImGui::EndChild();
}

void GTProxyPanel::RenderGameUpdatePacketStructView(const UICapturedPacket& capturedPacket)
{
    float actionButtonHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f;

    if (capturedPacket.rawData.size() < 4 + sizeof(GameUpdatePacket))
    {
        ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "Invalid size for GameUpdatePacket (%zu / %zu byte)",
                           capturedPacket.rawData.size(), 4 + sizeof(GameUpdatePacket));
        RenderRawHexView(capturedPacket, actionButtonHeight);
        return;
    }

    GameUpdatePacket* pGamePacket = (GameUpdatePacket*)(capturedPacket.rawData.data() + 4);

    float availHeight = ImGui::GetContentRegionAvail().y - actionButtonHeight;
    ImGui::BeginChild("StructFieldsScroll", ImVec2(0, Max(40.0f, availHeight)), true);

    if (ImGui::CollapsingHeader("GameUpdatePacket Struct Fields", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("GUPFieldsTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            auto DrawRow = [](const char* label, const char* fmt, ...)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                va_list args;
                va_start(args, fmt);
                ImGui::TextV(fmt, args);
                va_end(args);
            };

            DrawRow("type", "%u (0x%02X)", pGamePacket->type, pGamePacket->type);
            DrawRow("field_1", "%u", pGamePacket->field_1);
            DrawRow("field_2", "%u", pGamePacket->field_2);
            DrawRow("field_3", "%u", pGamePacket->field_3);
            DrawRow("field_4", "%d", pGamePacket->field_4);
            DrawRow("field_5", "%d", pGamePacket->field_5);
            DrawRow("flags", "0x%08X", pGamePacket->flags);
            DrawRow("field_6", "%.4f", pGamePacket->field_6);
            DrawRow("field_7", "%d", pGamePacket->field_7);
            DrawRow("field_8", "X: %.2f, Y: %.2f", pGamePacket->field_8.x, pGamePacket->field_8.y);
            DrawRow("field_9", "X: %.2f, Y: %.2f", pGamePacket->field_9.x, pGamePacket->field_9.y);
            DrawRow("field_10", "%.4f", pGamePacket->field_10);
            DrawRow("field_11", "%d", pGamePacket->field_11);
            DrawRow("field_12", "%d", pGamePacket->field_12);
            DrawRow("extraDataSize", "%d", pGamePacket->extraDataSize);

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Active Flags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool anyFlag = false;
        for (int32 i = 0; i < gGamePacketFlagMapCount; ++i)
        {
            auto& flagInfo = gGamePacketFlagMap[i];
            if (pGamePacket->flags & flagInfo.first)
            {
                ImGui::TextColored(ImVec4(0.10f, 0.88f, 0.45f, 1.00f), "  [X] %s (1 << %d)", flagInfo.second,
                                   flagInfo.first);
                anyFlag = true;
            }
        }
        if (!anyFlag)
        {
            ImGui::TextDisabled("  (No active flags)");
        }
    }

    if (pGamePacket->HasFlag(GAME_PACKET_FLAG_EXTENDED_DATA) || pGamePacket->extraDataSize > 0)
    {
        if (ImGui::CollapsingHeader("Extended Data Buffer", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int32 structOffset = 4 + sizeof(GameUpdatePacket);

            if (capturedPacket.rawData.size() < structOffset)
            {
                ImGui::TextDisabled("Invalid offset for extended data.");
                ImGui::EndChild();
                return;
            }

            uint32 packetExtraSize = capturedPacket.rawData.size() - structOffset;
            uint32 safeExtraSize = pGamePacket->extraDataSize;
            if (safeExtraSize == 0 || safeExtraSize > packetExtraSize)
                safeExtraSize = packetExtraSize;

            if (safeExtraSize > 0)
            {
                const uint8* pExtra = capturedPacket.rawData.data() + structOffset;
                ImGui::Spacing();
                ImGui::BeginChild("ExtraDataHexArea", ImVec2(0, 180.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

                int32 bytesPerLine = 16;

                ImGuiListClipper extClipper;
                extClipper.Begin((safeExtraSize + bytesPerLine - 1) / bytesPerLine);

                char hexStr[16 * 3 + 1];
                char asciiStr[16 + 1];

                while (extClipper.Step())
                {
                    for (int32 line = extClipper.DisplayStart; line < extClipper.DisplayEnd; ++line)
                    {
                        usize i = line * bytesPerLine;
                        if (i >= safeExtraSize)
                            break;

                        ImGui::Text("%08zX:", i);
                        ImGui::SameLine();

                        usize hexPos = 0;
                        usize asciiPos = 0;

                        for (usize j = 0; j < bytesPerLine; ++j)
                        {
                            usize index = i + j;
                            if (index < safeExtraSize)
                            {
                                const uint8 b = pExtra[index];
                                hexStr[hexPos++] = gHexByteTable[b][0];
                                hexStr[hexPos++] = gHexByteTable[b][1];
                                hexStr[hexPos++] = ' ';
                                asciiStr[asciiPos++] = (b >= 32 && b <= 126) ? (char)(b) : '.';
                            }
                            else
                            {
                                hexStr[hexPos++] = ' ';
                                hexStr[hexPos++] = ' ';
                                hexStr[hexPos++] = ' ';
                            }
                        }

                        hexStr[hexPos] = '\0';
                        asciiStr[asciiPos] = '\0';

                        ImGui::TextUnformatted(hexStr);
                        ImGui::SameLine();
                        ImGui::Text("| %s", asciiStr);
                    }
                }

                ImGui::EndChild();
            }
            else
            {
                ImGui::TextDisabled("No extended data available.");
            }
        }
    }

    ImGui::EndChild();
}

void GTProxyPanel::RenderVariantView(const UICapturedPacket& capturedPacket)
{
    float actionButtonHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f;
    uint32 structOffset = 4 + sizeof(GameUpdatePacket);

    if (capturedPacket.rawData.size() <= structOffset)
    {
        ImGui::TextDisabled("No extra data available for VariantList.");
        RenderRawHexView(capturedPacket, actionButtonHeight);
        return;
    }

    GameUpdatePacket* pGamePacket = (GameUpdatePacket*)(capturedPacket.rawData.data() + 4);

    uint32 availableExtraBytes = capturedPacket.rawData.size() - structOffset;
    uint32 extraSize = pGamePacket->extraDataSize;
    if (extraSize == 0 || extraSize > availableExtraBytes)
        extraSize = availableExtraBytes;

    uint8* pExtra = (uint8*)(capturedPacket.rawData.data() + structOffset);

    VariantVector varVec;
    int32 bytesRead = 0;
    if (!Proton::SerializeFromMem(pExtra, extraSize, varVec, &bytesRead))
    {
        ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "Failed to deserialize VariantList from extended data.");
        RenderRawHexView(capturedPacket, actionButtonHeight);
        return;
    }

    float availHeight = ImGui::GetContentRegionAvail().y - actionButtonHeight;

    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), "CallFunction Variant List (%zu Argument(s))",
                       varVec.size());
    ImGui::Separator();

    if (ImGui::BeginTable("VariantTable", 3,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0, Max(40.0f, availHeight))))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (usize i = 0; i < varVec.size(); ++i)
        {
            auto& var = varVec[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.25f, 1.00f), "%s", UIUtils::GetVariantTypeName(var.GetType()));

            ImGui::TableSetColumnIndex(2);
            string valStr = UIUtils::GetVariantValueString(var);

            ImGui::PushTextWrapPos(0.0f);
            if (var.GetType() == VARIANT_TYPE_STRING)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.90f, 0.55f, 1.00f));
                ImGui::TextUnformatted(valStr.c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::TextUnformatted(valStr.c_str());
            }
            ImGui::PopTextWrapPos();
        }

        ImGui::EndTable();
    }
}

string GTProxyPanel::GetSubTypeName(const UICapturedPacket& capturedPacket)
{
    if (capturedPacket.msgType != NET_MESSAGE_GAME_PACKET)
        return "";

    const char* gameName = GetGamePacketTypeName(capturedPacket.gameType);
    if (capturedPacket.gameType != NET_GAME_PACKET_CALL_FUNCTION)
        return gameName;

    auto it = m_cachedFunctionNames.find(capturedPacket.id);
    if (it != m_cachedFunctionNames.end())
    {
        return it->second;
    }

    string result = gameName;
    uint32 structOffset = 4 + sizeof(GameUpdatePacket);

    if (capturedPacket.rawData.size() > structOffset)
    {
        GameUpdatePacket* pGamePacket = (GameUpdatePacket*)(capturedPacket.rawData.data() + 4);
        usize availableExtraBytes = capturedPacket.rawData.size() - structOffset;

        uint32 extraSize = pGamePacket->extraDataSize;
        if (extraSize == 0 || extraSize > availableExtraBytes)
            extraSize = availableExtraBytes;

        if (extraSize > 0)
        {
            uint8* pExtra = (uint8*)(capturedPacket.rawData.data() + structOffset);
            VariantVector varVec;
            int32 bytesRead = 0;
            if (Proton::SerializeFromMem(pExtra, extraSize, varVec, &bytesRead))
            {
                if (!varVec.empty() && varVec[0].GetType() == VARIANT_TYPE_STRING)
                {
                    string funcName = varVec[0].GetString();
                    if (!funcName.empty())
                    {
                        result += " / " + funcName;
                    }
                }
            }
        }
    }

    m_cachedFunctionNames[capturedPacket.id] = result;
    return result;
}

bool GTProxyPanel::PacketMatchesFilter(const UICapturedPacket& capturedPacket)
{
    if (m_directionFilter == UI_PACKET_FILTER_DIR_C2S && !capturedPacket.isC2S)
        return false;
    if (m_directionFilter == UI_PACKET_FILTER_DIR_S2C && capturedPacket.isC2S)
        return false;

    if (capturedPacket.isC2S && !m_displayFilter.c2s)
        return false;
    if (!capturedPacket.isC2S && !m_displayFilter.s2c)
        return false;

    if (!m_displayFilter.IsMessageAllowed(capturedPacket.msgType))
        return false;

    if (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET)
    {
        if (!m_displayFilter.IsGameTypeAllowed(capturedPacket.gameType))
            return false;
    }

    if (m_searchBuf[0] != '\0')
    {
        string searchLower = ToLower(m_searchBuf);

        const char* msgName = GetMessageTypeName(capturedPacket.msgType);
        bool matchMsg = UIUtils::CaseInsensitiveContains(msgName, searchLower);

        string subName = GetSubTypeName(capturedPacket);
        bool matchGame = (capturedPacket.msgType == NET_MESSAGE_GAME_PACKET) &&
                         UIUtils::CaseInsensitiveContains(subName.c_str(), searchLower);

        if (!matchMsg && !matchGame)
            return false;
    }

    return true;
}

void GTProxyPanel::UpdateFilterCache()
{
    m_filteredIndices.clear();

    const usize total = m_packets.size();
    m_filteredIndices.reserve(total);

    for (usize i = 0; i < total; ++i)
    {
        if (PacketMatchesFilter(m_packets[i]))
        {
            m_filteredIndices.push_back(i);
        }
    }

    m_filterDirty = false;
}