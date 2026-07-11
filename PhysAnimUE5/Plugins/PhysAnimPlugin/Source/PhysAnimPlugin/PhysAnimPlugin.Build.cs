using UnrealBuildTool;

public class PhysAnimPlugin : ModuleRules
{
    public PhysAnimPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "NNE",
                "PhysicsCore",
                "PhysicsControl",
                "PoseSearch",
                "Chooser",
                "Json"
            });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new[]
                {
                    "LevelEditor",
                    "UnrealEd",
                    "ImageWrapper"
                });
        }
    }
}
