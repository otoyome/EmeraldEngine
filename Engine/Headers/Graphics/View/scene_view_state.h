#pragma once

#include "Graphics/Culling/hiz_buffer.h"
#include "Graphics/Culling/occlusion_culling_system.h"
#include "Graphics/View/exposure_controller.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class scene_view;
class meshlet_culling_system;
class clustered_lighting_system;

using scene_view_state_key = core::hash_key;

//	描画制御情報
class scene_view_state
{
	friend class scene_view_state_cache;

public:
	//	描画前後に呼ぶ
	void begin_frame(const scene_view& current, bool camera_cut);
	void end_frame(const scene_view& current);

	//	過去の視線投影行列
	const math::float4x4& get_previous_view_projection_matrix() const { return previous_view_projection_matrix; }

	//	階層Z-Buffer
	const std::shared_ptr<hiz_buffer>& get_hiz_buffer() const { return hiz; }

	//	オクルージョンカリングシステム
	const std::shared_ptr<occlusion_culling_system>& get_occlusion_culling() const { return occlusion_culling; }

	//	メッシュレットカリングシステム
	const std::shared_ptr<meshlet_culling_system>& get_meshlet_culling() const { return meshlet_culling; }

	//	クラスタードライティングシステム
	const std::shared_ptr<clustered_lighting_system>& get_clustered_lighting() const { return clustered_lighting; }

	//	シーンの露出
	const std::shared_ptr<exposure_state>& get_exposure() const { return exposure; }

#if defined(USE_IMGUI)
	//	デバッグ用にカリング結果を固定する
	bool is_freeze_culling() const { return freeze_culling; }
	void set_freeze_culling(bool flag) { freeze_culling = flag; }
	bool resolve_debug_visible(UINT unique_id, bool current_visible);

	//	デバッグ用にメッシュレット単位で色分け表示する
	bool is_debug_visualize_meshlets() const { return debug_visualize_meshlets; }
	void set_debug_visualize_meshlets(bool flag) { debug_visualize_meshlets = flag; }
#endif	//	defined(USE_IMGUI)

public:
	scene_view_state();
	~scene_view_state();

private:
	//	前フレームの情報
	bool has_previous = false;
	math::vector3d previous_render_origin;
	math::float4x4 previous_view_matrix;
	math::float4x4 previous_projection_matrix;
	math::float4x4 previous_view_projection_matrix;

	std::shared_ptr<occlusion_culling_system> occlusion_culling;
	std::shared_ptr<hiz_buffer> hiz;
	std::shared_ptr<meshlet_culling_system> meshlet_culling;
	std::shared_ptr<clustered_lighting_system> clustered_lighting;
	std::shared_ptr<exposure_state> exposure;

	uint64_t last_used_frame = 0;

#if defined(USE_IMGUI)
	bool freeze_culling = false;
	bool debug_visualize_meshlets = false;
	std::mutex frozen_visibilities_mutex;
	std::unordered_map<UINT, bool> frozen_visibilities;
#endif	//	defined(USE_IMGUI)
};

using scene_view_state_ptr = std::shared_ptr<scene_view_state>;

//	view_stateのキャッシュクラス
class scene_view_state_cache
{
public:
	//	view_state 生成/取得
	const scene_view_state_ptr& acquire(scene_view_state_key key, uint64_t frame);

	//	view_state 検索
	scene_view_state_ptr find(scene_view_state_key key) const;

	//	keep_frames フレーム以上使われていない view_state を破棄する
	void release_unused(uint64_t frame, uint64_t keep_frames);

public:
	//	ハッシュキー生成
	static constexpr scene_view_state_key make_key(std::string_view name) { return core::generate_hash(name); }

private:
	std::unordered_map<scene_view_state_key, scene_view_state_ptr> states;
};
