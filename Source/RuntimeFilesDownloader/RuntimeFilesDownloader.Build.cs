// Georgy Treshchev 2022.

using UnrealBuildTool;

public class RuntimeFilesDownloader : ModuleRules
{
	public RuntimeFilesDownloader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Core",
				"HTTP"
			}
		);
		
	}
}