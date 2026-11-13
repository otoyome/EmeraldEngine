#pragma once

#include <memory>
#include <vector>

#include "Math/math_all.h"
#include "Texture/texture.h"
#include "Graphics/Core/buffer_view.h"

class material_instance;

//	UI要素情報
struct ui_element
{
	math::float2 x_axis;		//	姿勢行列の_11, _12
	math::float2 y_axis;		//	姿勢行列の_21, _22
	math::float2 translation;	//	姿勢行列の_31, _32
	math::float2 size;			//	矩形の大きさ
	math::float4 uv_rect;		//	xy : 左上, zw : 大きさ
	math::float4 color;			//	色
	math::float4 clip_rect;		//	クリップ矩形
};

//	文字等の頂点情報
struct ui_vertex
{
	math::float2 position;	//	矩形の中の 0～1
	math::float2 texcoord;	//	UV座標
	math::uint color;		//	色
	math::uint element;		//	テクスチャ配列 : 8bit、UI要素番号 : 24bit

	static constexpr std::uint32_t pack_element(std::uint32_t element_index, std::uint32_t layer) { return (layer << 24) | (element_index & 0x00ffffffu); }
};

//	UIバッチ
struct ui_batch
{
	std::shared_ptr<const material_instance> material;	//	マテリアル情報
	math::uint first_index = 0;
	math::uint index_count = 0;
};

//	キャンバスプロキシ
struct ui_canvas_render_proxy
{
	std::unordered_map<semantic, buffer_view> vertex_buffer_views;	//	頂点バッファ
	buffer_view index_buffer_view;									//	インデックスバッファ
	buffer_view element_buffer_view;								//	UI要素バッファ
	std::vector<ui_batch> batches;									//	バッチリスト
};

//	描画リストの要素
struct ui_draw_item
{
	std::shared_ptr<const ui_canvas_render_proxy> proxy;
	const ui_batch* batch = nullptr;
};

//	UIコンテナ
struct ui_container
{
	std::vector<ui_draw_item> draw_items;	//	描画アイテム

	void clear()
	{
		draw_items.clear();
	}
};

