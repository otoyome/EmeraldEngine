#pragma once

#include "Graphics/View/view_parameters.h"
#include "collision/frustum_culling.h"

#include <memory>

class scene_view_state;

//	描画視点情報
class scene_view
{
public:
	//	描画原点
	const math::vector3d& get_render_origin() const { return render_origin; }

	//	描画原点から見たカメラ座標
	const math::float3& get_relative_position() const { return relative_position; }

	//	各軸方向
	const math::float3& get_right() const { return right; }
	const math::float3& get_up() const { return up; }
	const math::float3& get_front() const { return front; }

	//	各種行列
	const math::float4x4& get_view_matrix() const { return view_matrix; }
	const math::float4x4& get_projection_matrix() const { return projection_matrix; }
	const math::float4x4& get_view_projection_matrix() const { return view_projection_matrix; }

	//	視野角
	float get_fov_y() const { return fov_y; }

	//	クリップ距離
	const math::float2& get_clip_distance() const { return clip_distance; }

	//	スクリーン矩形
	const math::float4& get_screen_rect() const { return screen_rect; }

	//	視錐台カリング情報
	const collision::frustum_culling& get_frustum_culling() const { return frustum_culling; }

	//	シーン定数バッファ情報生成
	void fill_scene_constants(common_pipeline_states::scene_constants& constants, const scene_view_state* state) const;

public:
	//	視点の設定値から作る
	static scene_view from_parameters(const view_parameters& parameters, const math::vector3d& render_origin);
	//	描画原点を求める
	static math::vector3d compute_render_origin(const view_parameters& parameters, bool camera_relative);

private:
	math::vector3d render_origin;
	math::float3 relative_position;
	math::float3 right;
	math::float3 up;
	math::float3 front;
	math::float4x4 view_matrix;
	math::float4x4 projection_matrix;
	math::float4x4 view_projection_matrix;

	float fov_y = 0.0f;
	math::float2 clip_distance;
	math::float4 screen_rect;

	collision::frustum_culling frustum_culling;
};

using scene_view_ptr = std::shared_ptr<const scene_view>;
