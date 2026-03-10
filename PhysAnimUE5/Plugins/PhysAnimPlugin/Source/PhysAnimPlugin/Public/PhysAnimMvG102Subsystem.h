#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PhysAnimMvG102Subsystem.generated.h"

class ACharacter;
class UPhysicsControlComponent;
class USkeletalMeshComponent;

UCLASS()
class PHYSANIMPLUGIN_API UPhysAnimMvG102Subsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	bool StartControlPathTest();
	void StopControlPathTest(bool bResetTarget);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	ACharacter* ResolvePlayerCharacter() const;
	USkeletalMeshComponent* ResolveCharacterMesh(ACharacter* Character) const;
	bool EnsureHarnessCreated(ACharacter* Character, USkeletalMeshComponent* MeshComponent);
	void UpdateControlTarget(float DeltaTime);
	void ResetControlTarget();
	void LogStatus(const FString& Message) const;

	TWeakObjectPtr<ACharacter> ControlledCharacter;
	TWeakObjectPtr<USkeletalMeshComponent> ControlledMesh;
	TObjectPtr<UPhysicsControlComponent> ControlComponent = nullptr;

	FVector InitialHandLocation = FVector::ZeroVector;
	FRotator InitialHandOrientation = FRotator::ZeroRotator;
	float ElapsedTimeSeconds = 0.0f;
	bool bTestActive = false;
};
