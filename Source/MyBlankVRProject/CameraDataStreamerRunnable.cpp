#include "CameraDataStreamerRunnable.h"
#include "HttpModule.h"
#include "Networking.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "interfaces/IHttpRequest.h"
#include "interfaces/IHttpResponse.h"

FCameraDataStreamerRunnable::FCameraDataStreamerRunnable(
    TQueue<FRobotControlData *, EQueueMode::Spsc> *InDataQueue,
    UCameraDataStreamer *InStreamer)
    : bStopThread(false), DataQueue(InDataQueue), Streamer(InStreamer),
      ListenSocket(nullptr), ServerPort(6778), ControlStreamSocket(nullptr),
      ControlStreamPort(6779), average_offset(0) {}

FCameraDataStreamerRunnable::~FCameraDataStreamerRunnable() {
  DeconstructSocket();
}

void FCameraDataStreamerRunnable::DeconstructSocket() {
  if (ListenSocket) {
    ListenSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
        ->DestroySocket(ListenSocket);
  }
}

void FCameraDataStreamerRunnable::SendServerAnnouncement() {
  // 1) Get local IP
  bool bCanBindAll;
  TSharedRef<FInternetAddr> LocalAddr =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
          ->GetLocalHostAddr(*GLog, bCanBindAll);
  FString LocalIP = LocalAddr->IsValid() ? LocalAddr->ToString(false)
                                         : TEXT("UnknownLocalIP");

  // 2) Get public IP from a simple external service
  TSharedRef<IHttpRequest> PublicIPRequest = FHttpModule::Get().CreateRequest();
  PublicIPRequest->SetURL(TEXT(
      "https://api.ipify.org?format=text")); // or another service you prefer
  PublicIPRequest->SetVerb(TEXT("GET"));

  PublicIPRequest->OnProcessRequestComplete().BindLambda(
      [this, LocalIP](FHttpRequestPtr Request, FHttpResponsePtr Response,
                      bool bConnectedSuccessfully) {
        if (!bConnectedSuccessfully || !Response.IsValid()) {
          UE_LOG(LogTemp, Warning, TEXT("Failed to retrieve public IP."));
          return;
        }

        FString PublicIP = Response->GetContentAsString();
        UE_LOG(LogTemp, Log, TEXT("Local IP: %s, Public IP: %s"), *LocalIP,
               *PublicIP);

        // 3) Now POST to http://{IP}:{PORT}/server with the JSON payload
        FString ServerIP = TEXT("3.215.138.208"); // hard-coded server to notify
        FString PostURL =
            FString::Printf(TEXT("http://%s:%d/server"), *ServerIP, 4337);

        TSharedRef<IHttpRequest> PostRequest =
            FHttpModule::Get().CreateRequest();
        PostRequest->SetURL(PostURL);
        PostRequest->SetVerb(TEXT("POST"));
        PostRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

        FString Payload =
            FString::Printf(TEXT("{\"server_port\":\"12345\",\"server_local_"
                                 "ip\":\"%s\",\"server_public_ip\":\"%s\"}"),
                            *LocalIP, *PublicIP);
        PostRequest->SetContentAsString(Payload);

        PostRequest->OnProcessRequestComplete().BindLambda(
            [](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConn) {
              if (bConn && Resp.IsValid()) {
                UE_LOG(LogTemp, Log, TEXT("POST response: %s"),
                       *Resp->GetContentAsString());
              } else {
                UE_LOG(LogTemp, Warning, TEXT("POST failed."));
              }
            });
        PostRequest->ProcessRequest();
      });

  // Start the GET to find public IP
  PublicIPRequest->ProcessRequest();
}

bool FCameraDataStreamerRunnable::Init() {
  InitializeClockSyncSocket();
  InitializeControlStreamSocket();
  return true;
}

uint32 FCameraDataStreamerRunnable::Run() {
  SendServerAnnouncement();
  CalibrateClockOffset();
  return 0;
}

void FCameraDataStreamerRunnable::CalibrateClockOffset() {
  FSocket *ClientSocket = nullptr;

  int32 system_time_calibration_counter = 0;
  int32 system_time_calibration_required_samples = 20;
  TArray<int64> sample_offsets;
  bool isCalibrated = false;

  // Variable to hold the last time speed and distance data was received
  double lastDataReceivedTime = FPlatformTime::Seconds();

  while (!bStopThread && !isCalibrated) {
    // Accept if we don't already have a client
    if (!ClientSocket) {
      UE_LOG(LogTemp, Log, TEXT("Waiting for client connection..."));
      ClientSocket = ListenSocket->Accept(TEXT("IncomingClient"));
      if (ClientSocket) {
        UE_LOG(LogTemp, Log, TEXT("Client connected!"));
        ClientSocket->SetNonBlocking(false);

        UE_LOG(LogTemp, Log, TEXT("Calibrating system time..."));
        for (int i = 0;
             i < system_time_calibration_required_samples && !bStopThread;
             i++) {
          UE_LOG(LogTemp, Log, TEXT("Calibrating system time... sample %d"), i);
          uint8 Buffer[1];
          FMemory::Memset(Buffer, 0, sizeof(Buffer));

          // Get timestamp from system in milliseconds

          int32 Sent = 0;
          uint64 t1 = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
          bool bSendSuccess = ClientSocket->Send(Buffer, sizeof(Buffer), Sent);

          UE_LOG(LogTemp, Log, TEXT("Sent %d bytes to client. %d"), Sent, i);

          // Receive the timestamp from the client
          uint8 ReceiveBuffer[sizeof(uint64)];
          int32 BytesRead = 0;
          bool bReceived = ClientSocket->Recv(ReceiveBuffer,
                                              sizeof(ReceiveBuffer), BytesRead);
          uint64 t4 = FDateTime::UtcNow().ToUnixTimestamp() * 1000;

          UE_LOG(LogTemp, Log, TEXT("Received %d bytes from client. %d"),
                 BytesRead, i);

          if (bReceived && BytesRead == sizeof(ReceiveBuffer)) {
            UE_LOG(LogTemp, Log, TEXT("Received timestamp from client. %d"), i);

            uint64 t_client;
            FMemory::Memcpy(&t_client, ReceiveBuffer, sizeof(uint64));

            UE_LOG(LogTemp, Log, TEXT("copied timestamp from client. %d"), i);

            // UE_LOG(LogTemp, Log, TEXT("t1: %d, t_client: %d, t4: %d"), t1,
            // t_client, t4);
            int64 diff1 = static_cast<int64>(t_client) - static_cast<int64>(t1);
            int64 diff2 = static_cast<int64>(t4) - static_cast<int64>(t_client);
            int64 offset = (diff1 + diff2) / 2;
            sample_offsets.Add(offset);

            UE_LOG(LogTemp, Log, TEXT("offset: %d %d"), offset, i);
          } else {
            UE_LOG(LogTemp, Log,
                   TEXT("Failed to receive timestamp from client."));
            break;
          }

          FPlatformProcess::Sleep(0.01f);
        }

        if (sample_offsets.Num() >= system_time_calibration_required_samples) {
          // UE_LOG(LogTemp, Log, TEXT("avg_offset System time calibration
          // complete. Calculating average offset...")); Calculate the average
          // offset
          int64 sum = 0;
          for (int sample_offset_index = 0;
               sample_offset_index < sample_offsets.Num();
               sample_offset_index++) {
            sum += sample_offsets[sample_offset_index];
          }
          // UE_LOG(LogTemp, Log, TEXT("avg_offset Sum of offsets: %d"), sum);
          // UE_LOG(LogTemp, Log, TEXT("avg_offset Number of samples: %d"),
          // sample_offsets.Num());
          average_offset = int64(sum / sample_offsets.Num());
          // UE_LOG(LogTemp, Log, TEXT("avg_offset: %d"), average_offset);
          isCalibrated = true;
          UE_LOG(LogTemp, Log,
                 TEXT("System time calibration complete. Average offset: %d"),
                 average_offset);
        } else {
          UE_LOG(LogTemp, Log,
                 TEXT("System time calibration failed. Not enough samples."));
          // Calibration failed
          ClientSocket->Close();
          ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
              ->DestroySocket(ClientSocket);
          ClientSocket = nullptr;
          continue;
        }

        ClientSocket->SetNonBlocking(true);
      }
    }
  }

  // Cleanup
  if (ClientSocket) {
    ClientSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
        ->DestroySocket(ClientSocket);
    ClientSocket = nullptr;
  }
}

void FCameraDataStreamerRunnable::StreamControlData() {
  // else if (ClientSocket && isCalibrated) {
  //   // Set non-blocking mode
  //   ClientSocket->SetNonBlocking(true);

  //   // Receive data from the client
  //   uint8 ReceiveBuffer[sizeof(uint64) + (sizeof(float) * 2) +
  //                       (sizeof(int) * 2)];
  //   int32 BytesRead = 0;

  //   bool bReceived =
  //       ClientSocket->Recv(ReceiveBuffer, sizeof(ReceiveBuffer), BytesRead);

  //   UE_LOG(LogTemp, Log, TEXT("Received speed data from client: %d bytes"),
  //          BytesRead);

  //   if (bReceived && BytesRead == sizeof(ReceiveBuffer)) {
  //     // Extract the data from the buffer
  //     uint64 placeholder;
  //     float speed_mph;
  //     float distance_ft;
  //     int control_battery_percentage;
  //     int drive_battery_percentage;

  //     FMemory::Memcpy(&placeholder, ReceiveBuffer, sizeof(uint64));
  //     FMemory::Memcpy(&speed_mph, ReceiveBuffer + sizeof(uint64),
  //                     sizeof(float));
  //     FMemory::Memcpy(&distance_ft,
  //                     ReceiveBuffer + sizeof(uint64) + sizeof(float),
  //                     sizeof(float));
  //     FMemory::Memcpy(&control_battery_percentage,
  //                     ReceiveBuffer + sizeof(uint64) + (sizeof(float) * 2),
  //                     sizeof(int));
  //     FMemory::Memcpy(&drive_battery_percentage,
  //                     ReceiveBuffer + sizeof(uint64) + (sizeof(float) * 2) +
  //                         sizeof(int),
  //                     sizeof(int));

  //     UE_LOG(LogTemp, Log, TEXT("telemetry data copied: %f, %f, %d, %d"),
  //            speed_mph, distance_ft, control_battery_percentage,
  //            drive_battery_percentage);

  //     AsyncTask(ENamedThreads::GameThread, [this, speed_mph, distance_ft,
  //                                           control_battery_percentage,
  //                                           drive_battery_percentage]() {
  //       Streamer->SpeedMph = speed_mph;
  //       Streamer->DistanceFeet = distance_ft;
  //       Streamer->ControlBatteryPercentage = control_battery_percentage;
  //       Streamer->DriveBatteryPercentage = drive_battery_percentage;
  //     });

  //     UE_LOG(LogTemp, Log, TEXT("Telemetry receive async task triggered"));

  //     lastDataReceivedTime = FPlatformTime::Seconds();
  //   }

  //   // If it's been more than 0.25 seconds since the last data was received,
  //   // reset the speed and distance to 0
  //   if (FPlatformTime::Seconds() - lastDataReceivedTime > 1) {
  //     AsyncTask(ENamedThreads::GameThread, [this]() {
  //       Streamer->SpeedMph = 0.0f;
  //       Streamer->DistanceFeet = 0.0f;
  //     });
  //   }

  //   FRobotControlData *DataToSend = nullptr;
  //   while (DataQueue->Dequeue(DataToSend)) {
  //   }

  //   if (DataToSend) {
  //     // Create byte array to send
  //     // 1 unsigned long long int (8 bytes) + 3 floats (4 bytes each)
  //     uint8 Buffer[sizeof(uint64) + (sizeof(float) * 4)];
  //     // Get timestamp from system in milliseconds
  //     uint64 timeStamp =
  //         (FDateTime::UtcNow().ToUnixTimestamp() * 1000) + average_offset;
  //     FMemory::Memcpy(Buffer, &timeStamp, sizeof(uint64));
  //     FMemory::Memcpy(Buffer + sizeof(uint64), &DataToSend->Pitch,
  //                     sizeof(float));
  //     FMemory::Memcpy(Buffer + sizeof(uint64) + sizeof(float),
  //                     &DataToSend->Yaw, sizeof(float));
  //     FMemory::Memcpy(Buffer + sizeof(uint64) + (sizeof(float) * 2),
  //                     &DataToSend->TriggerPosition, sizeof(float));
  //     FMemory::Memcpy(Buffer + sizeof(uint64) + (sizeof(float) * 3),
  //                     &DataToSend->ThumbstickX, sizeof(float));

  //     int32 Sent = 0;
  //     bool bSendSuccess = ClientSocket->Send(Buffer, sizeof(Buffer), Sent);

  //     delete DataToSend;
  //     if (!bSendSuccess) {
  //       UE_LOG(LogTemp, Warning, TEXT("Send failed. Closing client
  //       socket.")); ClientSocket->Close();
  //       ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
  //           ->DestroySocket(ClientSocket);
  //       ClientSocket = nullptr;
  //     }
  //   } else {
  //     FPlatformProcess::Sleep(0.001f);
  //   }
  // } else {
  //   // No client yet; avoid busy wait
  //   FPlatformProcess::Sleep(0.01f);
  // }
}

void FCameraDataStreamerRunnable::Stop() { bStopThread = true; }

bool FCameraDataStreamerRunnable::InitializeClockSyncSocket() {
  // Create the listening socket
  ListenSocket =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
          ->CreateSocket(NAME_Stream, TEXT("CameraDataListenSocket"), false);

  // 0.0.0.0 on the given port
  TSharedRef<FInternetAddr> Addr =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
  Addr->SetAnyAddress();
  Addr->SetPort(ServerPort);

  ListenSocket->Bind(*Addr);
  ListenSocket->Listen(1);

  UE_LOG(LogTemp, Log, TEXT("Server listening on port %d"), ServerPort);
  return true;
}

bool FCameraDataStreamerRunnable::InitializeControlStreamSocket() {
  // Create the socket. UDP this time
  ControlStreamSocket =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
          ->CreateSocket(NAME_DGram,
                         TEXT("CameraDataControlStreamSocketSocket"), false);

  // 0.0.0.0 on the given port
  TSharedRef<FInternetAddr> Addr =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
  Addr->SetAnyAddress();
  Addr->SetPort(ControlStreamPort);

  ControlStreamSocket->Bind(*Addr);

  UE_LOG(LogTemp, Log, TEXT("Server listening on port %d"), ServerPort);
  return true;
}
