#include "CameraDataStreamerRunnable.h"
#include "Networking.h"
#include "HttpModule.h"
#include "interfaces/IHttpRequest.h"
#include "interfaces/IHttpResponse.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

FCameraDataStreamerRunnable::FCameraDataStreamerRunnable(TQueue<FRobotControlData*, EQueueMode::Spsc>* InDataQueue)
    : bStopThread(false), DataQueue(InDataQueue), ListenSocket(nullptr), ServerPort(12345)
{
}

FCameraDataStreamerRunnable::~FCameraDataStreamerRunnable()
{
  DeconstructSocket();
}

void FCameraDataStreamerRunnable::DeconstructSocket()
{
    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
    }
}


void FCameraDataStreamerRunnable::SendServerAnnouncement()
{
    // 1) Get local IP
    bool bCanBindAll;
    TSharedRef<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    FString LocalIP = LocalAddr->IsValid() ? LocalAddr->ToString(false) : TEXT("UnknownLocalIP");

    // 2) Get public IP from a simple external service
    TSharedRef<IHttpRequest> PublicIPRequest = FHttpModule::Get().CreateRequest();
    PublicIPRequest->SetURL(TEXT("https://api.ipify.org?format=text")); // or another service you prefer
    PublicIPRequest->SetVerb(TEXT("GET"));
    
    PublicIPRequest->OnProcessRequestComplete().BindLambda(
        [this, LocalIP](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
        {
            if (!bConnectedSuccessfully || !Response.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to retrieve public IP."));
                return;
            }

            FString PublicIP = Response->GetContentAsString();
            UE_LOG(LogTemp, Log, TEXT("Local IP: %s, Public IP: %s"), *LocalIP, *PublicIP);

            // 3) Now POST to http://{IP}:{PORT}/server with the JSON payload
            FString ServerIP = TEXT("192.168.0.14");  // hard-coded server to notify
            FString PostURL = FString::Printf(TEXT("http://%s:%d/server"), *ServerIP, 4337);

            TSharedRef<IHttpRequest> PostRequest = FHttpModule::Get().CreateRequest();
            PostRequest->SetURL(PostURL);
            PostRequest->SetVerb(TEXT("POST"));
            PostRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

            FString Payload = FString::Printf(
                TEXT("{\"server_port\":\"12345\",\"server_local_ip\":\"%s\",\"server_public_ip\":\"%s\"}"),
                *LocalIP, *PublicIP
            );
            PostRequest->SetContentAsString(Payload);

            PostRequest->OnProcessRequestComplete().BindLambda(
                [](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConn)
                {
                    if (bConn && Resp.IsValid())
                    {
                        UE_LOG(LogTemp, Log, TEXT("POST response: %s"), *Resp->GetContentAsString());
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("POST failed."));
                    }
                }
            );
            PostRequest->ProcessRequest();
        }
    );

    // Start the GET to find public IP
    PublicIPRequest->ProcessRequest();
}


bool FCameraDataStreamerRunnable::Init()
{
    // Initialize the socket connection
    return InitializeSocket();
}


uint32 FCameraDataStreamerRunnable::Run()
{
    SendServerAnnouncement();

    FSocket* ClientSocket = nullptr;

    while (!bStopThread)
    {
        // Accept if we don't already have a client
        if (!ClientSocket)
        {
            ClientSocket = ListenSocket->Accept(TEXT("IncomingClient"));
            if (ClientSocket)
            {
                UE_LOG(LogTemp, Log, TEXT("Client connected!"));
                ClientSocket->SetNonBlocking(false);
            }
        }

        // If we have a client, try sending data
        if (ClientSocket)
        {
            FRobotControlData* DataToSend = nullptr;
            while (DataQueue->Dequeue(DataToSend)) {}

            if (DataToSend)
            {
                // Create byte array to send
                uint8 Buffer[sizeof(float) * 4];
                FMemory::Memcpy(Buffer, &DataToSend->Pitch, sizeof(float));
                FMemory::Memcpy(Buffer + sizeof(float), &DataToSend->Yaw, sizeof(float));
                FMemory::Memcpy(Buffer + (sizeof(float) * 2), &DataToSend->TriggerPosition, sizeof(float));
                FMemory::Memcpy(Buffer + (sizeof(float) * 3), &DataToSend->ThumbstickX, sizeof(float));

                int32 Sent = 0;
                bool bSendSuccess = ClientSocket->Send(Buffer, sizeof(Buffer), Sent);

                delete DataToSend;
                if (!bSendSuccess)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Send failed. Closing client socket."));
                    ClientSocket->Close();
                    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
                    ClientSocket = nullptr;
                }
            }
            else
            {
                FPlatformProcess::Sleep(0.001f);
            }
        }
        else
        {
            // No client yet; avoid busy wait
            FPlatformProcess::Sleep(0.01f);
        }
    }

    // Cleanup
    if (ClientSocket)
    {
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
    }
    return 0;
}

// uint32 FCameraDataStreamerRunnable::Run()
// {
//     // Set the thread to highest priority
//     FPlatformProcess::SetThreadPriority(EThreadPriority::TPri_Highest);

//     while (!bStopThread)
//     {
//         FRobotControlData* DataToSend = nullptr;

//         while (DataQueue->Dequeue(DataToSend)) {}

//         if (DataToSend)
//         {
//             // Create byte array to send
//             uint8 Buffer[sizeof(float) * 4];
//             FMemory::Memcpy(Buffer, &DataToSend->Pitch, sizeof(float));
//             FMemory::Memcpy(Buffer + sizeof(float), &DataToSend->Yaw, sizeof(float));
//             FMemory::Memcpy(Buffer + (sizeof(float) * 2), &DataToSend->TriggerPosition, sizeof(float));
//             FMemory::Memcpy(Buffer + (sizeof(float) * 3), &DataToSend->ThumbstickX, sizeof(float));

//             int32 Sent = 0;

//             UE_LOG(LogTemp, Warning, TEXT("Socket connection state: %d"), Socket->GetConnectionState());

//             // Send data over the socket
//             bool bSendSuccess = Socket->Send(Buffer, sizeof(Buffer), Sent);

//             // Clean up
//             delete DataToSend;

//             // If the send failed, attempt to reconnect
//             if (!bSendSuccess)
//             {
//                 UE_LOG(LogTemp, Warning, TEXT("Socket connection lost. Reconnecting..."));

//                 DeconstructSocket();

//                 // Reconnect
//                 if (!InitializeSocket())
//                 {
//                     // Sleep to prevent busy-waiting
//                     FPlatformProcess::Sleep(0.002f);
//                     continue;
//                 }
//             }
//         }
//         else
//         {
//             // Sleep to prevent busy-waiting
//             FPlatformProcess::Sleep(0.002f);
//         }
//     }

//     return 0;
// }

void FCameraDataStreamerRunnable::Stop()
{
    bStopThread = true;
}

bool FCameraDataStreamerRunnable::InitializeSocket()
{
    // Create the listening socket
    ListenSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(
        NAME_Stream, TEXT("CameraDataListenSocket"), false);

    // 0.0.0.0 on the given port
    TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Addr->SetAnyAddress();
    Addr->SetPort(ServerPort);

    // Bind and listen
    ListenSocket->Bind(*Addr);
    ListenSocket->Listen(1);

    UE_LOG(LogTemp, Log, TEXT("Server listening on port %d"), ServerPort);
    return true;

//     // Create a socket
//     Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("CameraDataSocket"), false);

//     // Set the socket to non-blocking mode
//     Socket->SetNonBlocking(true);

//     // Create a server address
//     FIPv4Address IP;
//     if (!FIPv4Address::Parse(ServerIP, IP))
//     {
//         UE_LOG(LogTemp, Error, TEXT("Invalid IP address: %s"), *ServerIP);
//         return false;
//     }

//     TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
//     Addr->SetIp(IP.Value);
//     Addr->SetPort(ServerPort);

//     float TimeoutSeconds = 2.0f;

//     // Attempt to connect
//     bool bIsConnected = Socket->Connect(*Addr);
//     if (!bIsConnected)
//     {
//         // Poll for connection status with a timeout
//         double StartTime = FPlatformTime::Seconds();
//         while (FPlatformTime::Seconds() - StartTime < TimeoutSeconds)
//         {
//             if (Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
//             {
//                 // Connection successful
//                 Socket->SetNonBlocking(false); // Restore blocking mode
//                 return true;
//             }

//             FPlatformProcess::Sleep(0.01f); // Sleep for a short time before polling again
//         }

//         // Timeout reached
//         UE_LOG(LogTemp, Warning, TEXT("Connection attempt timed out after %f seconds."), TimeoutSeconds);
//         return false;
//     }

//     // Already connected
//     Socket->SetNonBlocking(false); // Restore blocking mode
//     return true;
}
