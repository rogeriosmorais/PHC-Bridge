#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UPhysAnimComponent* FindStandingComponent(UWorld* World)
{
	for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			return *It;
		}
	}
	return nullptr;
}

class FValidateStandingActivationIntegrationCommand final : public IAutomationLatentCommand
{
public:
	explicit FValidateStandingActivationIntegrationCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* const World = GEditor ? GEditor->PlayWorld : nullptr;
		UPhysAnimComponent* const Component = FindStandingComponent(World);
		if (!Component)
		{
			Test->AddError(TEXT("Standing activation component was not found in PIE."));
			return true;
		}

		const FPhysAnimStandingActivationStatus Status = Component->GetStandingActivationStatus();
		Test->TestEqual(TEXT("Standing activation reaches the locked final state"), Component->GetRuntimeState(), EPhysAnimRuntimeState::BalanceActive_Standing);
		Test->TestEqual(TEXT("Shared standing alpha reaches one"), Status.LinearBlendAlpha, 1.0f);
		Test->TestTrue(TEXT("Full simulation commit remains latched"), Status.bFullSimulationCommitted);
		Test->TestEqual(TEXT("All modifier readbacks remain simulated"), Status.ModifierSimulationMatchCount, 22);
		Test->TestEqual(TEXT("All raw Chaos bodies remain simulated"), Status.RawSimulationMatchCount, 22);
		Test->TestEqual(TEXT("All control gain readbacks remain matched"), Status.ControlGainMatchCount, 21);
		Test->TestTrue(
			TEXT("Atomic standing activation publishes the committed pelvis simulation state to target dispatch"),
			Component->WasPelvisSimulatingLastFrame());
		if (!Status.FailureReason.IsEmpty())
		{
			Test->AddError(FString::Printf(TEXT("Standing activation failure: %s"), *Status.FailureReason));
		}

		ACharacter* const Character = Cast<ACharacter>(Component->GetOwner());
		UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
		UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
		Test->TestNotNull(TEXT("Manny CharacterMovement exists"), Movement);
		Test->TestNotNull(TEXT("Manny capsule exists"), Capsule);
		if (Movement)
		{
			Test->TestFalse(TEXT("CharacterMovement is inactive"), Movement->IsActive());
			Test->TestFalse(TEXT("CharacterMovement tick is disabled"), Movement->IsComponentTickEnabled());
			Test->TestNull(TEXT("CharacterMovement has no updated component"), Movement->UpdatedComponent);
		}
		if (Capsule)
		{
			Test->TestEqual(TEXT("Capsule collision is disabled"), Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
		Test->TestNotNull(TEXT("Manny skeletal mesh exists"), Mesh);
		if (Mesh)
		{
			for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				const FBodyInstance* const Body = Mesh->GetBodyInstance(BoneName);
				if (Body)
				{
					Test->TestTrue(
						*FString::Printf(TEXT("Manny %s overrides contact correction velocity"), *BoneName.ToString()),
						Body->GetOverrideMaxDepenetrationVelocity());
					Test->TestEqual(
						*FString::Printf(TEXT("Manny %s bounds contact correction velocity"), *BoneName.ToString()),
						Body->GetMaxDepenetrationVelocity(),
						200.0f);
				}
			}
			const FBodyInstance* const FootBody = Mesh->GetBodyInstance(TEXT("foot_l"));
			const FBodyInstance* const BallBody = Mesh->GetBodyInstance(TEXT("ball_l"));
			Test->TestNotNull(TEXT("Manny left foot body exists"), FootBody);
			Test->TestNotNull(TEXT("Manny left ball body exists"), BallBody);
			if (FootBody)
			{
				Test->TestEqual(TEXT("Manny foot body remains simulation-enabled"), FootBody->GetCollisionEnabled(false), ECollisionEnabled::QueryAndPhysics);
				Test->TestEqual(TEXT("Manny foot shape remains load-bearing"), FootBody->GetShapeCollisionEnabled(0), ECollisionEnabled::QueryAndPhysics);
			}
			if (BallBody)
			{
				Test->TestEqual(TEXT("Manny ball body stays simulation-enabled"), BallBody->GetCollisionEnabled(false), ECollisionEnabled::QueryAndPhysics);
				Test->TestEqual(TEXT("Manny ball shape is removed from contact only"), BallBody->GetShapeCollisionEnabled(0), ECollisionEnabled::NoCollision);
				Test->TestTrue(TEXT("Manny ball body remains simulated"), BallBody->IsInstanceSimulatingPhysics());
			}
		}
		Test->TestFalse(TEXT("Standing never owns an explicit shell lock"), Component->HasExplicitTransitionOwnedShellLock());
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingActivationIntegrationTest,
	"PhysAnim.Component.StandingActivation.MannyIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingActivationIntegrationTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"), true))
	{
		AddError(TEXT("Failed to open Manny PIE map."));
		return false;
	}
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FUntilCommand(
		[]() -> bool { return GEditor && IsValid(GEditor->PlayWorld); },
		[this]() -> bool { AddError(TEXT("PIE did not start.")); return true; },
		5.0f));
	AddCommand(new FUntilCommand(
		[]() -> bool
		{
			if (UPhysAnimComponent* const Component = FindStandingComponent(GEditor ? GEditor->PlayWorld : nullptr))
			{
				return Component->GetRuntimeState() == EPhysAnimRuntimeState::BalanceActive_Standing ||
					Component->GetRuntimeState() == EPhysAnimRuntimeState::FailStopped;
			}
			return false;
		},
		[this]() -> bool { AddError(TEXT("Standing activation did not reach a terminal development state.")); return true; },
		10.0f));
	AddCommand(new FValidateStandingActivationIntegrationCommand(this));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

#endif
