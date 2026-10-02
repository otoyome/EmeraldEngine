#pragma once

//	Math ライブラリの規約設定
//	「左手系・行ベクトル・リバースデプス」を基本として設計しており、

//	行列とベクトルの掛け方
//	0：行ベクトル（v * M。平行移動は _41, _42, _43。DirectX と同じ）
//	1：列ベクトル（M * v。平行移動は _14, _24, _34。OpenGL と同じ）
#if !defined(MATH_COLUMN_VECTOR)
#define	MATH_COLUMN_VECTOR	0
#endif

//	座標系
//	0：左手系（前方向 +Z。DirectX）
//	1：右手系（前方向 -Z。OpenGL）
#if !defined(MATH_RIGHT_HANDED)
#define	MATH_RIGHT_HANDED	0
#endif

//	射影の深度の向き
//	0：通常の深度（near = 0、far = 1）
//	1：リバースデプス（near = 1、far = 0）
#if !defined(MATH_REVERSE_DEPTH)
#define	MATH_REVERSE_DEPTH	1
#endif

//	規約が異なるオブジェクトファイルをリンクしようとした場合にリンカーエラーにする
#if defined(_MSC_VER)
#if MATH_COLUMN_VECTOR
#pragma detect_mismatch("math_column_vector", "1")
#else
#pragma detect_mismatch("math_column_vector", "0")
#endif
#if MATH_RIGHT_HANDED
#pragma detect_mismatch("math_right_handed", "1")
#else
#pragma detect_mismatch("math_right_handed", "0")
#endif
#if MATH_REVERSE_DEPTH
#pragma detect_mismatch("math_reverse_depth", "1")
#else
#pragma detect_mismatch("math_reverse_depth", "0")
#endif
#if defined(MATH_DISABLE_SIMD)
#pragma detect_mismatch("math_disable_simd", "1")
#else
#pragma detect_mismatch("math_disable_simd", "0")
#endif
#endif	//	defined(_MSC_VER)
