#pragma once
#include "Event/EventDispatcher.h"
#include "Packet/GamePacket.h"
#include "Packet/PacketUtils.h"
#include <enet/enet.h>

constexpr uint8 MAX_TEXT_PACKET_FIELDS = 64;

class EventManager
{
public:
    EventManager();
    ~EventManager();

    typedef EventDispatcher<uint32, bool, ENetPeer*, const ParsedTextPacket<MAX_TEXT_PACKET_FIELDS>&> TextDispatcher;
    typedef EventDispatcher<uint8, bool, ENetPeer*, GameUpdatePacket*> TankDispatcher;

public:
    static EventManager* GetInstance()
    {
        static EventManager instance;
        return &instance;
    }

public:
    TextDispatcher& GetTextDispatcher() { return m_textDispatcher; }
    TankDispatcher& GetTankDispatcher() { return m_tankDispatcher; }

    void RegisterEvents();

    bool ProcessIncomingPacket(ENetPeer* pPeer, ENetPacket* pOriginalPacket);

private:
    template <bool (*fn)(ENetPeer*, GameUpdatePacket*)> void RegisterTank(uint8 packetType)
    {
        m_tankDispatcher.Register(packetType, TankDispatcher::Handler::template Create<fn>());
    }

    template <bool (*fn)(ENetPeer*, const ParsedTextPacket<MAX_TEXT_PACKET_FIELDS>&)> void RegisterText(uint32 key)
    {
        m_textDispatcher.Register(key, TextDispatcher::Handler::template Create<fn>());
    }

private:
    TextDispatcher m_textDispatcher;
    TankDispatcher m_tankDispatcher;
};

EventManager* GetEventManager();