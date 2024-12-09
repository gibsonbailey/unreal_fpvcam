#include "CameraDataStreamerRunnable.h"
#include "Networking.h"

FCameraDataStreamerRunnable::FCameraDataStreamerRunnable(TQueue<FCameraAngles*, EQueueMode::Spsc>* InDataQueue)
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
        FCameraAngles* DataToSend = nullptr;
        if (DataQueue->Dequeue(DataToSend))
        {
            // Create byte array to send
            uint8 Buffer[sizeof(float) * 2];
            FMemory::Memcpy(Buffer, &DataToSend->Pitch, sizeof(float));
            FMemory::Memcpy(Buffer + sizeof(float), &DataToSend->Yaw, sizeof(float));

            int32 Sent = 0;

            // Send data over the socket
            Socket->Send(Buffer, sizeof(Buffer), Sent);

            // Clean up
            delete DataToSend;
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

    // Create a server address
    FIPv4Address IP;
    FIPv4Address::Parse(ServerIP, IP);
    TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Addr->SetIp(IP.Value);
    Addr->SetPort(ServerPort);

    // Connect to server
    return Socket->Connect(*Addr);
}
