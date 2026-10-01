#pragma once

//	@tool Math ライブラリ（float / double 両対応）の一括インクルード用ヘッダー
//	@tool 型の別名：vector2f / vector2d、vector3f / vector3d、vector4f / vector4d、
//	@tool quaternionf / quaterniond、matrixf / matrixd、exponential_quaternionf / exponential_quaterniond、rotatorf / rotatord
//	@tool 保存用の型：float2 / float3 / float4 / float4x4 など（Math/element.h）

#include "Math/element.h"
#include "Math/math_common.h"
#include "Math/math_simd.h"
#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Math/vector4.h"
#include "Math/quaternion.h"
#include "Math/exponential_quaternion.h"
#include "Math/matrix.h"
#include "Math/rotator.h"
#include "Math/angle.h"
#include "Math/utility.h"
#include "Math/interpolation.h"
#include "Math/easing.h"
