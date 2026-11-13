#include "Process/asset_preview_process.h"
#include "framework.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>

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

	bool hovered = false;
	//	アセットウィンドウ
	ImGui::SetNextWindowSize(ImVec2(host_width, host_height), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar))
	{
		//	ツールバー
		if (ImGui::BeginMenuBar())
		{
			debug_view_settings_manager::instance()->get(window_kind).imgui_menu();
			ImGui::EndMenuBar();
		}

		//	窓の大きさに描画先を合わせる
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const int width = std::max(static_cast<int>(available.x), 1);
		const int height = std::max(static_cast<int>(available.y), 1);
		if (output_texture->get_width() != width || output_texture->get_height() != height)
			resize_output(width, height);

		const ImVec2 image_position = ImGui::GetCursorScreenPos();
		const ImVec2 image_size(static_cast<float>(width), static_cast<float>(height));
		ImGui::Image(imgui_texture_keeper::instance()->keep(output_texture), image_size);

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

	preview_world->get_scenes().imgui_render();
}
#endif	//	defined(USE_IMGUI)
