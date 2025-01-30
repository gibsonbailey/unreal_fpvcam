#include "CameraDataStreamer.h"
#include "CameraDataStreamerRunnable.h"
#include "MyVRPawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"

UCameraDataStreamer::UCameraDataStreamer()
    : StreamerRunnable(nullptr), StreamerThread(nullptr)
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCameraDataStreamer::BeginPlay()
{
    Super::BeginPlay();

    // Start the worker thread
    StreamerRunnable = new FCameraDataStreamerRunnable(&DataQueue);
    StreamerThread = FRunnableThread::Create(StreamerRunnable, TEXT("CameraDataStreamerThread"));
    
    // Get camera rotation
    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    
    // Initialize Yaw
    if (PlayerController && PlayerController->PlayerCameraManager)
    {
        FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
        PreviousYaw = CameraRotation.Yaw;
        PreviousPitch = CameraRotation.Pitch;
        AccumulatedYaw = 0.0f;
        AccumulatedPitch = 0.0f;
    }
}

void UCameraDataStreamer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Stop the worker thread
    if (StreamerRunnable)
    {
        StreamerRunnable->Stop();
        StreamerThread->WaitForCompletion();

        delete StreamerRunnable;
        StreamerRunnable = nullptr;

        delete StreamerThread;
        StreamerThread = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UCameraDataStreamer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    TimeSinceLastSend += DeltaTime;
    
    if (TimeSinceLastSend >= SendInterval) {
        TimeSinceLastSend = 0;
        
        if (StreamerRunnable)
        {
            // Get camera rotation
            APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

            if (PlayerController && PlayerController->PlayerCameraManager)
            {
                FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
                
                float DeltaYaw = CameraRotation.Yaw - PreviousYaw;
                float DeltaPitch = CameraRotation.Pitch - PreviousPitch;
                
                if (DeltaYaw > 180.0f) {
                    DeltaYaw -= 360.0f;
                } else if (DeltaYaw < -180.f) {
                    DeltaYaw += 360.0f;
                }

                PreviousYaw = CameraRotation.Yaw;
                PreviousPitch = CameraRotation.Pitch;

                UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
                if (Subsystem)
                {

                    // Do not accumulate the yaw and pitch if the left X button is pressed
                    // This allows the user to manually recalibrate the camera
                    if (!Subsystem->GetPlayerInput()->GetActionValue(IA_Pause_Camera_Motors).Get<bool>())
                    {
                        AccumulatedYaw += DeltaYaw;
                        AccumulatedPitch += DeltaPitch;
                    }

                    // Get right trigger value
                    FInputActionValue CurrentRightTriggerPositionValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Hand_IndexCurl_Right);
                    CachedRightIndexCurlValue = CurrentRightTriggerPositionValue.Get<float>();

                    // Get left trigger value
                    FInputActionValue CurrentLeftTriggerPositionValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Hand_IndexCurl_Left);
                    float CachedLeftIndexCurlValue = CurrentLeftTriggerPositionValue.Get<float>();

                    // Left and right trigger values cancel each other out
                    CachedRightIndexCurlValue -= CachedLeftIndexCurlValue;

                    // IA_Turbo_Throttle
                    FInputActionValue AButtonPressed = Subsystem->GetPlayerInput()->GetActionValue(IA_Turbo_Throttle);
                    CachedRightThumbUpValue = AButtonPressed.Get<bool>();

                    // If A button is not pressed, then depress throttle
                    if (!CachedRightThumbUpValue) {
                       CachedRightIndexCurlValue *= .45f;
                    }

                    FInputActionValue CurrentThumbstickValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Hand_Thumbstick_Right);
                    CachedRightThumbstickValue = CurrentThumbstickValue.Get<float>();
                }

                // Enqueue the data
                DataQueue.Enqueue(new FRobotControlData(AccumulatedPitch, AccumulatedYaw, CachedRightIndexCurlValue, CachedRightThumbstickValue));
                
                UE_LOG(LogTemp, Warning, TEXT("RightIndexCurlValue: %f"), CachedRightIndexCurlValue);
            }
        }
    }
}

float UCameraDataStreamer::GetAccumulatedYaw() const
{
    return AccumulatedYaw;
}

float UCameraDataStreamer::GetAccumulatedPitch() const
{
    return AccumulatedPitch;
}
