#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "test_framework.h"
#include "Math/interpolation.h"

//	@tool DirectXMath からの移行用：ゲームで使っている DirectXMath の関数と同じ結果になることを確認する
//	@tool DirectXMath は行ベクトル規約・左手系なので、その構成のときだけ比較する（投影の深度方向は引数で明示する）
#if !MATH_COLUMN_VECTOR && !MATH_RIGHT_HANDED

using namespace math;

namespace
{
	//	@tool DirectXMath は float で計算するため、double の場合も float の精度で比較する
	constexpr double xm_tolerance = 2e-4;

	template<typename T>
	matrix<T> from_xm(DirectX::FXMMATRIX m)
	{
		DirectX::XMFLOAT4X4 f;
		DirectX::XMStoreFloat4x4(&f, m);
		matrix<T> ret;
		for (std::uint32_t row = 0; row < 4; ++row)
		{
			for (std::uint32_t column = 0; column < 4; ++column)
			{
				ret.storage[row][column] = static_cast<T>(f.m[row][column]);
			}
		}
		return ret;
	}

	template<typename T>
	vector3<T> from_xm3(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT3 f;
		DirectX::XMStoreFloat3(&f, v);
		return vector3<T>(static_cast<T>(f.x), static_cast<T>(f.y), static_cast<T>(f.z));
	}

	template<typename T>
	vector4<T> from_xm4(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT4 f;
		DirectX::XMStoreFloat4(&f, v);
		return vector4<T>(static_cast<T>(f.x), static_cast<T>(f.y), static_cast<T>(f.z), static_cast<T>(f.w));
	}

	template<typename T>
	quaternion<T> from_xmq(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT4 f;
		DirectX::XMStoreFloat4(&f, v);
		return quaternion<T>(static_cast<T>(f.x), static_cast<T>(f.y), static_cast<T>(f.z), static_cast<T>(f.w));
	}

	template<typename T>
	DirectX::XMVECTOR to_xm(const vector3<T>& v)
	{
		return DirectX::XMVectorSet(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z), 0);
	}

	template<typename T>
	DirectX::XMVECTOR to_xm(const quaternion<T>& q)
	{
		return DirectX::XMVectorSet(static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z), static_cast<float>(q.w));
	}

	template<typename T>
	DirectX::XMMATRIX to_xm(const matrix<T>& m)
	{
		DirectX::XMFLOAT4X4 f;
		for (std::uint32_t row = 0; row < 4; ++row)
		{
			for (std::uint32_t column = 0; column < 4; ++column)
			{
				f.m[row][column] = static_cast<float>(m.storage[row][column]);
			}
		}
		return DirectX::XMLoadFloat4x4(&f);
	}
}

//	@tool 回転（オイラー角・クォータニオン）
MATH_TEST_TYPED(directxmath_rotation)
{
	const double tol = xm_tolerance;
	const float pitch = 0.3f, yaw = -1.1f, roll = 0.7f;
	const vector3<T> euler(static_cast<T>(pitch), static_cast<T>(yaw), static_cast<T>(roll));

	//	@tool XMQuaternionRotationRollPitchYaw / XMMatrixRotationRollPitchYaw は既定の回転順（zxy）と同じ
	CHECK_SAME_ROTATION(quaternion<T>::rotation_xyz(euler), from_xmq<T>(DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll)), tol);
	CHECK_NEAR(matrix<T>::rotation_xyz(euler), from_xm<T>(DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll)), tol);
	CHECK_NEAR(matrix<T>::rotation_x(euler.x), from_xm<T>(DirectX::XMMatrixRotationX(pitch)), tol);
	CHECK_NEAR(matrix<T>::rotation_y(euler.y), from_xm<T>(DirectX::XMMatrixRotationY(yaw)), tol);
	CHECK_NEAR(matrix<T>::rotation_z(euler.z), from_xm<T>(DirectX::XMMatrixRotationZ(roll)), tol);

	const quaternion<T> a = quaternion<T>::rotation_xyz(euler);
	const quaternion<T> b = quaternion<T>::rotation_xyz(static_cast<T>(-0.4), static_cast<T>(0.9), static_cast<T>(0.2));
	CHECK_NEAR(matrix<T>(a), from_xm<T>(DirectX::XMMatrixRotationQuaternion(to_xm(a))), tol);
	//	@tool XMQuaternionMultiply(a, b) は「a の後に b」で、行ベクトル規約の a * b / concatenate(a, b) と同じ
	CHECK_SAME_ROTATION(a * b, from_xmq<T>(DirectX::XMQuaternionMultiply(to_xm(a), to_xm(b))), tol);
	CHECK_SAME_ROTATION(quaternion<T>::concatenate(a, b), from_xmq<T>(DirectX::XMQuaternionMultiply(to_xm(a), to_xm(b))), tol);

	//	@tool XMQuaternionRotationAxis は軸を正規化して使う（長さが 1 でない軸でも同じ結果になる）
	for (const vector3<T>& axis : { vector3<T>(0, 1, 0), vector3<T>(0, 2, 0), vector3<T>(1, 2, 3) })
	{
		CHECK_SAME_ROTATION(quaternion<T>::rotation_axis(axis, static_cast<T>(0.8)), from_xmq<T>(DirectX::XMQuaternionRotationAxis(to_xm(axis), 0.8f)), tol);
	}

	CHECK_SAME_ROTATION(slerp(a, b, static_cast<T>(0.3)), from_xmq<T>(DirectX::XMQuaternionSlerp(to_xm(a), to_xm(b), 0.3f)), tol);
	CHECK_NEAR(a.inverted(), from_xmq<T>(DirectX::XMQuaternionInverse(to_xm(a))), tol);

	vector3<T> axis;
	T angle;
	a.axis_angle(axis, angle);
	DirectX::XMVECTOR xm_axis;
	float xm_angle;
	DirectX::XMQuaternionToAxisAngle(&xm_axis, &xm_angle, to_xm(a));
	CHECK_NEAR(axis, from_xm3<T>(DirectX::XMVector3Normalize(xm_axis)), tol);
	CHECK_NEAR(angle, static_cast<T>(xm_angle), tol);

	const vector3<T> v(1, 2, 3);
	CHECK_NEAR(a.rotate(v), from_xm3<T>(DirectX::XMVector3Rotate(to_xm(v), to_xm(a))), tol);
}

//	@tool 行列の生成・演算・分解とベクトルの変換
MATH_TEST_TYPED(directxmath_matrix)
{
	const double tol = xm_tolerance;
	const quaternion<T> rotation = quaternion<T>::rotation_xyz(static_cast<T>(0.3), static_cast<T>(-1.1), static_cast<T>(0.7));
	const vector3<T> scale(2, 3, 4), position(5, 6, 7);

	CHECK_NEAR(matrix<T>::scaling(scale), from_xm<T>(DirectX::XMMatrixScaling(2, 3, 4)), tol);
	CHECK_NEAR(matrix<T>::translation(position), from_xm<T>(DirectX::XMMatrixTranslation(5, 6, 7)), tol);
	CHECK_NEAR(matrix<T>::scaling(scale) * matrix<T>::translation(position), from_xm<T>(DirectX::XMMatrixMultiply(DirectX::XMMatrixScaling(2, 3, 4), DirectX::XMMatrixTranslation(5, 6, 7))), tol);

	//	@tool XMMatrixAffineTransformation(S, 0, R, T) は transform(S, R, T) と同じ
	const matrix<T> world = matrix<T>::affine_transformation(scale, rotation, position);
	const DirectX::XMMATRIX xm_world = DirectX::XMMatrixAffineTransformation(to_xm(scale), DirectX::XMVectorZero(), to_xm(rotation), to_xm(position));
	CHECK_NEAR(world, from_xm<T>(xm_world), tol);
	CHECK_NEAR(world.inverted(), from_xm<T>(DirectX::XMMatrixInverse(nullptr, xm_world)), tol);

	//	@tool XMMatrixDecompose（負のスケールを含む）
	for (const matrix<T>& m : { world, matrix<T>::scaling(-1, 1, 1) * world })
	{
		vector3<T> out_scale, out_position;
		quaternion<T> out_rotation;
		CHECK(m.decompose(out_scale, out_rotation, out_position));
		DirectX::XMVECTOR xm_scale, xm_rotation, xm_position;
		CHECK(DirectX::XMMatrixDecompose(&xm_scale, &xm_rotation, &xm_position, to_xm(m)));
		CHECK_NEAR(out_scale, from_xm3<T>(xm_scale), tol);
		CHECK_SAME_ROTATION(out_rotation, from_xmq<T>(xm_rotation), tol);
		CHECK_NEAR(out_position, from_xm3<T>(xm_position), tol);
	}

	//	@tool 逆行列が存在しない場合：DirectXMath は行列式 0 を返す。try_inverse は false を返す
	DirectX::XMVECTOR determinant;
	DirectX::XMMatrixInverse(&determinant, DirectX::XMMatrixScaling(1, 0, 1));
	matrix<T> inverse_out;
	CHECK(DirectX::XMVectorGetX(determinant) == 0 && !matrix<T>::try_inverse(matrix<T>::scaling(1, 0, 1), inverse_out));

	const vector3<T> v(1, 2, 3);
	CHECK_NEAR(vector3<T>::transform_coord(v, world), from_xm3<T>(DirectX::XMVector3TransformCoord(to_xm(v), xm_world)), tol);
	CHECK_NEAR(vector3<T>::transform_normal(v, world), from_xm3<T>(DirectX::XMVector3TransformNormal(to_xm(v), xm_world)), tol);
	CHECK_NEAR(vector3<T>::transform(v, world), from_xm4<T>(DirectX::XMVector3Transform(to_xm(v), xm_world)), tol);
}

//	@tool 投影・視線行列とスクリーン座標からの逆変換
MATH_TEST_TYPED(directxmath_projection)
{
	const double tol = xm_tolerance;
	const T near_z = static_cast<T>(0.1), far_z = static_cast<T>(1000);
	const float xm_near = 0.1f, xm_far = 1000.0f;

	//	@tool DirectXMath は near / far を入れ替えてリバースデプスにする。このライブラリは near / far の順のまま projection_z_order で指定する
	CHECK_NEAR(matrix<T>::perspective_fov(1, static_cast<T>(1.6), near_z, far_z, coordinate_order::lh, projection_z_order::reverse), from_xm<T>(DirectX::XMMatrixPerspectiveFovLH(1, 1.6f, xm_far, xm_near)), tol);
	CHECK_NEAR(matrix<T>::perspective_fov(1, static_cast<T>(1.6), near_z, far_z, coordinate_order::lh, projection_z_order::normal), from_xm<T>(DirectX::XMMatrixPerspectiveFovLH(1, 1.6f, xm_near, xm_far)), tol);
	CHECK_NEAR(matrix<T>::orthographic(20, 10, near_z, far_z, coordinate_order::lh, projection_z_order::reverse), from_xm<T>(DirectX::XMMatrixOrthographicLH(20, 10, xm_far, xm_near)), tol);
	CHECK_NEAR(matrix<T>::orthographic(20, 10, near_z, far_z, coordinate_order::lh, projection_z_order::normal), from_xm<T>(DirectX::XMMatrixOrthographicLH(20, 10, xm_near, xm_far)), tol);

	//	@tool 中心点移動付きの投影は引数の順番（left, right, bottom, top）も DirectXMath と同じ
	CHECK_NEAR(matrix<T>::orthographic_offset_center(-3, 5, -2, 4, near_z, far_z, coordinate_order::lh, projection_z_order::reverse), from_xm<T>(DirectX::XMMatrixOrthographicOffCenterLH(-3, 5, -2, 4, xm_far, xm_near)), tol);
	CHECK_NEAR(matrix<T>::orthographic_offset_center(-3, 5, -2, 4, near_z, far_z, coordinate_order::lh, projection_z_order::normal), from_xm<T>(DirectX::XMMatrixOrthographicOffCenterLH(-3, 5, -2, 4, xm_near, xm_far)), tol);
	CHECK_NEAR(matrix<T>::perspective_off_center(static_cast<T>(-0.1), static_cast<T>(0.2), static_cast<T>(-0.05), static_cast<T>(0.07), near_z, far_z, coordinate_order::lh, projection_z_order::normal),
			   from_xm<T>(DirectX::XMMatrixPerspectiveOffCenterLH(-0.1f, 0.2f, -0.05f, 0.07f, xm_near, xm_far)), tol);

	const vector3<T> eye(1, 2, -5), direction(static_cast<T>(0.2), static_cast<T>(-0.1), 1), up(0, 1, 0);
	const matrix<T> view = matrix<T>::look_at(eye, eye + direction, up, coordinate_order::lh);
	CHECK_NEAR(view, from_xm<T>(DirectX::XMMatrixLookAtLH(to_xm(eye), to_xm(eye + direction), to_xm(up))), tol);
	//	@tool XMMatrixLookToLH(eye, direction, up) は look_at(eye, eye + direction, up) と同じ
	CHECK_NEAR(view, from_xm<T>(DirectX::XMMatrixLookToLH(to_xm(eye), to_xm(direction), to_xm(up))), tol);

	//	@tool XMVector3Unproject は calc_screen_to_world とビューポート行列で置き換えられる
	const matrix<T> projection = matrix<T>::perspective_fov(1, static_cast<T>(1.6), near_z, far_z, coordinate_order::lh, projection_z_order::reverse);
	const matrix<T> viewport = matrix<T>::viewport(0, 1280, 0, 720, 0, 1);
	const DirectX::XMVECTOR xm_world = DirectX::XMVector3Unproject(DirectX::XMVectorSet(300, 200, 0.5f, 1), 0, 0, 1280, 720, 0, 1,
		DirectX::XMMatrixPerspectiveFovLH(1, 1.6f, xm_far, xm_near), to_xm(view), DirectX::XMMatrixIdentity());
	CHECK_NEAR(vector3<T>::calc_screen_to_world(300, 200, static_cast<T>(0.5), view, projection, viewport), from_xm3<T>(xm_world), 1e-3);
}

//	@tool ベクトル演算
MATH_TEST_TYPED(directxmath_vector)
{
	const double tol = xm_tolerance;
	for (const vector3<T>& v : { vector3<T>(1, 2, 3), vector3<T>(1, -2, 3), vector3<T>(-1, 2, -3), vector3<T>(1, 2, -3), vector3<T>(0, 0, 1) })
	{
		CHECK_NEAR(vector3<T>::orthogonal(v), from_xm3<T>(DirectX::XMVector3Orthogonal(to_xm(v))), tol);
	}

	//	@tool 正規化：長さ 0 は 0 ベクトル、それ以外は短いベクトルも正規化する
	for (const vector3<T>& v : { vector3<T>(0, 0, 0), vector3<T>(static_cast<T>(1e-7), 0, 0), vector3<T>(3, 4, 12) })
	{
		CHECK_NEAR(v.normalized(), from_xm3<T>(DirectX::XMVector3Normalize(to_xm(v))), tol);
	}

	const vector3<T> l(1, 2, 3), r(-4, 5, 0);
	CHECK_NEAR(lerp(l, r, static_cast<T>(0.3)), from_xm3<T>(DirectX::XMVectorLerp(to_xm(l), to_xm(r), 0.3f)), tol);
	CHECK_NEAR(vector3<T>::cross(l, r), from_xm3<T>(DirectX::XMVector3Cross(to_xm(l), to_xm(r))), tol);

	//	@tool XMVectorGreater + XMVectorOrInt + XMVector3AnyTrue の組み合わせは any_greater で置き換えられる
	const vector3<T> box_min(0, 0, 0), box_max(1, 1, 1), triangle_min(static_cast<T>(0.5), 2, static_cast<T>(0.5)), triangle_max(1, 3, 1);
	const bool xm_disjoint = DirectX::Internal::XMVector3AnyTrue(DirectX::XMVectorOrInt(DirectX::XMVectorGreater(to_xm(triangle_min), to_xm(box_max)), DirectX::XMVectorGreater(to_xm(box_min), to_xm(triangle_max))));
	CHECK(xm_disjoint == (any_greater(triangle_min, box_max) || any_greater(box_min, triangle_max)));

	T s, c;
	float xm_s, xm_c;
	angle::sin_cos(static_cast<T>(2.5), s, c);
	DirectX::XMScalarSinCos(&xm_s, &xm_c, 2.5f);
	CHECK_NEAR(s, static_cast<T>(xm_s), tol);
	CHECK_NEAR(c, static_cast<T>(xm_c), tol);
}

#endif
