#include "Process/asset_preview_process.h"
#include "framework.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>

#include "Graphics/render_data.h"
#include "Effect/effect_instance.h"
#include "Object/world.h"
#include "Object/object_manager.h"
#include "Renderer/world_renderer.h"
#include "Renderer/render_output.h"
#include "Renderer/render_system.h"
#include "System/imgui_texture_keeper.h"
#include "Scenes/scene_base.h"
#include "Object/engine_context.h"
#include "Input/input_key.h"

namespace
{
	constexpr int default_width = 640;
	constexpr int default_height = 360;
}

asset_preview_process::asset_preview_process(framework& owner, const cache_string& file_path, std::string_view window_kind, scene_opener open_scene)
: framework_process(owner)
, file_path(file_path)
, window_kind(window_kind)
, key(make_key(window_kind, file_path))
, open_scene(std::move(open_scene))
{
}

std::string asset_preview_process::make_key(std::string_view window_kind, const cache_string& file_path)
{
	return std::string(window_kind) + ":" + file_path.str();
}

asset_preview_process::~asset_preview_process()
{
}

bool asset_preview_process::enter()
{
	framework& self = get_owner();

	preview_world = std::make_unique<world>();
	engine_context& context = preview_world->get_context();
	context.set_embedded(true);
	context.set_window_id("##" + window_kind + ":" + std::string(file_path.get<utf8_cache>()));
	context.set_open_file(file_path.str());
	open_scene(preview_world->get_scenes());

	//	ï`âÊ
	output = render_output::create_texture(default_width, default_height);
	renderer = std::make_unique<world_renderer>(self.renderer->get_executor(), output);
	if (!renderer->initialize())
		return false;

	return true;
}

void asset_preview_process::exit()
{
	renderer.reset();
	output.reset();
	preview_world.reset();
}

void asset_preview_process::update(float elapsed_time)
{
	if (!preview_world)
		return;

	preview_world->get_scenes().updates(elapsed_time);
	preview_world->get_objects().updates(elapsed_time);
	preview_world->get_effects().update(elapsed_time);
}

void asset_preview_process::render(float elapsed_time)
{
	if (!preview_world)
		return;

	preview_world->get_objects().pre_render();
	renderer->collect(*preview_world);
	pipeline_state_manager::instance()->rebuild_pipeline_bindings();
}

void asset_preview_process::submit_render_jobs(float elapsed_time)
{
	if (!preview_world)
		return;

	renderer->submit_render_jobs(*preview_world, elapsed_time);
}

void asset_preview_process::post_render()
{
	if (!preview_world)
		return;

	preview_world->get_objects().post_render();
}

void asset_preview_process::imgui_render()
{
	if (!output)
		return;

	const cache_string file_name = std::filesystem::path(file_path.str()).filename().string();
	const std::string title = std::string(file_name.get<utf8_cache>()) + "###" + window_kind + ":" + std::string(file_path.get<utf8_cache>());

	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(default_width), static_cast<float>(default_height) + 40.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		//	ëãÇÃëÂÇ´Ç≥Ç…ï`âÊêÊÇçáÇÌÇπÇÈ
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const int width = std::max(static_cast<int>(available.x), 1);
		const int height = std::max(static_cast<int>(available.y), 1);
		if (width != output->get_width() || height != output->get_height())
			output->resize(width, height);

		const ImVec2 image_position = ImGui::GetCursorScreenPos();
		const ImVec2 image_size(static_cast<float>(width), static_cast<float>(height));
		ImGui::Image(imgui_texture_keeper::instance()->keep(output->get_texture()), image_size);

		//	âÊëúÇÃè„Ç…ÉJÅ[É\ÉãÇ™Ç†ÇÈÇ©Çí≤Ç◊ÇÈ
		ImGui::SetCursorScreenPos(image_position);
		ImGui::InvisibleButton("##view", image_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
		preview_world->get_context().set_viewport_hovered(hovered);
		preview_world->get_context().set_viewport_rect({ image_position.x, image_position.y, image_size.x, image_size.y });
		if (hovered)
			input::pass_mouse_through_ui();
	}
	ImGui::End();

	if (preview_world)
		preview_world->get_scenes().imgui_render();
}
#endif	//	defined(USE_IMGUI)
