#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "CameraDataStreamer.h"

class FCameraDataStreamerRunnable : public FRunnable
{
public:
    FCameraDataStreamerRunnable(TQueue<FCameraAngles*, EQueueMode::Spsc>* InDataQueue);
    virtual ~FCameraDataStreamerRunnable();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;

private:
    FThreadSafeBool bStopThread;
    TQueue<FCameraAngles*, EQueueMode::Spsc>* DataQueue;

    // Socket variables
    FSocket* Socket;
    FString ServerIP;
    int32 ServerPort;

    // Functions to manage socket
    bool InitializeSocket();
    void DeconstructSocket();
};
