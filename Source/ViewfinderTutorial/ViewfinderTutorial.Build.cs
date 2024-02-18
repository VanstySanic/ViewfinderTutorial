// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ViewfinderTutorial : ModuleRules
{
	public ViewfinderTutorial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput" });
		PublicDependencyModuleNames.AddRange(new string[] { "GeometryScriptingCore", "GeometryFramework" });
	}
}
