#include "Process/asset_preview_process.h"
#include "imgui_internal.h"
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

	//	最初の大きさ
	constexpr float host_width = 1280.0f;
	constexpr float host_height = 720.0f;

	//	最初の配置でシーンの窓に割り当てる幅の割合
	constexpr float scene_panel_ratio = 0.35f;
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

	//	描画
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
	engine_context& context = preview_world->get_context();
	const std::string viewport_title = context.make_window_name("Viewport");

	//	アセットウィンドウ
	ImGui::SetNextWindowSize(ImVec2(host_width, host_height), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open))
	{
		const ImGuiID dockspace_id = ImGui::GetID("##dockspace");
		if (!ImGui::DockBuilderGetNode(dockspace_id))
		{
			//	最初に開いた時の配置
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetContentRegionAvail());
			ImGuiID center_id = dockspace_id;
			scene_dock_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, scene_panel_ratio, nullptr, &center_id);
			viewport_dock_id = center_id;
			ImGui::DockBuilderDockWindow(viewport_title.c_str(), center_id);
			ImGui::DockBuilderFinish(dockspace_id);
		}
		if (scene_dock_id == 0)
		{
			ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
			if (node && node->IsSplitNode() && node->ChildNodes[0] && node->ChildNodes[1])
			{
				viewport_dock_id = node->ChildNodes[0]->ID;
				scene_dock_id = node->ChildNodes[1]->ID;
			}
			else
			{
				viewport_dock_id = dockspace_id;
				scene_dock_id = dockspace_id;
			}
		}
		ImGui::DockSpace(dockspace_id);
	}
	ImGui::End();

	//	ビュー
	bool hovered = false;
	if (viewport_dock_id != 0)
		ImGui::SetNextWindowDockID(viewport_dock_id, ImGuiCond_Once);
	if (ImGui::Begin(viewport_title.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		//	窓の大きさに描画先を合わせる
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const int width = std::max(static_cast<int>(available.x), 1);
		const int height = std::max(static_cast<int>(available.y), 1);
		if (width != output->get_width() || height != output->get_height())
			output->resize(width, height);

		const ImVec2 image_position = ImGui::GetCursorScreenPos();
		const ImVec2 image_size(static_cast<float>(width), static_cast<float>(height));
		ImGui::Image(imgui_texture_keeper::instance()->keep(output->get_texture()), image_size);

		//	画像の上にカーソルがあるかを調べる
		ImGui::SetCursorScreenPos(image_position);
		ImGui::InvisibleButton("##view", image_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
		context.set_viewport_rect({ image_position.x, image_position.y, image_size.x, image_size.y });
		if (hovered)
			input::pass_mouse_through_ui();
	}
	ImGui::End();
	context.set_viewport_hovered(hovered);

	//	シーンウィンドウはアセットウィンドウにドッキングする
	context.set_dock_id(scene_dock_id);
	preview_world->get_scenes().imgui_render();
}
#endif	//	defined(USE_IMGUI)
