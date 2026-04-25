#include "PhysAnimSupportTruth.h"

namespace
{
	double Cross(const FVector2D& Origin, const FVector2D& A, const FVector2D& B)
	{
		return (A.X - Origin.X) * (B.Y - Origin.Y) - (A.Y - Origin.Y) * (B.X - Origin.X);
	}

	double CalculatePolygonAreaCm2(const TArray<FVector2D>& Points)
	{
		double TwiceArea = 0.0;

		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FVector2D& Current = Points[Index];
			const FVector2D& Next = Points[(Index + 1) % Points.Num()];
			TwiceArea += Current.X * Next.Y - Next.X * Current.Y;
		}

		return FMath::Abs(TwiceArea) * 0.5;
	}

	TArray<FVector2D> BuildConvexHull(TArray<FVector2D> Points)
	{
		Points.Sort(
			[](const FVector2D& Left, const FVector2D& Right)
			{
				if (!FMath::IsNearlyEqual(Left.X, Right.X))
				{
					return Left.X < Right.X;
				}

				return Left.Y < Right.Y;
			});

		for (int32 Index = Points.Num() - 1; Index > 0; --Index)
		{
			if (Points[Index].Equals(Points[Index - 1], UE_SMALL_NUMBER))
			{
				Points.RemoveAt(Index);
			}
		}

		if (Points.Num() < 3)
		{
			return TArray<FVector2D>();
		}

		TArray<FVector2D> Hull;
		Hull.Reserve(Points.Num() * 2);

		for (const FVector2D& Point : Points)
		{
			while (Hull.Num() >= 2 && Cross(Hull[Hull.Num() - 2], Hull.Last(), Point) <= UE_SMALL_NUMBER)
			{
				Hull.Pop(EAllowShrinking::No);
			}

			Hull.Add(Point);
		}

		const int32 LowerHullCount = Hull.Num();

		for (int32 Index = Points.Num() - 2; Index >= 0; --Index)
		{
			const FVector2D& Point = Points[Index];

			while (Hull.Num() > LowerHullCount && Cross(Hull[Hull.Num() - 2], Hull.Last(), Point) <= UE_SMALL_NUMBER)
			{
				Hull.Pop(EAllowShrinking::No);
			}

			Hull.Add(Point);
		}

		if (!Hull.IsEmpty())
		{
			Hull.Pop(EAllowShrinking::No);
		}

		if (Hull.Num() < 3 || CalculatePolygonAreaCm2(Hull) <= UE_SMALL_NUMBER)
		{
			return TArray<FVector2D>();
		}

		return Hull;
	}
}

namespace PhysAnimSupportTruth
{
	FPhysAnimSupportPatch ExtractPatchHull(const TArray<FPhysAnimSupportPoint2D>& Points)
	{
		FPhysAnimSupportPatch Result;

		if (Points.IsEmpty())
		{
			return Result;
		}

		Result.BodyName = Points[0].BodyName;
		Result.SupportSide = Points[0].SupportSide;

		TArray<FVector2D> SortedPoints;
		SortedPoints.Reserve(Points.Num());

		for (const FPhysAnimSupportPoint2D& Point : Points)
		{
			if (Point.BodyName != Result.BodyName || Point.SupportSide != Result.SupportSide)
			{
				Result.HullPointsCm.Reset();
				Result.PatchAreaCm2 = 0.0;
				Result.bValidInput = false;
				return Result;
			}

			SortedPoints.Add(Point.PositionCm);
		}

		TArray<FVector2D> Hull = BuildConvexHull(MoveTemp(SortedPoints));
		const double AreaCm2 = Hull.Num() >= 3 ? CalculatePolygonAreaCm2(Hull) : 0.0;
		if (AreaCm2 <= UE_SMALL_NUMBER)
		{
			return Result;
		}

		Result.HullPointsCm = MoveTemp(Hull);
		Result.PatchAreaCm2 = AreaCm2;
		return Result;
	}

	FPhysAnimFrameHull BuildFrameHull(const TArray<FPhysAnimSupportPatch>& Patches)
	{
		FPhysAnimFrameHull Result;

		TArray<FVector2D> Points;
		bool bHasLeftSupport = false;
		bool bHasRightSupport = false;

		for (const FPhysAnimSupportPatch& Patch : Patches)
		{
			if (!Patch.bValidInput || Patch.HullPointsCm.IsEmpty())
			{
				continue;
			}

			Points.Append(Patch.HullPointsCm);

			if (Patch.SupportSide == EPhysAnimSupportSide::Left)
			{
				bHasLeftSupport = true;
			}
			else if (Patch.SupportSide == EPhysAnimSupportSide::Right)
			{
				bHasRightSupport = true;
			}
		}

		Result.ActiveSupportSideCount = (bHasLeftSupport ? 1 : 0) + (bHasRightSupport ? 1 : 0);
		Result.HullPointsCm = BuildConvexHull(MoveTemp(Points));
		Result.SupportHullAreaCm2 = Result.HullPointsCm.Num() >= 3 ? CalculatePolygonAreaCm2(Result.HullPointsCm) : 0.0;
		return Result;
	}
}
