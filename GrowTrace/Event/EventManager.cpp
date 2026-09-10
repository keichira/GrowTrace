#include "EventManager.h"
#include "../Core/Application.h"
#include "../Core/ProxyData.h"
#include "../Server/ENetServer.h"
#include "Item/ItemInfoManager.h"
#include "Packet/NetPacket.h"
#include "Proton/ProtonUtils.h"
#include "Utils/ZLibUtils.h"

EventManager::EventManager() {}

EventManager::~EventManager() {}

static bool Tank_CallFunction(ENetPeer* pPeer, GameUpdatePacket* pGamePacket)
{
    if (!pPeer || !pGamePacket)
        return true;

    uint8* pData = GetExtendedDataFromGamePacket(pGamePacket);
    if (!pData)
        return true;

    VariantVector varVec;
    if (!Proton::SerializeFromMem(pData, pGamePacket->extraDataSize, varVec))
        return true;

    if (varVec.empty())
        return true;

    const string& packetType = varVec[0].GetString();

    switch (HashString(packetType))
    {
        case "OnSendToServer"_hash:
        {
            if (varVec.size() >= 5)
            {
                uint16 targetPort = varVec[1].GetINT();
                string rawAddress = varVec[4].GetString();

                string targetIp = rawAddress;
                string extraParams = "";

                usize pipePos = rawAddress.find('|');
                if (pipePos != string::npos)
                {
                    targetIp = rawAddress.substr(0, pipePos);
                    extraParams = rawAddress.substr(pipePos);
                }

                gENetServer.SetReconnectTarget(targetIp, targetPort);
                gENetServer.SetReconnecting(true);

                LogInfo("[OnSendToServer] Original Target -> %s:%d", targetIp.c_str(), targetPort);

                varVec[1] = (int32)19000;
                varVec[4] = "127.0.0.1" + extraParams;

                uint32 newMemSize = 0;
                uint8* pNewMem = Proton::SerializeToMem(varVec, &newMemSize, nullptr);

                if (pNewMem)
                {
                    GameUpdatePacket packet = *pGamePacket;
                    packet.extraDataSize = newMemSize;

                    SendENetPacketRaw(false, eMessagePacketType::NET_MESSAGE_GAME_PACKET, &packet,
                                      sizeof(GameUpdatePacket), pNewMem);

                    SAFE_DELETE_ARRAY(pNewMem);
                }

                return false;
            }

            return true;
        }

        case "OnSuperMainStartAcceptLogonFB211131d"_hash:
        case "OnSuperMainStartAcceptLogonFB211131dd"_hash:
        case "OnSuperMainStartAcceptLogonFB211131ddf"_hash:
        case "OnSuperMainStartAcceptLogonHrdxs47254722215a"_hash:
        {
            if (varVec.size() >= 2)
            {
                uint32 realServerItemHash = varVec[1].GetUINT();

                if (GetItemInfoManager()->GetHash() != realServerItemHash)
                {
                    GetItemInfoManager()->SetHash(realServerItemHash);
                    LogInfo("[ItemData] Local cache missing or outdated (Server Hash: %u). Spoofing client hash to "
                            "force refresh.",
                            realServerItemHash);

                    varVec[1] = (uint32)0;

                    uint32 newMemSize = 0;
                    uint8* pNewMem = Proton::SerializeToMem(varVec, &newMemSize, nullptr);
                    if (pNewMem)
                    {
                        GameUpdatePacket packet = *pGamePacket;
                        packet.extraDataSize = newMemSize;

                        SendENetPacketRaw(false, eMessagePacketType::NET_MESSAGE_GAME_PACKET, &packet,
                                          sizeof(GameUpdatePacket), pNewMem);

                        SAFE_DELETE_ARRAY(pNewMem);
                        return false;
                    }
                }
                else
                {
                    LogInfo("[ItemData] Proxy cache is valid....");
                }
            }
            return true;
        }

        case "OnSpawn"_hash:
        {
            if (varVec.size() >= 2)
            {
                ParsedTextPacket<16> spawnPacket;

                const string& spawnData = varVec[1].GetString();
                ParseTextPacket(spawnData.c_str(), spawnData.size(), spawnPacket);

                bool isLocal = false;

                if (auto pType = spawnPacket.Find("type"_hash))
                {
                    if (pType->GetStringView() == "local")
                        isLocal = true;
                }

                ProxyPlayerInfo player;

                if (auto pNetID = spawnPacket.Find("netID"_hash))
                {
                    if (pNetID->GetInt(player.netID) != TO_INT_SUCCESS)
                    {
                        LogError("Failed to get netID from OnSpawn");
                        return true;
                    }
                }

                if (auto pUserID = spawnPacket.Find("userID"_hash))
                {
                    if (pUserID->GetInt(player.userID) != TO_INT_SUCCESS)
                    {
                        LogError("Failed to get userID from Onspawn");
                    }
                }

                if (auto pName = spawnPacket.Find("name"_hash))
                {
                    player.name = pName->GetString();
                }

                if (player.netID != -1)
                {
                    if (isLocal)
                    {
                        gProxyData.playerSession.netID = player.netID;
                        gProxyData.playerSession.userID = player.userID;
                        gProxyData.playerSession.name = player.name;
                    }
                    gProxyData.players[player.netID] = player;
                }
            }
            return true;
        }

        case "OnRemove"_hash:
        {
            if (varVec.size() >= 2)
            {
                ParsedTextPacket<2> removePacket;

                const string& removeData = varVec[1].GetString();
                ParseTextPacket(removeData.c_str(), removeData.size(), removePacket);

                int32 netID = -1;

                if (auto pNetID = removePacket.Find("netID"_hash))
                {
                    if (pNetID->GetInt(netID) != TO_INT_SUCCESS)
                        return true;
                }

                if (netID != -1)
                {
                    if (gProxyData.playerSession.netID != -1 && netID == gProxyData.playerSession.netID == netID)
                    {
                        gProxyData.ClearPlayers();
                    }
                    else
                    {
                        gProxyData.players.erase(netID);
                    }
                }
            }
            return true;
        }
    }

    return true;
}

static bool Tank_SendItemDatabaseData(ENetPeer* pPeer, GameUpdatePacket* pGamePacket)
{
    if (!pGamePacket)
        return true;

    uint8* pData = GetExtendedDataFromGamePacket(pGamePacket);
    uint32 dataSize = pGamePacket->extraDataSize;

    if (pData && dataSize > 0)
    {
        LogInfo("[ItemData] Intercepted items.dat payload (%u bytes). Parsing...", dataSize);

        bool succeed = false;

        if (pGamePacket->field_7 != 0)
        {
            uint8* pInflateData = zLibInflateToMemory(pData, dataSize, pGamePacket->field_7);
            if (!pInflateData)
            {
                LogError("[ItemData] Failed to decompress.");
                return true;
            }

            succeed = GetItemInfoManager()->LoadRaw(pInflateData, pGamePacket->field_7);
            SAFE_DELETE_ARRAY(pInflateData);
        }
        else
            succeed = GetItemInfoManager()->LoadRaw(pData, dataSize);

        if (succeed)
        {
            LogInfo("[ItemData] Successfully parsed %d items into proxy memory!", GetItemInfoManager()->GetItemCount());
        }
        else
        {
            LogError("[ItemData] Failed to parse items.dat!");
        }
    }

    return true;
}

static bool Tank_SendMapData(ENetPeer* pPeer, GameUpdatePacket* pGamePacket)
{
    if (!pGamePacket)
        return true;

    uint8* pData = GetExtendedDataFromGamePacket(pGamePacket);
    if (pData && pGamePacket->extraDataSize > 0)
    {
        if (gProxyData.LoadWorld(pData, pGamePacket->extraDataSize))
        {
            LogInfo("[World] Successfully loaded map for: %s", gProxyData.worldInfo.GetWorlName().c_str());
        }
        else
        {
            LogError("[World] Failed to load map data.");
        }
    }

    return true;
}

static bool Tank_State(ENetPeer* pPeer, GameUpdatePacket* pGamePacket)
{
    if (!pGamePacket)
        return true;

    int32 netID = pGamePacket->field_5;
    int32 posX = (int32)(pGamePacket->field_8.x / 32.0f);
    int32 posY = (int32)(pGamePacket->field_8.y / 32.0f);

    gProxyData.UpdatePlayerPos(netID, posX, posY);
    return true;
}

static bool Tank_ItemChangeObject(ENetPeer* pPeer, GameUpdatePacket* pGamePacket)
{
    if (!pGamePacket)
        return true;

    if (gProxyData.hasWorldLoaded && gProxyData.worldInfo.GetObjectManager())
    {
        gProxyData.worldInfo.GetObjectManager()->HandleObjectPackets(pGamePacket);
    }
    return true;
}

void EventManager::RegisterEvents()
{
    RegisterTank<Tank_CallFunction>(NET_GAME_PACKET_CALL_FUNCTION);
    RegisterTank<Tank_SendItemDatabaseData>(NET_GAME_PACKET_SEND_ITEM_DATABASE_DATA);
    RegisterTank<Tank_SendMapData>(NET_GAME_PACKET_SEND_MAP_DATA);
    RegisterTank<Tank_State>(NET_GAME_PACKET_STATE);
    RegisterTank<Tank_ItemChangeObject>(NET_GAME_PACKET_ITEM_CHANGE_OBJECT);
}

bool EventManager::ProcessIncomingPacket(ENetPeer* pPeer, ENetPacket* pOriginalPacket)
{
    if (!pOriginalPacket || pOriginalPacket->dataLength < 4)
        return true;

    uint32 msgType = GetMessageTypeFromEnetPacket(pOriginalPacket->data, pOriginalPacket->dataLength);

    switch (msgType)
    {
        case NET_MESSAGE_GENERIC_TEXT:
        case NET_MESSAGE_GAME_MESSAGE:
        {
            const char* textData = GetTextFromEnetPacket(pOriginalPacket->data, pOriginalPacket->dataLength);
            if (!textData)
                break;

            string strText(textData);
            if (strText.find("action|quit") != string::npos ||
                strText.find("action|quit_to_exit") != string::npos) // little hack
            {
                gProxyData.ClearPlayers();
            }

            uint32 textLen = (uint32)(pOriginalPacket->dataLength - 4);
            ParsedTextPacket<MAX_TEXT_PACKET_FIELDS> parsedPacket;
            ParseTextPacket<MAX_TEXT_PACKET_FIELDS>(textData, textLen, parsedPacket);

            if (TextPacketField* pAction = parsedPacket.Find(HashString("action", 6)))
            {
                uint32 hashAction = HashString(pAction->value, pAction->valueSize);
                if (!m_textDispatcher.HasHandler(hashAction))
                    return true;

                return m_textDispatcher.Dispatch(hashAction, pPeer, parsedPacket);
            }

            return true;
        }

        case NET_MESSAGE_GAME_PACKET:
        {
            GameUpdatePacket* pGamePacket =
                GetGamePacketFromEnetPacket(pOriginalPacket->data, pOriginalPacket->dataLength);
            if (!pGamePacket)
                return true;

            if (!m_tankDispatcher.HasHandler(pGamePacket->type))
                return true;

            return m_tankDispatcher.Dispatch(pGamePacket->type, pPeer, pGamePacket);
        }

        default:
            break;
    }

    return true;
}

EventManager* GetEventManager()
{
    return EventManager::GetInstance();
}