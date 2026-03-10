#pragma once

#include "Animation/AnimInstance.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchResult.h"

#include "PhysAnimAnimInstance.generated.h"

UCLASS(BlueprintType, Blueprintable)
class PHYSANIMPLUGIN_API UPhysAnimAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PhysAnim|PoseSearch")
	void SetStage1MotionMatchingNode(const FMotionMatchingAnimNodeReference& InMotionMatchingNode);

	UFUNCTION(BlueprintCallable, Category = "PhysAnim|PoseSearch")
	void ClearStage1MotionMatchingNode();

	bool GetStage1MotionMatchResult(FPoseSearchBlueprintResult& OutResult) const;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PhysAnim|PoseSearch")
	FMotionMatchingAnimNodeReference Stage1MotionMatchingNode;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PhysAnim|PoseSearch")
	bool bHasStage1MotionMatchingNode = false;
};
