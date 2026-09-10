#pragma once

#include "../Core/ProxyData.h"
#include "Item/ItemInfoManager.h"
#include "UIUtils.h"
#include <deque>
#include <unordered_map>
#include <vector>

struct UICapturedPacket
{
    uint32 id = 0;
    string timestamp;
    bool isC2S = true;
    bool isCustom = false;
    bool isBlocked = false;
    eMessagePacketType msgType = NET_MESSAGE_UNKNOWN;
    eGamePacketType gameType = NET_GAME_PACKET_STATE;
    std::vector<uint8> rawData;
};

struct CustomVariantField
{
    int32 type = 0;
    char strVal[4096] = "";
    int32 intVal = 0;
    uint32 uintVal = 0;
    float floatVal = 0.0f;
    float vec2Val[2] = {0.0f, 0.0f};
    float vec3Val[3] = {0.0f, 0.0f, 0.0f};
};

enum eUITab
{
    UI_TAB_DASHBOARD,
    UI_TAB_PACKETSNIFFER,
    UI_TAB_WORLD,
    UI_TAB_ITEMDATA,
    UI_TAB_PLAYER,
    UI_TAB_COUNT
};

enum eUIPacketFilterDirection
{
    UI_PACKET_FILTER_DIR_ALL,
    UI_PACKET_FILTER_DIR_C2S,
    UI_PACKET_FILTER_DIR_S2C
};

enum eUIPacketViewMode
{
    UI_PACKET_VIEW_STRUCT,
    UI_PACKET_VIEW_RAWHEX,
    UI_PACKET_VIEW_VARIANT,
    UI_PACKET_VIEW_TEXT,
};

struct UIPacketFilterSet
{
    bool c2s = true;
    bool s2c = true;
    std::unordered_map<int32, bool> messageTypes;
    std::unordered_map<int32, bool> gameTypes;

    UIPacketFilterSet() { SetAll(true); }

    void SetAll(bool enabled)
    {
        c2s = enabled;
        s2c = enabled;
        for (auto type : gMessagePacketTypes)
            messageTypes[(int32)type] = enabled;

        for (auto type : gGamePacketTypes)
            gameTypes[(int32)type] = enabled;
    }

    bool IsMessageAllowed(eMessagePacketType type) const
    {
        auto it = messageTypes.find((int32)type);
        return it != messageTypes.end() ? it->second : true;
    }

    bool IsGameTypeAllowed(eGamePacketType type) const
    {
        auto it = gameTypes.find((int32)type);
        return it != gameTypes.end() ? it->second : true;
    }
};

class GTProxyPanel : public Panel
{
public:
    GTProxyPanel();
    ~GTProxyPanel() override = default;

public:
    void OnUpdate(EditorContext& ctx, float dt) override;
    void OnImGuiRender(EditorContext& ctx) override;

    void PushPacket(bool isC2S, eMessagePacketType msgType, const uint8* pData, usize length,
                    eGamePacketType gameType = NET_GAME_PACKET_STATE);
    void AddLog(const string& msg, const ImVec4& color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

private:
    void RenderTopStatus();
    void RenderTabBar();
    void RenderBottomLogPanel(float height);

    void RenderDashboardTab();
    void RenderPacketSnifferTab();
    void RenderPacketTable(float width);
    void RenderHexView(float width);
    void RenderRawHexView(const UICapturedPacket& capturedPacket, float reservedBottomHeight);
    void RenderGameUpdatePacketStructView(const UICapturedPacket& capturedPacket);
    void RenderVariantView(const UICapturedPacket& capturedPacket);

    void RenderItemDataTab();
    void RenderItemFlagsView(const ItemInfo& item);
    void UpdateItemFilterCache();

    void RenderPlayerTab();

    void RenderWorldTab();
    void RenderWorldInfoSection();
    void RenderWorldActionsSection();
    void RenderPlayersSection(float height);
    void RenderDroppedObjectsSection(float height);

    void RenderCaptureFilterModal();
    void RenderFilterSubSet(UIPacketFilterSet& filterSet, const char* idPrefix);

    void UpdatePlayerFilterCache();
    void UpdateWorldObjFilterCache();

    eGamePacketType ParseGameTypeFromBuffer(const uint8* pData, usize length, eGamePacketType fallback);
    bool ShouldCapturePacket(bool isC2S, eMessagePacketType msgType, eGamePacketType gameType) const;
    bool PacketMatchesFilter(const UICapturedPacket& capturedPacket);
    void UpdateFilterCache();
    string GetSubTypeName(const UICapturedPacket& capturedPacket);

private:
    eUITab m_currentTab;
    eUIPacketFilterDirection m_directionFilter;
    eUIPacketViewMode m_detailViewMode;

    uint32 m_packetCounter;
    bool m_isPaused;
    bool m_autoScrollPackets;
    bool m_autoScrollLogs;

    uint32 m_selectedPacketId;
    usize m_selectedPacketIndex;

    char m_searchBuf[128] = "";
    UIPacketFilterSet m_captureFilter;
    UIPacketFilterSet m_displayFilter;

    std::deque<UICapturedPacket> m_packets;
    std::vector<usize> m_filteredIndices;
    bool m_filterDirty = true;
    uint32 m_poppedFilteredCount;

    struct LogEntry
    {
        string timestamp;
        string message;
        ImVec4 color;
    };
    std::deque<LogEntry> m_logs;

    std::unordered_map<uint32, string> m_cachedFunctionNames;

    int32 m_selectedItemId;
    int32 m_selectedItemType;
    char m_itemSearchBuf[128] = "";
    std::vector<int32> m_filteredItemIndices;
    bool m_itemFilterDirty;
    uint32 m_lastItemCount;

    std::vector<int32> m_filteredPlayerIndices;
    std::vector<uint32> m_filteredWorldObjIndices;

    char m_playerSearchBuf[128] = "";
    char m_worldObjSearchBuf[128] = "";
    bool m_playerFilterDirty;
    bool m_worldObjFilterDirty;
};