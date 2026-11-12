#pragma once

#include <memory>
#include <vector>

#include "Math/math_all.h"
#include "Texture/texture.h"

class material_instance;

//	矩形情報
struct ui_item_parameters
{
	math::float2 x_axis;		//	姿勢行列の_11, _12
	math::float2 y_axis;		//	姿勢行列の_21, _22
	math::float2 translation;	//	姿勢行列の_31, _32
	math::float2 size;			//	矩形の大きさ
	math::float4 uv_rect;		//	xy : 左上, zw : 大きさ
	math::float4 color;			//	色
};

//	文字等の頂点情報
struct ui_vertex
{
	math::float2 position;	//	矩形の中の 0～1
	math::float2 texcoord;	//	UV座標
	math::uint color;		//	色
	math::uint item_index;	//	ui_item_parameters の番号
};

//	
struct ui_draw_item
{
};
