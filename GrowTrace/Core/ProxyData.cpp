#include "ProxyData.h"
#include "../Server/ENetServer.h"
#include "Application.h"
#include "IO/File.h"
#include "Utils/StringUtils.h"

ProxyData gProxyData;

bool ProxyData::LoadWorld(const uint8* pData, usize size)
{
    if (!pData || size == 0)
        return false;

    MemoryBuffer memBuffer(pData, size);

    if (!worldInfo.Serialize(memBuffer, false, false, 5.56))
    {
        hasWorldLoaded = false;
        return false;
    }

    hasWorldLoaded = true;
    return true;
}

void ProxyData::UnloadWorld()
{
    hasWorldLoaded = false;
    worldInfo = WorldInfo();
}

bool ProxyData::SaveWorld(bool database, string* pOutPath)
{
    if (!hasWorldLoaded)
        return false;

    uint32 memEstimate = worldInfo.GetMemEstimate(database);
    MemoryBuffer memBuffer(memEstimate);

    if (!worldInfo.Serialize(memBuffer, true, database, 5.56))
        return false;

    string fileName = worldInfo.GetWorlName();
    if (fileName.empty())
        fileName = "EMPTY_WORLD_NAME";

    fileName += "_v" + ToString(worldInfo.GetWorldVersion()) + (database ? "_gtopia.bin" : "_orig.bin");

    File file;
    if (!file.Open(GetProgramPath() + "/" + fileName, FILE_MODE_WRITE))
        return false;

    if (file.Write(memBuffer.GetData(), memBuffer.GetOffset()) != memBuffer.GetOffset())
        return false;

    file.Close();

    if (pOutPath)
        *pOutPath = fileName;

    return true;
}

void ProxyData::UpdatePlayerPos(int32 netID, int32 posX, int32 posY)
{
    auto it = players.find(netID);
    if (it != players.end())
    {
        it->second.posX = posX;
        it->second.posY = posY;
    }
}

void ProxyData::ClearPlayers()
{
    players.clear();
    playerSession.netID = -1;
    playerSession.userID = 0;
    playerSession.name.clear();
}

bool SendENetPacketRaw(bool sendToRealServer, eMessagePacketType messageType, void* pData, uint32 dataSize,
                       uint8* pExtraData)
{
    ENetPeer* pPeer = sendToRealServer ? gENetClient.GetPeer() : gENetServer.GetPeer();
    if (!pPeer)
        return false;

    ENetPacket* pPacket = CreateENetPacketRaw(messageType, pData, dataSize, pExtraData);
    if (!pPacket)
        return false;

    CaptureAndPushToUI(sendToRealServer, pPacket, true, false);

    if (enet_peer_send(pPeer, 0, pPacket) != 0)
    {
        enet_packet_destroy(pPacket);
        return false;
    }

    return true;
}

bool SendENetPacket(bool sendToRealServer, eMessagePacketType messageType, const char* message)
{
    ENetPeer* pPeer = sendToRealServer ? gENetClient.GetPeer() : gENetServer.GetPeer();
    if (!pPeer)
        return false;

    ENetPacket* pPacket = CreateENetPacket(messageType, message);
    if (!pPacket)
        return false;

    CaptureAndPushToUI(sendToRealServer, pPacket, true, false);

    if (enet_peer_send(pPeer, 0, pPacket) != 0)
    {
        enet_packet_destroy(pPacket);
        return false;
    }

    return true;
}