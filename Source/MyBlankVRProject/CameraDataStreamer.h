#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Queue.h"
#include "CameraDataStreamer.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYBLANKVRPROJECT_API UCameraDataStreamer : public UActorComponent
{
    GENERATED_BODY()

public:
    UCameraDataStreamer();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    class FCameraDataStreamerRunnable* StreamerRunnable;
    FRunnableThread* StreamerThread;

    TQueue<FString*, EQueueMode::Spsc> DataQueue;
    
    float TimeSinceLastSend = 0.0f;
    float SendInterval = 0.001f;
    
    // Used to get rid of discontinuity at 0 - 360 degrees.
    float PreviousYaw = 0.0f; // initialized in beginPlay
    float AccumulatedYaw = 0.0f;
};
