// FFmpegWorker.cpp
#include "FFmpegWorker.h"
#include "DynamicTextureActor.h"
#include "Misc/ScopeLock.h"

FFmpegWorker::FFmpegWorker(ADynamicTextureActor* InOwner)
    : Owner(InOwner),
      Thread(nullptr),
      bStopThread(false),
      LatestFrameData(nullptr),
      FrameDataSize(0)
{
}

FFmpegWorker::~FFmpegWorker()
{
    if (Thread)
    {
        delete Thread;
        Thread = nullptr;
    }

    // Free allocated frame data
    if (LatestFrameData)
    {
        av_free(LatestFrameData);
        LatestFrameData = nullptr;
    }
}

bool FFmpegWorker::Init()
{
    // Initialization if needed
    return true;
}

uint32 FFmpegWorker::Run()
{
    // Keep trying to initialize until success or stop requested
    while (!bStopThread)
    {
        int ret = Owner->InitializeUDPVideoStream();
        if (ret == 0)
        {
            break; // Initialization successful
        }
        
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize FFmpeg stream. Retrying in 1 second..."));
        FPlatformProcess::Sleep(3.0f);
    }

    while (!bStopThread)
    {
        // Process FFmpeg frames
        int src_width = Owner->codecContext->width;
        int src_height = Owner->codecContext->height;

        int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, Owner->texture_width, Owner->texture_height, 1);
        if (num_bytes <= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("FFmpegWorker: Invalid buffer size."));
            FPlatformProcess::Sleep(0.01f); // Sleep briefly to prevent tight loop
            continue;
        }

        uint8_t* buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
        if (!buffer)
        {
            UE_LOG(LogTemp, Warning, TEXT("FFmpegWorker: Failed to allocate buffer."));
            FPlatformProcess::Sleep(0.01f);
            continue;
        }

        if (av_read_frame(Owner->formatContext, Owner->packet) >= 0)
        {
            if (Owner->packet->stream_index == Owner->videoStreamIndex)
            {
                if (avcodec_send_packet(Owner->codecContext, Owner->packet) < 0)
                {
                    UE_LOG(LogTemp, Warning, TEXT("FFmpegWorker: Failed to send packet to decoder."));
                }
            }
            av_packet_unref(Owner->packet);
        }

        bool have_new_frame = false;
        while (avcodec_receive_frame(Owner->codecContext, Owner->frame) == 0)
        {
            av_frame_unref(Owner->latest_frame);
            av_frame_move_ref(Owner->latest_frame, Owner->frame);
            have_new_frame = true;
        }

        if (have_new_frame)
        {
            uint8_t* dest_data[4] = { nullptr };
            int dest_linesize[4] = { 0 };

            av_image_fill_arrays(
                dest_data,
                dest_linesize,
                buffer,
                AV_PIX_FMT_BGRA,
                Owner->texture_width,
                Owner->texture_height,
                1
            );

            // Convert the frame to BGRA
            sws_scale(
                Owner->swsCtx,
                Owner->latest_frame->data,
                Owner->latest_frame->linesize,
                0,
                src_height,
                dest_data,
                dest_linesize
            );

            // Lock and update frame data
            {
                FScopeLock Lock(&FrameDataLock);
                if (LatestFrameData)
                {
                    av_free(LatestFrameData);
                }
                LatestFrameData = buffer;
                FrameDataSize = num_bytes;
                buffer = nullptr; // Ownership transferred
            }

            // Notify Owner to update texture
            Owner->OnNewFrameAvailable();
        }

        if (buffer)
        {
            av_free(buffer);
        }

        // Sleep to prevent high CPU usage
        FPlatformProcess::Sleep(0.005f); // Adjust as needed
    }

    return 0;
}

void FFmpegWorker::Stop()
{
    bStopThread = true;
}

bool FFmpegWorker::GetLatestFrame(uint8*& OutData, int& OutSize)
{
    FScopeLock Lock(&FrameDataLock);
    if (LatestFrameData && FrameDataSize > 0)
    {
        OutData = (uint8*)av_malloc(FrameDataSize);
        if (OutData)
        {
            FMemory::Memcpy(OutData, LatestFrameData, FrameDataSize);
            OutSize = FrameDataSize;
            return true;
        }
    }
    return false;
}
