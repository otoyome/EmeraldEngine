#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Math/math_config.h"
#include "Math/math_simd.h"
#include "Math/element.h"

#if !defined(MATH_DISABLE_SIMD)
#define	MATH_USE_SIMD_FLOAT		1
#else
#define	MATH_USE_SIMD_FLOAT		0
#endif

#if !defined(MATH_DISABLE_SIMD)
#define	MATH_USE_SIMD_DOUBLE	1
#else
#define	MATH_USE_SIMD_DOUBLE	0
#endif

namespace math
{
	//	回転順序定数
	enum class rotation_order
	{
		xyz = 0,
		xzy,
		yxz,
		yzx,
		zxy,
		zyx,

		invalid = -1,
	};
	//	既定の回転順序
	constexpr rotation_order default_rotation_order = rotation_order::zxy;

	//	デカルト座標系定数
	enum class coordinate_order
	{
		lh = 0,		//	左手座標系
		rh,			//	右手座標系

		invalid = -1,
	};
#if MATH_RIGHT_HANDED
	constexpr coordinate_order default_coordinate_order = coordinate_order::rh;
#else
	constexpr coordinate_order default_coordinate_order = coordinate_order::lh;
#endif

	//	行列とベクトルの掛け方
	enum class vector_convention
	{
		row,		//	行ベクトル（v * M）
		column,		//	列ベクトル（M * v）
	};
#if MATH_COLUMN_VECTOR
	constexpr vector_convention default_vector_convention = vector_convention::column;
#else
	constexpr vector_convention default_vector_convention = vector_convention::row;
#endif
	//	列ベクトル規約か
	constexpr bool is_column_vector = default_vector_convention == vector_convention::column;

	//	射影行列の深度方向
	enum class projection_z_order
	{
		normal = 0,		//	0～1
		reverse,		//	1～0
	};
#if MATH_REVERSE_DEPTH
	constexpr projection_z_order default_projection_z_order = projection_z_order::reverse;
#else
	constexpr projection_z_order default_projection_z_order = projection_z_order::normal;
#endif

	//	ミラーリング定数
	enum class mirroring_order
	{
		x,
		y,
		z,
	};

	//	デフォルトが逆深度か判定
	constexpr bool is_reverse_depth()
	{
		return default_projection_z_order == projection_z_order::reverse;
	}

	//	型ごとの定数
	template<typename T>
	struct constants;

	template<>
	struct constants<float>
	{
		static constexpr float epsilon = 1.192092896e-07f;
		static constexpr float pi_half = 1.57079633f;
		static constexpr float pi = 3.14159265f;
		static constexpr float pi2 = 6.28318530f;
		static constexpr float sqrt2 = 1.41421356f;
		static constexpr float degree_to_radian = pi / 180.0f;
		static constexpr float radian_to_degree = 180.0f / pi;
		static constexpr float normalize_threshold = 1e-6f;
		static constexpr float gimbal_lock_threshold = 5.5e-3f;
	};

	template<>
	struct constants<double>
	{
		static constexpr double epsilon = 2.2204460492503131e-016;
		static constexpr double pi_half = 1.57079632679489661923132169139750144;
		static constexpr double pi = 3.14159265358979323846264338327950288;
		static constexpr double pi2 = 6.28318530717958647692528676655900576;
		static constexpr double sqrt2 = 1.41421356237309504880168872420969808;
		static constexpr double degree_to_radian = pi / 180.0;
		static constexpr double radian_to_degree = 180.0 / pi;
		static constexpr double normalize_threshold = 1e-12;
		static constexpr double gimbal_lock_threshold = 2.4e-7;
	};

	template<typename T>
	constexpr bool is_supported_scalar_v = std::is_same_v<T, float> || std::is_same_v<T, double>;

	//	SIMD レジスタ型の切り替え（SIMD を使わない型には空の構造体を置く）
	struct no_simd_register {};

	template<typename T>
	struct simd_traits
	{
		static constexpr bool enabled = false;
		using ops = void;
		using register_type = no_simd_register;
	};

#if MATH_USE_SIMD_FLOAT
	template<>
	struct simd_traits<float>
	{
		static constexpr bool enabled = true;
		using ops = simd::f4;
		using register_type = simd::f4::reg;
	};
#endif	//	MATH_USE_SIMD_FLOAT

#if MATH_USE_SIMD_DOUBLE
	template<>
	struct simd_traits<double>
	{
		static constexpr bool enabled = true;
		using ops = simd::d4;
		using register_type = simd::d4::reg;
	};
#endif	//	MATH_USE_SIMD_DOUBLE

	template<typename T>
	constexpr bool use_simd_v = simd_traits<T>::enabled;

	//	SIMD 演算の実装（f4 / d4）
	template<typename T>
	using simd_ops_t = typename simd_traits<T>::ops;

	template<typename T>
	using simd_register_t = typename simd_traits<T>::register_type;

	template<typename T>
	constexpr std::size_t simd_alignment_v = use_simd_v<T> ? alignof(simd_register_t<T>) : alignof(T);

	//	前方宣言
	template<typename T> class vector2;
	template<typename T> class vector3;
	template<typename T> class vector4;
	template<typename T> class quaternion;
	template<typename T> class matrix;
	template<typename T> class exponential_quaternion;
	template<typename T> class basic_rotator;

}	//	namespace math
