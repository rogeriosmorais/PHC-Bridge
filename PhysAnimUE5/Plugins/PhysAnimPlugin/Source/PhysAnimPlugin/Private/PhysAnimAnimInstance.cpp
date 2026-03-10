#include "PhysAnimAnimInstance.h"

void UPhysAnimAnimInstance::SetStage1MotionMatchingNode(const FMotionMatchingAnimNodeReference& InMotionMatchingNode)
{
	Stage1MotionMatchingNode = InMotionMatchingNode;
	bHasStage1MotionMatchingNode = true;
}

void UPhysAnimAnimInstance::ClearStage1MotionMatchingNode()
{
	Stage1MotionMatchingNode = FMotionMatchingAnimNodeReference();
	bHasStage1MotionMatchingNode = false;
}

bool UPhysAnimAnimInstance::GetStage1MotionMatchResult(FPoseSearchBlueprintResult& OutResult) const
{
	OutResult = FPoseSearchBlueprintResult();

	if (!bHasStage1MotionMatchingNode)
	{
		return false;
	}

	bool bIsResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(Stage1MotionMatchingNode, OutResult, bIsResultValid);
	return bIsResultValid;
}
