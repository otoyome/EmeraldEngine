#include "Process/asset_preview_process.h"
#include "framework.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "Graphics/Core/render_data.h"
#include "Effect/effect_instance.h"
#include "Object/world.h"
#include "Object/object_manager.h"
#include "Renderer/world_renderer.h"
#include "Renderer/render_system.h"
#include "Graphics/Core/render_pass.h"
#include "Texture/texture_factory.h"
#include "System/imgui_texture_keeper.h"
#include "Scenes/scene_manager.h"
#include "Object/engine_context.h"
#include "Input/input_key.h"

namespace
{
	constexpr int default_width = 640;
	constexpr int default_height = 360;

	//	最初の大きさ
	constexpr float host_width = 1280.0f;
	constexpr float host_height = 720.0f;

	//	パネルとビューの最小の大きさ、境目の太さ
	constexpr float min_panel_size = 80.0f;
	constexpr float min_view_size = 64.0f;
	constexpr float splitter_size = 4.0f;
}

std::unordered_map<std::string, asset_preview_process::panel_layout> asset_preview_process::panel_layouts;

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
	renderer = std::make_unique<world_renderer>(self.renderer->get_executor());
	if (!resize_output(default_width, default_height))
		return false;

	return true;
}

void asset_preview_process::exit()
{
	renderer.reset();
	output_texture.reset();
	preview_world.reset();
}

bool asset_preview_process::resize_output(int width, int height)
{
	if (width <= 0 || height <= 0)
		return false;

	//	スワップチェーンと同じ形式で作る
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
	render_data::instance()->get_swap_chain()->GetDesc1(&swap_chain_desc);

	D3D11_TEXTURE2D_DESC texture2d_desc{};
	texture2d_desc.Width = static_cast<UINT>(width);
	texture2d_desc.Height = static_cast<UINT>(height);
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = swap_chain_desc.Format;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;

	texture_ptr created;
	if (!texture_factory::create_texture2d(created, "asset_preview_output", texture2d_desc))
		return false;

	//	描画情報解放
	const bool initialized = output_texture != nullptr;
	renderer->release_targets();
	output_texture = created;
	if (!initialized)
		return renderer->initialize(render_target_view(output_texture));
	return renderer->create_targets(render_target_view(output_texture));
}

void asset_preview_process::update(float elapsed_time)
{
	if (!preview_world)
		return;

	preview_world->get_scenes().updates(elapsed_time);
	preview_world->get_objects().updates(elapsed_time);
	preview_world->get_scenes().post_updates(elapsed_time);
	preview_world->get_effects().update(elapsed_time);
}

void asset_preview_process::render(float elapsed_time)
{
	if (!preview_world)
		return;

	preview_world->get_objects().pre_render();
	renderer->collect(*preview_world);
	renderer->collect_debug_primitives(*preview_world, debug_view_settings_manager::instance()->get(window_kind));
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
	if (!output_texture)
		return;

	const cache_string file_name = std::filesystem::path(file_path.str()).filename().string();
	const std::string title = std::string(file_name.get<utf8_cache>()) + "###" + window_kind + ":" + std::string(file_path.get<utf8_cache>());
	engine_context& context = preview_world->get_context();
	scene_manager& scenes = preview_world->get_scenes();

	bool hovered = false;
	//	アセットウィンドウ
	ImGui::SetNextWindowSize(ImVec2(host_width, host_height), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar))
	{
		//	ツールバー
		if (ImGui::BeginMenuBar())
		{
			scenes.imgui_render_panel(scene_system::panel_regions::menu_bar);
			debug_view_settings_manager::instance()->get(window_kind).imgui_menu();
			ImGui::EndMenuBar();
		}

		hovered = imgui_render_layout(context);
	}
	ImGui::End();
	context.set_viewport_hovered(hovered);

	scenes.imgui_render();
}

bool asset_preview_process::imgui_render_layout(engine_context& context)
{
	using regions = scene_system::panel_regions;
	scene_manager& scenes = preview_world->get_scenes();
	panel_layout& layout = panel_layouts[window_kind];

	const bool has_left = scenes.has_panel(regions::left);
	const bool has_right = scenes.has_panel(regions::right);
	const bool has_bottom = scenes.has_panel(regions::bottom);

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 available = ImGui::GetContentRegionAvail();

	//	ビューが残る範囲にパネルの大きさを収める
	if (has_bottom)
		layout.bottom_height = std::clamp(layout.bottom_height, min_panel_size, std::max(min_panel_size, available.y - min_view_size - splitter_size));
	if (has_left)
		layout.left_width = std::clamp(layout.left_width, min_panel_size, std::max(min_panel_size, available.x - min_view_size - splitter_size - (has_right ? layout.right_width + splitter_size : 0.0f)));
	if (has_right)
		layout.right_width = std::clamp(layout.right_width, min_panel_size, std::max(min_panel_size, available.x - min_view_size - splitter_size - (has_left ? layout.left_width + splitter_size : 0.0f)));

	const float left_width = has_left ? layout.left_width + splitter_size : 0.0f;
	const float right_width = has_right ? layout.right_width + splitter_size : 0.0f;
	const float upper_height = std::max(available.y - (has_bottom ? layout.bottom_height + splitter_size : 0.0f), 1.0f);

	//	左
	if (has_left)
	{
		imgui_render_panel(regions::left, "##left_panel", origin, ImVec2(layout.left_width, upper_height));
		imgui_splitter("##left_splitter", ImVec2(origin.x + layout.left_width, origin.y), ImVec2(splitter_size, upper_height), true, 1.0f, layout.left_width);
	}

	//	右
	if (has_right)
	{
		const float right_x = origin.x + available.x - layout.right_width;
		imgui_splitter("##right_splitter", ImVec2(right_x - splitter_size, origin.y), ImVec2(splitter_size, upper_height), true, -1.0f, layout.right_width);
		imgui_render_panel(regions::right, "##right_panel", ImVec2(right_x, origin.y), ImVec2(layout.right_width, upper_height));
	}

	//	下
	if (has_bottom)
	{
		imgui_splitter("##bottom_splitter", ImVec2(origin.x, origin.y + upper_height), ImVec2(available.x, splitter_size), false, -1.0f, layout.bottom_height);
		imgui_render_panel(regions::bottom, "##bottom_panel", ImVec2(origin.x, origin.y + upper_height + splitter_size), ImVec2(available.x, layout.bottom_height));
	}

	//	窓の大きさに描画先を合わせる
	const int width = std::max(static_cast<int>(available.x - left_width - right_width), 1);
	const int height = std::max(static_cast<int>(upper_height), 1);
	if (output_texture->get_width() != width || output_texture->get_height() != height)
		resize_output(width, height);

	const ImVec2 image_position(origin.x + left_width, origin.y);
	const ImVec2 image_size(static_cast<float>(width), static_cast<float>(height));
	ImGui::SetCursorScreenPos(image_position);
	ImGui::Image(imgui_texture_keeper::instance()->keep(output_texture), image_size);

	//	画像の上にカーソルがあるかを調べる
	ImGui::SetCursorScreenPos(image_position);
	ImGui::InvisibleButton("##view", image_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
	context.set_viewport_rect({ image_position.x, image_position.y, image_size.x, image_size.y });
	if (hovered)
		input::pass_mouse_through_ui();
	return hovered;
}

void asset_preview_process::imgui_render_panel(scene_system::panel_regions region, const char* id, const ImVec2& position, const ImVec2& size)
{
	ImGui::SetCursorScreenPos(position);
	if (ImGui::BeginChild(id, size, ImGuiChildFlags_Borders))
		preview_world->get_scenes().imgui_render_panel(region);
	ImGui::EndChild();
}

void asset_preview_process::imgui_splitter(const char* id, const ImVec2& position, const ImVec2& size, bool vertical, float sign, float& value)
{
	ImGui::SetCursorScreenPos(position);
	ImGui::InvisibleButton(id, size);
	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();
	if (!active && !hovered)
		return;

	ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
	if (active)
	{
		const ImVec2 delta = ImGui::GetIO().MouseDelta;
		value += sign * (vertical ? delta.x : delta.y);
	}
	const ImU32 color = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : ImGuiCol_SeparatorHovered);
	ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), color);
}
#endif	//	defined(USE_IMGUI)
