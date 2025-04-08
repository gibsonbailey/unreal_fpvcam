// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "FFmpegWorker.h"
#include "DynamicTextureActor.generated.h"

UCLASS()
class MYBLANKVRPROJECT_API ADynamicTextureActor : public AActor
{
  	GENERATED_BODY()
	
public:	
	  // Sets default values for this actor's properties
	  ADynamicTextureActor();
    
    UPROPERTY(Transient)
    UTexture2D* DynamicTexture;

    UPROPERTY(Transient)
    UStaticMeshComponent* PlaneMesh; // The plane to apply the texture to

    // Callback for ffmpeg frame
    void OnNewFrameAvailable();

    int InitializeUDPVideoStream();
    void FFMpegCleanup();

    // ffmpeg
    AVFormatContext* formatContext;
    AVIOContext* avio_ctx;
    struct SwsContext* swsCtx;
    AVCodecContext* codecContext;
    AVFrame* frame;
    AVFrame* latest_frame;
    AVPacket* packet;
    
    int texture_width;
    int texture_height;
    
    int videoStreamIndex;
    
    bool stream_initialized;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FFmpegWorker* FFmpegWorkerInstance;
    FRunnableThread* Thread;

    FThreadSafeBool bHasNewFrame;
    FCriticalSection NewFrameLock;
    uint8* PendingFrameData;
    int PendingFrameSize;

    void UpdateTexture(uint8_t* img_data, int num_bytes);
    void Tick(float delta_time);
};
