// Georgy Treshchev 2021.

using UnrealBuildTool;

public class RuntimeFilesDownloader : ModuleRules
{
	public RuntimeFilesDownloader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"HTTP"
			}
			);



		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine"
			}
			);
			
	}
}
