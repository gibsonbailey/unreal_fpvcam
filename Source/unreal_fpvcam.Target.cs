// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class unreal_fpvcamTarget : TargetRules
{
	public unreal_fpvcamTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
  
		ExtraModuleNames.AddRange( new string[] { "unreal_fpvcam" } );
	}
}
