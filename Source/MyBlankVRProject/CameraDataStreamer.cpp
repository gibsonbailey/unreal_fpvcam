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
            // Bind the right index curl input
            if (IA_Hand_IndexCurl_Right)
            {
                EnhancedInputComponent->BindAction(IA_Hand_IndexCurl_Right, ETriggerEvent::Triggered, this, &UCameraDataStreamer::HandleRightTriggerInput);
            }

            // Bind the right thumbstick input
            if (IA_Hand_Thumbstick_Right)
            {
                EnhancedInputComponent->BindAction(IA_Hand_Thumbstick_Right, ETriggerEvent::Triggered, this, &UCameraDataStreamer::HandleRightThumbstickInput);
            }
            
            if (IA_Hand_ThumbUp_Right) {
                EnhancedInputComponent->BindAction(IA_Hand_ThumbUp_Right, ETriggerEvent::Triggered, this, &UCameraDataStreamer::HandleRightThumbUpInput);
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

void UCameraDataStreamer::HandleRightThumbstickInput(const FInputActionValue& Value)
{
    // Get the x and y values of the thumbstick
    float ThumbstickValue = Value.Get<float>();
    UE_LOG(LogTemp, Warning, TEXT("Right Thumbstick Value in Actor: %f"), ThumbstickValue);
    CachedRightThumbstickValue = ThumbstickValue;
}

void UCameraDataStreamer::HandleRightThumbUpInput(const FInputActionValue& Value)
{
    bool ThumbUpValue = Value.Get<bool>();
    UE_LOG(LogTemp, Warning, TEXT("Right ThumbUp Value in Actor: %d"), ThumbUpValue);
    CachedRightThumbUpValue = ThumbUpValue;
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
                
                // Do not accumulate the yaw and pitch if the right A button is pressed
                // if(CachedRightIndexCurlValue < 0.1f)
                if (true)
                {
                    AccumulatedYaw += DeltaYaw;
                    AccumulatedPitch += DeltaPitch;

                    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
                    if (Subsystem)
                    {
                        // This might vary depending on your exact setup, but you can retrieve the current value
                        FInputActionValue CurrentTriggerPositionValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Hand_IndexCurl_Right);
                        CachedRightIndexCurlValue = CurrentTriggerPositionValue.Get<float>();

                        // IA_Hand_ThumbUp_Right
                        FInputActionValue AButtonPressed = Subsystem->GetPlayerInput()->GetActionValue(IA_Hand_ThumbUp_Right);
                        CachedRightThumbUpValue = AButtonPressed.Get<bool>();

                        if (CachedRightThumbUpValue) {
                          // Make the value negative
                            CachedRightIndexCurlValue *= -1.0f;
                        }

                        FInputActionValue CurrentThumbstickValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Hand_Thumbstick_Right);
                        CachedRightThumbstickValue = CurrentThumbstickValue.Get<float>();

                    }

                    // Enqueue the data
                    DataQueue.Enqueue(new FRobotControlData(AccumulatedPitch, AccumulatedYaw, CachedRightIndexCurlValue, CachedRightThumbstickValue));
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
