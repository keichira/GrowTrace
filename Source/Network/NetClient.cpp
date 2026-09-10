#include "NetClient.h"
#include "NetSocket.h"
#include "../Packet/TCPPacket.h"

bool NetClient::Send(TCPPacketWriter& data)
{
    if (socket < 0 || !pNetSocket)
        return false;

    uint32 totalSize = 0;
    uint8* pData = data.Finalize(totalSize);

    if (!pData || totalSize == 0)
        return false;

    return pNetSocket->Send(this, pData, totalSize);
}