// Respirant 2020.

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
        "HTTP",
				// ... add other public dependencies that you statically link with here ...
			}
			);



		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...
			}
			);
			
	}
}
