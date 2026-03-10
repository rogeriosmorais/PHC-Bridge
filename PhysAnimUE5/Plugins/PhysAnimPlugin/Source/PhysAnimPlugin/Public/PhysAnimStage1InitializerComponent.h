#pragma once

#include "Components/ActorComponent.h"
#include "PhysicsControlActor.h"

#include "PhysAnimStage1InitializerComponent.generated.h"

class UPhysicsControlComponent;

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent, DisplayName = "PhysAnim Stage1 Physics Control Initializer"))
class PHYSANIMPLUGIN_API UPhysAnimStage1InitializerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhysAnimStage1InitializerComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "PhysicsControl")
	void CreateControls(UPhysicsControlComponent* PhysicsControlComponent);

	void PrepareRuntimeDefaults();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsControls")
	TMap<FName, FInitialPhysicsControl> InitialControls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsControls")
	TMap<FName, FInitialBodyModifier> InitialBodyModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsControls")
	bool bCreateControlsAtBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Setup")
	TWeakObjectPtr<AActor> DefaultControlParentActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Setup")
	TWeakObjectPtr<AActor> DefaultControlChildActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Setup")
	TWeakObjectPtr<AActor> DefaultBodyModifierActor;

private:
	void CreateOrUpdateInitialControls(UPhysicsControlComponent* PhysicsControlComponent);
	void CreateOrUpdateInitialBodyModifiers(UPhysicsControlComponent* PhysicsControlComponent);
};
