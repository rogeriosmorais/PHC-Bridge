#include "PhysAnimComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Templates/Function.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr float GDefaultPresentationPerturbationLeadInSeconds = 1.0f;
	constexpr float GDefaultPresentationPerturbationObserveSeconds = 3.0f;
	constexpr float GDefaultPresentationPerturbationDurationSeconds =
		GDefaultPresentationPerturbationLeadInSeconds + GDefaultPresentationPerturbationObserveSeconds;

	static bool MatchesFilter(const UPhysAnimComponent& Component, const FString& FilterLower)
	{
		if (FilterLower.IsEmpty() || FilterLower == TEXT("<all>") || FilterLower == TEXT("all"))
		{
			return true;
		}

		const AActor* const Owner = Component.GetOwner();
		if (!Owner)
		{
			return false;
		}

		const FString OwnerNameLower = Owner->GetName().ToLower();
		const FString PathNameLower = Owner->GetPathName().ToLower();
		return OwnerNameLower.Contains(FilterLower) || PathNameLower.Contains(FilterLower);
	}

	static bool ResolveWorldFromConsole(UWorld* InWorld, UWorld*& OutWorld)
	{
		if (InWorld)
		{
			OutWorld = InWorld;
			return true;
		}

		if (!GEngine)
		{
			OutWorld = nullptr;
			return false;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				OutWorld = Context.World();
				return OutWorld != nullptr;
			}
		}

		OutWorld = nullptr;
		return false;
	}

	static void ForEachMatchingPhysAnimComponent(
		UWorld* World,
		const FString& FilterLower,
		TFunctionRef<void(UPhysAnimComponent&)> Visitor,
		int32& OutMatched)
	{
		OutMatched = 0;
		if (!World)
		{
			return;
		}

		for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
		{
			UPhysAnimComponent* const Component = *It;
			if (!IsValid(Component) || Component->GetWorld() != World)
			{
				continue;
			}

			if (!MatchesFilter(*Component, FilterLower))
			{
				continue;
			}

			++OutMatched;
			Visitor(*Component);
		}
	}

	static float ParseOptionalFloat(const TArray<FString>& Args, int32 Index, float DefaultValue)
	{
		if (!Args.IsValidIndex(Index))
		{
			return DefaultValue;
		}

		return FCString::Atof(*Args[Index]);
	}

	static FString ParseOptionalFilter(const TArray<FString>& Args, int32 Index)
	{
		return Args.IsValidIndex(Index) ? Args[Index].ToLower() : FString();
	}

	static void ApplyPresentationPerturbationCommand(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = nullptr;
		if (!ResolveWorldFromConsole(InWorld, World))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PhysAnim] pa.ApplyPresentationPerturbation failed: no active PIE/game world."));
			return;
		}

		const float DurationSeconds = FMath::Max(0.0f, ParseOptionalFloat(Args, 0, GDefaultPresentationPerturbationDurationSeconds));
		const FString FilterLower = ParseOptionalFilter(Args, 1);

		int32 Matched = 0;
		int32 Applied = 0;
		ForEachMatchingPhysAnimComponent(
			World,
			FilterLower,
			[&Applied, DurationSeconds](UPhysAnimComponent& Component)
			{
				Component.SetPresentationPerturbationOverrideSeconds(DurationSeconds);
				++Applied;
			},
			Matched);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[PhysAnim] pa.ApplyPresentationPerturbation duration=%.3fs matched=%d applied=%d filter='%s'"),
			DurationSeconds,
			Matched,
			Applied,
			FilterLower.IsEmpty() ? TEXT("<all>") : *FilterLower);
	}

	static void ClearPresentationPerturbationCommand(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = nullptr;
		if (!ResolveWorldFromConsole(InWorld, World))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PhysAnim] pa.ClearPresentationPerturbation failed: no active PIE/game world."));
			return;
		}

		const FString FilterLower = ParseOptionalFilter(Args, 0);

		int32 Matched = 0;
		int32 Cleared = 0;
		ForEachMatchingPhysAnimComponent(
			World,
			FilterLower,
			[&Cleared](UPhysAnimComponent& Component)
			{
				Component.ClearPresentationPerturbationOverride();
				++Cleared;
			},
			Matched);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[PhysAnim] pa.ClearPresentationPerturbation matched=%d cleared=%d filter='%s'"),
			Matched,
			Cleared,
			FilterLower.IsEmpty() ? TEXT("<all>") : *FilterLower);
	}

	static void StartBalanceModeCommand(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = nullptr;
		if (!ResolveWorldFromConsole(InWorld, World)) return;

		int32 Matched = 0;
		ForEachMatchingPhysAnimComponent(World, ParseOptionalFilter(Args, 0), [](UPhysAnimComponent& C) { C.StartBalancePerturbationMode(); }, Matched);
		UE_LOG(LogTemp, Log, TEXT("[PhysAnim] pa.StartBalanceMode matched=%d"), Matched);
	}

	static void StopBalanceModeCommand(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = nullptr;
		if (!ResolveWorldFromConsole(InWorld, World)) return;

		int32 Matched = 0;
		ForEachMatchingPhysAnimComponent(World, ParseOptionalFilter(Args, 0), [](UPhysAnimComponent& C) { C.StopBalancePerturbationMode(); }, Matched);
		UE_LOG(LogTemp, Log, TEXT("[PhysAnim] pa.StopBalanceMode matched=%d"), Matched);
	}

	static FAutoConsoleCommandWithWorldAndArgs GApplyPresentationPerturbationCommand(
		TEXT("pa.ApplyPresentationPerturbation"),
		TEXT("Applies the existing component-side presentation perturbation override. Optional args: [durationSeconds] [ownerFilter]. Defaults to the same 4.0s presentation window used by the comparison subsystem."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ApplyPresentationPerturbationCommand));

	static FAutoConsoleCommandWithWorldAndArgs GClearPresentationPerturbationCommand(
		TEXT("pa.ClearPresentationPerturbation"),
		TEXT("Clears the component-side presentation perturbation override. Optional args: [ownerFilter]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearPresentationPerturbationCommand));

	static FAutoConsoleCommandWithWorldAndArgs GStartBalanceModeCommand(
		TEXT("pa.StartBalanceMode"),
		TEXT("Starts the dedicated Balance Perturbation Mode scenarios. Optional args: [ownerFilter]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartBalanceModeCommand));

	static FAutoConsoleCommandWithWorldAndArgs GStopBalanceModeCommand(
		TEXT("pa.StopBalanceMode"),
		TEXT("Stops the Balance Perturbation Mode. Optional args: [ownerFilter]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StopBalanceModeCommand));
}
