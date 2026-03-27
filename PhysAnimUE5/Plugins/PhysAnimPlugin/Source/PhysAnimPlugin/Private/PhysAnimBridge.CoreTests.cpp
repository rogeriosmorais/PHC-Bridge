#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimBridge;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFutureScheduleTest,
		"PhysAnim.Bridge.FutureSchedule",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFutureScheduleTest::RunTest(const FString& Parameters)
	{
		const TArray<float> Schedule = BuildFutureSampleTimeSchedule();
		TestEqual(TEXT("Future step count"), Schedule.Num(), NumFutureSteps);
		TestEqual(TEXT("First future step"), Schedule[0], FutureStepSeconds);
		TestEqual(TEXT("Last future step"), Schedule.Last(), NumFutureSteps * FutureStepSeconds);
		TestEqual(
			TEXT("Unclamped future target time keeps the requested offset"),
			ResolveFutureTargetTimeSeconds(0.5f, 0.2f, 2.0f),
			0.2f);
		TestEqual(
			TEXT("Clamped future target time shrinks at animation end"),
			ResolveFutureTargetTimeSeconds(1.9f, 0.4f, 2.0f),
			0.1f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTensorIndexMapTest,
		"PhysAnim.Bridge.TensorIndexMap",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTensorIndexMapTest::RunTest(const FString& Parameters)
	{
		TArray<UE::NNE::FTensorDesc> TensorDescs;
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({1, SelfObsSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("mimic_target_poses"), UE::NNE::FSymbolicTensorShape::Make({1, MimicTargetPosesSize}), ENNETensorDataType::Float));

		FPhysAnimTensorIndexMap IndexMap;
		FString Error;
		TestTrue(TEXT("Tensor map should succeed"), BuildInputTensorIndexMap(TensorDescs, IndexMap, Error));
		TestEqual(TEXT("self_obs index"), IndexMap.SelfObs, 1);
		TestEqual(TEXT("mimic_target_poses index"), IndexMap.MimicTargetPoses, 2);
		TestEqual(TEXT("terrain index"), IndexMap.Terrain, 0);

		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
		TestFalse(TEXT("Duplicate tensor map should fail"), BuildInputTensorIndexMap(TensorDescs, IndexMap, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSmplOrderContractTest,
		"PhysAnim.Bridge.SmplOrderContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSmplOrderContractTest::RunTest(const FString& Parameters)
	{
		const TArray<FName>& Bones = GetSmplObservationBoneNames();
		TestEqual(TEXT("SMPL body count"), Bones.Num(), NumSmplBodies);
		if (Bones.Num() >= NumSmplBodies)
		{
			TestEqual(TEXT("Index 0: Pelvis"), Bones[0], TEXT("pelvis"));
			TestEqual(TEXT("Index 1: L_Hip"), Bones[1], TEXT("thigh_l"));
			TestEqual(TEXT("Index 3: L_Ankle"), Bones[3], TEXT("foot_l"));
			TestEqual(TEXT("Index 9: Torso"), Bones[9], TEXT("spine_01"));
			TestEqual(TEXT("Index 23: R_Hand"), Bones[23], TEXT("hand_r"));
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFrameConversionTest,
		"PhysAnim.Bridge.FrameConversion",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFrameConversionTest::RunTest(const FString& Parameters)
	{
		const FVector SmplUp(0.0, 1.0, 0.0);
		const FVector UeUp = SmplVectorToUe(SmplUp);
		TestTrue(TEXT("SMPL local authoring Y-up becomes UE Z-up"), UeUp.Equals(FVector(0.0, 0.0, 1.0), KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity SMPL local rotation stays identity after UE local basis conversion"),
			SmplQuaternionToUe(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity UE local rotation stays identity after SMPL local basis conversion"),
			UeQuaternionToSmpl(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));

		const FQuat SmplRotation = ExpMapToQuaternion(FVector(0.2, -0.1, 0.3));
		const FQuat RoundTrip = UeQuaternionToSmpl(SmplQuaternionToUe(SmplRotation));
		TestTrue(TEXT("Quaternion roundtrip should stay close"), RoundTrip.Equals(SmplRotation, 1.0e-3f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionToBoneMappingContractTest,
		"PhysAnim.Bridge.ActionToBoneMappingContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionToBoneMappingContractTest::RunTest(const FString& Parameters)
	{
		TArray<float> Actions;
		Actions.Init(0.0f, NumActionFloats);
		
		// Joint 0 in Smpl is L_Hip -> thigh_l
		Actions[0] = 0.5f; // X axis in Smpl
		
		TMap<FName, FQuat> ControlRotations;
		FString Error;
		TestTrue(TEXT("Action conversion succeed"), ConvertModelActionsToControlRotations(Actions, ControlRotations, Error));
		
		TestTrue(TEXT("thigh_l target exists"), ControlRotations.Contains(TEXT("thigh_l")));
		if (ControlRotations.Contains(TEXT("thigh_l")))
		{
			const FQuat ThighRot = ControlRotations[TEXT("thigh_l")];
			TestFalse(TEXT("thigh_l rotation is non-identity"), ThighRot.IsIdentity());
		}

		// Distal Collapse Verification: Joint 16 (L_Wrist) and 17 (L_Hand)
		Actions.Init(0.0f, NumActionFloats);
		Actions[16 * 3 + 1] = 0.1f; // wrist Y
		Actions[17 * 3 + 1] = 0.1f; // hand Y
		
		ControlRotations.Reset();
		TestTrue(TEXT("Collapse conversion succeed"), ConvertModelActionsToControlRotations(Actions, ControlRotations, Error));
		TestTrue(TEXT("hand_l target exists"), ControlRotations.Contains(TEXT("hand_l")));
		TestFalse(TEXT("No standalone wrist target"), ControlRotations.Contains(TEXT("wrist_l")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSelfObservationContractTest,
		"PhysAnim.Bridge.SelfObservationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSelfObservationContractTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodySample> BodySamples;
		BodySamples.SetNum(NumSmplBodies);
		for (int32 i = 0; i < NumSmplBodies; ++i)
		{
			BodySamples[i].Position = FVector::ZeroVector;
			BodySamples[i].Rotation = FQuat::Identity;
			BodySamples[i].LinearVelocity = FVector::ZeroVector;
			BodySamples[i].AngularVelocity = FVector::ZeroVector;
		}

		// Root height test (Pelvis is at 100cm, Ground at 0cm)
		BodySamples[0].Position = FVector(0, 0, 100);
		
		TArray<float> SelfObs;
		FString Error;
		TestTrue(TEXT("SelfObs build succeed"), BuildSelfObservation(BodySamples, 0.0f, SelfObs, Error));
		TestEqual(TEXT("SelfObs size"), SelfObs.Num(), SelfObsSize);
		
		// Index 0 in self_obs is root height relative to ground (meters)
		TestEqual(TEXT("Root height (100cm -> 1.0)"), SelfObs[0], 1.0f);

		// Thigh_l (Index 1) position relative to root
		BodySamples[1].Position = FVector(0, 50, 100); // 50cm to the right
		SelfObs.Reset();
		TestTrue(TEXT("Relative pos build succeed"), BuildSelfObservation(BodySamples, 0.0f, SelfObs, Error));
		
		// Relative positions start at index 1. Index 1: X, 2: Y, 3: Z in SMPL basis.
		// Ue(0, 50, 0) -> Smpl(50, 0, 0) -> Meters(0.5, 0.0, 0.0)
		TestEqual(TEXT("Thigh_l relative X (meters)"), SelfObs[1], 0.5f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMimicTargetPosesContractTest,
		"PhysAnim.Bridge.MimicTargetPosesContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMimicTargetPosesContractTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodySample> CurrentSamples;
		CurrentSamples.SetNum(NumSmplBodies);
		for (int32 i = 0; i < NumSmplBodies; ++i) { CurrentSamples[i] = {FVector::ZeroVector, FQuat::Identity}; }

		TArray<FPhysAnimFuturePoseSample> FutureSamples;
		for (int32 i = 0; i < NumFutureSteps; ++i)
		{
			FPhysAnimFuturePoseSample Sample;
			Sample.FutureTimeSeconds = (i + 1) * FutureStepSeconds;
			Sample.BodyTransforms.SetNum(NumSmplBodies);
			for (int32 b = 0; b < NumSmplBodies; ++b) { Sample.BodyTransforms[b] = FTransform::Identity; }
			FutureSamples.Add(Sample);
		}

		// Move future pelvis to verify delta packing (Frame 0 -> Frame 1)
		FutureSamples[0].BodyTransforms[0].SetLocation(FVector(10, 0, 0)); // 10cm forward

		TArray<float> MimicObs;
		FString Error;
		TestTrue(TEXT("Mimic build succeed"), BuildMimicTargetPoses(CurrentSamples, FutureSamples, MimicObs, Error));
		TestEqual(TEXT("Mimic size"), MimicObs.Num(), MimicTargetPosesSize);

		// First block is relative positions to PREVIOUS frame. 
		// Ue(10, 0, 0) -> Smpl(0, 0, 10) -> Meters(0.0, 0.0, 0.1)
		// Index 0: X, 1: Y, 2: Z
		TestEqual(TEXT("Future Pelvis relative Z (meters)"), MimicObs[2], 0.1f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTerrainObservationContractTest,
		"PhysAnim.Bridge.TerrainObservationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTerrainObservationContractTest::RunTest(const FString& Parameters)
	{
		TArray<float> Heights;
		Heights.Init(0.0f, TerrainSize);
		Heights[0] = -50.0f; // 50cm below ground

		TArray<float> Terrain;
		FString Error;
		// Character root at 100cm. Sample 0 is at -50cm. Delta is 150cm -> 1.5m
		TestTrue(TEXT("Terrain build succeed"), BuildTerrainObservation(100.0f, Heights, Terrain, Error));
		TestEqual(TEXT("Sample 0 height (meters)"), Terrain[0], 1.5f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBalanceStatelessTests,
		"PhysAnim.Bridge.BalanceStateless",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBalanceStatelessTests::RunTest(const FString& Parameters)
	{
		auto GetDefaultSettings = []()
		{
			FPhysAnimStabilizationSettings S;
			S.MaxRootLinearSpeedCmPerSecond = 100.0f;
			S.MaxRootAngularSpeedDegPerSecond = 45.0f;
			S.BalanceEntryMaxGroundDistanceCm = 15.0f;
			S.BalancePhase2EntryMaxRootTiltDeg = 20.0f;
			S.BalancePhase2EntryMaxShellOffsetDelta = 5.0f;
			S.BalancePhase2EntryMaxShellVelocityDelta = 10.0f;
			S.BalancePhase2EntryMaxTargetDeltaDeg = 15.0f;
			return S;
		};

		auto GetStableDomain = []()
		{
			FPhysAnimStabilizationDomain D;
			D.bRootSimulating = true;
			D.RootLinearSpeed = 10.0f;
			D.RootAngularSpeed = 5.0f;
			D.RootGroundDistance = 5.0f;
			D.RootTiltDeg = 2.0f;
			D.ShellPlanarOffsetCm = 1.0f;
			D.ShellPlanarVelocityCmPerSec = 2.0f;
			D.MaxTargetDeltaDegrees = 3.0f;
			D.MeanTargetDeltaDegrees = 1.0f;
			return D;
		};

		FString Reason;
		TestTrue(TEXT("Baseline stable domain"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(GetStableDomain(), GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("RootOn resolver reports pelvis-spine margin insufficiency"),
			FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
				EBalanceReadyRootOnReadinessClassification::RootCoupledReady,
				true,
				true,
				true,
				false,
				true,
				true,
				true,
				true,
				true,
				false,
				1.0f),
			TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient"));
		TestEqual(
			TEXT("RootOn resolver reports pelvis-thigh margin insufficiency"),
			FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
				EBalanceReadyRootOnReadinessClassification::RootCoupledReady,
				true,
				true,
				false,
				true,
				true,
				true,
				true,
				true,
				true,
				false,
				1.0f),
			TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient"));
		TestEqual(
			TEXT("RootOn resolver reports tilt-limited viability before generic angular incoherence"),
			FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
				EBalanceReadyRootOnReadinessClassification::RootCoupledReady,
				true,
				false,
				true,
				true,
				true,
				true,
				true,
				true,
				true,
				true,
				1.0f),
			TEXT("phase1_root_on_readiness_tilt_limited_viability"));
		FPhysAnimStabilizationDomain Phase2TopologyLoss = GetStableDomain();
		Phase2TopologyLoss.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		Phase2TopologyLoss.CertifiedSimCount = 6;
		Phase2TopologyLoss.SimCount = 5;
		Phase2TopologyLoss.CertifiedDistalSimCount = 0;
		Phase2TopologyLoss.DistalSimCount = 0;
		TestFalse(
			TEXT("Phase 2 topology loss fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase2TopologyLoss, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 2 topology loss uses the RootOn-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase2TopologyNotPreserved);
		FPhysAnimStabilizationDomain Phase2RootDropped = GetStableDomain();
		Phase2RootDropped.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		Phase2RootDropped.bRootSimulating = false;
		TestFalse(
			TEXT("Phase 2 root simulation drop fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase2RootDropped, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 2 root simulation drop uses the RootOn-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase2RootSimulationDropped);
		FPhysAnimStabilizationDomain Phase2Instability = GetStableDomain();
		Phase2Instability.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		Phase2Instability.RootLinearSpeed = 300.0f;
		TestFalse(
			TEXT("Phase 2 instability fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase2Instability, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 2 instability uses the RootOn-specific precursor reason"),
			Reason,
			BalanceReadinessReasons::Phase2FailStopPrecursor);
		FPhysAnimStabilizationDomain Phase3TopologyRegression = GetStableDomain();
		Phase3TopologyRegression.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3TopologyRegression.CertifiedSimCount = 6;
		Phase3TopologyRegression.SimCount = 5;
		Phase3TopologyRegression.CertifiedDistalSimCount = 0;
		Phase3TopologyRegression.DistalSimCount = 0;
		TestFalse(
			TEXT("Phase 3 topology regression fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3TopologyRegression, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 topology regression uses the Settle-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase3TopologyRegressed);
		FPhysAnimStabilizationDomain Phase3RootDropped = GetStableDomain();
		Phase3RootDropped.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3RootDropped.bRootSimulating = false;
		TestFalse(
			TEXT("Phase 3 root simulation drop fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3RootDropped, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 root simulation drop uses the Settle-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase3RootSimulationDropped);
		FPhysAnimStabilizationDomain Phase3Instability = GetStableDomain();
		Phase3Instability.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3Instability.RootLinearSpeed = 300.0f;
		TestFalse(
			TEXT("Phase 3 instability fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3Instability, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 instability uses the Settle-specific instability reason"),
			Reason,
			BalanceReadinessReasons::Phase3InstabilitySpike);

		TestEqual(
			TEXT("Phase 2 root simulation drop is owned by RootOn execution"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase2_root_simulation_dropped")),
			EBalanceReadyConditionOwner::Phase2RootOnExecution);
		TestEqual(
			TEXT("Phase 2 topology loss is owned by topology enforcement"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase2_topology_not_preserved")),
			EBalanceReadyConditionOwner::Phase2TopologyEnforcement);
		TestEqual(
			TEXT("Phase 3 post-root instability is owned by RootOn execution"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_post_root_on_instability")),
			EBalanceReadyConditionOwner::Phase2RootOnExecution);
		TestEqual(
			TEXT("Phase 3 shell correction is owned by shell maintenance"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_material_shell_correction")),
			EBalanceReadyConditionOwner::ShellAuthorityMaintenance);
		TestEqual(
			TEXT("Phase 3 timeout is owned by transition recovery"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_no_convergence_path")),
			EBalanceReadyConditionOwner::TransitionRecovery);
		TestEqual(
			TEXT("Current pelvis-spine readiness blocker is owned by Phase 1 topology shaping"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")),
			EBalanceReadyConditionOwner::Phase1TopologyShaping);
		TestTrue(
			TEXT("Direct current blocker is recognized as Phase 1 owned"),
			FPhysAnimBalanceReadyTransition::IsPhase1OwnedCondition(TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")));
		TestEqual(
			TEXT("Composite no-convergence reason preserves the underlying current blocker owner"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase1_no_convergence_path_phase1_root_on_readiness_pelvis_spine_margin_insufficient")),
			EBalanceReadyConditionOwner::Phase1TopologyShaping);
		TestTrue(
			TEXT("Composite no-convergence reason is still recognized as Phase 1 owned"),
			FPhysAnimBalanceReadyTransition::IsPhase1OwnedCondition(TEXT("phase1_no_convergence_path_phase1_root_on_readiness_pelvis_spine_margin_insufficient")));

		TestTrue(
			TEXT("Topology not preserved remains retryable as a failure class"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_topology_not_preserved")));
		TestFalse(
			TEXT("RootOn spike is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_root_on_spike")));
		TestFalse(
			TEXT("Phase 3 instability is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase3_post_root_on_instability")));
		TestFalse(
			TEXT("Phase 3 timeout is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase3_no_convergence_path")));
		TestFalse(
			TEXT("Current pelvis-spine readiness blocker is not retryable as a failure class"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")));
		TestFalse(
			TEXT("Automatic retry is denied for the direct current blocker"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient"),
				true,
				true,
				true,
				true,
				true));

		TestTrue(
			TEXT("Automatic retry requires the full retry gate set"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase2_topology_not_preserved"),
				true,
				true,
				true,
				true,
				true));
		TestFalse(
			TEXT("Automatic retry is denied for terminal RootOn spike even if gates are satisfied"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase2_root_on_spike"),
				true,
				true,
				true,
				true,
				true));
		TestFalse(
			TEXT("Automatic retry is denied when recovery gates are incomplete"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase2_topology_not_preserved"),
				true,
				false,
				true,
				true,
				true));
		return true;
	}
}
