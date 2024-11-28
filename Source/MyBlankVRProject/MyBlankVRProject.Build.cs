// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class MyBlankVRProject : ModuleRules
{
	public MyBlankVRProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Sockets", "Networking" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    
     string PlatformName = "mac";
     if (Target.Platform == UnrealTargetPlatform.Mac)
     {
       PlatformName = "mac";
     }
     else if (Target.Platform == UnrealTargetPlatform.Android)
     {
       PlatformName = "android";
     }
     
     // Base directory for ThirdParty libraries
     string ThirdPartyPath = Path.Combine(Path.Combine(ModuleDirectory, "../../ThirdParty/"), PlatformName);
     
     // libary paths
     string IncludePath = Path.Combine(ThirdPartyPath, "include");
     string LibPath = Path.Combine(ThirdPartyPath, "lib");

     // Add third party include paths
     PublicIncludePaths.Add(IncludePath);

     PublicSystemLibraryPaths.Add(LibPath);

     PublicDefinitions.Add("NDEBUG");


     // Log all public definitions
     foreach (string Definition in PublicDefinitions)
     {
         System.Console.WriteLine("Definition: " + Definition);
     }

     // Specify the FFmpeg libraries to link
     PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavcodec.a"));
     PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavformat.a"));
     PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libswscale.a"));
     PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libswresample.a"));
     PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavutil.a"));
     PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libx264.a"));
     /* PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libiconv.a")); */


     // Log target platform
     System.Console.WriteLine("Target.Platform: " + Target.Platform);
     System.Console.WriteLine("Target.MacPlatform: " + UnrealTargetPlatform.Mac);
     System.Console.WriteLine("Target.AndroidPlatform: " + UnrealTargetPlatform.Android);
     
     
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
     else if (Target.Platform == UnrealTargetPlatform.Android)
     {
         // Add system libraries
         PublicSystemLibraries.AddRange(new string[] {
             /* "iconv", */
             /* "c++_shared", */
             /* "log", */
             "z",
             "m",
             "c",
             /* "dl", */
             /* "GLESv2", */
             /* "EGL", */
             /* "OpenSLES" */
         });

     }

	}
}
