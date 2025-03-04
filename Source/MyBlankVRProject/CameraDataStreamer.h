#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Components/ActorComponent.h"
#include "Containers/Queue.h"
#include "CameraDataStreamer.generated.h"


struct FRobotControlData
{
    float Pitch;
    float Yaw;

    float TriggerPosition;
    float ThumbstickX;

    // Constructor for convenience
    FRobotControlData(float InPitch = 0.0f, float InYaw = 0.0f, float InTriggerPosition = 0.0f, float InThumbstickX = 0.0f)
        : Pitch(InPitch), Yaw(InYaw), TriggerPosition(InTriggerPosition), ThumbstickX(InThumbstickX) {}
};


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

    float SpeedMph = 0;
    float DistanceFeet = 0;
    int ControlBatteryPercentage = 0;
    int DriveBatteryPercentage = 0;

    UFUNCTION(BlueprintCallable, Category = "Camera Data Streamer")
    float GetAccumulatedYaw() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Data Streamer")
    float GetAccumulatedPitch() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Data Streamer")
    float GetSpeedMph() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Data Streamer")
    float GetDistanceFeet() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Data Streamer")
    int GetControlBatteryPercentage() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Data Streamer")
    int GetDriveBatteryPercentage() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* IA_Hand_IndexCurl_Right;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* IA_Hand_IndexCurl_Left;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* IA_Hand_Thumbstick_Right;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* IA_Turbo_Throttle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* IA_Pause_Camera_Motors;

private:
    class FCameraDataStreamerRunnable* StreamerRunnable;
    FRunnableThread* StreamerThread;

    TQueue<FRobotControlData*, EQueueMode::Spsc> DataQueue;
    
    float TimeSinceLastSend = 0.0f;
    float SendInterval = 0.02f;
    
    // Used to get rid of discontinuity at 0 - 360 degrees.
    float PreviousYaw = 0.0f; // initialized in beginPlay
    float AccumulatedYaw = 0.0f;

    float PreviousPitch = 0.0f; // initialized in beginPlay
    float AccumulatedPitch = 0.0f;

    float CachedRightIndexCurlValue = 0.0f;
    float CachedRightThumbstickValue = 0.0f;
    bool CachedRightThumbUpValue = false;
};
