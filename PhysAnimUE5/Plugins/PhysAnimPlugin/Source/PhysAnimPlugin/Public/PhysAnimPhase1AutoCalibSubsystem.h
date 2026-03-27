#pragma once

#include "PhysAnimComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include "PhysAnimPhase1AutoCalibSubsystem.generated.h"

UCLASS()
class PHYSANIMPLUGIN_API UPhysAnimPhase1AutoCalibSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	bool StartPhase1AutoCalib(const FPhase1AutoCalibRequest& Request, FString& OutError);
	void StopPhase1AutoCalib(const FString& Reason = TEXT("manual_stop"));
	bool IsPhase1AutoCalibActive() const { return bRunActive; }
	const FPhase1AutoCalibReport& GetLatestReport() const { return LatestReport; }
	const FString& GetLastError() const { return LastError; }

	static void BuildStageACandidates(int32 Seed, int32 MaxTrials, TArray<FPhase1AutoCalibParams>& OutCandidates);
	static void BuildStageBRefinementCandidates(const TArray<FPhase1AutoCalibTrialResult>& StageAResults, int32 MaxTrials, TArray<FPhase1AutoCalibParams>& OutCandidates);
	static void BuildStageCReproCandidates(const TArray<FPhase1AutoCalibTrialResult>& StageBResults, int32 MaxTrials, TArray<FPhase1AutoCalibParams>& OutCandidates);
	static bool AreTrialResultsReproducible(const TArray<FPhase1AutoCalibTrialResult>& Trials, float Epsilon = 1.0e-3f);

private:
	enum class EAutoCalibStage : uint8
	{
		Inactive,
		AwaitingReadiness,
		StageA,
		StageB,
		StageC,
		Completed,
		Failed
	};

	struct FPendingTrial
	{
		FPhase1AutoCalibParams Params;
		FString StageName;
		int32 RepetitionIndex = 0;
	};

	bool ResolveTargetComponent(const FString& OwnerFilter, UPhysAnimComponent*& OutComponent, FString& OutError) const;
	bool RunDeterminismPreflight(UPhysAnimComponent& Component, const FPhase1AutoCalibBaselineSnapshot& Baseline, FString& OutError) const;
	bool BeginNextTrial();
	void TickAwaitingReadiness();
	void TickActiveTrial();
	void FinalizeActiveTrial(bool bTimedOut);
	void AdvanceStageOrFinish();
	void UpdatePeakMetrics(const FPhase1AutoCalibLiveMetrics& Metrics);
	FPhase1AutoCalibTrialResult BuildTrialResult(bool bTimedOut) const;
	void FinalizeReport();
	void WriteArtifacts();
	FString BuildOutputDirectory() const;

	bool bRunActive = false;
	bool bTrialActive = false;
	EAutoCalibStage CurrentStage = EAutoCalibStage::Inactive;
	FPhase1AutoCalibRequest ActiveRequest;
	TWeakObjectPtr<UPhysAnimComponent> TargetComponent;
	FPhase1AutoCalibBaselineSnapshot BaselineSnapshot;
	TArray<FPendingTrial> PendingTrials;
	TArray<FPhase1AutoCalibTrialResult> StageAResults;
	TArray<FPhase1AutoCalibTrialResult> StageBResults;
	TArray<FPhase1AutoCalibTrialResult> StageCResults;
	FPendingTrial ActiveTrial;
	double ActiveTrialStartTimeSeconds = -1.0;
	double CurrentStageStartTimeSeconds = -1.0;
	double LastReadinessLogTimeSeconds = -1.0;
	FPhase1AutoCalibLiveMetrics ActiveTrialPeakMetrics;
	FString LastError;
	FPhase1AutoCalibReport LatestReport;
	FPhase1AutoCalibDeterminismFingerprint BaselineFingerprint;
	int32 NextTrialId = 0;
	float OriginalBalanceEntryMinPolicyAlpha = 0.9f;
};


