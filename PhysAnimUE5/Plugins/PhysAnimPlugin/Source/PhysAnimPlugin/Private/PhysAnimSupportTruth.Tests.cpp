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
}
