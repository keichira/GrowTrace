#pragma once

#include "Precompiled.h"
#include <concurrentqueue.h>
#include <enet/enet.h>

class ENetServer;

extern ENetServer gENetServer;
extern ENetServer gENetClient;

class ENetServer
{
public:
    typedef moodycamel::ConcurrentQueue<ENetEvent> EventQueue;

    enum class Mode
    {
        None,
        Server,
        Client
    };

public:
    ENetServer();
    ~ENetServer();

public:
    bool Init(Mode mode, const string& host = "", uint16 port = 0, uint32 maxPeers = ENET_PROTOCOL_MAXIMUM_PEER_ID);
    bool Connect(const string& host, uint16 port, uint32 timeoutMs = 5000);
    void Kill();

    void Update(uint32 timeoutMs = 0);
    bool PopEvent(ENetEvent& outEvent);

    void DisconnectPeer(ENetPeer* pPeer, uint32 data = 0);
    void SetENetIncomeCmdType(ENetHostIncomeCommandType type);
    void Flush();

    ENetPeer* GetPeer() const { return m_pPeer; }
    void SetPeer(ENetPeer* pPeer) { m_pPeer = pPeer; }

    ENetHost* GetHost() const { return m_pHost; }
    Mode GetMode() const { return m_mode; }
    EventQueue& GetEvents() { return m_eventQueue; }

    bool IsReconnecting() const { return m_isReconnecting; }
    void SetReconnecting(bool reconnecting) { m_isReconnecting = reconnecting; }

    void SetReconnectTarget(const string& host, uint16 port)
    {
        m_reconnectHost = host;
        m_reconnectPort = port;
    }
    const string& GetReconnectHost() const { return m_reconnectHost; }
    uint16 GetReconnectPort() const { return m_reconnectPort; }

private:
    ENetHost* m_pHost;
    ENetPeer* m_pPeer;
    Mode m_mode;
    EventQueue m_eventQueue;
    bool m_isEnetInitialized;

    bool m_isReconnecting;
    string m_reconnectHost;
    uint16 m_reconnectPort;

    string address;
    uint16 port;
};