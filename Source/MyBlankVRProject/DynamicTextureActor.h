// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
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

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FTimerHandle TimerHandle;
    
    // ffmpeg
    AVFormatContext* formatContext;
    struct SwsContext* swsCtx;
    AVCodecContext* codecContext;
    AVFrame* frame;
    AVFrame* latest_frame;
    AVPacket* packet;
    
    int texture_width;
    int texture_height;
    
    int videoStreamIndex;
    
    bool stream_initialized;

    void UpdateTexture(uint8_t* img_data, int num_bytes);
    int InitializeUDPVideoStream();
    void Tick(float delta_time);
};
