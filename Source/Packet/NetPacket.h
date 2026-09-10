#pragma once

#include "../Precompiled.h"
#include "GamePacket.h"
#include <concurrentqueue.h>
#include <enet/enet.h>

ENetPacket* CreateENetPacketRaw(eMessagePacketType messageType, void* pData, uint32 dataSize, uint8* pExtraData);
ENetPacket* CreateENetPacket(eMessagePacketType messageType, const char* message);

const char* GetTextFromEnetPacket(uint8* pData, uint32 dataLength);
uint32 GetMessageTypeFromEnetPacket(uint8* pData, uint32 dataLength);
GameUpdatePacket* GetGamePacketFromEnetPacket(uint8* pData, uint32 dataLength, bool checkExtra = true);
uint8* GetExtendedDataFromGamePacket(GameUpdatePacket* pUpdatePacket);