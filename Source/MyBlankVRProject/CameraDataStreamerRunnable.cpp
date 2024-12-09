#include "CameraDataStreamerRunnable.h"
#include "Networking.h"

FCameraDataStreamerRunnable::FCameraDataStreamerRunnable(TQueue<FCameraAngles*, EQueueMode::Spsc>* InDataQueue)
    : bStopThread(false), DataQueue(InDataQueue), Socket(nullptr), ServerIP(TEXT("192.168.0.3")), ServerPort(12345)
{
}

FCameraDataStreamerRunnable::~FCameraDataStreamerRunnable()
{
  DeconstructSocket();
}

void FCameraDataStreamerRunnable::DeconstructSocket()
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
    }
}


bool FCameraDataStreamerRunnable::Init()
{
    // Initialize the socket connection
    return InitializeSocket();
}

uint32 FCameraDataStreamerRunnable::Run()
{
    // Set the thread to highest priority
    FPlatformProcess::SetThreadPriority(EThreadPriority::TPri_Highest);

    while (!bStopThread)
    {
        FCameraAngles* DataToSend = nullptr;

        while (DataQueue->Dequeue(DataToSend)) {}

        if (DataToSend)
        {
            // Create byte array to send
            uint8 Buffer[sizeof(float) * 2];
            FMemory::Memcpy(Buffer, &DataToSend->Pitch, sizeof(float));
            FMemory::Memcpy(Buffer + sizeof(float), &DataToSend->Yaw, sizeof(float));

            int32 Sent = 0;

            UE_LOG(LogTemp, Warning, TEXT("Socket connection state: %d"), Socket->GetConnectionState());

            // Send data over the socket
            bool bSendSuccess = Socket->Send(Buffer, sizeof(Buffer), Sent);

            // Clean up
            delete DataToSend;

            // If the send failed, attempt to reconnect
            if (!bSendSuccess)
            {
                UE_LOG(LogTemp, Warning, TEXT("Socket connection lost. Reconnecting..."));

                DeconstructSocket();

                // Reconnect
                if (!InitializeSocket())
                {
                    // Sleep to prevent busy-waiting
                    FPlatformProcess::Sleep(0.002f);
                    continue;
                }
            }
        }
        else
        {
            // Sleep to prevent busy-waiting
            FPlatformProcess::Sleep(0.002f);
        }
    }

    return 0;
}

void FCameraDataStreamerRunnable::Stop()
{
    bStopThread = true;
}

bool FCameraDataStreamerRunnable::InitializeSocket()
{
    // Create a socket
    Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("CameraDataSocket"), false);

    // Set the socket to non-blocking mode
    Socket->SetNonBlocking(true);

    // Create a server address
    FIPv4Address IP;
    if (!FIPv4Address::Parse(ServerIP, IP))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid IP address: %s"), *ServerIP);
        return false;
    }

    TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Addr->SetIp(IP.Value);
    Addr->SetPort(ServerPort);

    float TimeoutSeconds = 2.0f;

    // Attempt to connect
    bool bIsConnected = Socket->Connect(*Addr);
    if (!bIsConnected)
    {
        // Poll for connection status with a timeout
        double StartTime = FPlatformTime::Seconds();
        while (FPlatformTime::Seconds() - StartTime < TimeoutSeconds)
        {
            if (Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
            {
                // Connection successful
                Socket->SetNonBlocking(false); // Restore blocking mode
                return true;
            }

            FPlatformProcess::Sleep(0.01f); // Sleep for a short time before polling again
        }

        // Timeout reached
        UE_LOG(LogTemp, Warning, TEXT("Connection attempt timed out after %f seconds."), TimeoutSeconds);
        return false;
    }

    // Already connected
    Socket->SetNonBlocking(false); // Restore blocking mode
    return true;
}
