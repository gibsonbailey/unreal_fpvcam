// FFmpegWorker.h
#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "DynamicTextureActor.h"

class FFmpegWorker : public FRunnable
{
public:
    FFmpegWorker(ADynamicTextureActor* InOwner);
    virtual ~FFmpegWorker();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;

    // Thread-safe method to retrieve frame data
    bool GetLatestFrame(uint8*& OutData, int& OutSize);

private:
    ADynamicTextureActor* Owner;
    FRunnableThread* Thread;
    FThreadSafeBool bStopThread;
    
    // Frame data
    FCriticalSection FrameDataLock;
    uint8* LatestFrameData;
    int FrameDataSize;
};
