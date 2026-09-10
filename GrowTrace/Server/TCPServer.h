#pragma once

#include "Network/NetSocket.h"
#include "Utils/Timer.h"

class TCPServer;
extern TCPServer gTCPServer;

string ResolveDoH(const string& dohProvider, const string& domain, int32 depth = 0);
string GetTargetServerIP(bool denyLocal, string& outDomain);

struct TCPClient
{
    NetClient* pClient = nullptr;
    Timer lastHeartBeat;
    bool isReady = false;

    bool isHTTPServerActive = false;
    bool isTogglePending = false;
    bool toggleHasError = false;
    Timer togglePendingTimer;
};

class TCPServer
{
public:
    TCPServer();
    ~TCPServer();

public:
    bool Init(const string& host, uint16 port, int32 backLog = 50);
    void Kill();
    void OnClientConnect(NetClient* pClient);
    void OnClientReceive(NetClient* pClient);
    void OnClientDisconnect(NetClient* pClient);
    void RegisterEvents();
    void Update();

    void SendHello();
    void SendHTTPData(const string& httpData);
    void SendToggleHTTPData(int32 enable);

    const string& GetLastAddress() const { return m_lastAddress; }
    uint16 GetLastPort() const { return m_lastPort; }

    bool IsHTTPConnected() { return (m_httpClient.pClient != nullptr); };
    bool ISHTTPReady() { return (m_httpClient.pClient && m_httpClient.isReady); }
    uint64 GetHTTPLastHeartbeatElapsedMS() { return m_httpClient.lastHeartBeat.GetElapsedTime(); }
    bool IsHTTPServerActive() const { return m_httpClient.isHTTPServerActive; }
    bool IsTogglePending() const { return m_httpClient.isTogglePending; }
    bool HasToggleError() const { return m_httpClient.toggleHasError; }

private:
    NetSocket* m_pNetSocket;
    TCPClient m_httpClient;

    string m_lastAddress;
    uint16 m_lastPort;
};