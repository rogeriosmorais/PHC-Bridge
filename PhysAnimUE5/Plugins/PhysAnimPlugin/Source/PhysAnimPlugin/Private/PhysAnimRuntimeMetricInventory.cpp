#include "PhysAnimRuntimeMetricInventory.h"

#include <initializer_list>

namespace
{
	FPhysAnimRuntimeMetricFieldMapping MakeMapping(
		const TCHAR* ArtifactFieldName,
		std::initializer_list<const TCHAR*> SourceProvenance,
		std::initializer_list<const TCHAR*> TestCoverageNames)
	{
		FPhysAnimRuntimeMetricFieldMapping Mapping;
		Mapping.ArtifactFieldName = ArtifactFieldName;
		for (const TCHAR* Item : SourceProvenance)
		{
			Mapping.SourceProvenance.Add(Item);
		}
		for (const TCHAR* Item : TestCoverageNames)
		{
			Mapping.TestCoverageNames.Add(Item);
		}
		return Mapping;
	}
}

namespace PhysAnimRuntimeMetricInventory
{
	FPhysAnimRuntimeMetricInventory BuildExistingCounterInventory()
	{
		FPhysAnimRuntimeMetricInventory Inventory;
		Inventory.bGameplayBehaviorChanges = false;
		Inventory.Segments.Reserve(5);

		{
			FPhysAnimRuntimeMetricSegmentInventory Segment;
			Segment.Segment = EPhysAnimEvidenceBaselineSegment::PoseSearch;
			Segment.SegmentName = TEXT("PoseSearch");
			Segment.bRequiresNewTelemetry = false;
			Segment.ExistingCounterMappings.Add(FPhysAnimRuntimeMetricFieldMapping(
				TEXT("PoseSearchQueryCount"),
				{ TEXT("PhysAnimComponent.Core.cpp::UPhysAnimComponent::TickComponent"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidencePoseSearchQueryResult") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidencePoseSearchCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(FPhysAnimRuntimeMetricFieldMapping(
				TEXT("PoseSearchValidResultCount"),
				{ TEXT("PhysAnimComponent.Core.cpp::UPhysAnimComponent::TickComponent"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidencePoseSearchQueryResult") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidencePoseSearchCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(FPhysAnimRuntimeMetricFieldMapping(
				TEXT("PoseSearchSelectedAnimationName"),
				{ TEXT("PhysAnimComponent.Core.cpp::UPhysAnimComponent::TickComponent"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidencePoseSearchQueryResult") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidencePoseSearchCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(FPhysAnimRuntimeMetricFieldMapping(
				TEXT("PoseSearchSelectedTime"),
				{ TEXT("PhysAnimComponent.Core.cpp::UPhysAnimComponent::TickComponent"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidencePoseSearchQueryResult") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidencePoseSearchCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(FPhysAnimRuntimeMetricFieldMapping(
				TEXT("PoseSearchConsecutiveInvalidFrameCount"),
				{ TEXT("PhysAnimComponent.Core.cpp::UPhysAnimComponent::TickComponent"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidencePoseSearchQueryResult") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidencePoseSearchCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Inventory.Segments.Add(MoveTemp(Segment));
		}

		{
			FPhysAnimRuntimeMetricSegmentInventory Segment;
			Segment.Segment = EPhysAnimEvidenceBaselineSegment::PhcPolicy;
			Segment.SegmentName = TEXT("PhcPolicy");
			Segment.bRequiresNewTelemetry = false;
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("PolicyInferenceSuccessCount"),
				{ TEXT("PhysAnimComponent.Inference.cpp::RunInference"), TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("PolicyActionSampleCount"),
				{ TEXT("PhysAnimComponent.Inference.cpp::ConditionModelActions"), TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Inventory.Segments.Add(MoveTemp(Segment));
		}

		{
			FPhysAnimRuntimeMetricSegmentInventory Segment;
			Segment.Segment = EPhysAnimEvidenceBaselineSegment::PhysicsControl;
			Segment.SegmentName = TEXT("PhysicsControl");
			Segment.bRequiresNewTelemetry = false;
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("ControlTargetTotalWrites"),
				{ TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::TickLiveRuntimeEvidenceProof") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("ControlTargetNormalWrites"),
				{ TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::TickLiveRuntimeEvidenceProof") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Inventory.Segments.Add(MoveTemp(Segment));
		}

		{
			FPhysAnimRuntimeMetricSegmentInventory Segment;
			Segment.Segment = EPhysAnimEvidenceBaselineSegment::Chaos;
			Segment.SegmentName = TEXT("Chaos");
			Segment.bRequiresNewTelemetry = false;
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("RuntimeBodySampleCount"),
				{ TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::TickLiveRuntimeEvidenceProof") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("RuntimeSimulatingBodyCount"),
				{ TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::TickLiveRuntimeEvidenceProof") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("bPhysicalContinuityValidatorPassed"),
				{ TEXT("PhysAnimValidators.cpp::ValidateContinuity"), TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot") },
				{ TEXT("PhysAnim.Validators.Continuity.ValidateContinuity"), TEXT("PhysAnim.EvidenceSummary.WriterSuccess") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("bContinuityBookkeepingMismatch"),
				{ TEXT("PhysAnimValidators.cpp::ValidateContinuity"), TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot") },
				{ TEXT("PhysAnim.Validators.ContinuitySnapshot"), TEXT("PhysAnim.EvidenceSummary.WriterSuccess") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("SupportMode"),
				{ TEXT("PhysAnimRuntimeAdapter.cpp::BuildSupportObservationFromHits"), TEXT("PhysAnimProofArtifactEmitter.cpp::PhysAnimProof_BuildEvidenceBaselineInput") },
				{ TEXT("PhysAnim.SupportTruth.ClassifySupportMode"), TEXT("PhysAnim.EvidenceSummary.WriterSuccess") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("ActiveSupportSideCount"),
				{ TEXT("PhysAnimRuntimeAdapter.cpp::CaptureSupportSnapshotFromHits"), TEXT("PhysAnimValidators.cpp::ValidateSupport") },
				{ TEXT("PhysAnim.SupportTruth.BuildFrameHull"), TEXT("PhysAnim.RuntimeAdapter.SupportHits") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("SupportHullAreaCm2"),
				{ TEXT("PhysAnimRuntimeAdapter.cpp::BuildSupportObservationFromHits"), TEXT("PhysAnimValidators.cpp::ValidateSupport") },
				{ TEXT("PhysAnim.SupportTruth.BuildFrameHull"), TEXT("PhysAnim.RuntimeAdapter.SupportObservationArtifact") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("SupportGapTimerMs"),
				{ TEXT("PhysAnimRuntimeAdapter.cpp::CaptureSupportSnapshotFromHits"), TEXT("PhysAnimValidators.cpp::ValidateSupport") },
				{ TEXT("PhysAnim.SupportTruth.AdjudicateProxy"), TEXT("PhysAnim.EvidenceSummary.WriterSuccess") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("ProxyInsideHull"),
				{ TEXT("PhysAnimRuntimeAdapter.cpp::BuildSupportObservationFromHits"), TEXT("PhysAnimValidators.cpp::ValidateSupport") },
				{ TEXT("PhysAnim.SupportTruth.AdjudicateProxy"), TEXT("PhysAnim.RuntimeAdapter.SupportObservationArtifact") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("bCalfContactTerminal"),
				{ TEXT("PhysAnimProofArtifactEmitter.cpp::PhysAnimProof_BuildEvidenceBaselineInput"), TEXT("PhysAnimProofArtifactEmitter.cpp::PhysAnimProof_ArtifactToJson") },
				{ TEXT("PhysAnim.EvidenceSummary.WriterSuccess"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Inventory.Segments.Add(MoveTemp(Segment));
		}

		{
			FPhysAnimRuntimeMetricSegmentInventory Segment;
			Segment.Segment = EPhysAnimEvidenceBaselineSegment::RendererFacingMotion;
			Segment.SegmentName = TEXT("RendererFacingMotion");
			Segment.bRequiresNewTelemetry = false;
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("RendererFacingMotionSampleCount"),
				{ TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidenceRendererFacingMotionSample") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidenceRendererFacingMotionCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("RendererFacingMotionActiveSampleCount"),
				{ TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics"), TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::RecordLiveRuntimeEvidenceRendererFacingMotionSample") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidenceRendererFacingMotionCounterCapture"), TEXT("PhysAnim.EvidenceSummary.ProofEmitterHook") }));
			Segment.ExistingCounterMappings.Add(MakeMapping(
				TEXT("RendererFacingMotionMaxRootWorldPositionDriftCm"),
				{ TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics"), TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot") },
				{ TEXT("PhysAnim.Component.LiveRuntimeEvidenceRendererFacingMotionCounterCapture"), TEXT("PhysAnim.Validators.BuildRunArtifactSnapshot") }));
			Inventory.Segments.Add(MoveTemp(Segment));
		}

		return Inventory;
	}
}
