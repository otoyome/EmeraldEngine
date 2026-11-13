#include "Graphics/View/scene_view_state.h"
#include "Graphics/View/scene_view.h"
#include "Graphics/Culling/meshlet_culling_system.h"
#include "Graphics/Lighting/clustered_lighting_system.h"

scene_view_state::scene_view_state()
: occlusion_culling(std::make_shared<occlusion_culling_system>())
, hiz(std::make_shared<hiz_buffer>())
, meshlet_culling(std::make_shared<meshlet_culling_system>())
, clustered_lighting(std::make_shared<clustered_lighting_system>())
{
}

scene_view_state::~scene_view_state()
{
}

void scene_view_state::begin_frame(const scene_view& current, bool camera_cut)
{
	//	前回の情報が無い場合は現在の行列を使う
	if (camera_cut || !has_previous)
	{
		previous_view_projection_matrix = current.get_view_projection_matrix();
		return;
	}

	//	前回の視線行列を、現在の描画の原点から見たものに直す
	const math::vector3f origin_offset((current.get_render_origin() - previous_render_origin).to_float3());
	const math::matrixf previous_view = math::matrixf::translation(origin_offset) * math::matrixf(previous_view_matrix);
	previous_view_projection_matrix = (previous_view * math::matrixf(previous_projection_matrix)).to_float4x4();
}

void scene_view_state::end_frame(const scene_view& current)
{
	has_previous = true;
	previous_render_origin = current.get_render_origin();
	previous_view_matrix = current.get_view_matrix();
	previous_projection_matrix = current.get_projection_matrix();
}

#if defined(USE_IMGUI)
bool scene_view_state::resolve_debug_visible(UINT unique_id, bool current_visible)
{
	std::lock_guard<std::mutex> lock(frozen_visibilities_mutex);

	if (!freeze_culling)
	{
		frozen_visibilities[unique_id] = current_visible;
		return current_visible;
	}

	auto it = frozen_visibilities.find(unique_id);
	if (it == frozen_visibilities.end())
	{
		frozen_visibilities.emplace(unique_id, current_visible);
		return current_visible;
	}
	return it->second;
}
#endif	//	defined(USE_IMGUI)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const scene_view_state_ptr& scene_view_state_cache::acquire(scene_view_state_key key, uint64_t frame)
{
	auto& state = states[key];
	if (!state)
		state = std::make_shared<scene_view_state>();

	//	最後に使用した事を記録
	state->last_used_frame = frame;
	return state;
}

scene_view_state_ptr scene_view_state_cache::find(scene_view_state_key key) const
{
	auto it = states.find(key);
	return it != states.end() ? it->second : nullptr;
}

void scene_view_state_cache::release_unused(uint64_t frame, uint64_t keep_frames)
{
	for (auto it = states.begin(); it != states.end();)
	{
		if (frame - it->second->last_used_frame > keep_frames)
			it = states.erase(it);
		else
			++it;
	}
}
