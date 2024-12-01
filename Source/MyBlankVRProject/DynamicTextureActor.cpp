#include "DynamicTextureActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"



// Sets default values
ADynamicTextureActor::ADynamicTextureActor()
    : formatContext(nullptr),
    swsCtx(nullptr),
    codecContext(nullptr),
    frame(nullptr),
    latest_frame(nullptr),
    packet(nullptr),
    texture_width(854),
    texture_height(480),
    videoStreamIndex(-1),
    stream_initialized(false)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

  // Create a scene component and set as root
  USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
  RootComponent = SceneRoot;

  RootComponent->SetMobility(EComponentMobility::Movable);

  // Create the plane mesh component
  PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
  PlaneMesh->SetupAttachment(RootComponent);

  PlaneMesh->SetMobility(EComponentMobility::Movable);

  // Set default mesh
  static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
  if (PlaneMeshAsset.Succeeded())
  {
    PlaneMesh->SetStaticMesh(PlaneMeshAsset.Object);
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("Failed to load plane mesh asset."));
  }

  // Assign the custom material
  static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(TEXT("Material'/Game/CustomStuff/M_DynamicTexture.M_DynamicTexture'"));
  if (MaterialAsset.Succeeded())
  {
      PlaneMesh->SetMaterial(0, MaterialAsset.Object);
  }
  else
  {
      UE_LOG(LogTemp, Error, TEXT("Failed to load dynamic material asset."));
  }

  // Disable collisions
  PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

  // Adjust position, rotation, and scale
  PlaneMesh->SetUsingAbsoluteRotation(false);
  PlaneMesh->SetUsingAbsoluteLocation(false);
  PlaneMesh->SetRelativeLocation(FVector(200.0f, 0.0f, 0.0f));
  PlaneMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 90.0f));
  PlaneMesh->SetRelativeScale3D(FVector(4.0f, 1.3f, 1.0f));
  PlaneMesh->SetVisibility(true);
  PlaneMesh->SetHiddenInGame(false);
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
    
    // Ensure PlaneMesh is set
    if (PlaneMesh)
    {
        UMaterialInterface* Material = PlaneMesh->GetMaterial(0);
        if (Material)
        {
            UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
            if (DynamicMaterial)
            {
                // Ensure the texture is valid
                if (DynamicTexture)
                {
                    DynamicMaterial->SetTextureParameterValue(FName("DynamicTexture"), DynamicTexture);
                    UE_LOG(LogTemp, Log, TEXT("Dynamic texture successfully assigned to material."));
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("DynamicTexture is null. Cannot assign to material."));
                }
        
                PlaneMesh->SetMaterial(0, DynamicMaterial);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create dynamic material instance."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("PlaneMesh does not have a valid material assigned to slot 0."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlaneMesh is not set. Please assign it in the editor."));
    }
    
    UE_LOG(LogTemp, Error, TEXT("Prepare to open UDP stream."));

    // int ret = InitializeUDPVideoStream();
    // if (ret == -1) {
    //     UE_LOG(LogTemp, Error, TEXT("UDP Stream Failed to Open"));
    // }

    bHasNewFrame = false;
    PendingFrameData = nullptr;
    PendingFrameSize = 0;

    // Start the worker thread
    FFmpegWorkerInstance = new FFmpegWorker(this);
    Thread = FRunnableThread::Create(FFmpegWorkerInstance, TEXT("FFmpegWorkerThread"));
    if (Thread)
    {
        UE_LOG(LogTemp, Log, TEXT("FFmpegWorker thread started."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start FFmpegWorker thread."));
    }
}

int ADynamicTextureActor::InitializeUDPVideoStream() {
    UE_LOG(LogTemp, Error, TEXT("Network init..."));
    
    avformat_network_init();

    const char* url = "udp://@:5253";  // Listen on all network interfaces, port 5000

    UE_LOG(LogTemp, Error, TEXT("Opening UDP Stream: %hs"), url);
    
    av_log_set_level(AV_LOG_DEBUG);
    
    int ret;
    if ((ret = avformat_open_input(&formatContext, url, nullptr, nullptr)) != 0) {
        UE_LOG(LogTemp, Error, TEXT("Error: Could not open UDP stream. %d"), ret);
        return -1;
    }

    UE_LOG(LogTemp, Error, TEXT("Opened UDP stream."));

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        UE_LOG(LogTemp, Error, TEXT("Error: Could not find stream information."));
        return -1;
    }

    UE_LOG(LogTemp, Error, TEXT("Found stream information."));
    UE_LOG(LogTemp, Error, TEXT("Number of streams: %d"), formatContext->nb_streams);

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
    
    stream_initialized = true;
    
    return 0;
}

void ADynamicTextureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Always call the base class EndPlay first
    Super::EndPlay(EndPlayReason);



    if (FFmpegWorkerInstance)
    {
        FFmpegWorkerInstance->Stop();
    }

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }

    if (FFmpegWorkerInstance)
    {
        delete FFmpegWorkerInstance;
        FFmpegWorkerInstance = nullptr;
    }

    // FFmpeg cleanup
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

    // Deinitialize network components
    avformat_network_deinit();

}


void ADynamicTextureActor::OnNewFrameAvailable()
{
    // This is called from the worker thread
    // Use a thread-safe way to mark that a new frame is ready
    {
        FScopeLock Lock(&NewFrameLock);
        if (FFmpegWorkerInstance->GetLatestFrame(PendingFrameData, PendingFrameSize))
        {
            bHasNewFrame = true;
        }
    }
}


void ADynamicTextureActor::Tick(float delta_time) {
    Super::Tick(delta_time);

    if (bHasNewFrame)
    {
        uint8* FrameData = nullptr;
        int FrameSize = 0;

        // Copy the frame data
        {
            FScopeLock Lock(&NewFrameLock);
            if (PendingFrameData && PendingFrameSize > 0)
            {
                FrameData = PendingFrameData;
                FrameSize = PendingFrameSize;
                PendingFrameData = nullptr;
                PendingFrameSize = 0;
            }
            bHasNewFrame = false;
        }

        if (FrameData && FrameSize > 0)
        {
            // Update the texture
            UpdateTexture(FrameData, FrameSize);
            av_free(FrameData);
        }
    }
    
    // if (!stream_initialized) {
    //     // UE_LOG(LogTemp, Error, TEXT("Tried to Tick, but not yet initialized."));
    //     return;
    // }
    
    // UE_LOG(LogTemp, Error, TEXT("Ticking..."));
    
    // int src_width = codecContext->width;
    // int src_height = codecContext->height;
    
    // // int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, src_width, src_height, 1);
    // int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, texture_width, texture_height, 1);
    // uint8_t* buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
    
    // if (av_read_frame(formatContext, packet) >= 0) {
    //     if (packet->stream_index == videoStreamIndex) {
    //         avcodec_send_packet(codecContext, packet);
    //     }
    //     av_packet_unref(packet);
    // }

    // bool have_new_frame = false;
    // while (avcodec_receive_frame(codecContext, frame) == 0) {
    //     av_frame_unref(latest_frame);
    //     av_frame_move_ref(latest_frame, frame);
    //     have_new_frame = true;
    // }

    // if (have_new_frame) {
    //     UE_LOG(LogTemp, Error, TEXT("Have new frame!!!"));
        
    //     // Set up destination pointers and linesizes
    //     uint8_t* dest_data[4] = { nullptr };
    //     int dest_linesize[4] = { 0 };

    //     av_image_fill_arrays(
    //         dest_data,
    //         dest_linesize,
    //         buffer,
    //         AV_PIX_FMT_BGRA,
    //         texture_width,
    //         texture_height,
    //         1
    //     );
        
    //     // Convert the frame to BGRA
    //     sws_scale(
    //         swsCtx,
    //         latest_frame->data,
    //         latest_frame->linesize,
    //         0,
    //         src_height,
    //         dest_data,
    //         dest_linesize
    //     );
    //     UpdateTexture(dest_data[0], num_bytes);
    // }

    // av_free(buffer);
}

void ADynamicTextureActor::UpdateTexture(uint8_t* img_data, int num_bytes) {
    UE_LOG(LogTemp, Log, TEXT("Updating texture..."));

    if (img_data == nullptr || num_bytes == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Image data is null or number of bytes is 0."));
        return;
    }

    // Access PlatformData
    FTexturePlatformData* PlatformData = DynamicTexture->GetPlatformData();

    if (!PlatformData || PlatformData->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PlatformData or Mips is null."));
        return;
    }

    // Access the first mipmap level
    FTexture2DMipMap& Mip = PlatformData->Mips[0];

    UE_LOG(LogTemp, Log, TEXT("Mip width: %d, height: %d"), Mip.SizeX, Mip.SizeY);

    // Print bulk data size
    UE_LOG(LogTemp, Log, TEXT("Bulk data size: %d"), Mip.BulkData.GetBulkDataSize());

    if (Mip.BulkData.GetBulkDataSize() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateTexture: Mip.BulkData size is 0. Attempting to realloc."));
        // Reallocate BulkData with the expected size
        Mip.BulkData.Realloc(num_bytes);
        if (Mip.BulkData.GetBulkDataSize() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("UpdateTexture: Realloc failed. BulkData size is still 0."));
            return;
        }
    }

    // Lock the mipmap's bulk data for read/write access
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

    UE_LOG(LogTemp, Log, TEXT("Data pointer: %p"), Data);

    if (!Data)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to lock the bulk data."));
        return;
    }


    UE_LOG(LogTemp, Log, TEXT("Number of bytes: %d"), num_bytes);

//    FMemory::Memcpy(Data, img_data, num_bytes);

    // Calculate the total number of pixels
    int32 total_pixels = texture_width * texture_height;

    if (total_pixels * 4 != num_bytes) {
        UE_LOG(LogTemp, Error, TEXT("Number of bytes does not match expected number of pixels."));
        UE_LOG(LogTemp, Error, TEXT("Expected: %d, Actual: %d"), total_pixels * 4, num_bytes);
        return;
    }

    // Pointer to the pixel data
    uint8* pixel_data = static_cast<uint8*>(Data);

    // Override color: Red
    uint32 override_color = 0xFF0000FF;

    // Iterate over each pixel
    for (int32 i = 0; i < total_pixels; ++i)
    {
        int32 pixel_index = i * 4; // 4 bytes per pixel

        // pixel_data[pixel_index + 0] = override_color & 0xFF;
        // pixel_data[pixel_index + 1] = (override_color >> 8) & 0xFF;
        // pixel_data[pixel_index + 2] = (override_color >> 16) & 0xFF;
        // pixel_data[pixel_index + 3] = (override_color >> 24) & 0xFF;

        pixel_data[pixel_index + 0] = img_data[pixel_index + 0];
        pixel_data[pixel_index + 1] = img_data[pixel_index + 1];
        pixel_data[pixel_index + 2] = img_data[pixel_index + 2];
        pixel_data[pixel_index + 3] = img_data[pixel_index + 3];
    }

    // Unlock the bulk data
    Mip.BulkData.Unlock();

    // Update the texture resource to apply changes
    DynamicTexture->UpdateResource();

    UE_LOG(LogTemp, Error, TEXT("Texture updated."));
}
