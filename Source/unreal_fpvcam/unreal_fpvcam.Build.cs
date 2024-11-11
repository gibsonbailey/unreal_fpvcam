// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class unreal_fpvcam : ModuleRules
{
	public unreal_fpvcam(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
        
        // Base directory for ThirdParty libraries
        string ThirdPartyPath = Path.Combine(ModuleDirectory, "../../ThirdParty/");
        
        // x264 paths
        string x264IncludePath = Path.Combine(ThirdPartyPath, "x264/include");
        string x264LibPath = Path.Combine(ThirdPartyPath, "x264/lib");

        // FFmpeg paths
        string FFmpegIncludePath = Path.Combine(ThirdPartyPath, "ffmpeg/include");
        string FFmpegLibPath = Path.Combine(ThirdPartyPath, "ffmpeg/lib");

        // Add third party include paths
        PublicIncludePaths.Add(FFmpegIncludePath);
        PublicIncludePaths.Add(x264IncludePath);

        // Add FFmpeg library path
        // PublicLibraryPaths.Add(FFmpegLibPath);

        // Specify the FFmpeg libraries to link
        PublicAdditionalLibraries.Add(Path.Combine(FFmpegLibPath, "libavcodec.a"));
        PublicAdditionalLibraries.Add(Path.Combine(FFmpegLibPath, "libavformat.a"));
        PublicAdditionalLibraries.Add(Path.Combine(FFmpegLibPath, "libswscale.a"));
        PublicAdditionalLibraries.Add(Path.Combine(FFmpegLibPath, "libavutil.a"));
        
        // x264 library
        PublicAdditionalLibraries.Add(Path.Combine(x264LibPath, "libx264.a"));
        
        
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicFrameworks.AddRange(new string[]
            {
                "CoreMedia",
                "CoreVideo",
                "AudioToolbox", // Add AudioToolbox framework
                "AVFoundation",  // Add AVFoundation if needed
                "VideoToolbox"
            });
            
            // Add system libraries
            PublicSystemLibraries.AddRange(new string[] {
                "iconv",    // Add iconv library
                "bz2",
                "z",
                "lzma",
                "m",
                "pthread"
            });
        }
	}
}
