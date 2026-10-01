#pragma once

//	@tool Math ライブラリの規約設定
//	@tool ライブラリは「左手系・行ベクトル・リバースデプス」を基本として設計しており、
//	@tool ここの設定に応じて内部の計算（要素の解釈・掛ける順序・前方向・深度の向き）を切り替える
//	@tool ソリューション内のすべてのプロジェクトで同じ値にすること（異なる値が混ざるとリンク時にエラーになる）
//	@tool ここを書き換えるか、プロジェクトのプリプロセッサ定義で上書きする

//	@tool 行列とベクトルの掛け方
//	@tool 0：行ベクトル（v * M。平行移動は _41, _42, _43。DirectX / UE と同じ）
//	@tool 1：列ベクトル（M * v。平行移動は _14, _24, _34。OpenGL / GLM / Eigen と同じ）
//	@tool 行列の格納（メモリ上の並び）はどちらでも同じ（列ベクトル規約は列優先で格納した形になる）
#if !defined(MATH_COLUMN_VECTOR)
#define	MATH_COLUMN_VECTOR	0
#endif

//	@tool 座標系
//	@tool 0：左手系（前方向 +Z。DirectX / Unity と同じ）
//	@tool 1：右手系（前方向 -Z。OpenGL / Godot と同じ）
#if !defined(MATH_RIGHT_HANDED)
#define	MATH_RIGHT_HANDED	0
#endif

//	@tool 射影の深度の向き
//	@tool 1：リバースデプス（near = 1、far = 0）
//	@tool 0：通常の深度（near = 0、far = 1）
#if !defined(MATH_REVERSE_DEPTH)
#define	MATH_REVERSE_DEPTH	1
#endif

//	@tool 規約が異なるオブジェクトファイルをリンクしようとした場合にリンカーエラーにする
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
