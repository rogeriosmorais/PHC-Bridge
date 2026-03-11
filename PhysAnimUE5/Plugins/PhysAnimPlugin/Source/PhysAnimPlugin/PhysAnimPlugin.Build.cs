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
                "PoseSearch"
            });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new[]
                {
                    "LevelEditor",
                    "UnrealEd"
                });
        }
    }
}
