#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

void UPhysAnimComponent::StartBridgeTraceSession()
{
	BridgeTraceWriter.Reset();
	CurrentBridgeTraceSessionId.Reset();
	BridgeTraceTickCounter = 0;
	BridgeTraceLastFlushTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	const EPhysAnimBridgeTraceOutputMode ResolvedMode = ResolveBridgeTraceOutputMode();
	if (ResolvedMode == EPhysAnimBridgeTraceOutputMode::Off)
	{
		return;
	}

	const UWorld* const World = GetWorld();
	const AActor* const OwnerActor = GetOwner();
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();

	FPhysAnimBridgeTraceSessionMetadata Metadata;
	Metadata.TraceVersion = 1;
	Metadata.SessionId = PhysAnimComponentInternal::BuildTraceSessionId(World, OwnerActor);
	Metadata.TimestampUtc = FDateTime::UtcNow().ToIso8601();
	Metadata.MapName = PhysAnimComponentInternal::GetTraceMapName(World);
	Metadata.ActorName = PhysAnimComponentInternal::GetTraceActorName(OwnerActor);
	Metadata.RuntimeState = GetRuntimeStateName(RuntimeState);
	Metadata.NneRuntimeName = ActiveRuntimeName;
	Metadata.ModelAssetPath = ModelDataAsset.ToSoftObjectPath().ToString();
	Metadata.PoseSearchDatabasePath = GetPathNameSafe(LoadedPoseSearchDatabase);
	Metadata.TraceMode = static_cast<int32>(ResolvedMode);
	Metadata.SampleEveryNthFrame = FMath::Max(BridgeTraceSampleEveryNthFrame, 1);
	Metadata.FlushIntervalSeconds = FMath::Max(BridgeTraceFlushIntervalSeconds, 0.1f);
	Metadata.bForceZeroActions = EffectiveSettings.bForceZeroActions;
	Metadata.ActionScale = EffectiveSettings.ActionScale;
	Metadata.ActionClampAbs = EffectiveSettings.ActionClampAbs;
	Metadata.ActionSmoothingAlpha = EffectiveSettings.ActionSmoothingAlpha;
	Metadata.StartupRampSeconds = EffectiveSettings.StartupRampSeconds;
	Metadata.PolicyControlRateHz = EffectiveSettings.PolicyControlRateHz;
	Metadata.MaxAngularStepDegreesPerSecond = EffectiveSettings.MaxAngularStepDegreesPerSecond;
	Metadata.AngularStrengthMultiplier = EffectiveSettings.AngularStrengthMultiplier;
	Metadata.AngularDampingRatioMultiplier = EffectiveSettings.AngularDampingRatioMultiplier;
	Metadata.AngularExtraDampingMultiplier = EffectiveSettings.AngularExtraDampingMultiplier;
	Metadata.bEnableInstabilityFailStop = EffectiveSettings.bEnableInstabilityFailStop;
	Metadata.MaxRootHeightDeltaCm = EffectiveSettings.MaxRootHeightDeltaCm;
	Metadata.MaxRootLinearSpeedCmPerSecond = EffectiveSettings.MaxRootLinearSpeedCmPerSecond;
	Metadata.MaxRootAngularSpeedDegPerSecond = EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	Metadata.InstabilityGracePeriodSeconds = EffectiveSettings.InstabilityGracePeriodSeconds;
	Metadata.StabilizationSummary = PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings);

	TSharedPtr<FPhysAnimBridgeTraceWriter> Writer = MakeShared<FPhysAnimBridgeTraceWriter>(
		PhysAnimComponentInternal::ToTraceMode(ResolvedMode));
	FString TraceError;
	if (!Writer->StartSession(PhysAnimComponentInternal::GetTraceRootPath(), Metadata, TraceError))
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] Bridge trace disabled: %s"), *TraceError);
		return;
	}

	BridgeTraceWriter = MoveTemp(Writer);
	CurrentBridgeTraceSessionId = Metadata.SessionId;
	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Bridge trace session started: %s"),
		*BridgeTraceWriter->GetSessionFolderPath());
	EmitBridgeTraceEvent(TEXT("trace_started"), TEXT("Bridge trace session started."));
}


void UPhysAnimComponent::StopBridgeTraceSession(const TCHAR* StopContext, const FString& Message)
{
	if (!BridgeTraceWriter.IsValid())
	{
		return;
	}

	EmitBridgeTraceEvent(TEXT("trace_stopped"), Message.IsEmpty() ? FString(StopContext) : Message);
	BridgeTraceWriter->Flush(true);
	BridgeTraceWriter->Shutdown();
	BridgeTraceWriter.Reset();
	CurrentBridgeTraceSessionId.Reset();
	BridgeTraceTickCounter = 0;
	BridgeTraceLastFlushTimeSeconds = -1.0;
}


void UPhysAnimComponent::FlushBridgeTrace(const bool bForce)
{
	if (!BridgeTraceWriter.IsValid())
	{
		return;
	}

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : BridgeTraceLastFlushTimeSeconds;
	if (!bForce)
	{
		const double FlushIntervalSeconds = FMath::Max(BridgeTraceFlushIntervalSeconds, 0.1f);
		if (BridgeTraceLastFlushTimeSeconds >= 0.0 &&
			(CurrentTimeSeconds - BridgeTraceLastFlushTimeSeconds) < FlushIntervalSeconds)
		{
			return;
		}
	}

	BridgeTraceWriter->Flush(bForce);
	BridgeTraceLastFlushTimeSeconds = CurrentTimeSeconds;
}


void UPhysAnimComponent::EmitBridgeTraceEvent(
	const TCHAR* EventType,
	const FString& Message,
	const FString& Error,
	const TCHAR* PreviousRuntimeState,
	const TCHAR* NewRuntimeState)
{
	if (!BridgeTraceWriter.IsValid() || !BridgeTraceWriter->IsEnabled())
	{
		return;
	}

	FPhysAnimBridgeTraceEvent Event;
	Event.SessionId = CurrentBridgeTraceSessionId;
	Event.EventTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Event.EventType = EventType;
	Event.RuntimeState = GetRuntimeStateName(RuntimeState);
	Event.Message = Message;
	Event.Error = Error;
	Event.PreviousRuntimeState = PreviousRuntimeState ? FString(PreviousRuntimeState) : FString();
	Event.NewRuntimeState = NewRuntimeState ? FString(NewRuntimeState) : FString();
	Event.NneRuntimeName = ActiveRuntimeName;
	Event.ModelAssetPath = GetPathNameSafe(LoadedModelData);
	Event.MapName = PhysAnimComponentInternal::GetTraceMapName(GetWorld());
	Event.ActorName = PhysAnimComponentInternal::GetTraceActorName(GetOwner());
	BridgeTraceWriter->AppendEvent(Event);
}
