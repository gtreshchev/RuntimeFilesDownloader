// Georgy Treshchev 2024.

using UnrealBuildTool;

public class RuntimeFilesDownloaderEditor : ModuleRules
{
	public RuntimeFilesDownloaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd"
			}
		);
		
		// To access settings for adding the permissions needed for downloading files.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AndroidRuntimeSettings"
			}
		);
	}
}