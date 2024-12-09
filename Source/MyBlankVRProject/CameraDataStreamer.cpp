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

        if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
        {
            if (IA_Hand_IndexCurl_Right)
            {
                EnhancedInputComponent->BindAction(IA_Hand_IndexCurl_Right, ETriggerEvent::Triggered, this, &UCameraDataStreamer::HandleRightTriggerInput);
            }
        }
    }
}


void UCameraDataStreamer::HandleRightTriggerInput(const FInputActionValue& Value)
{
    float RightIndexCurlValue = Value.Get<float>(); // Extract the trigger value
    UE_LOG(LogTemp, Warning, TEXT("Right Trigger Value in Actor: %f"), RightIndexCurlValue);
    CachedRightIndexCurlValue = RightIndexCurlValue;
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
                
                // Do not accumulate the yaw and pitch if the right index trigger is pressed
                if(CachedRightIndexCurlValue < 0.1f)
                {
                    AccumulatedYaw += DeltaYaw;
                    AccumulatedPitch += DeltaPitch;

                    // Enqueue the data
                    DataQueue.Enqueue(new FCameraAngles(AccumulatedPitch, AccumulatedYaw));
                }
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
