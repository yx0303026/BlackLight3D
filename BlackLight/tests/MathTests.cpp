#include "engine/Math.h"
#include "engine/Camera2D.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace BlackLight::Math;

namespace
{
	/// Terminates the test process with the failed expression and source line.
	void Check(bool condition, const char *expression, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "Check failed at line %d: %s\n", line, expression);
			std::abort();
		}
	}

	/// Terminates the test process when a scalar is not within tolerance.
	void CheckNear(float actual, float expected, float tolerance, const char *expression, int line)
	{
		if (!Approximately(actual, expected, tolerance))
		{
			std::fprintf(stderr, "Check failed at line %d: %s (actual %g, expected %g)\n",
						 line, expression, static_cast<double>(actual), static_cast<double>(expected));
			std::abort();
		}
	}

#define CHECK(...) Check((__VA_ARGS__), #__VA_ARGS__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance) \
	CheckNear((actual), (expected), (tolerance), #actual, __LINE__)

	/// Verifies angle conversion, clamping, interpolation, and approximate comparison.
	void TestScalarHelpers()
	{
		CHECK_NEAR(ToDegrees(ToRadians(90.0f)), 90.0f, 1.0e-4f);
		CHECK_NEAR(ToRadians(180.0f), kPi, 1.0e-6f);
		CHECK_NEAR(Clamp(-2.0f, 0.0f, 1.0f), 0.0f, 0.0f);
		CHECK_NEAR(Clamp(0.5f, 0.0f, 1.0f), 0.5f, 0.0f);
		CHECK_NEAR(Clamp(2.0f, 0.0f, 1.0f), 1.0f, 0.0f);
		CHECK_NEAR(Clamp01(-0.25f), 0.0f, 0.0f);
		CHECK_NEAR(Lerp(10.0f, 20.0f, 0.25f), 12.5f, 1.0e-6f);
		CHECK_NEAR(Lerp(10.0f, 20.0f, 1.5f), 25.0f, 1.0e-6f);
		CHECK_NEAR(Abs(-3.5f), 3.5f, 0.0f);
		CHECK_NEAR(Sign(-3.0f), -1.0f, 0.0f);
		CHECK_NEAR(Sign(0.0f), 0.0f, 0.0f);
		CHECK(Approximately(1.0f, 1.0000005f));
		CHECK(!Approximately(1.0f, 1.1f));
	}

	/// Verifies vector arithmetic, normalization, and interpolation.
	void TestVectors()
	{
		const Vector2 offset{3.0f, 4.0f};
		CHECK_NEAR(offset.Length(), 5.0f, 1.0e-5f);
		CHECK(offset.Normalized().ApproximatelyEquals({0.6f, 0.8f}, 1.0e-6f));
		CHECK(Vector2{0.0f, 0.0f}.Normalized().ApproximatelyEquals({0.0f, 0.0f}));
		CHECK_NEAR(Vector2::UnitX().Cross(Vector2::UnitY()), 1.0f, 0.0f);
		CHECK(Vector2::UnitX().Perpendicular().ApproximatelyEquals(Vector2::UnitY()));
		CHECK_NEAR(Vector2::UnitX().Dot(Vector2::UnitY()), 0.0f, 0.0f);
		CHECK(Vector2::Lerp({0.0f, 0.0f}, {10.0f, 20.0f}, 0.25f).ApproximatelyEquals({2.5f, 5.0f}));
		CHECK((2.0f * offset).ApproximatelyEquals({6.0f, 8.0f}));
		CHECK((offset / 2.0f).ApproximatelyEquals({1.5f, 2.0f}));
		CHECK_NEAR(offset[1], 4.0f, 0.0f);
		Vector2 mutable_vector{1.0f, 2.0f};
		mutable_vector += {1.0f, 1.0f};
		mutable_vector *= 2.0f;
		mutable_vector -= {1.0f, 1.0f};
		CHECK(mutable_vector.ApproximatelyEquals({3.0f, 5.0f}));

		CHECK(Vector3::UnitX().Cross(Vector3::UnitY()).ApproximatelyEquals(Vector3::UnitZ()));
		CHECK(Vector3::UnitZ().Cross(Vector3::UnitX()).ApproximatelyEquals(Vector3::UnitY()));
		CHECK_NEAR((Vector3{1.0f, 2.0f, 3.0f}.Dot({4.0f, 5.0f, 6.0f})), 32.0f, 0.0f);
		CHECK(Vector3{0.0f, 3.0f, 4.0f}.Normalized().ApproximatelyEquals({0.0f, 0.6f, 0.8f}, 1.0e-6f));
		CHECK_NEAR((Vector3{1.0f, 1.0f, 1.0f}.Distance({1.0f, 1.0f, 4.0f})), 3.0f, 1.0e-6f);
		CHECK_NEAR((Vector3{1.0f, 2.0f, 3.0f}[2]), 3.0f, 0.0f);

		CHECK_NEAR((Vector4{1.0f, 2.0f, 3.0f, 4.0f}.Dot({1.0f, 1.0f, 1.0f, 1.0f})), 10.0f, 0.0f);
		CHECK_NEAR(Vector4::UnitW().Length(), 1.0f, 0.0f);
		CHECK(Vector4::UnitZ().ApproximatelyEquals({0.0f, 0.0f, 1.0f, 0.0f}));
		CHECK(Vector4::Lerp(Vector4::Zero(), Vector4::One(), 0.5f)
				  .ApproximatelyEquals({0.5f, 0.5f, 0.5f, 0.5f}));
	}

	/// Verifies color conversion, tinting, alpha blending, and interpolation.
	void TestColors()
	{
		const Color packed = Color::FromRgba8(12, 34, 56, 78);
		CHECK_NEAR(packed.r, 12.0f / 255.0f, 1.0e-6f);
		CHECK(packed.ToRgba8() == 0x0C22384Eu);
		CHECK(Color::FromHex(0x11223344u).ToRgba8() == 0x11223344u);
		CHECK(Color::FromHex(0xFF800080u).ApproximatelyEquals({1.0f, 128.0f / 255.0f, 0.0f,
															  128.0f / 255.0f}, 1.0e-6f));
		CHECK(Color::White().ApproximatelyEquals({1.0f, 1.0f, 1.0f, 1.0f}));
		CHECK(Color::Transparent().a == 0.0f);
		CHECK(Color::Lerp(Color::Black(), Color::White(), 0.25f)
				  .ApproximatelyEquals({0.25f, 0.25f, 0.25f, 1.0f}, 1.0e-6f));
		CHECK(Color{1.0f, 1.0f, 1.0f, 1.0f}.Modulate({0.5f, 0.25f, 1.0f, 0.5f})
				  .ApproximatelyEquals({0.5f, 0.25f, 1.0f, 0.5f}, 1.0e-6f));
		CHECK(Color{0.5f, 0.5f, 0.5f, 0.5f}.WithAlpha(0.25f)
				  .ApproximatelyEquals({0.5f, 0.5f, 0.5f, 0.25f}, 0.0f));
		CHECK(Color{0.5f, 0.5f, 0.5f, 0.5f}.Scaled(2.0f)
				  .ApproximatelyEquals({1.0f, 1.0f, 1.0f, 1.0f}, 0.0f));

		const Color half_red_over_blue = Color::Blend({1.0f, 0.0f, 0.0f, 0.5f},
													  {0.0f, 0.0f, 1.0f, 1.0f});
		CHECK(half_red_over_blue.ApproximatelyEquals({0.5f, 0.0f, 0.5f, 1.0f}, 1.0e-6f));
		CHECK(Color::Blend(Color::Transparent(), {0.2f, 0.4f, 0.6f, 0.8f})
				  .ApproximatelyEquals({0.2f, 0.4f, 0.6f, 0.8f}, 1.0e-6f));
		CHECK(Color::Blend({0.0f, 0.0f, 0.0f, 0.0f}, Color::Transparent())
				  .ApproximatelyEquals(Color::Transparent()));
	}

	/// Verifies matrix multiplication order, transforms, and transposition.
	void TestMatrixComposition()
	{
		const Matrix4 translation = Matrix4::CreateTranslation({1.0f, 2.0f, 3.0f});
		const Matrix4 scale = Matrix4::CreateScale({2.0f, 4.0f, 8.0f});
		CHECK((translation * scale).TransformPoint({1.0f, 1.0f, 1.0f})
				  .ApproximatelyEquals({4.0f, 12.0f, 32.0f}, 1.0e-6f));
		CHECK((scale * translation).TransformPoint({1.0f, 1.0f, 1.0f})
				  .ApproximatelyEquals({3.0f, 6.0f, 11.0f}, 1.0e-6f));
		CHECK(Matrix4::Identity().TransformPoint({2.0f, 3.0f, 4.0f})
				  .ApproximatelyEquals({2.0f, 3.0f, 4.0f}, 0.0f));

		const Matrix4 translation_vector = Matrix4::CreateTranslation({1.0f, 2.0f, 3.0f});
		CHECK(translation_vector.Transform({1.0f, 1.0f, 1.0f, 1.0f})
				  .ApproximatelyEquals({2.0f, 3.0f, 4.0f, 1.0f}, 0.0f));
		CHECK(translation_vector.Transform({1.0f, 1.0f, 1.0f, 0.0f})
				  .ApproximatelyEquals({1.0f, 1.0f, 1.0f, 0.0f}, 0.0f));
		CHECK(translation_vector.TransformDirection({1.0f, 1.0f, 1.0f})
				  .ApproximatelyEquals({1.0f, 1.0f, 1.0f}, 0.0f));

		const Matrix4 rotation = Matrix4::CreateRotationYawPitchRoll(0.3f, -0.2f, 0.5f);
		CHECK(rotation.Transposed().ApproximatelyEquals(rotation.Inverted(), 1.0e-5f));
		CHECK(rotation.Transposed().Transposed().ApproximatelyEquals(rotation, 0.0f));
		CHECK(rotation.Transposed().TransformDirection({1.0f, 2.0f, 3.0f})
				  .ApproximatelyEquals(rotation.Inverted().TransformDirection({1.0f, 2.0f, 3.0f}),
									   1.0e-5f));

		const Matrix4 first = Matrix4::CreateRotationX(0.4f) * translation;
		const Matrix4 second = Matrix4::CreateScale({3.0f, 2.0f, 1.5f}) * Matrix4::CreateRotationY(-0.6f);
		const Matrix4 third = Matrix4::CreateRotationZ(0.15f);
		CHECK(((first * second) * third).ApproximatelyEquals(first * (second * third), 1.0e-5f));
		CHECK((first * second).TransformDirection({1.0f, 0.0f, 0.0f})
				  .ApproximatelyEquals(second.TransformDirection(first.TransformDirection({1.0f, 0.0f, 0.0f})),
									   1.0e-5f));

		const Matrix3 rotation2d = Matrix3::CreateRotation(kHalfPi);
		CHECK(rotation2d.TransformPoint({1.0f, 0.0f}).ApproximatelyEquals({0.0f, 1.0f}, 1.0e-6f));
		const Matrix3 transform2d = Matrix3::CreateScale({2.0f, 3.0f}) * rotation2d *
									Matrix3::CreateTranslation({4.0f, -1.0f});
		const Vector2 point2d{1.0f, 2.0f};
		CHECK(transform2d.TransformPoint(point2d).ApproximatelyEquals({-2.0f, 1.0f}, 1.0e-6f));
		CHECK(transform2d.TransformDirection(point2d).ApproximatelyEquals({-6.0f, 2.0f}, 1.0e-6f));
	}

	/// Verifies determinant and inverse behavior for regular and singular matrices.
	void TestMatrixInverse()
	{
		const Matrix4 transform = Matrix4::CreateScale({2.0f, 3.0f, 4.0f}) *
								  Matrix4::CreateRotationYawPitchRoll(0.4f, -0.2f, 0.7f) *
								  Matrix4::CreateTranslation({3.0f, -5.0f, 8.0f});
		CHECK_NEAR(transform.Determinant(), 24.0f, 1.0e-4f);
		CHECK((transform * transform.Inverted()).ApproximatelyEquals(Matrix4::Identity(), 1.0e-4f));
		CHECK((transform.Inverted() * transform).ApproximatelyEquals(Matrix4::Identity(), 1.0e-4f));

		const Vector3 point{1.5f, -2.0f, 4.0f};
		CHECK(transform.Inverted().TransformPoint(transform.TransformPoint(point))
				  .ApproximatelyEquals(point, 1.0e-4f));

		const Matrix4 singular{};
		CHECK_NEAR(singular.Determinant(), 0.0f, 0.0f);
		CHECK(singular.Inverted().ApproximatelyEquals(Matrix4::Identity(), 0.0f));
		CHECK(Matrix4::CreateScale({1.0f, 1.0f, 0.0f}).Inverted()
				  .ApproximatelyEquals(Matrix4::Identity(), 0.0f));

		const Matrix3 transform2d = Matrix3::CreateScale({2.0f, 2.0f}) *
									Matrix3::CreateRotation(0.35f) *
									Matrix3::CreateTranslation({1.0f, 3.0f});
		CHECK_NEAR(transform2d.Determinant(), 4.0f, 1.0e-5f);
		CHECK((transform2d * transform2d.Inverted()).ApproximatelyEquals(Matrix3::Identity(), 1.0e-5f));
		CHECK(transform2d.Inverted().TransformPoint(transform2d.TransformPoint({1.0f, 2.0f}))
				  .ApproximatelyEquals({1.0f, 2.0f}, 1.0e-5f));
		CHECK(Matrix3{}.Inverted().ApproximatelyEquals(Matrix3::Identity(), 0.0f));

		// Deterministic pseudo-random transforms exercise every cofactor of the general inverse.
		for (int index = 1; index < 8; ++index)
		{
			const float seed = static_cast<float>(index) * 0.37f;
			const Matrix4 transform =
				Matrix4::CreateScale({1.0f + seed, 2.0f + seed * 0.5f, 3.0f + seed * 0.25f}) *
				Matrix4::CreateRotationYawPitchRoll(seed, -seed * 1.7f, seed * 0.9f) *
				Matrix4::CreateTranslation({seed, 4.0f + seed, -seed * 7.0f});
			const Matrix4 inverse = transform.Inverted();
			CHECK((transform * inverse).ApproximatelyEquals(Matrix4::Identity(), 1.0e-3f));
			CHECK((inverse * transform).ApproximatelyEquals(Matrix4::Identity(), 1.0e-3f));
			CHECK_NEAR((transform.Determinant() * inverse.Determinant()), 1.0f, 1.0e-4f);
			CHECK(inverse.TransformPoint(transform.TransformPoint({1.0f, 1.1f, -0.5f}))
					  .ApproximatelyEquals({1.0f, 1.1f, -0.5f}, 1.0e-3f));
		}
	}

	/// Verifies perspective, orthographic, and view matrix construction.
	void TestCameraMatrices()
	{
		const float near_plane = 0.25f;
		const float far_plane = 250.0f;
		const Matrix4 perspective =
			Matrix4::CreatePerspectiveFieldOfView(ToRadians(60.0f), 4.0f / 3.0f, near_plane, far_plane);

		const Vector4 near_center = perspective.Transform({0.0f, 0.0f, -near_plane, 1.0f});
		CHECK_NEAR(near_center.z / near_center.w, 0.0f, 1.0e-5f);
		const Vector4 far_center = perspective.Transform({0.0f, 0.0f, -far_plane, 1.0f});
		CHECK_NEAR(far_center.z / far_center.w, 1.0f, 1.0e-5f);
		CHECK_NEAR(near_center.w, near_plane, 1.0e-6f);
		CHECK_NEAR(far_center.w, far_plane, 1.0e-4f);

		const float half_height = near_plane * std::tan(ToRadians(30.0f));
		const Vector4 top_edge = perspective.Transform({0.0f, half_height, -near_plane, 1.0f});
		CHECK_NEAR(top_edge.y / top_edge.w, 1.0f, 1.0e-5f);
		const Vector4 right_edge =
			perspective.Transform({half_height * (4.0f / 3.0f), 0.0f, -near_plane, 1.0f});
		CHECK_NEAR(right_edge.x / right_edge.w, 1.0f, 1.0e-5f);
		CHECK_NEAR(right_edge.y / right_edge.w, 0.0f, 1.0e-6f);

		CHECK(Matrix4::CreatePerspectiveFieldOfView(0.0f, 1.0f, 1.0f, 2.0f)
				  .ApproximatelyEquals(Matrix4::Identity(), 0.0f));
		CHECK(Matrix4::CreatePerspectiveFieldOfView(kPi, 1.0f, 1.0f, 2.0f)
				  .ApproximatelyEquals(Matrix4::Identity(), 0.0f));
		CHECK(Matrix4::CreatePerspectiveFieldOfView(1.0f, 0.0f, 1.0f, 2.0f)
				  .ApproximatelyEquals(Matrix4::Identity(), 0.0f));
		CHECK(Matrix4::CreatePerspectiveFieldOfView(1.0f, 1.0f, 2.0f, 2.0f)
				  .ApproximatelyEquals(Matrix4::Identity(), 0.0f));
		CHECK(Matrix4::CreatePerspective(16.0f, 9.0f, 1.0f, 100.0f)
				  .ApproximatelyEquals(
					  Matrix4::CreatePerspectiveFieldOfView(
						  2.0f * std::atan(4.5f), 16.0f / 9.0f, 1.0f, 100.0f),
					  1.0e-5f));

		const Matrix4 orthographic =
			Matrix4::CreateOrthographicOffCenter(-4.0f, 4.0f, -2.0f, 2.0f, 1.0f, 11.0f);
		const Vector4 top_right = orthographic.Transform({4.0f, 2.0f, -11.0f, 1.0f});
		CHECK_NEAR(top_right.x / top_right.w, 1.0f, 1.0e-6f);
		CHECK_NEAR(top_right.y / top_right.w, 1.0f, 1.0e-6f);
		CHECK_NEAR(top_right.z / top_right.w, 1.0f, 1.0e-6f);
		const Vector4 bottom_left = orthographic.Transform({-4.0f, -2.0f, -1.0f, 1.0f});
		CHECK_NEAR(bottom_left.x / bottom_left.w, -1.0f, 1.0e-6f);
		CHECK_NEAR(bottom_left.y / bottom_left.w, -1.0f, 1.0e-6f);
		CHECK_NEAR(bottom_left.z / bottom_left.w, 0.0f, 1.0e-6f);
		CHECK(Matrix4::CreateOrthographicOffCenter(1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f)
				  .ApproximatelyEquals(Matrix4::Identity(), 0.0f));

		const Matrix4 centered = Matrix4::CreateOrthographic(10.0f, 6.0f, 0.0f, 1.0f);
		CHECK(centered.Transform({0.0f, 0.0f, 0.0f, 1.0f})
				  .ApproximatelyEquals({0.0f, 0.0f, 0.0f, 1.0f}, 1.0e-6f));
		CHECK_NEAR((centered.Transform({5.0f, 3.0f, -1.0f, 1.0f}).x), 1.0f, 1.0e-6f);

		const Matrix4 view = Matrix4::CreateLookAt({0.0f, 2.0f, 6.0f}, {0.0f, 2.0f, 0.0f},
												   {0.0f, 1.0f, 0.0f});
		CHECK(view.TransformPoint({0.0f, 2.0f, 6.0f}).ApproximatelyEquals({0.0f, 0.0f, 0.0f}, 1.0e-5f));
		CHECK(view.TransformPoint({0.0f, 2.0f, 0.0f}).ApproximatelyEquals({0.0f, 0.0f, -6.0f}, 1.0e-5f));
		CHECK(view.TransformDirection({0.0f, 1.0f, 0.0f}).ApproximatelyEquals({0.0f, 1.0f, 0.0f}, 1.0e-5f));
		CHECK(Matrix4::CreateLookAt({1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f})
				  .ApproximatelyEquals(Matrix4::Identity(), 1.0e-5f));
	}

	/// Verifies quaternion rotation, composition order, conversion, and interpolation.
	void TestQuaternions()
	{
		const Quaternion identity = Quaternion::Identity();
		CHECK(identity.Rotate({1.0f, 2.0f, 3.0f}).ApproximatelyEquals({1.0f, 2.0f, 3.0f}, 0.0f));
		CHECK(identity.ToMatrix4().ApproximatelyEquals(Matrix4::Identity(), 0.0f));

		const Quaternion quarter_turn = Quaternion::FromAxisAngle(Vector3::UnitZ(), kHalfPi);
		CHECK_NEAR(quarter_turn.Length(), 1.0f, 1.0e-6f);
		CHECK(quarter_turn.Rotate(Vector3::UnitX()).ApproximatelyEquals(Vector3::UnitY(), 1.0e-6f));
		CHECK(quarter_turn.ToMatrix4().TransformDirection(Vector3::UnitX())
				  .ApproximatelyEquals(Vector3::UnitY(), 1.0e-6f));
		CHECK(quarter_turn.ToMatrix3().ApproximatelyEquals(Matrix3::CreateRotation(kHalfPi), 1.0e-6f));
		CHECK(Quaternion::FromAxisAngle({0.0f, 0.0f, 0.0f}, 1.0f)
				  .ApproximatelyEquals(identity, 0.0f));

		const float yaw = 0.6f;
		const float pitch = -0.35f;
		const float roll = 1.1f;
		const Quaternion combined = Quaternion::FromYawPitchRoll(yaw, pitch, roll);
		const Matrix4 combined_matrix = Matrix4::CreateRotationYawPitchRoll(yaw, pitch, roll);
		CHECK(combined.ToMatrix4().ApproximatelyEquals(combined_matrix, 1.0e-5f));
		CHECK(Quaternion::FromRotationMatrix(combined_matrix)
				  .ToMatrix4()
				  .ApproximatelyEquals(combined_matrix, 1.0e-5f));

		for (const Matrix4 rotation : {Matrix4::CreateRotationX(kPi), Matrix4::CreateRotationY(kPi),
									   Matrix4::CreateRotationZ(kPi)})
		{
			CHECK(Quaternion::FromRotationMatrix(rotation)
					  .ToMatrix4()
					  .ApproximatelyEquals(rotation, 1.0e-4f));
		}

		const Vector3 vector{1.0f, 2.0f, 3.0f};
		const Quaternion first = Quaternion::FromAxisAngle(Vector3::UnitZ(), 0.7f);
		const Quaternion second = Quaternion::FromAxisAngle(Vector3::UnitX(), -0.4f);
		CHECK((first * second).Rotate(vector)
				  .ApproximatelyEquals(second.Rotate(first.Rotate(vector)), 1.0e-5f));
		CHECK((first * second).ToMatrix4()
				  .ApproximatelyEquals(first.ToMatrix4() * second.ToMatrix4(), 1.0e-5f));
		CHECK((first.ToMatrix4() * second.ToMatrix4())
				  .TransformDirection(vector)
				  .ApproximatelyEquals(
					  second.ToMatrix4().TransformDirection(
						  first.ToMatrix4().TransformDirection(vector)),
					  1.0e-5f));

		CHECK((combined * combined.Inverted()).ApproximatelyEquals(identity, 1.0e-5f));
		CHECK(combined.Conjugate().ApproximatelyEquals(combined.Inverted(), 1.0e-6f));
		CHECK_NEAR(combined.Dot(identity), combined.w, 1.0e-6f);

		const Quaternion target = Quaternion::FromAxisAngle(Vector3::UnitY(), kHalfPi);
		CHECK(Quaternion::Slerp(identity, target, 0.0f).ApproximatelyEquals(identity, 1.0e-6f));
		CHECK(Quaternion::Slerp(identity, target, 1.0f).ApproximatelyEquals(target, 1.0e-5f));
		const Quaternion halfway = Quaternion::Slerp(identity, target, 0.5f);
		const float diagonal = 0.70710678f;
		CHECK(halfway.Rotate(Vector3::UnitX())
				  .ApproximatelyEquals({diagonal, 0.0f, -diagonal}, 1.0e-5f));
	}

	/// Verifies rectangle, box, sphere, plane, and ray intersection tests.
	void TestGeometry()
	{
		const Rect rectangle = Rect::FromPositionSize({10.0f, 20.0f}, {30.0f, 40.0f});
		CHECK(rectangle.minimum.ApproximatelyEquals({10.0f, 20.0f}));
		CHECK(rectangle.maximum.ApproximatelyEquals({40.0f, 60.0f}));
		CHECK(rectangle.GetSize().ApproximatelyEquals({30.0f, 40.0f}));
		CHECK(rectangle.GetCenter().ApproximatelyEquals({25.0f, 40.0f}));
		CHECK(rectangle.Contains({10.0f, 20.0f}));
		CHECK(rectangle.Contains({40.0f, 60.0f}));
		CHECK(!rectangle.Contains({41.0f, 60.0f}));
		CHECK(rectangle.Intersects(Rect::FromPositionSize({35.0f, 50.0f}, {20.0f, 20.0f})));
		CHECK(!rectangle.Intersects(Rect::FromPositionSize({40.0f, 20.0f}, {10.0f, 10.0f})));
		CHECK(rectangle.Expanded(5.0f).Contains({44.0f, 64.0f}));
		CHECK(Rect::FromCenterSize({0.0f, 0.0f}, {10.0f, 10.0f})
				  .minimum.ApproximatelyEquals({-5.0f, -5.0f}));

		const AABB box{{-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f}};
		CHECK(box.GetCenter().ApproximatelyEquals({0.0f, 0.0f, 0.0f}));
		CHECK(box.GetSize().ApproximatelyEquals({2.0f, 4.0f, 6.0f}));
		CHECK(box.Contains({1.0f, 2.0f, 3.0f}));
		CHECK(!box.Contains({1.1f, 0.0f, 0.0f}));
		CHECK(box.Intersects(AABB{{0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}}));
		CHECK(!box.Intersects(AABB{{1.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}}));
		CHECK(AABB::FromCenterSize({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f})
				  .maximum.ApproximatelyEquals({1.0f, 1.0f, 1.0f}));

		const Sphere sphere{{0.0f, 0.0f, 0.0f}, 2.0f};
		CHECK(sphere.Contains({2.0f, 0.0f, 0.0f}));
		CHECK(!sphere.Contains({2.1f, 0.0f, 0.0f}));
		CHECK(sphere.Intersects(Sphere{{3.9f, 0.0f, 0.0f}, 2.0f}));
		CHECK(!sphere.Intersects(Sphere{{4.1f, 0.0f, 0.0f}, 2.0f}));
		CHECK(sphere.Intersects(AABB{{1.5f, -1.0f, -1.0f}, {3.0f, 1.0f, 1.0f}}));
		CHECK(!sphere.Intersects(AABB{{2.1f, -1.0f, -1.0f}, {3.0f, 1.0f, 1.0f}}));

		const Plane plane = Plane::FromPointNormal({0.0f, 5.0f, 0.0f}, {0.0f, 2.0f, 0.0f});
		CHECK_NEAR(plane.normal.Length(), 1.0f, 1.0e-6f);
		CHECK_NEAR((plane.GetSignedDistance({0.0f, 7.0f, 0.0f})), 2.0f, 1.0e-6f);
		CHECK_NEAR((plane.GetSignedDistance({0.0f, 3.0f, 0.0f})), -2.0f, 1.0e-6f);
		CHECK(plane.Project({3.0f, 7.0f, 4.0f}).ApproximatelyEquals({3.0f, 5.0f, 4.0f}, 1.0e-6f));
		CHECK(plane.Intersects(Sphere{{0.0f, 6.0f, 0.0f}, 1.5f}));
		CHECK(!plane.Intersects(Sphere{{0.0f, 7.0f, 0.0f}, 1.5f}));
		const Plane triangle = Plane::FromPoints({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
												 {0.0f, 1.0f, 0.0f});
		CHECK_NEAR((triangle.GetSignedDistance({0.0f, 0.0f, 1.0f})), 1.0f, 1.0e-6f);

		const Ray forward{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
		CHECK(forward.PointAt(3.0f).ApproximatelyEquals({0.0f, 0.0f, 3.0f}));
		const Plane facing_plane = Plane::FromPointNormal({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 1.0f});
		float distance = -1.0f;
		CHECK(forward.Intersects(facing_plane, &distance));
		CHECK_NEAR(distance, 5.0f, 1.0e-6f);
		CHECK(!Ray{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}.Intersects(facing_plane));
		CHECK(!Ray{{0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 1.0f}}.Intersects(facing_plane));
		CHECK(!forward.Intersects(plane));

		distance = -1.0f;
		CHECK(forward.Intersects(sphere, &distance));
		CHECK_NEAR(distance, 2.0f, 1.0e-6f);
		distance = -1.0f;
		CHECK(forward.Intersects(Sphere{{0.0f, 0.0f, 5.0f}, 1.0f}, &distance));
		CHECK_NEAR(distance, 4.0f, 1.0e-6f);
		CHECK(!forward.Intersects(Sphere{{4.0f, 0.0f, 5.0f}, 1.0f}));
		CHECK(!Ray{{0.0f, 0.0f, 6.5f}, {0.0f, 0.0f, 1.0f}}
					   .Intersects(Sphere{{0.0f, 0.0f, 5.0f}, 1.0f}));

		distance = -1.0f;
		const AABB slab{{-1.0f, -1.0f, 4.0f}, {1.0f, 1.0f, 6.0f}};
		CHECK(forward.Intersects(slab, &distance));
		CHECK_NEAR(distance, 4.0f, 1.0e-6f);
		CHECK(!Ray{{5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}.Intersects(slab));
		distance = -1.0f;
		CHECK(Ray{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 1.0f}}.Intersects(slab, &distance));
		CHECK_NEAR(distance, 0.0f, 0.0f);
		const AABB flat{{-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 1.0f}};
		distance = -1.0f;
		CHECK(Ray{{0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}.Intersects(flat, &distance));
		CHECK_NEAR(distance, 5.0f, 1.0e-6f);
	}

	/// Verifies that the 2D camera projection follows the shared orthographic convention.
	void TestCamera2DProjection()
	{
		BlackLight::Engine::Camera2D camera;
		camera.SetViewport(0.0f, 0.0f, 800.0f, 600.0f);
		camera.SetZoom(2.0f);
		camera.SetPosition({100.0f, 50.0f});
		const Matrix4 projection = camera.GetProjection();
		CHECK(projection.TransformPoint({100.0f, 50.0f, 0.0f})
				  .ApproximatelyEquals({0.0f, 0.0f, 0.0f}, 1.0e-6f));
		CHECK(projection.TransformPoint({300.0f, 50.0f, 0.0f})
				  .ApproximatelyEquals({1.0f, 0.0f, 0.0f}, 1.0e-6f));
		CHECK(projection.TransformPoint({100.0f, 200.0f, 0.0f})
				  .ApproximatelyEquals({0.0f, -1.0f, 0.0f}, 1.0e-6f));
		CHECK(camera.ScreenToWorld({400.0f, 300.0f}).ApproximatelyEquals({100.0f, 50.0f}, 1.0e-5f));
		CHECK(camera.WorldToScreen({100.0f, 50.0f}).ApproximatelyEquals({400.0f, 300.0f}, 1.0e-5f));
	}

	/// Verifies frustum plane extraction and conservative culling tests.
	void TestFrustum()
	{
		const Matrix4 view = Matrix4::CreateLookAt({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f},
												   {0.0f, 1.0f, 0.0f});
		const Matrix4 projection =
			Matrix4::CreatePerspectiveFieldOfView(ToRadians(60.0f), 1.0f, 0.5f, 20.0f);
		const Frustum frustum = Frustum::FromViewProjection(view * projection);
		for (const Plane &plane : frustum.planes)
			CHECK_NEAR(plane.normal.Length(), 1.0f, 1.0e-4f);

		CHECK(frustum.Contains({0.0f, 0.0f, 0.0f}));
		CHECK(frustum.Contains({0.0f, 0.0f, 4.0f}));
		CHECK(!frustum.Contains({0.0f, 0.0f, 4.9f}));
		CHECK(!frustum.Contains({0.0f, 0.0f, 5.2f}));
		CHECK(!frustum.Contains({0.0f, 0.0f, -16.0f}));
		CHECK(!frustum.Contains({8.0f, 0.0f, 0.0f}));
		CHECK(frustum.Contains({2.0f, 0.0f, 0.0f}));
		CHECK(frustum.GetPlane(Frustum::Side::Near).GetSignedDistance({0.0f, 0.0f, 4.0f}) > 0.0f);
		CHECK(frustum.GetPlane(Frustum::Side::Near).GetSignedDistance({0.0f, 0.0f, 5.5f}) < 0.0f);

		CHECK(frustum.Intersects(Sphere{{0.0f, 0.0f, 0.0f}, 1.0f}));
		CHECK(frustum.Intersects(Sphere{{0.0f, 0.0f, -16.0f}, 2.0f}));
		CHECK(!frustum.Intersects(Sphere{{0.0f, 0.0f, -25.0f}, 1.0f}));
		CHECK(!frustum.Intersects(Sphere{{0.0f, 30.0f, 0.0f}, 1.0f}));

		CHECK(frustum.Intersects(AABB{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}));
		CHECK(!frustum.Intersects(AABB{{20.0f, 20.0f, 20.0f}, {21.0f, 21.0f, 21.0f}}));
	}

} // namespace

int main()
{
	TestScalarHelpers();
	TestVectors();
	TestColors();
	TestMatrixComposition();
	TestMatrixInverse();
	TestCameraMatrices();
	TestCamera2DProjection();
	TestQuaternions();
	TestGeometry();
	TestFrustum();
	std::puts("Math tests passed.");
	return 0;
}
