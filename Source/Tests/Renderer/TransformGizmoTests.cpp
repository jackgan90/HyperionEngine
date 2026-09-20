#include "Hyperion/Renderer/TransformGizmo.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

namespace
{
using namespace Hyperion;

FSceneCameraView Camera()
{
	FSceneCameraView Result;
	Result.World = SceneCameraTransform({0, 0, 10}, {});
	return Result;
}

void Near(float InActual, float InExpected)
{
	HYP_CHECK(std::abs(InActual - InExpected) < 1e-4f);
}

FMat4 ScaleDrag(FMat4 InLocal, float InFactor)
{
	FTransformGizmo Gizmo;
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, InLocal, Identity(), ETransformGizmoMode::Scale));
	FVec2 Tip;
	for (const auto& Stroke : Gizmo.Geometry())
	{
		if (Stroke.Handle == ETransformGizmoHandle::X && !Stroke.bFilled)
		{
			Tip = Stroke.Points.back();
			break;
		}
	}
	const FVec2 Start{400 + (Tip.X - 400) * .7f, 300 + (Tip.Y - 300) * .7f};
	HYP_CHECK(Gizmo.Begin(Start));
	HYP_CHECK(Gizmo.ActiveHandle() == ETransformGizmoHandle::X);
	FMat4 Result;
	HYP_CHECK(Gizmo.Drag({Start.X + (Tip.X - 400) * (InFactor - 1), Start.Y + (Tip.Y - 300) * (InFactor - 1)}, Result));
	HYP_CHECK(IsAffine(Result));
	return Result;
}

void CheckScale()
{
	Near(ScaleDrag(Identity(), 2).Values[0], 2);
	const auto Zero = ScaleDrag(Identity(), 0);
	Near(Zero.Values[0], 0);
	Near(Zero.Values[5], 1);
	Near(ScaleDrag(Identity(), -1).Values[0], -1);
	Near(ScaleDrag(Scale({1e-12f, 1, 1}), 2).Values[0], 2e-12f);
	Near(ScaleDrag(Scale({-2, 3, 4}), 2).Values[0], -4);
	Near(Length({ScaleDrag(Zero, 2).Values[0], ScaleDrag(Zero, 2).Values[1], ScaleDrag(Zero, 2).Values[2]}), 1);
	const auto AllZero = ScaleDrag(Scale({0, 0, 0}), 2);
	HYP_CHECK(IsAffine(AllZero));
	Near(Length({AllZero.Values[0], AllZero.Values[1], AllZero.Values[2]}), 1);
	FTransformGizmo Gizmo;
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Scale({-2, 3, 4}), Identity(), ETransformGizmoMode::Scale));
	HYP_CHECK(Gizmo.Begin({400, 300}));
	FMat4 Result;
	HYP_CHECK(Gizmo.Drag({485, 300}, Result));
	Near(Result.Values[0], -4);
	Near(Result.Values[5], 6);
	Near(Result.Values[10], 8);
}

void CheckPosition()
{
	FTransformGizmo Gizmo;
	const auto Parent = ComposeAffine({{}, {.2f, .3f, .4f}, {-2, 3, 4}, {.1f, .2f, .3f}});
	const auto Local = Scale({0, -1, 2});
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Local, Parent, ETransformGizmoMode::Position));
	HYP_CHECK(Gizmo.Begin({460, 300}));
	FMat4 Result;
	HYP_CHECK(Gizmo.Drag({500, 300}, Result));
	const auto World = Multiply(Parent, Result);
	HYP_CHECK(World.Values[12] > 0);
	Near(World.Values[13], 0);
	Near(World.Values[14], 0);
	for (unsigned Index = 0; Index < 12; ++Index)
	{
		HYP_CHECK(Result.Values[Index] == Local.Values[Index]);
	}
	HYP_CHECK(!Gizmo.Drag({std::numeric_limits<float>::infinity(), 300}, Result));
	Gizmo.End();
	HYP_CHECK(!Gizmo.Drag({500, 300}, Result));
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Local, Identity(), ETransformGizmoMode::Position));
	HYP_CHECK(Gizmo.Begin({400, 300}));
	HYP_CHECK(Gizmo.Drag({450, 250}, Result));
	HYP_CHECK(Result.Values[12] > 0 && Result.Values[13] > 0);
	Near(Result.Values[14], 0);
}

void CheckUniformShearScale()
{
	const auto Initial = ComposeAffine({{}, {.2f, .3f, .4f}, {-2, 3, 4}, {1, 2, 3}});
	for (const float Factor : {2.f, 0.f, -1.f})
	{
		FTransformGizmo Gizmo;
		HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Initial, Identity(), ETransformGizmoMode::Scale));
		HYP_CHECK(Gizmo.Begin({400, 300}));
		HYP_CHECK(Gizmo.ActiveHandle() == ETransformGizmoHandle::Screen);
		FMat4 Result;
		HYP_CHECK(Gizmo.Drag({400 + 85 * (Factor - 1), 300}, Result));
		for (unsigned Column = 0; Column < 3; ++Column)
		{
			for (unsigned Row = 0; Row < 3; ++Row)
			{
				Near(Result.Values[Column * 4 + Row], Initial.Values[Column * 4 + Row] * Factor);
			}
		}
		HYP_CHECK(IsAffine(Result));
		Near(Result.Values[12], Initial.Values[12]);
		Near(Result.Values[13], Initial.Values[13]);
		Near(Result.Values[14], Initial.Values[14]);
	}
}

void CheckRotation()
{
	FTransformGizmo Gizmo;
	const auto Local = ComposeAffine({{}, {}, {-2, 3, 4}, {.2f, .3f, .4f}});
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Local, Identity(), ETransformGizmoMode::Rotation));
	const float Diagonal = 85 / std::sqrt(2.f);
	HYP_CHECK(Gizmo.Begin({400 + Diagonal, 300 - Diagonal}));
	HYP_CHECK(Gizmo.ActiveHandle() == ETransformGizmoHandle::Z);
	FMat4 Result;
	HYP_CHECK(Gizmo.Drag({400 - Diagonal, 300 - Diagonal}, Result));
	const auto Parts = DecomposeAffine(Result);
	Near(Parts.Rotation.Z, std::numbers::pi_v<float> / 2);
	Near(Parts.Scale.X, -2);
	Near(Parts.Scale.Y, 3);
	Near(Parts.Shear.X, .2f);
	Near(Parts.Shear.Y, .3f);
	Near(Parts.Shear.Z, .4f);
	// Edge-on rings use the initial screen tangent, including under mirrored parents.
	Gizmo.End();
	HYP_CHECK(
	    Gizmo.Configure(Camera(), {0, 0, 800, 600}, Identity(), Scale({-1, 1, 1}), ETransformGizmoMode::Rotation));
	HYP_CHECK(Gizmo.Begin({430, 300}));
	HYP_CHECK(Gizmo.Drag({450, 300}, Result));
	HYP_CHECK(IsAffine(Result));
}

void CheckGroupTransforms()
{
	const auto Parent = ComposeAffine({{}, {}, {2, 3, 4}, {.2f, .1f, .3f}});
	const auto Initial = ComposeAffine({{}, {}, {-1, 2, 3}, {.1f, .2f, .3f}});
	const auto PrimaryWorld = Multiply(Parent, Initial);
	const auto Secondary = Translation({3, 0, 0});
	for (const auto Mode : {ETransformGizmoMode::Position, ETransformGizmoMode::Rotation, ETransformGizmoMode::Scale})
	{
		FTransformGizmo Gizmo;
		HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Initial, Parent, Mode));
		FVec2 Start{400, 300};
		FVec2 End{485, 300};
		if (Mode == ETransformGizmoMode::Rotation)
		{
			std::vector<FVec2> Ring;
			for (const auto& Stroke : Gizmo.Geometry())
			{
				if (Stroke.Handle == ETransformGizmoHandle::Z)
				{
					Ring.push_back(Stroke.Points.front());
				}
			}
			Start = Ring.at(12);
			End = Ring.at(36);
		}
		HYP_CHECK(Gizmo.Begin(Start));
		FMat4 Local;
		HYP_CHECK(Gizmo.Drag(End, Local));
		const auto Delta = Gizmo.GroupDelta(Local);
		const auto Actual = Multiply(Delta, PrimaryWorld);
		const auto Expected = Multiply(Parent, Local);
		for (unsigned Index = 0; Index < 16; ++Index)
		{
			Near(Actual.Values[Index], Expected.Values[Index]);
		}
		HYP_CHECK(Multiply(Delta, Secondary).Values != Secondary.Values);
	}
	FTransformGizmo Gizmo;
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Initial, Parent, ETransformGizmoMode::Scale));
	HYP_CHECK(Gizmo.Begin({400, 300}));
	for (const float Factor : {0.f, -1.f, 1.f})
	{
		FMat4 Local;
		HYP_CHECK(Gizmo.Drag({400 + 85 * (Factor - 1), 300}, Local));
		const auto Delta = Gizmo.GroupDelta(Local);
		Near(Multiply(Delta, Secondary).Values[12], 3 * Factor);
	}
	Gizmo.End();
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Scale({0, 1, 1}), Identity(), ETransformGizmoMode::Scale));
	HYP_CHECK(Gizmo.Begin({400, 300}));
	bool bRejected{};
	try
	{
		(void)Gizmo.GroupDelta(Scale({0, 1, 1}));
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Gizmo.End();
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Scale({0, 0, 0}), Identity(), ETransformGizmoMode::Rotation));
	const float Diagonal = 85 / std::sqrt(2.f);
	HYP_CHECK(Gizmo.Begin({400 + Diagonal, 300 - Diagonal}));
	FMat4 Local;
	HYP_CHECK(Gizmo.Drag({400 - Diagonal, 300 - Diagonal}, Local));
	const auto Rotated = Multiply(Gizmo.GroupDelta(Local), Secondary);
	Near(Rotated.Values[12], 0);
	Near(Rotated.Values[13], 3);
}

void CheckDegenerate()
{
	FTransformGizmo Gizmo;
	HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Identity(), Scale({0, 1, 1}), ETransformGizmoMode::Position));
	HYP_CHECK(!Gizmo.Begin({400, 300}));
	HYP_CHECK(!Gizmo.Configure(Camera(), {0, 0, 0, 0}, Identity(), Identity(), ETransformGizmoMode::Position));
	HYP_CHECK(Gizmo.Geometry().empty());
	HYP_CHECK(!Gizmo.Configure(Camera(), {0, 0, 800, 600}, Translation({0, 0, 11}), Identity(),
	                           ETransformGizmoMode::Position));
	auto Invalid = Identity();
	Invalid.Values[0] = std::numeric_limits<float>::quiet_NaN();
	HYP_CHECK(!Gizmo.Configure(Camera(), {0, 0, 800, 600}, Invalid, Identity(), ETransformGizmoMode::Scale));
	// Finite inputs can overflow during composition/decomposition; the controller must reject safely.
	const float Huge = std::numeric_limits<float>::max();
	Invalid = Identity();
	Invalid.Values[0] = Invalid.Values[1] = Invalid.Values[2] = Huge;
	Gizmo.Configure(Camera(), {0, 0, 800, 600}, Invalid, Identity(), ETransformGizmoMode::Scale);
	Gizmo.Configure(Camera(), {0, 0, 800, 600}, Identity(), Scale({Huge, Huge, Huge}), ETransformGizmoMode::Position);
	for (const auto Mode : {ETransformGizmoMode::Position, ETransformGizmoMode::Rotation, ETransformGizmoMode::Scale})
	{
		HYP_CHECK(Gizmo.Configure(Camera(), {0, 0, 800, 600}, Scale({0, 0, 0}), Identity(), Mode, 2));
		for (const auto& Stroke : Gizmo.Geometry())
		{
			for (const auto Point : Stroke.Points)
			{
				HYP_CHECK(std::isfinite(Point.X) && std::isfinite(Point.Y));
			}
		}
		HYP_CHECK(Gizmo.HitTest({-1, 300}) == ETransformGizmoHandle::None);
	}
}
} // namespace

int main()
{
	try
	{
		CheckScale();
		CheckUniformShearScale();
		CheckPosition();
		CheckRotation();
		CheckDegenerate();
		CheckGroupTransforms();
		std::cout << "Transform gizmo tests passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
