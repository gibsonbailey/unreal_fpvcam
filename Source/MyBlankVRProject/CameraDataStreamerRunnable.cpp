#include "CameraDataStreamerRunnable.h"
#include "Networking.h"

FCameraDataStreamerRunnable::FCameraDataStreamerRunnable(TQueue<FString*, EQueueMode::Spsc>* InDataQueue)
    : bStopThread(false), DataQueue(InDataQueue), Socket(nullptr), ServerIP(TEXT("192.168.0.3")), ServerPort(12345)
{
}

FCameraDataStreamerRunnable::~FCameraDataStreamerRunnable()
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
        FString* DataToSend = nullptr;
        if (DataQueue->Dequeue(DataToSend))
        {
            // Convert FString to UTF8
            FTCHARToUTF8 Convert(*DataToSend);
            int32 Size = Convert.Length();
            int32 Sent = 0;

            // Send data over the socket
            Socket->Send((uint8*)Convert.Get(), Size, Sent);

            // Clean up
            delete DataToSend;
        }
        else
        {
            // Sleep to prevent busy-waiting
            FPlatformProcess::Sleep(0.005f);
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

    // Create a server address
    FIPv4Address IP;
    FIPv4Address::Parse(ServerIP, IP);
    TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Addr->SetIp(IP.Value);
    Addr->SetPort(ServerPort);

    // Connect to server
    return Socket->Connect(*Addr);
}
