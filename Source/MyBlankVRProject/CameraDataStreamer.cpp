#include "CameraDataStreamer.h"
#include "CameraDataStreamerRunnable.h"
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
                
                // Prepare data to send
                FString* DataToSend = new FString(FString::Printf(TEXT("%f,%f,%f"), CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll));
                
                // Enqueue the data
                DataQueue.Enqueue(DataToSend);
            }
        }
    }
}