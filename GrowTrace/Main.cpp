#include "Core/Application.h"
#include "Server/ENetServer.h"
#include "Server/TCPServer.h"
#include <thread>

void NetworkThreadFunc()
{
    Application* pApp = GetApp();

    LogInfo("Network thread started");

    while (pApp->IsRunning())
    {
        OutgoingPacket outPkt;
        while (gOutgoingQueue.try_dequeue(outPkt))
        {
            if (outPkt.targetPeer && outPkt.targetPeer->state == ENET_PEER_STATE_CONNECTED)
            {
                ENetPacket* pPacket = enet_packet_create(outPkt.data.data(), outPkt.data.size(), outPkt.flags);
                if (enet_peer_send(outPkt.targetPeer, outPkt.channelID, pPacket) != 0)
                    enet_packet_destroy(pPacket);
            }
        }

        gTCPServer.Update();
        gENetServer.Update(1);
        gENetClient.Update(1);

        pApp->ProcessNetworkEvents();

        SleepMS(1);
    }

    LogWarn("Network thread stopped");
}

int main(int argc, char const* argv[])
{
    Application* pApp = GetApp();

    if (!pApp->Init(1280, 720, "GrowTrace"))
    {
        LogError("Failed to initialize app.");
        return 0;
    }
    LogInfo("Initialized app.");

    if (!gTCPServer.Init("127.0.0.1", 19500))
    {
        LogError("Failed to init TCP server.");
        return 0;
    }
    LogInfo("Started TCP Server on 127.0.0.1:19500");

    if (enet_initialize() != 0)
    {
        LogError("Failed to initialize ENet.");
        return 0;
    }
    atexit(enet_deinitialize);

    if (!gENetServer.Init(ENetServer::Mode::Server, "127.0.0.1", 19000))
    {
        LogError("Failed to initialize ENet Server.");
        return 0;
    }
    LogInfo("Started ENet Server on 127.0.0.1:19000");

    if (!gENetClient.Init(ENetServer::Mode::Client))
    {
        LogError("Failed to initialize ENet Client.");
        return 0;
    }
    LogInfo("Initialized ENet Client");

    std::thread networkThread(NetworkThreadFunc);

    pApp->Run();
    pApp->Shutdown();

    if (networkThread.joinable())
        networkThread.join();

    return 0;
}