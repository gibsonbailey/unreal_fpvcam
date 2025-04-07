#pragma once

#include "CameraDataStreamer.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

class FCameraDataStreamerRunnable : public FRunnable {
public:
  FCameraDataStreamerRunnable(
      TQueue<FRobotControlData *, EQueueMode::Spsc> *InDataQueue,
      UCameraDataStreamer *InStreamer);
  virtual ~FCameraDataStreamerRunnable();

  // FRunnable interface
  virtual bool Init() override;
  virtual uint32 Run() override;
  virtual void Stop() override;

private:
  FThreadSafeBool bStopThread;
  TQueue<FRobotControlData *, EQueueMode::Spsc> *DataQueue;
  UCameraDataStreamer *Streamer;

  // Socket variables
  FSocket *ListenSocket;
  int32 ServerPort;
  FSocket *ControlStreamSocket;
  int32 ControlStreamPort;

  // Clock calibration offset
  int64 average_offset;

  // Functions to manage socket
  bool InitializeClockSyncSocket();
  bool InitializeControlStreamSocket();
  void DeconstructSocket();
  void SendServerAnnouncement();

  // Building blocks of the streaming process
  void CalibrateClockOffset();
  void StreamControlData();
};
