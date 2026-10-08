#pragma once

#include "collision/frustum_culling.h"
#include "graphics/hiz_buffer.h"
#include "graphics/occlusion_culling_system.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class meshlet_culling_system;

class camera
{
public:
	enum class flags
	{
		active,

		dirty,
		update_previous_matrix,

		hybrid_projection,

		camera_relative,
	};

public:
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

	//	ワールド座標
	const math::vector3d& get_position() const;
	void set_position(const math::vector3d& position);

	//	描画の原点
	math::vector3d get_render_origin() const;

	//	描画の原点から見たカメラ位置
	math::float3 get_relative_position() const;

	const math::rotatorf& get_rotation() const;
	void set_rotation(const math::rotatorf& rotation);

	float get_fov_y() const;
	void set_fov_y(float fov_y);

	float get_hybrid_rate() const;
	void set_hybrid_rate(float hybrid_rate);

	const math::float4& get_screen_rect() const;
	void set_screen_rect(math::float4 screen_rect);
	const math::float2& get_clip_distance() const;
	void set_clip_distance(math::float2 clip_distance);
	math::float2 get_effective_clip_distance() const;

	const math::float3& get_right();
	const math::float3& get_up();
	const math::float3& get_front();
	const math::float4x4& get_view_matrix();
	const math::float4x4& get_projection_matrix();
	const math::float4x4& get_view_projection_matrix();
	const math::float4x4& get_previous_view_projection_matrix();

	const collision::frustum_culling& get_frustum_culling();

	std::shared_ptr<occlusion_culling_system> get_occlusion_culling();
	std::shared_ptr<hiz_buffer> get_hiz_buffer();
	std::shared_ptr<meshlet_culling_system> get_meshlet_culling();

	float get_occlusion_animation_distance() const { return occlusion_animation_distance; }
	void set_occlusion_animation_distance(float distance) { occlusion_animation_distance = distance; }

#if defined(USE_IMGUI)
	//	デバッグ用にカリング結果を固定する
	bool is_freeze_culling() const { return freeze_culling; }
	void set_freeze_culling(bool flag) { freeze_culling = flag; }
	bool resolve_debug_visible(UINT unique_id, bool current_visible);

	//	デバッグ用にメッシュレット単位で色分け表示する
	bool is_debug_visualize_meshlets() const { return debug_visualize_meshlets; }
	void set_debug_visualize_meshlets(bool flag) { debug_visualize_meshlets = flag; }
#endif	//	defined(USE_IMGUI)
	void cache_previous_view_projection_matrix();

public:
	camera();
	~camera();

private:
	void update_parameters();

private:
	UINT system_flags;
	math::vector3d position;
	math::rotatorf rotation;
	math::float3 right;
	math::float3 up;
	math::float3 front;
	math::float4x4 view_matrix;
	math::float4x4 projection_matrix;
	math::float4x4 view_projection_matrix;
	math::float4x4 previous_view_projection_matrix;

	//	前フレームの姿勢と射影
	math::vector3d previous_position;
	math::quaternionf previous_rotation;
	math::float4x4 previous_projection_matrix;

	float fov_y;
	math::float4 screen_rect;
	math::float2 clip_distance;
	float hybrid_rate;

	collision::frustum_culling frustum_culling;

	std::shared_ptr<occlusion_culling_system> occlusion_culling;
	std::shared_ptr<hiz_buffer> hiz;
	std::shared_ptr<meshlet_culling_system> meshlet_culling;
	float occlusion_animation_distance = system_units::convert_cm_to_unit(1.0f);

#if defined(USE_IMGUI)
	bool freeze_culling = false;
	bool debug_visualize_meshlets = false;
	std::mutex frozen_visibilities_mutex;
	std::unordered_map<UINT, bool> frozen_visibilities;
#endif	//	defined(USE_IMGUI)
};

using camera_ptr = std::shared_ptr<camera>;

//	カメラコンテナ
struct camera_container
{
	struct camera_data
	{
		camera_ptr camera;
		Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	};
	std::vector<camera_data> cameras;
};

//	カメラマネージャー
class camera_manager
{
	SINGLETON(camera_manager)

private:
	camera_manager();
	~camera_manager();

	void generate_constant_bufferes();

public:
	bool generate_camera(camera_ptr& out_camera);
	void get_container(camera_container& container);

private:
	std::vector<std::weak_ptr<camera>> cameras;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constant_bufferes;
};
