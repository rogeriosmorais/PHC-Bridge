#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PhysAnimMvG103Subsystem.generated.h"

class ACharacter;
class UPhysicsControlComponent;
class USkeletalMeshComponent;

UCLASS()
class PHYSANIMPLUGIN_API UPhysAnimMvG103Subsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	bool StartSmokeTest();
	void StopSmokeTest(bool bResetTargets);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	ACharacter* ResolvePlayerCharacter() const;
	USkeletalMeshComponent* ResolveCharacterMesh(ACharacter* Character) const;
	bool EnsureHarnessCreated(ACharacter* Character, USkeletalMeshComponent* MeshComponent);
	void ActivateVisualPhysicsState(USkeletalMeshComponent* MeshComponent);
	void ResetVisualPhysicsState(USkeletalMeshComponent* MeshComponent);
	void UpdatePoseTargets(float DeltaTime);
	void ResetPoseTargets();
	void LogStatus(const FString& Message) const;

	TWeakObjectPtr<ACharacter> ControlledCharacter;
	TWeakObjectPtr<USkeletalMeshComponent> ControlledMesh;
	TObjectPtr<UPhysicsControlComponent> ControlComponent = nullptr;

	float ElapsedTimeSeconds = 0.0f;
	bool bTestActive = false;
};
