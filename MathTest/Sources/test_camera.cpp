#include <limits>

#include "test_framework.h"

using namespace math;

namespace
{
	//	@tool ビュー空間の点を射影して NDC の深度を求める
	template<typename T>
	T projected_depth(const matrix<T>& projection, const vector3<T>& view_position)
	{
		const vector4<T> clip = vector4<T>::transform(vector4<T>(view_position, 1), projection);
		return clip.z / clip.w;
	}

	template<typename T>
	vector3<T> forward_in(coordinate_order coordinate)
	{
		return vector3<T>::forward(coordinate);
	}
}

//	@tool 視線行列：視点が原点に、注視点が前方向（左手系 +Z / 右手系 -Z）に来る
MATH_TEST_TYPED(camera_look_at)
{
	const double tol = std::is_same_v<T, float> ? 1e-3 : 1e-9;
	const vector3<T> eye(1, 2, 3), target(4, -1, 7);
	const T distance = (target - eye).length();
	for (coordinate_order coordinate : { coordinate_order::lh, coordinate_order::rh })
	{
		const matrix<T> view = matrix<T>::look_at(eye, target, vector3<T>::up(), coordinate);
		CHECK_NEAR(vector3<T>::transform_coord(eye, view), vector3<T>(), tol);
		CHECK_NEAR(vector3<T>::transform_coord(target, view), forward_in<T>(coordinate) * distance, tol);
		//	@tool ビュー行列はカメラ姿勢（look_rotation の回転 -> 視点への平行移動）の逆行列
		const matrix<T> pose = matrix<T>::concatenate(matrix<T>(quaternion<T>::look_rotation((target - eye).normalized(), vector3<T>::up(), coordinate)), matrix<T>::translation(eye));
		CHECK_NEAR(view, pose.inverted(), tol);
	}
}

//	@tool 透視投影：near / far の深度（リバースデプス：1 / 0、通常：0 / 1）と無限遠
MATH_TEST_TYPED(camera_perspective_depth)
{
	const double tol = std::is_same_v<T, float> ? 1e-4 : 1e-9;
	const T near_z = static_cast<T>(0.1), far_z = static_cast<T>(1000);
	for (coordinate_order coordinate : { coordinate_order::lh, coordinate_order::rh })
	{
		const vector3<T> forward = forward_in<T>(coordinate);
		for (projection_z_order z_order : { projection_z_order::reverse, projection_z_order::normal })
		{
			const T near_depth = z_order == projection_z_order::reverse ? static_cast<T>(1) : static_cast<T>(0);
			const matrix<T> fov = matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, far_z, coordinate, z_order);
			CHECK_NEAR(projected_depth(fov, forward * near_z), near_depth, tol);
			CHECK_NEAR(projected_depth(fov, forward * far_z), 1 - near_depth, tol);

			const matrix<T> box = matrix<T>::perspective(static_cast<T>(2), static_cast<T>(1), near_z, far_z, coordinate, z_order);
			CHECK_NEAR(projected_depth(box, forward * near_z), near_depth, tol);
			CHECK_NEAR(projected_depth(box, forward * far_z), 1 - near_depth, tol);

			const matrix<T> off_center = matrix<T>::perspective_off_center(static_cast<T>(-0.1), static_cast<T>(0.2), static_cast<T>(-0.05), static_cast<T>(0.05), near_z, far_z, coordinate, z_order);
			CHECK_NEAR(projected_depth(off_center, forward * near_z), near_depth, tol);

			//	@tool far < near で無限遠（near の深度は同じ、遠方は far 側の値に近づく）
			const matrix<T> infinite = matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, static_cast<T>(-1), coordinate, z_order);
			CHECK_NEAR(projected_depth(infinite, forward * near_z), near_depth, tol);
			CHECK_NEAR(projected_depth(infinite, forward * static_cast<T>(1e7)), 1 - near_depth, 1e-6);
		}
	}

	//	@tool far == near は、どの透視投影でも無限遠として扱う
	const matrix<T> same_near_far = matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, near_z);
	CHECK_NEAR(same_near_far, matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, static_cast<T>(-1)), tol);
	const matrix<T> same_off_center = matrix<T>::perspective_off_center(static_cast<T>(-0.1), static_cast<T>(0.1), static_cast<T>(-0.1), static_cast<T>(0.1), near_z, near_z);
	CHECK_NEAR(same_off_center, matrix<T>::perspective_off_center(static_cast<T>(-0.1), static_cast<T>(0.1), static_cast<T>(-0.1), static_cast<T>(0.1), near_z, static_cast<T>(-1)), tol);
	//	@tool 中心を移動しない perspective_fov_offset_center は perspective_fov と同じ
	CHECK_NEAR(matrix<T>::perspective_fov_offset_center(static_cast<T>(1), static_cast<T>(1.5), near_z, far_z, 0, 0), matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, far_z), 1e-3);

	//	@tool far に無限大を渡しても無限遠として扱う
	CHECK_NEAR(matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, std::numeric_limits<T>::infinity()), matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), near_z, static_cast<T>(-1)), tol);
	CHECK_NEAR(convert_depth(static_cast<T>(0.5), near_z, std::numeric_limits<T>::infinity()), convert_depth(static_cast<T>(0.5), near_z, static_cast<T>(-1)), tol);

	//	@tool 既定値は MATH_REVERSE_DEPTH に従う
	CHECK(default_projection_z_order == (MATH_REVERSE_DEPTH ? projection_z_order::reverse : projection_z_order::normal));
	CHECK(is_reverse_depth() == (MATH_REVERSE_DEPTH != 0));
}

//	@tool 透視投影：視野角の端が NDC の端（±1）に来る
MATH_TEST_TYPED(camera_perspective_frustum)
{
	const double tol = std::is_same_v<T, float> ? 1e-4 : 1e-9;
	const T fov_y = static_cast<T>(1.2), aspect = static_cast<T>(16.0 / 9.0);
	const matrix<T> projection = matrix<T>::perspective_fov(fov_y, aspect, static_cast<T>(0.1), static_cast<T>(100));
	const T distance = 10;
	const T half_height = distance * std::tan(fov_y * static_cast<T>(0.5));
	const vector3<T> forward = vector3<T>::forward();
	const vector4<T> top = vector4<T>::transform(vector4<T>(forward * distance + vector3<T>(0, half_height, 0), 1), projection);
	const vector4<T> right = vector4<T>::transform(vector4<T>(forward * distance + vector3<T>(half_height * aspect, 0, 0), 1), projection);
	CHECK_NEAR(top.y / top.w, static_cast<T>(1), tol);
	CHECK_NEAR(right.x / right.w, static_cast<T>(1), tol);
	CHECK(top.w > 0);
}

//	@tool 正射影
MATH_TEST_TYPED(camera_orthographic)
{
	const double tol = std::is_same_v<T, float> ? 1e-4 : 1e-9;
	for (coordinate_order coordinate : { coordinate_order::lh, coordinate_order::rh })
	{
		const vector3<T> forward = forward_in<T>(coordinate);
		for (projection_z_order z_order : { projection_z_order::reverse, projection_z_order::normal })
		{
			const T near_depth = z_order == projection_z_order::reverse ? static_cast<T>(1) : static_cast<T>(0);
			const matrix<T> ortho = matrix<T>::orthographic(static_cast<T>(20), static_cast<T>(10), static_cast<T>(1), static_cast<T>(500), coordinate, z_order);
			CHECK_NEAR(projected_depth(ortho, forward * static_cast<T>(1)), near_depth, tol);
			CHECK_NEAR(projected_depth(ortho, forward * static_cast<T>(500)), 1 - near_depth, tol);
			const vector4<T> corner = vector4<T>::transform(vector4<T>(10, 5, forward.z * 50, 1), ortho);
			CHECK_NEAR(corner.x, static_cast<T>(1), tol);
			CHECK_NEAR(corner.y, static_cast<T>(1), tol);
		}
	}
	//	@tool 左・右・下・上の順で指定し（DirectXMath と同じ）、左 = -1、右 = +1、上 = +1、下 = -1 になる
	const matrix<T> offset = matrix<T>::orthographic_offset_center(0, 100, 50, 0, 1, 10, coordinate_order::lh, projection_z_order::normal);
	CHECK_NEAR(projected_depth(offset, vector3<T>(0, 0, 1)), static_cast<T>(0), tol);
	CHECK_NEAR(vector4<T>::transform(vector4<T>(50, 25, 5, 1), offset).x, static_cast<T>(0), tol);
	CHECK_NEAR(vector4<T>::transform(vector4<T>(0, 0, 5, 1), offset).x, static_cast<T>(-1), tol);
	CHECK_NEAR(vector4<T>::transform(vector4<T>(0, 0, 5, 1), offset).y, static_cast<T>(1), tol);
	CHECK_NEAR(vector4<T>::transform(vector4<T>(100, 50, 5, 1), offset).y, static_cast<T>(-1), tol);
	const matrix<T> perspective_box = matrix<T>::perspective_off_center(-1, 1, -1, 1, 1, 10, coordinate_order::lh, projection_z_order::normal);
	CHECK(vector4<T>::transform(vector4<T>(0, 1, 1, 1), perspective_box).y > 0);
}

//	@tool ビューポートとスクリーン座標からの逆変換
MATH_TEST_TYPED(camera_screen_to_world)
{
	const double tol = std::is_same_v<T, float> ? 2e-3 : 1e-9;
	const vector3<T> eye(1, 2, 3), target(4, -1, 7);
	const matrix<T> view = matrix<T>::look_at(eye, target, vector3<T>::up());
	const matrix<T> projection = matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1.5), static_cast<T>(0.1), static_cast<T>(1000));
	const matrix<T> viewport = matrix<T>::viewport(0, 1920, 0, 1080, 0, 1);

	//	@tool ビューポートは NDC (-1, 1) を左上 (0, 0)、(1, -1) を右下 (1920, 1080) に写す
	CHECK_NEAR(vector3<T>::transform_coord(vector3<T>(-1, 1, 0), viewport), vector3<T>(0, 0, 0), tol);
	CHECK_NEAR(vector3<T>::transform_coord(vector3<T>(1, -1, 1), viewport), vector3<T>(1920, 1080, 1), tol);

	//	@tool 画面中央の near 面上の点は視点から注視点の方向にある
	const T near_depth = is_reverse_depth() ? static_cast<T>(1) : static_cast<T>(0);
	const vector3<T> world = vector3<T>::calc_screen_to_world(960, 540, near_depth, view, projection, viewport);
	CHECK_NEAR((world - eye).normalized(), (target - eye).normalized(), tol);
	CHECK_NEAR((world - eye).length(), static_cast<T>(0.1), tol);

	//	@tool calc_screen_to_view はビュー空間の座標を返す（画面中央・near 面なら前方向に near の距離）
	const vector3<T> view_position = vector3<T>::calc_screen_to_view(960, 540, near_depth, projection, viewport);
	CHECK_NEAR(view_position, vector3<T>::forward() * static_cast<T>(0.1), tol);
	CHECK_NEAR(vector3<T>::transform_coord(view_position, view.inverted()), world, tol);
}

//	@tool 深度の変換（正規化済み深度 <-> ビュー空間の距離）
MATH_TEST_TYPED(camera_depth_conversion)
{
	const double tol = std::is_same_v<T, float> ? 1e-3 : 1e-9;
	const T near_z = static_cast<T>(0.1), far_z = static_cast<T>(1000);
	for (projection_z_order z_order : { projection_z_order::reverse, projection_z_order::normal })
	{
		const matrix<T> projection = matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1), near_z, far_z, coordinate_order::lh, z_order);
		for (T distance : { static_cast<T>(0.1), static_cast<T>(1), static_cast<T>(42), static_cast<T>(999) })
		{
			const T depth = projected_depth(projection, vector3<T>(0, 0, distance));
			CHECK_NEAR(convert_depth(depth, near_z, far_z, z_order) / distance, static_cast<T>(1), tol);
			CHECK_NEAR(convert_linear_depth(depth, near_z, far_z, z_order), (distance - near_z) / (far_z - near_z), tol);

			//	@tool 無限遠（far < near）でも距離に戻せる
			const matrix<T> infinite = matrix<T>::perspective_fov(static_cast<T>(1), static_cast<T>(1), near_z, static_cast<T>(-1), coordinate_order::lh, z_order);
			const T infinite_depth = projected_depth(infinite, vector3<T>(0, 0, distance));
			CHECK_NEAR(convert_depth(infinite_depth, near_z, static_cast<T>(-1), z_order) / distance, static_cast<T>(1), tol);
		}
	}
}
