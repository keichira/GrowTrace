#include "ENetServer.h"

ENetServer gENetServer;
ENetServer gENetClient;

ENetServer::ENetServer()
    : m_pHost(nullptr), m_pPeer(nullptr), m_mode(Mode::None), m_isEnetInitialized(false), m_isReconnecting(false),
      m_reconnectPort(0)
{
}

ENetServer::~ENetServer()
{
    Kill();
}

bool ENetServer::Init(Mode mode, const string& host, uint16 port, uint32 maxPeers)
{
    if (m_pHost)
    {
        Kill();
    }

    ENetAddress addr{};

    if (mode == Mode::Server)
    {
        if (!host.empty() && host != "0.0.0.0")
        {
            if (enet_address_set_host(&addr, host.c_str()) != 0)
                return false;
        }
        else
        {
            addr.host = ENET_HOST_ANY;
        }

        addr.port = port;
    }
    else if (mode == Mode::Client)
    {
        maxPeers = 1;
    }

    if (mode == Mode::Client)
    {
        m_pHost = enet_host_create(nullptr, maxPeers, 2, 0, 0);
    }
    else
    {
        m_pHost = enet_host_create(&addr, maxPeers, 2, 0, 0);
    }

    if (!m_pHost)
        return false;

    m_pHost->checksum = enet_crc32;
    enet_host_compress_with_range_coder(m_pHost);

    if (mode == Mode::Client)
    {
        m_pHost->useNewPacket = 1;
    }

    m_mode = mode;
    return true;
}

bool ENetServer::Connect(const string& host, uint16 port, uint32 timeoutMs)
{
    if (!Init(Mode::Client))
        return false;

    if (m_pPeer)
    {
        enet_peer_reset(m_pPeer);
        m_pPeer = nullptr;
    }

    ENetAddress addr;
    if (enet_address_set_host(&addr, host.c_str()) != 0)
        return false;

    addr.port = port;

    m_pPeer = enet_host_connect(m_pHost, &addr, 2, 0);
    if (!m_pPeer)
        return false;

    ENetEvent event;
    if (enet_host_service(m_pHost, &event, timeoutMs) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        m_eventQueue.enqueue(event);
        return true;
    }

    enet_peer_reset(m_pPeer);
    m_pPeer = nullptr;
    return false;
}

void ENetServer::Kill()
{
    if (m_pHost)
    {
        enet_host_flush(m_pHost);
        enet_host_destroy(m_pHost);
        m_pHost = nullptr;
    }

    m_pPeer = nullptr;
    m_mode = Mode::None;
}

void ENetServer::Update(uint32 timeoutMs)
{
    if (!m_pHost)
        return;

    ENetEvent event;
    while (enet_host_service(m_pHost, &event, timeoutMs) > 0)
    {
        m_eventQueue.enqueue(event);
        timeoutMs = 0;
    }
}

bool ENetServer::PopEvent(ENetEvent& outEvent)
{
    return m_eventQueue.try_dequeue(outEvent);
}

void ENetServer::DisconnectPeer(ENetPeer* pPeer, uint32 data)
{
    if (!m_pHost || !pPeer)
        return;

    enet_peer_disconnect(pPeer, data);
    enet_host_flush(m_pHost);
}

void ENetServer::SetENetIncomeCmdType(ENetHostIncomeCommandType type)
{
    if (m_pHost)
    {
        m_pHost->incomeCommandType = type;
    }
}

void ENetServer::Flush()
{
    if (m_pHost)
    {
        enet_host_flush(m_pHost);
    }
}
