#include "DynamicTextureActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetMathLibrary.h"



// Sets default values
ADynamicTextureActor::ADynamicTextureActor()
    : formatContext(nullptr),
    swsCtx(nullptr),
    codecContext(nullptr),
    frame(nullptr),
    latest_frame(nullptr),
    packet(nullptr),
    texture_width(256),
    texture_height(256),
    videoStreamIndex(-1)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
    
    avformat_network_init();
}

// Called when the game starts or when spawned
void ADynamicTextureActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Error, TEXT("Begin play called."));
    
    DynamicTexture = UTexture2D::CreateTransient(texture_width, texture_height, PF_B8G8R8A8);
    if (DynamicTexture)
    {
        DynamicTexture->UpdateResource();
    }
    
    // Ensure PlaneActor is set
    if (PlaneActor)
    {
        UMaterialInterface* Material = PlaneActor->GetStaticMeshComponent()->GetMaterial(0);
        UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetTextureParameterValue(FName("DynamicTexture"), DynamicTexture);
            PlaneActor->GetStaticMeshComponent()->SetMaterial(0, DynamicMaterial);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlaneActor is not set. Please assign it in the editor."));
    }
    
    // Set up a timer to update the texture every few seconds
//    GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ADynamicTextureActor::UpdateTexture, 2.0f, true);
//    UpdateTexture();
    
    int ret = InitializeUDPVideoStream();
    if (ret == -1) {
        UE_LOG(LogTemp, Error, TEXT("UDP Stream Failed to Open"));
    }
    DigestStream();
}

int ADynamicTextureActor::InitializeUDPVideoStream() {
    const char* url = "udp://@:5000";  // Listen on all network interfaces, port 5000

    UE_LOG(LogTemp, Error, TEXT("Opening UDP Stream: %hs"), url);

    if (avformat_open_input(&formatContext, url, nullptr, nullptr) != 0) {
        UE_LOG(LogTemp, Error, TEXT("Error: Could not open UDP stream."));
        return -1;
    }

    UE_LOG(LogTemp, Error, TEXT("Opened UDP stream."));

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        UE_LOG(LogTemp, Error, TEXT("Error: Could not find stream information."));
        return -1;
    }

    UE_LOG(LogTemp, Error, TEXT("Found stream information."));
//    UE_LOG(LogTemp, Error, TEXT("Number of streams: %d"), formatContext->nb_streams);

    const AVCodec* codec = nullptr;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            codec = avcodec_find_decoder(formatContext->streams[i]->codecpar->codec_id);
            break;
        }
    }
    if (videoStreamIndex == -1) {
        UE_LOG(LogTemp, Error, TEXT("Error: Could not find a video stream."));
        return -1;
    }

    codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex]->codecpar);
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        UE_LOG(LogTemp, Error, TEXT("Error: Could not open codec."));
        return -1;
    }
    
    frame = av_frame_alloc();
    latest_frame = av_frame_alloc();
    packet = av_packet_alloc();
 
    swsCtx = sws_getContext(
        codecContext->width,
        codecContext->height,
        codecContext->pix_fmt,
        texture_width,
        texture_height,
        AV_PIX_FMT_BGRA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr
    );
    
    return 0;
}

void ADynamicTextureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Always call the base class EndPlay first
    Super::EndPlay(EndPlayReason);

    // Place your cleanup code here
    if (swsCtx)
    {
        sws_freeContext(swsCtx);
        swsCtx = nullptr;
    }
    if (frame)
    {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (latest_frame)
    {
        av_frame_free(&latest_frame);
        latest_frame = nullptr;
    }
    if (packet)
    {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (codecContext)
    {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }
    if (formatContext)
    {
        avformat_close_input(&formatContext);
        formatContext = nullptr;
    }

    // Deinitialize network components if needed
    avformat_network_deinit();
}

void ADynamicTextureActor::DigestStream() {
    while (true) {
        if (av_read_frame(formatContext, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                avcodec_send_packet(codecContext, packet);
            }
            av_packet_unref(packet);
        }

        bool have_new_frame = false;
        while (avcodec_receive_frame(codecContext, frame) == 0) {
            av_frame_unref(latest_frame);
            av_frame_move_ref(latest_frame, frame);
            have_new_frame = true;
        }
        
        int src_width = codecContext->width;
        int src_height = codecContext->height;

        if (have_new_frame) {
            int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, src_width, src_height, 1);
            uint8_t* buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
            
            // Set up destination pointers and linesizes
            uint8_t* dest_data[4] = { nullptr };
            int dest_linesize[4] = { 0 };

            av_image_fill_arrays(
                dest_data,
                dest_linesize,
                buffer, 
                AV_PIX_FMT_BGRA,
                src_width,
                src_height,
                1
            );
            
            // Convert the frame to BGRA
            sws_scale(
                swsCtx,
                latest_frame->data,
                latest_frame->linesize,
                0,
                src_height,
                dest_data,
                dest_linesize
            );
            
            
            // Convert the frame from its native format to BGR (used by OpenCV)
//            cv::Mat image(codecContext->height, codecContext->width, CV_8UC3);
//            uint8_t* dest[4] = { image.data, nullptr, nullptr, nullptr };
//            int dest_linesize[4] = { static_cast<int>(image.step[0]), 0, 0, 0 };
//            sws_scale(
//                swsCtx,
//                latest_frame->data, 
//                latest_frame->linesize,
//                0,
//                codecContext->height,
//                dest,
//                dest_linesize
//            );
//
//            // Display the image
//            cv::imshow("UDP Video Stream", image);
//
//            // Exit if 'q' is pressed
//            if (cv::waitKey(1) == 'q') {
//                goto end;  // Break out of nested loops
//            }
        }

    }

}

void ADynamicTextureActor::UpdateTexture(uint8_t* img_data, int num_bytes) {
    // Access PlatformData
    FTexturePlatformData* PlatformData = DynamicTexture->GetPlatformData();

    if (!PlatformData || PlatformData->Mips.Num() == 0)
    {
        return;
    }

    // Access the first mipmap level
    FTexture2DMipMap& Mip = PlatformData->Mips[0];

    // Lock the mipmap's bulk data for read/write access
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

    FMemory::Memcpy(Data, img_data, num_bytes);
    
//    // Get the texture dimensions
//    int32 TextureWidth = Mip.SizeX;
//    int32 TextureHeight = Mip.SizeY;
//    
//    UE_LOG(LogTemp, Error, TEXT("Width %d, Height %d"), TextureWidth, TextureHeight);

//    // Calculate the total number of pixels
//    int32 TotalPixels = TextureWidth * TextureHeight;
//
//    // Pointer to the pixel data
//    uint8* PixelData = static_cast<uint8*>(Data);
//    
//    FColor RandomColor = FColor::MakeRandomColor();
//
//    // Iterate over each pixel
//    for (int32 i = 0; i < TotalPixels; ++i)
//    {
//        int32 PixelIndex = i * 4; // 4 bytes per pixel
//
//        // Set pixel to red
//        PixelData[PixelIndex + 0] = RandomColor.B;    // Blue component
//        PixelData[PixelIndex + 1] = RandomColor.G;    // Green component
//        PixelData[PixelIndex + 2] = RandomColor.R;  // Red component
//        PixelData[PixelIndex + 3] = 255;  // Alpha component
//    }

    // Unlock the bulk data
    Mip.BulkData.Unlock();

    // Update the texture resource to apply changes
    DynamicTexture->UpdateResource();
}


//void ADynamicTextureActor::UpdateTexture()
//{
//
//    // Access PlatformData
//    FTexturePlatformData* PlatformData = DynamicTexture->GetPlatformData();
//
//    if (!PlatformData || PlatformData->Mips.Num() == 0)
//    {
//        return;
//    }
//
//    // Access the first mipmap level
//    FTexture2DMipMap& Mip = PlatformData->Mips[0];
//
//    // Lock the mipmap's bulk data for read/write access
//    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
//
//    // Get the texture dimensions
//    int32 TextureWidth = Mip.SizeX;
//    int32 TextureHeight = Mip.SizeY;
//    
//    UE_LOG(LogTemp, Error, TEXT("Width %d, Height %d"), TextureWidth, TextureHeight);
//
//    // Calculate the total number of pixels
//    int32 TotalPixels = TextureWidth * TextureHeight;
//
//    // Pointer to the pixel data
//    uint8* PixelData = static_cast<uint8*>(Data);
//    
//    FColor RandomColor = FColor::MakeRandomColor();
//
//    // Iterate over each pixel
//    for (int32 i = 0; i < TotalPixels; ++i)
//    {
//        int32 PixelIndex = i * 4; // 4 bytes per pixel
//
//        // Set pixel to red
//        PixelData[PixelIndex + 0] = RandomColor.B;    // Blue component
//        PixelData[PixelIndex + 1] = RandomColor.G;    // Green component
//        PixelData[PixelIndex + 2] = RandomColor.R;  // Red component
//        PixelData[PixelIndex + 3] = 255;  // Alpha component
//    }
//
//    // Unlock the bulk data
//    Mip.BulkData.Unlock();
//
//    // Update the texture resource to apply changes
//    DynamicTexture->UpdateResource();
//}
