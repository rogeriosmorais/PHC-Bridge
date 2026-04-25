#include "PhysAnimSupportTruth.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthHarnessTest,
		"PhysAnim.SupportTruth.Harness.CompilesAndRuns",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthHarnessTest::RunTest(const FString& Parameters)
	{
		TestTrue(TEXT("Support truth harness compiles and runs"), true);
		return true;
	}

	FPhysAnimSupportPoint2D MakeSupportPoint(const double X, const double Y, const FName BodyName = TEXT("LeftFoot"), const EPhysAnimSupportSide SupportSide = EPhysAnimSupportSide::Left)
	{
		FPhysAnimSupportPoint2D Point;
		Point.PositionCm = FVector2D(X, Y);
		Point.BodyName = BodyName;
		Point.SupportSide = SupportSide;
		return Point;
	}

	bool ContainsPoint(const TArray<FVector2D>& Points, const FVector2D& Expected)
	{
		return Points.ContainsByPredicate(
			[Expected](const FVector2D& Point)
			{
				return Point.Equals(Expected, UE_SMALL_NUMBER);
			});
	}

	FPhysAnimSupportPatch MakeSupportPatch(const EPhysAnimSupportSide SupportSide, const TArray<FVector2D>& HullPointsCm, const bool bValidInput = true)
	{
		FPhysAnimSupportPatch Patch;
		Patch.BodyName = SupportSide == EPhysAnimSupportSide::Left ? FName(TEXT("LeftFoot")) : FName(TEXT("RightFoot"));
		Patch.SupportSide = SupportSide;
		Patch.HullPointsCm = HullPointsCm;
		Patch.PatchAreaCm2 = HullPointsCm.Num() >= 3 ? 1.0 : 0.0;
		Patch.bValidInput = bValidInput;
		return Patch;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthExtractPatchHullTest,
		"PhysAnim.SupportTruth.ExtractPatchHull",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthExtractPatchHullTest::RunTest(const FString& Parameters)
	{
		{
			TArray<FPhysAnimSupportPoint2D> Points;
			Points.Add(MakeSupportPoint(0.0, 0.0));
			Points.Add(MakeSupportPoint(10.0, 0.0));
			Points.Add(MakeSupportPoint(10.0, 10.0));
			Points.Add(MakeSupportPoint(0.0, 10.0));
			Points.Add(MakeSupportPoint(5.0, 5.0));

			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(Points);

			TestTrue(TEXT("LOGIC-01 valid input remains valid"), Patch.bValidInput);
			TestEqual(TEXT("LOGIC-01 body name is preserved"), Patch.BodyName, FName(TEXT("LeftFoot")));
			TestEqual(TEXT("LOGIC-01 support side is preserved"), static_cast<uint8>(Patch.SupportSide), static_cast<uint8>(EPhysAnimSupportSide::Left));
			TestEqual(TEXT("LOGIC-01 convex hull has four points"), Patch.HullPointsCm.Num(), 4);
			TestEqual(TEXT("LOGIC-01 area is positive square area"), Patch.PatchAreaCm2, 100.0);
			TestTrue(TEXT("LOGIC-01 hull contains lower-left"), ContainsPoint(Patch.HullPointsCm, FVector2D(0.0, 0.0)));
			TestTrue(TEXT("LOGIC-01 hull contains lower-right"), ContainsPoint(Patch.HullPointsCm, FVector2D(10.0, 0.0)));
			TestTrue(TEXT("LOGIC-01 hull contains upper-right"), ContainsPoint(Patch.HullPointsCm, FVector2D(10.0, 10.0)));
			TestTrue(TEXT("LOGIC-01 hull contains upper-left"), ContainsPoint(Patch.HullPointsCm, FVector2D(0.0, 10.0)));
		}

		{
			TArray<FPhysAnimSupportPoint2D> Points;
			Points.Add(MakeSupportPoint(0.0, 0.0));
			Points.Add(MakeSupportPoint(5.0, 0.0));
			Points.Add(MakeSupportPoint(10.0, 0.0));

			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(Points);

			TestTrue(TEXT("LOGIC-02 collinear input remains valid"), Patch.bValidInput);
			TestEqual(TEXT("LOGIC-02 collinear input has empty hull"), Patch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-02 collinear area is zero"), Patch.PatchAreaCm2, 0.0);
		}

		{
			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(TArray<FPhysAnimSupportPoint2D>());

			TestTrue(TEXT("LOGIC-03 empty input remains valid"), Patch.bValidInput);
			TestEqual(TEXT("LOGIC-03 empty input has empty hull"), Patch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-03 empty input area is zero"), Patch.PatchAreaCm2, 0.0);
		}

		{
			TArray<FPhysAnimSupportPoint2D> MixedBodyPoints;
			MixedBodyPoints.Add(MakeSupportPoint(0.0, 0.0, TEXT("LeftFoot"), EPhysAnimSupportSide::Left));
			MixedBodyPoints.Add(MakeSupportPoint(10.0, 0.0, TEXT("LeftBall"), EPhysAnimSupportSide::Left));
			MixedBodyPoints.Add(MakeSupportPoint(0.0, 10.0, TEXT("LeftFoot"), EPhysAnimSupportSide::Left));

			const FPhysAnimSupportPatch MixedBodyPatch = PhysAnimSupportTruth::ExtractPatchHull(MixedBodyPoints);

			TestFalse(TEXT("LOGIC-03A mixed body input is invalid"), MixedBodyPatch.bValidInput);
			TestEqual(TEXT("LOGIC-03A mixed body has empty hull"), MixedBodyPatch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-03A mixed body area is zero"), MixedBodyPatch.PatchAreaCm2, 0.0);

			TArray<FPhysAnimSupportPoint2D> MixedSidePoints;
			MixedSidePoints.Add(MakeSupportPoint(0.0, 0.0, TEXT("Foot"), EPhysAnimSupportSide::Left));
			MixedSidePoints.Add(MakeSupportPoint(10.0, 0.0, TEXT("Foot"), EPhysAnimSupportSide::Right));
			MixedSidePoints.Add(MakeSupportPoint(0.0, 10.0, TEXT("Foot"), EPhysAnimSupportSide::Left));

			const FPhysAnimSupportPatch MixedSidePatch = PhysAnimSupportTruth::ExtractPatchHull(MixedSidePoints);

			TestFalse(TEXT("LOGIC-03A mixed side input is invalid"), MixedSidePatch.bValidInput);
			TestEqual(TEXT("LOGIC-03A mixed side has empty hull"), MixedSidePatch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-03A mixed side area is zero"), MixedSidePatch.PatchAreaCm2, 0.0);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthBuildFrameHullTest,
		"PhysAnim.SupportTruth.BuildFrameHull",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthBuildFrameHullTest::RunTest(const FString& Parameters)
	{
		{
			TArray<FPhysAnimSupportPatch> Patches;
			Patches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Left,
				{FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(1.0, 1.0), FVector2D(0.0, 1.0)}));
			Patches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Right,
				{FVector2D(2.0, 0.0), FVector2D(3.0, 0.0), FVector2D(3.0, 1.0), FVector2D(2.0, 1.0)}));

			const FPhysAnimFrameHull FrameHull = PhysAnimSupportTruth::BuildFrameHull(Patches);

			TestEqual(TEXT("LOGIC-04 active support side count is two"), FrameHull.ActiveSupportSideCount, 2);
			TestEqual(TEXT("LOGIC-04 frame hull has four points"), FrameHull.HullPointsCm.Num(), 4);
			TestEqual(TEXT("LOGIC-04 frame hull area spans offset squares"), FrameHull.SupportHullAreaCm2, 3.0);
			TestTrue(TEXT("LOGIC-04 hull contains lower-left"), ContainsPoint(FrameHull.HullPointsCm, FVector2D(0.0, 0.0)));
			TestTrue(TEXT("LOGIC-04 hull contains lower-right"), ContainsPoint(FrameHull.HullPointsCm, FVector2D(3.0, 0.0)));
			TestTrue(TEXT("LOGIC-04 hull contains upper-right"), ContainsPoint(FrameHull.HullPointsCm, FVector2D(3.0, 1.0)));
			TestTrue(TEXT("LOGIC-04 hull contains upper-left"), ContainsPoint(FrameHull.HullPointsCm, FVector2D(0.0, 1.0)));
		}

		{
			const FPhysAnimFrameHull EmptyFrameHull = PhysAnimSupportTruth::BuildFrameHull(TArray<FPhysAnimSupportPatch>());

			TestEqual(TEXT("LOGIC-04A empty patch list has zero active sides"), EmptyFrameHull.ActiveSupportSideCount, 0);
			TestEqual(TEXT("LOGIC-04A empty patch list has empty hull"), EmptyFrameHull.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-04A empty patch list has zero area"), EmptyFrameHull.SupportHullAreaCm2, 0.0);

			TArray<FPhysAnimSupportPatch> IgnoredPatches;
			IgnoredPatches.Add(MakeSupportPatch(EPhysAnimSupportSide::Left, TArray<FVector2D>()));
			IgnoredPatches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Right,
				{FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0)},
				false));

			const FPhysAnimFrameHull IgnoredFrameHull = PhysAnimSupportTruth::BuildFrameHull(IgnoredPatches);

			TestEqual(TEXT("LOGIC-04A invalid and empty patches do not count active sides"), IgnoredFrameHull.ActiveSupportSideCount, 0);
			TestEqual(TEXT("LOGIC-04A invalid and empty patches have empty hull"), IgnoredFrameHull.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-04A invalid and empty patches have zero area"), IgnoredFrameHull.SupportHullAreaCm2, 0.0);
		}

		{
			TArray<FPhysAnimSupportPatch> Patches;
			Patches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Left,
				{FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(1.0, 1.0), FVector2D(0.0, 1.0)}));
			Patches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Left,
				{FVector2D(2.0, 0.0), FVector2D(3.0, 0.0), FVector2D(3.0, 1.0), FVector2D(2.0, 1.0)}));

			const FPhysAnimFrameHull FrameHull = PhysAnimSupportTruth::BuildFrameHull(Patches);

			TestEqual(TEXT("LOGIC-04B multiple patches on same side count once"), FrameHull.ActiveSupportSideCount, 1);
			TestEqual(TEXT("LOGIC-04B same-side frame hull area is still computed"), FrameHull.SupportHullAreaCm2, 3.0);
		}

		{
			TArray<FPhysAnimSupportPatch> Patches;
			Patches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Left,
				{FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0)}));
			Patches.Add(MakeSupportPatch(
				EPhysAnimSupportSide::Right,
				{FVector2D(2.0, 0.0), FVector2D(3.0, 0.0), FVector2D(3.0, 1.0)}));

			const FPhysAnimFrameHull FrameHull = PhysAnimSupportTruth::BuildFrameHull(Patches);

			TestEqual(TEXT("LOGIC-04C left and right patches count two sides"), FrameHull.ActiveSupportSideCount, 2);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthClassifySupportModeTest,
		"PhysAnim.SupportTruth.ClassifySupportMode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthClassifySupportModeTest::RunTest(const FString& Parameters)
	{
		// LOGIC-05: TwoFootStable when both sides active
		TestEqual(TEXT("LOGIC-05 both sides active"), 
			static_cast<uint8>(PhysAnimSupportTruth::ClassifySupportMode(true, true, 0.0, 100.0)), 
			static_cast<uint8>(EPhysAnimSupportMode::TwoFootStable));

		// LOGIC-06: SingleFootSurvival when exactly one side active
		TestEqual(TEXT("LOGIC-06 left only active"), 
			static_cast<uint8>(PhysAnimSupportTruth::ClassifySupportMode(true, false, 0.0, 100.0)), 
			static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
		TestEqual(TEXT("LOGIC-06 right only active"), 
			static_cast<uint8>(PhysAnimSupportTruth::ClassifySupportMode(false, true, 0.0, 100.0)), 
			static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));

		// LOGIC-07: TransientRecovery when neither side active and timer <= max
		TestEqual(TEXT("LOGIC-07 neither side active, timer at limit"), 
			static_cast<uint8>(PhysAnimSupportTruth::ClassifySupportMode(false, false, 100.0, 100.0)), 
			static_cast<uint8>(EPhysAnimSupportMode::TransientRecovery));
		TestEqual(TEXT("LOGIC-07 neither side active, timer below limit"), 
			static_cast<uint8>(PhysAnimSupportTruth::ClassifySupportMode(false, false, 50.0, 100.0)), 
			static_cast<uint8>(EPhysAnimSupportMode::TransientRecovery));

		// LOGIC-08: Airborne when neither side active and timer > max
		TestEqual(TEXT("LOGIC-08 neither side active, timer exceeds limit"), 
			static_cast<uint8>(PhysAnimSupportTruth::ClassifySupportMode(false, false, 100.1, 100.0)), 
			static_cast<uint8>(EPhysAnimSupportMode::Airborne));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthAdjudicateProxyTest,
		"PhysAnim.SupportTruth.AdjudicateProxy",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthAdjudicateProxyTest::RunTest(const FString& Parameters)
	{
		{
			// LOGIC-12: No support hull
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 0;
			Input.ProxyPositionCm = FVector2D(0.0, 0.0);
			Input.DeltaMs = 10.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestFalse(TEXT("LOGIC-12 proxy_inside_hull is unset"), Result.ProxyInsideHull.IsSet());
			TestFalse(TEXT("LOGIC-12 proxy_outside_hull_duration_ms is unset"), Result.ProxyOutsideHullDurationMs.IsSet());
			TestEqual(TEXT("LOGIC-12 TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// LOGIC-12A: Degenerate hull
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0)}; // Collinear
			Input.ProxyPositionCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			Input.ProxyDriftLimitMs = 100.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-12A proxy_inside_hull is false"), Result.ProxyInsideHull.IsSet() && !Result.ProxyInsideHull.GetValue());
			TestEqual(TEXT("LOGIC-12A duration follows outside rule"), Result.ProxyOutsideHullDurationMs.GetValue(), 10.0);
			TestEqual(TEXT("LOGIC-12A TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// LOGIC-09: Proxy inside hull
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			Input.PreviousProxyOutsideHullDurationMs = 50.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-09 proxy_inside_hull is true"), Result.ProxyInsideHull.IsSet() && Result.ProxyInsideHull.GetValue());
			TestEqual(TEXT("LOGIC-09 proxy_outside_hull_duration_ms is reset to 0"), Result.ProxyOutsideHullDurationMs.GetValue(), 0.0);
			TestEqual(TEXT("LOGIC-09 TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// LOGIC-10: Proxy outside hull under limit
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.PreviousProxyOutsideHullDurationMs = 50.0;
			Input.DeltaMs = 10.0;
			Input.ProxyDriftLimitMs = 100.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-10 proxy_inside_hull is false"), Result.ProxyInsideHull.IsSet() && !Result.ProxyInsideHull.GetValue());
			TestTrue(TEXT("LOGIC-10 proxy_outside_hull_duration_ms is under limit"), Result.ProxyOutsideHullDurationMs.GetValue() <= 100.0);
			TestEqual(TEXT("LOGIC-10 TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// Outside hull duration accumulates while under limit
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.PreviousProxyOutsideHullDurationMs = 50.0;
			Input.DeltaMs = 10.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestEqual(TEXT("Outside hull duration is incremented"), Result.ProxyOutsideHullDurationMs.GetValue(), 60.0);
		}

		{
			// LOGIC-11: Proxy outside hull over limit
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.PreviousProxyOutsideHullDurationMs = 95.0;
			Input.DeltaMs = 10.0;
			Input.ProxyDriftLimitMs = 100.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-11 proxy_inside_hull is false"), Result.ProxyInsideHull.IsSet() && !Result.ProxyInsideHull.GetValue());
			TestTrue(TEXT("LOGIC-11 proxy_outside_hull_duration_ms exceeds limit"), Result.ProxyOutsideHullDurationMs.GetValue() > 100.0);
			TestEqual(TEXT("LOGIC-11 TerminalReason is ActivationProxyOutsideSupportRegion"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
		}

		{
			// LOGIC-12B: First frame outside
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.DeltaMs = 10.0;
			Input.ProxyDriftLimitMs = 100.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestEqual(TEXT("LOGIC-12B proxy_outside_hull_duration_ms is DeltaMs"), Result.ProxyOutsideHullDurationMs.GetValue(), 10.0);
		}

		{
			// LOGIC-12C: First frame inside
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(5.0, 5.0);

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestEqual(TEXT("LOGIC-12C proxy_outside_hull_duration_ms is 0.0"), Result.ProxyOutsideHullDurationMs.GetValue(), 0.0);
		}

		{
			// Proxy on edge is inside
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(10.0, 5.0);

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("Proxy on edge is inside"), Result.ProxyInsideHull.IsSet() && Result.ProxyInsideHull.GetValue());
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthCalculateChurnHzTest,
		"PhysAnim.SupportTruth.CalculateChurnHz",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthCalculateChurnHzTest::RunTest(const FString& Parameters)
	{
		// LOGIC-13: 5 transitions in 1.0s
		FPhysAnimChurnCalculationInput Input;
		Input.CurrentTimestampSec = 1.0;
		Input.WindowSeconds = 1.0;
		
		FPhysAnimChurnEvent BoundaryExcluded; BoundaryExcluded.TimestampSec = 0.0; Input.HistoricalEvents.Add(BoundaryExcluded);
		FPhysAnimChurnEvent E1; E1.TimestampSec = 0.1; Input.HistoricalEvents.Add(E1);
		FPhysAnimChurnEvent E2; E2.TimestampSec = 0.25; Input.HistoricalEvents.Add(E2);
		FPhysAnimChurnEvent E3; E3.TimestampSec = 0.5; Input.HistoricalEvents.Add(E3);
		FPhysAnimChurnEvent E4; E4.TimestampSec = 0.75; Input.HistoricalEvents.Add(E4);
		FPhysAnimChurnEvent E5; E5.TimestampSec = 1.0; Input.HistoricalEvents.Add(E5);
		FPhysAnimChurnEvent FutureExcluded; FutureExcluded.TimestampSec = 1.1; Input.HistoricalEvents.Add(FutureExcluded);

		const FPhysAnimChurnResult Result = PhysAnimSupportTruth::CalculateChurnHz(Input);

		TestEqual(TEXT("LOGIC-13 SupportChurnCount is 5"), Result.SupportChurnCount, 5);
		TestEqual(TEXT("LOGIC-13 support_churn_hz is 5.0"), Result.SupportChurnHz, 5.0);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthReduceSupportModeForReportWindowTest,
		"PhysAnim.SupportTruth.ReduceSupportModeForReportWindow",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthReduceSupportModeForReportWindowTest::RunTest(const FString& Parameters)
	{
		{
			// Greatest accumulated duration wins
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = {EPhysAnimSupportMode::TwoFootStable, EPhysAnimSupportMode::SingleFootSurvival, EPhysAnimSupportMode::SingleFootSurvival};
			Input.DurationsMs = {100.0, 60.0, 60.0};

			const FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);

			TestEqual(TEXT("Survival wins by duration"), static_cast<uint8>(Result.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
			TestEqual(TEXT("Total duration is 220"), Result.TotalWindowDurationMs, 220.0);
			TestTrue(TEXT("Input is valid"), Result.bValidInput);
		}

		{
			// LOGIC-14: 30 Hz severity tie-break
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = {
				EPhysAnimSupportMode::TwoFootStable,
				EPhysAnimSupportMode::SingleFootSurvival,
				EPhysAnimSupportMode::TransientRecovery,
				EPhysAnimSupportMode::Airborne
			};
			Input.DurationsMs = {100.0, 100.0, 100.0, 100.0};

			const FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);

			TestEqual(TEXT("LOGIC-14 Airborne wins equal-duration severity tie-break"), static_cast<uint8>(Result.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::Airborne));
		}

		{
			// LOGIC-14A: Empty or zero input
			FPhysAnimSupportReportWindowInput Input;

			const FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);

			TestEqual(TEXT("LOGIC-14A empty input mode is Airborne"), static_cast<uint8>(Result.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::Airborne));
			TestEqual(TEXT("LOGIC-14A empty input total duration is zero"), Result.TotalWindowDurationMs, 0.0);
		}

		{
			// LOGIC-14A: Empty or zero input
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = {EPhysAnimSupportMode::TwoFootStable};
			Input.DurationsMs = {0.0};

			const FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);

			TestEqual(TEXT("LOGIC-14A zero-duration input mode is Airborne"), static_cast<uint8>(Result.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::Airborne));
			TestEqual(TEXT("LOGIC-14A zero-duration total duration is zero"), Result.TotalWindowDurationMs, 0.0);
		}

		{
			// LOGIC-14B: Array length mismatch
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = {EPhysAnimSupportMode::TwoFootStable};
			Input.DurationsMs = {100.0, 50.0};

			const FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);

			TestFalse(TEXT("LOGIC-14B invalid input detected"), Result.bValidInput);
			TestEqual(TEXT("LOGIC-14B mode defaults to Airborne"), static_cast<uint8>(Result.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::Airborne));
		}

		{
			// LOGIC-14C: Negative durations clamped to 0
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = {EPhysAnimSupportMode::TwoFootStable, EPhysAnimSupportMode::SingleFootSurvival};
			Input.DurationsMs = {-100.0, 50.0};

			const FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);

			TestEqual(TEXT("LOGIC-14C negative clamped to 0, Survival wins"), static_cast<uint8>(Result.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
			TestEqual(TEXT("LOGIC-14C total duration reflects clamp"), Result.TotalWindowDurationMs, 50.0);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthAggregationNoRuntimeDependencyProofTest,
		"PhysAnim.SupportTruth.Aggregation.NoRuntimeDependencyProof",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthAggregationNoRuntimeDependencyProofTest::RunTest(const FString& Parameters)
	{
		// 1. ExtractPatchHull
		TArray<FPhysAnimSupportPoint2D> Points;
		Points.Add(MakeSupportPoint(0.0, 0.0));
		Points.Add(MakeSupportPoint(10.0, 0.0));
		Points.Add(MakeSupportPoint(0.0, 10.0));
		const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(Points);
		TestTrue(TEXT("Aggregation: Patch extracted"), Patch.PatchAreaCm2 > 0);

		// 2. BuildFrameHull
		TArray<FPhysAnimSupportPatch> Patches;
		Patches.Add(Patch);
		const FPhysAnimFrameHull FrameHull = PhysAnimSupportTruth::BuildFrameHull(Patches);
		TestEqual(TEXT("Aggregation: Frame hull built"), FrameHull.ActiveSupportSideCount, 1);

		// 3. ClassifySupportMode
		const EPhysAnimSupportMode Mode = PhysAnimSupportTruth::ClassifySupportMode(true, false, 0.0, 100.0);
		TestEqual(TEXT("Aggregation: Mode classified"), static_cast<uint8>(Mode), static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));

		// 4. AdjudicateProxy
		FPhysAnimProxyAdjudicationInput AdjInput;
		AdjInput.ActiveSupportSideCount = FrameHull.ActiveSupportSideCount;
		AdjInput.HullPointsCm = FrameHull.HullPointsCm;
		AdjInput.ProxyPositionCm = FVector2D(5.0, 5.0);
		AdjInput.DeltaMs = 10.0;
		const FPhysAnimProxyAdjudicationResult AdjResult = PhysAnimSupportTruth::AdjudicateProxy(AdjInput);
		TestTrue(TEXT("Aggregation: Proxy adjudicated"), AdjResult.ProxyInsideHull.IsSet() && AdjResult.ProxyInsideHull.GetValue());

		// 5. CalculateChurnHz
		FPhysAnimChurnCalculationInput ChurnInput;
		ChurnInput.CurrentTimestampSec = 1.0;
		ChurnInput.WindowSeconds = 1.0;
		FPhysAnimChurnEvent Event; Event.TimestampSec = 0.5; ChurnInput.HistoricalEvents.Add(Event);
		const FPhysAnimChurnResult ChurnResult = PhysAnimSupportTruth::CalculateChurnHz(ChurnInput);
		TestEqual(TEXT("Aggregation: Churn calculated"), ChurnResult.SupportChurnCount, 1);

		// 6. ReduceSupportModeForReportWindow
		FPhysAnimSupportReportWindowInput RepInput;
		RepInput.Modes = {EPhysAnimSupportMode::TwoFootStable};
		RepInput.DurationsMs = {100.0};
		const FPhysAnimSupportReportWindowResult RepResult = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(RepInput);
		TestEqual(TEXT("Aggregation: Report window reduced"), static_cast<uint8>(RepResult.SupportMode), static_cast<uint8>(EPhysAnimSupportMode::TwoFootStable));

		return true;
	}
}
