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
			// LOGIC-09: No active support sides
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 0;
			Input.ProxyPositionCm = FVector2D(0.0, 0.0);
			Input.DeltaMs = 10.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestFalse(TEXT("LOGIC-09 ProxyInsideHull is unset"), Result.ProxyInsideHull.IsSet());
			TestFalse(TEXT("LOGIC-09 ProxyOutsideHullDurationMs is unset"), Result.ProxyOutsideHullDurationMs.IsSet());
			TestEqual(TEXT("LOGIC-09 TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// LOGIC-10: Active support but insufficient points for hull (e.g. collinear or single point)
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0)}; // Collinear
			Input.ProxyPositionCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			Input.ProxyDriftLimitMs = 100.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-10 ProxyInsideHull is set to false"), Result.ProxyInsideHull.IsSet() && !Result.ProxyInsideHull.GetValue());
			TestEqual(TEXT("LOGIC-10 ProxyOutsideHullDurationMs is DeltaMs"), Result.ProxyOutsideHullDurationMs.GetValue(), 10.0);
			TestEqual(TEXT("LOGIC-10 TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// LOGIC-11: Proxy inside hull
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			Input.PreviousProxyOutsideHullDurationMs = 50.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-11 ProxyInsideHull is true"), Result.ProxyInsideHull.IsSet() && Result.ProxyInsideHull.GetValue());
			TestEqual(TEXT("LOGIC-11 ProxyOutsideHullDurationMs is reset to 0"), Result.ProxyOutsideHullDurationMs.GetValue(), 0.0);
			TestEqual(TEXT("LOGIC-11 TerminalReason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// LOGIC-12: Proxy outside hull, no previous duration
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.DeltaMs = 10.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-12 ProxyInsideHull is false"), Result.ProxyInsideHull.IsSet() && !Result.ProxyInsideHull.GetValue());
			TestEqual(TEXT("LOGIC-12 ProxyOutsideHullDurationMs is DeltaMs"), Result.ProxyOutsideHullDurationMs.GetValue(), 10.0);
		}

		{
			// LOGIC-12A: Proxy outside hull, increment duration
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.PreviousProxyOutsideHullDurationMs = 50.0;
			Input.DeltaMs = 10.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestEqual(TEXT("LOGIC-12A ProxyOutsideHullDurationMs is incremented"), Result.ProxyOutsideHullDurationMs.GetValue(), 60.0);
		}

		{
			// LOGIC-12B: Proxy outside hull, exceed limit
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(15.0, 15.0);
			Input.PreviousProxyOutsideHullDurationMs = 95.0;
			Input.DeltaMs = 10.0;
			Input.ProxyDriftLimitMs = 100.0;

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestEqual(TEXT("LOGIC-12B ProxyOutsideHullDurationMs is 105"), Result.ProxyOutsideHullDurationMs.GetValue(), 105.0);
			TestEqual(TEXT("LOGIC-12B TerminalReason is ActivationProxyOutsideSupportRegion"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
		}

		{
			// LOGIC-12C: Proxy on edge is inside
			FPhysAnimProxyAdjudicationInput Input;
			Input.ActiveSupportSideCount = 1;
			Input.HullPointsCm = {FVector2D(0.0, 0.0), FVector2D(10.0, 0.0), FVector2D(10.0, 10.0), FVector2D(0.0, 10.0)};
			Input.ProxyPositionCm = FVector2D(10.0, 5.0); // Right edge

			const FPhysAnimProxyAdjudicationResult Result = PhysAnimSupportTruth::AdjudicateProxy(Input);

			TestTrue(TEXT("LOGIC-12C Proxy on edge is inside"), Result.ProxyInsideHull.IsSet() && Result.ProxyInsideHull.GetValue());
		}

		return true;
	}
}
