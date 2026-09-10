#include "NetPacket.h"
#include "../Proton/ProtonUtils.h"

const char* GetMessageTypeName(eMessagePacketType type)
{
    switch (type)
    {
        case NET_MESSAGE_SERVER_HELLO:
            return "SERVER_HELLO";
        case NET_MESSAGE_GENERIC_TEXT:
            return "GENERIC_TEXT";
        case NET_MESSAGE_GAME_MESSAGE:
            return "GAME_MESSAGE";
        case NET_MESSAGE_GAME_PACKET:
            return "GAME_PACKET";
        case NET_MESSAGE_ERROR:
            return "ERROR";
        case NET_MESSAGE_TRACK:
            return "TRACK";
        case NET_MESSAGE_CLIENT_LOG_REQUEST:
            return "CLIENT_LOG_REQUEST";
        case NET_MESSAGE_CLIENT_LOG_RESPONSE:
            return "CLIENT_LOG_RESPONSE";
        default:
            return "UNKNOWN";
    }
}

const char* GetGamePacketTypeName(eGamePacketType type)
{
    switch (type)
    {
        case NET_GAME_PACKET_STATE:
            return "STATE";
        case NET_GAME_PACKET_CALL_FUNCTION:
            return "CALL_FUNCTION";
        case NET_GAME_PACKET_UPDATE_STATUS:
            return "UPDATE_STATUS";
        case NET_GAME_PACKET_TILE_CHANGE_REQUEST:
            return "TILE_CHANGE_REQ";
        case NET_GAME_PACKET_SEND_MAP_DATA:
            return "SEND_MAP_DATA";
        case NET_GAME_PACKET_SEND_TILE_UPDATE_DATA:
            return "SEND_TILE_UPDATE";
        case NET_GAME_PACKET_SEND_TILE_UPDATE_DATA_MULTIPLE:
            return "SEND_TILE_UPDATE_MULTI";
        case NET_GAME_PACKET_TILE_ACTIVATE_REQUEST:
            return "TILE_ACTIVATE_REQ";
        case NET_GAME_PACKET_TILE_APPLY_DAMAGE:
            return "TILE_APPLY_DAMAGE";
        case NET_GAME_PACKET_SEND_INVENTORY_STATE:
            return "SEND_INVENTORY";
        case NET_GAME_PACKET_ITEM_ACTIVATE_REQUEST:
            return "ITEM_ACTIVATE_REQ";
        case NET_GAME_PACKET_ITEM_ACTIVATE_OBJECT_REQUEST:
            return "ITEM_ACTIVATE_OBJ_REQ";
        case NET_GAME_PACKET_SEND_TILE_TREE_STATE:
            return "SEND_TREE_STATE";
        case NET_GAME_PACKET_MODIFY_ITEM_INVENTORY:
            return "MODIFY_INVENTORY";
        case NET_GAME_PACKET_ITEM_CHANGE_OBJECT:
            return "ITEM_CHANGE_OBJ";
        case NET_GAME_PACKET_SEND_LOCK:
            return "SEND_LOCK";
        case NET_GAME_PACKET_SEND_ITEM_DATABASE_DATA:
            return "SEND_ITEM_DATABASE";
        case NET_GAME_PACKET_SEND_PARTICLE_EFFECT:
            return "SEND_PARTICLE";
        case NET_GAME_PACKET_SET_ICON_STATE:
            return "SET_ICON_STATE";
        case NET_GAME_PACKET_ITEM_EFFECT:
            return "ITEM_EFFECT";
        case NET_GAME_PACKET_SET_CHARACTER_STATE:
            return "SET_CHARACTER_STATE";
        case NET_GAME_PACKET_PING_REPLY:
            return "PING_REPLY";
        case NET_GAME_PACKET_PING_REQUEST:
            return "PING_REQUEST";
        case NET_GAME_PACKET_GOT_PUNCHED:
            return "GOT_PUNCHED";
        case NET_GAME_PACKET_APP_CHECK_RESPONSE:
            return "APP_CHECK_RESP";
        case NET_GAME_PACKET_APP_INTEGRITY_FAIL:
            return "APP_INTEGRITY_FAIL";
        case NET_GAME_PACKET_DISCONNECT:
            return "DISCONNECT";
        case NET_GAME_PACKET_BATTLE_JOIN:
            return "BATTLE_JOIN";
        case NET_GAME_PACKET_BATTLE_EVENT:
            return "BATTLE_EVENT";
        case NET_GAME_PACKET_USE_DOOR:
            return "USE_DOOR";
        case NET_GAME_PACKET_SEND_PARENTAL:
            return "SEND_PARENTAL";
        case NET_GAME_PACKET_GONE_FISHIN:
            return "GONE_FISHIN";
        case NET_GAME_PACKET_STEAM:
            return "STEAM";
        case NET_GAME_PACKET_PET_BATTLE:
            return "PET_BATTLE";
        case NET_GAME_PACKET_NPC:
            return "NPC";
        case NET_GAME_PACKET_SPECIAL:
            return "SPECIAL";
        case NET_GAME_PACKET_SEND_PARTICLE_EFFECT_V2:
            return "SEND_PARTICLE_V2";
        case NET_GAME_PACKET_ACTIVE_ARROW_TO_ITEM:
            return "ACTIVE_ARROW";
        case NET_GAME_PACKET_SELEC_TILE_INDEX:
            return "SELECT_TILE_INDEX";
        case NET_GAME_PACKET_SEND_PLAYER_TRIBUTE_DATA:
            return "SEND_TRIBUTE_DATA";
        case NET_GAME_PACKET_ON_STEP_ONILE_MOD:
            return "ON_STEP_ONILE_MOD";
        default:
            return "UNKNOWN_GAME_PACKET";
    }
}

ENetPacket* CreateENetPacketRaw(eMessagePacketType messageType, void* pData, uint32 dataSize, uint8* pExtraData)
{
    if (!pData && dataSize > 0)
        return nullptr;

    usize totalSize = 4 + dataSize;
    bool hasExtendedData = (messageType == NET_MESSAGE_GAME_PACKET) && pData &&
                           (((GameUpdatePacket*)pData)->flags & GAME_PACKET_FLAG_EXTENDED_DATA);

    if (hasExtendedData && pExtraData)
    {
        totalSize += ((GameUpdatePacket*)pData)->extraDataSize;
    }

    ENetPacket* pPacket = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);
    if (!pPacket)
        return nullptr;

    memcpy(pPacket->data, &messageType, 4);

    if (dataSize > 0)
        memcpy(pPacket->data + 4, pData, dataSize);

    if (hasExtendedData && pExtraData)
    {
        memcpy(pPacket->data + 4 + dataSize, pExtraData, ((GameUpdatePacket*)pData)->extraDataSize);
    }

    return pPacket;
}

ENetPacket* CreateENetPacket(eMessagePacketType messageType, const char* message)
{
    usize strLen = message ? strlen(message) : 0;
    usize totalSize = 4 + strLen + 1;

    ENetPacket* pPacket = enet_packet_create(nullptr, totalSize, ENET_PACKET_FLAG_RELIABLE);
    if (!pPacket)
        return nullptr;

    memcpy(pPacket->data, &messageType, 4);
    if (strLen > 0)
        memcpy(pPacket->data + 4, message, strLen);

    return pPacket;
}

const char* GetTextFromEnetPacket(uint8* pData, uint32 dataLength)
{
    if (!pData || dataLength < 4)
        return "";

    if (pData[dataLength - 1] != 0)
        pData[dataLength - 1] = 0;

    return (const char*)pData + 4;
}

uint32 GetMessageTypeFromEnetPacket(uint8* pData, uint32 dataLength)
{
    if (dataLength < 4)
        return 0;

    return *(uint32*)pData;
}

GameUpdatePacket* GetGamePacketFromEnetPacket(uint8* pData, uint32 dataLength, bool checkExtra)
{
    if (dataLength < sizeof(GameUpdatePacket))
        return nullptr;

    GameUpdatePacket* pGamePacket = (GameUpdatePacket*)(pData + 4);
    if (!(pGamePacket->flags & GAME_PACKET_FLAG_EXTENDED_DATA))
    {
        pGamePacket->extraDataSize = 0;
    }
    else if (checkExtra && dataLength < pGamePacket->extraDataSize + sizeof(pGamePacket))
        return nullptr;

    return pGamePacket;
}

uint8* GetExtendedDataFromGamePacket(GameUpdatePacket* pUpdatePacket)
{
    if (!(pUpdatePacket->flags & GAME_PACKET_FLAG_EXTENDED_DATA))
        return nullptr;

    return (uint8*)pUpdatePacket + sizeof(GameUpdatePacket);
}
