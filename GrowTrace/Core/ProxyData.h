#pragma once

#include "Packet/NetPacket.h"
#include "Precompiled.h"
#include "Proton/ProtonUtils.h"
#include "World/WorldInfo.h"

class ProxyData;
extern ProxyData gProxyData;

bool SendENetPacketRaw(bool sendToRealServer, eMessagePacketType messageType, void* pData, uint32 dataSize,
                       uint8* pExtraData);
bool SendENetPacket(bool sendToRealServer, eMessagePacketType messageType, const char* message);

struct ProxySettings
{
    bool localServerMode = false;
};

struct PlayerSessionState
{
    string name;
    int32 netID = -1;
    uint32 userID = 0;
};

struct ProxyPlayerInfo
{
    int32 netID = -1;
    int32 userID = -1;
    string name;
    int32 posX = 0;
    int32 posY = 0;
};

class ProxyData
{
public:
    ProxyData() = default;
    ~ProxyData() = default;

    bool LoadWorld(const uint8* pData, usize size);
    void UnloadWorld();
    bool SaveWorld(bool database, string* pOutPath = nullptr);

    void ParseOnSpawn(const string& spawnData);
    void ParseOnRemove(const Variant& var);
    void UpdatePlayerPos(int32 netID, int32 posX, int32 posY);
    void ClearPlayers();

    bool HasActiveLocalPlayer() const { return playerSession.netID != -1; }

public:
    ProxySettings proxySettings;
    PlayerSessionState playerSession;
    WorldInfo worldInfo;
    bool hasWorldLoaded = false;
    std::unordered_map<int32, ProxyPlayerInfo> players;
};