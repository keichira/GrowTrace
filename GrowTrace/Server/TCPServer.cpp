#include "TCPServer.h"
#include "../Core/Application.h"
#include "../Core/ProxyData.h"
#include "IO/Log.h"
#include "Network/NetHTTP.h"
#include "Packet/GamePacket.h"
#include "Packet/PacketUtils.h"
#include "Utils/StringUtils.h"

TCPServer gTCPServer;

TCPServer::TCPServer() : m_pNetSocket(nullptr) {}

TCPServer::~TCPServer()
{
    Kill();
}

bool TCPServer::Init(const string& host, uint16 port, int32 backLog)
{
    SAFE_DELETE(m_pNetSocket);
    m_pNetSocket = new NetSocket();

    if (!m_pNetSocket->Init(host, port, backLog))
        return false;

    RegisterEvents();
    return true;
}

void TCPServer::Kill()
{
    SAFE_DELETE(m_pNetSocket);
    m_httpClient.pClient = nullptr;
    m_httpClient.isReady = false;
}

void TCPServer::OnClientConnect(NetClient* pClient)
{
    if (!pClient)
        return;

    if (m_httpClient.pClient && m_httpClient.pClient == pClient)
    {
        m_httpClient.lastHeartBeat.Reset();
        return;
    }

    if (m_httpClient.pClient)
    {
        LogWarn("New HTTP bridge connecting while old one exists. Closing old client.");

        m_httpClient.pClient->status = SOCKET_CLIENT_CLOSE;
        m_httpClient.pClient = nullptr;
    }

    m_httpClient.pClient = pClient;
    m_httpClient.isReady = false;
    m_httpClient.lastHeartBeat.Reset();

    LogInfo("HTTP Bridge client connected, waiting for HELLO packet...");
}

void TCPServer::OnClientReceive(NetClient* pClient)
{
    if (!pClient || pClient->status == SOCKET_CLIENT_CLOSE || !m_httpClient.pClient ||
        m_httpClient.pClient != pClient || m_httpClient.pClient->status == SOCKET_CLIENT_CLOSE)
        return;

    while (true)
    {
        TCPPacketEvent packet;
        packet.pClient = pClient;
        packet.reqTime = Time::GetSystemTime();

        {
            std::lock_guard<std::mutex> lock(pClient->recvMutex);

            if (pClient->recvQueue.GetDataSize() < sizeof(TCPPacketHeader))
                break;

            pClient->recvQueue.Peek(&packet.header, sizeof(TCPPacketHeader));

            if (packet.header.bodySize > 1024 * 64)
            {
                pClient->status = SOCKET_CLIENT_CLOSE;
                break;
            }

            uint32 totalPacketSize = sizeof(TCPPacketHeader) + packet.header.bodySize;
            if (pClient->recvQueue.GetDataSize() < totalPacketSize)
                break;

            TCPPacketHeader dummyHeader;
            pClient->recvQueue.Read(&dummyHeader, sizeof(TCPPacketHeader));

            if (packet.header.bodySize > 0)
            {
                packet.payload.resize(packet.header.bodySize);
                pClient->recvQueue.Read(packet.payload.data(), packet.header.bodySize);
            }
        }

        uint16 packetID = packet.header.packetID;

        if (packetID != TCP_PACKET_HEARTBEAT)
        {
            LogInfo("Received TCP Packet %d (Size: %u)", packetID, packet.header.bodySize);
        }

        if (packetID != TCP_PACKET_HELLO && !m_httpClient.isReady)
        {
            LogWarn("TCPClient tried to send packet without sending hello first, closing it.");
            m_httpClient.pClient->status = SOCKET_CLIENT_CLOSE;
            break;
        }

        TCPPacketReader reader(packet.payload.data(), (uint32)(packet.payload.size()));
        m_httpClient.lastHeartBeat.Reset();

        switch (packetID)
        {
            case TCP_PACKET_HELLO:
            {
                m_httpClient.isReady = true;
                SendHello();
                LogInfo("Received hello from http, broadway connection is ready.");
                break;
            }

            case TCP_PACKET_HEARTBEAT:
            {
                m_httpClient.lastHeartBeat.Reset();
                break;
            }

            case TCP_PACKET_HTTP_DATA:
            {
                float version = 0.0f;
                int32 platform = 0;
                int32 protocol = 0;
                string userAgent;

                reader.Read(version);
                reader.Read(platform);
                reader.Read(protocol);
                reader.ReadString(userAgent);

                LogInfo("Received HTTP Data, Protocol: %d, Version: %.2f, Platform: %d\nUser-Agent: %s", protocol,
                        version, platform, userAgent.c_str());

                LogInfo("Fetching server ip address...");

                string outDomain;
                string targetIP = GetTargetServerIP(!gProxyData.proxySettings.localServerMode, outDomain);
                if (targetIP.empty())
                {
                    LogError("Failed to fetch ip address, its empty.");
                    SendHTTPData("");
                    break;
                }

                LogInfo("Fetched server ip %s, sending request for server data", targetIP.c_str());

                NetHTTP http;
                http.Init(targetIP);
                http.AddPostData("version", ToString(version));
                http.AddPostData("platform", ToString(platform));
                http.AddPostData("protocol", ToString(protocol));
                http.SetHeader("User-Agent", userAgent);

                if (!outDomain.empty())
                {
                    http.SetHeader("Host", outDomain);
                }

                if (!http.Post("/growtopia/server_data.php"))
                {
                    LogError("Failed to send post request to %s/growtopia/server_data.php, status: %d, errorCode: %d",
                             targetIP.c_str(), http.GetStatus(), http.GetError());
                    SendHTTPData("");
                    break;
                }

                string body = http.GetBody();

                if (body.empty())
                {
                    LogError("Failed post request, body is empty. status: %d, errorCode: %d", http.GetStatus(),
                             http.GetError());
                    SendHTTPData("");
                    break;
                }

                LogInfo("Fetched server data:\n%s", body.c_str());

                ParsedTextPacket<10> packet;
                ParseTextPacket(body.data(), body.size(), packet);

                auto pServer = packet.Find("server"_hash);
                auto pPort = packet.Find("port"_hash);

                uint32 port = 0;
                if ((!pServer || pServer->valueSize == 0) ||
                    (!pPort || pPort->value == 0 || pPort->GetUInt(port) != TO_INT_SUCCESS))
                {
                    LogError("Failed to parse server or port field from received data");
                    SendHTTPData("");
                    break;
                }

                m_lastAddress = pServer->GetString();
                m_lastPort = (uint16)port;

                SendHTTPData(body);
                break;
            }

            case TCP_PACKET_HTTP_TOGGLE:
            {
                int32 status = 0;
                reader.Read(status);

                m_httpClient.isHTTPServerActive = (status == 1);
                m_httpClient.isTogglePending = false;
                m_httpClient.toggleHasError = false;
                LogInfo("Received HTTP Proxy Status ACK: %d", status);
                break;
            }
        }
    }
}

void TCPServer::OnClientDisconnect(NetClient* pClient)
{
    if (!pClient)
        return;

    if (!m_httpClient.pClient || m_httpClient.pClient != pClient)
        return;

    m_httpClient.pClient = nullptr;
    m_httpClient.isReady = false;
    m_httpClient.isHTTPServerActive = true;
    m_httpClient.isTogglePending = false;
    m_httpClient.toggleHasError = false;
    m_httpClient.lastHeartBeat.Reset();
}

void TCPServer::RegisterEvents()
{
    m_pNetSocket->GetEvents().Register(
        SOCKET_EVENT_TYPE_CONNECT, Delegate<void, NetClient*>::Create<TCPServer, &TCPServer::OnClientConnect>(this));

    m_pNetSocket->GetEvents().Register(
        SOCKET_EVENT_TYPE_RECEIVE, Delegate<void, NetClient*>::Create<TCPServer, &TCPServer::OnClientReceive>(this));

    m_pNetSocket->GetEvents().Register(
        SOCKET_EVENT_TYPE_DISCONNECT,
        Delegate<void, NetClient*>::Create<TCPServer, &TCPServer::OnClientDisconnect>(this));
}

void TCPServer::Update()
{
    if (!m_pNetSocket)
        return;

    m_pNetSocket->Update(false);

    if (m_httpClient.isTogglePending && m_httpClient.togglePendingTimer.GetElapsedTime() > 5000)
    {
        m_httpClient.isTogglePending = false;
        m_httpClient.toggleHasError = true;
        LogError("HTTP Proxy Toggle Request timed out!");
    }
}

void TCPServer::SendHello()
{
    if (!m_httpClient.pClient || !m_httpClient.isReady)
        return;

    TCPPacketWriter writer(TCP_PACKET_HELLO);

    m_httpClient.pClient->Send(writer);
}

void TCPServer::SendHTTPData(const string& httpData)
{
    if (!m_httpClient.pClient || !m_httpClient.isReady)
        return;

    TCPPacketWriter writer(TCP_PACKET_HTTP_DATA);
    writer.WriteString(httpData);

    m_httpClient.pClient->Send(writer);
}

void TCPServer::SendToggleHTTPData(int32 enable)
{
    if (!m_httpClient.pClient || !m_httpClient.isReady)
        return;

    m_httpClient.isTogglePending = true;
    m_httpClient.toggleHasError = false;
    m_httpClient.togglePendingTimer.Reset();

    TCPPacketWriter writer(TCP_PACKET_HTTP_TOGGLE);
    writer.Write(enable);

    m_httpClient.pClient->Send(writer);
}

string ResolveDoH(const string& dohProvider, const string& domain, int32 depth)
{
    if (depth > 5)
        return "";

    NetHTTP http;
    http.Init(dohProvider);
    http.SetHeader("Accept", "application/dns-json");

    string path = "/dns-query?name=" + domain + "&type=A";

    if (http.Get(path) && http.GetStatus() == 200)
    {
        string jsonBody = http.GetBody();
        string foundCNAME;

        usize dataPos = 0;
        while ((dataPos = jsonBody.find("\"data\":\"", dataPos)) != string::npos)
        {
            dataPos += 8;
            usize endPos = jsonBody.find('"', dataPos);
            if (endPos == string::npos)
                break;

            string val(jsonBody.data() + dataPos, endPos - dataPos);
            if (!val.empty() && val.back() == '.')
            {
                val.pop_back();
            }

            uint32 ip = 0;
            if (ParseIPv4(val, ip))
            {
                if (!IsLocalOrInvalidIP(ip))
                    return val;
            }
            else if (foundCNAME.empty() && val.find('.') != string::npos)
            {
                foundCNAME = val;
            }
            dataPos = endPos;
        }

        if (!foundCNAME.empty())
        {
            return ResolveDoH(dohProvider, foundCNAME, depth + 1);
        }
    }
    return "";
}

string GetTargetServerIP(bool denyLocal, string& outDomain)
{
    std::vector<string> domains = {"www.growtopia2.com", "growtopia2.com"};
    std::vector<string> dohProviders = {"https://1.1.1.1", "https://8.8.8.8"};

    for (auto& domain : domains)
    {
        string osIP = ResolveDNSViaOS(domain);

        if (!osIP.empty())
        {
            if (!denyLocal || !IsLocalOrInvalidIP(osIP))
            {
                outDomain = domain;
                return osIP;
            }
        }
    }

    for (auto& domain : domains)
    {
        for (auto& provider : dohProviders)
        {
            string realIP = ResolveDoH(provider, domain);
            if (!realIP.empty())
            {
                outDomain = domain;
                return realIP;
            }
        }
    }

    return "";
}