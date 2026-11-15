#include "Editor/editor_viewport.h"

#if defined(USE_IMGUI)
#include <algorithm>

#include "Graphics/Core/render_data.h"
#include "Graphics/Core/render_pass.h"
#include "Renderer/world_renderer.h"
#include "Texture/texture_factory.h"
#include "System/imgui_texture_keeper.h"
#include "Object/engine_context.h"
#include "Input/input_key.h"
#include "System/console_manager.h"
#include "System/engine_config_data.h"

#include <cmath>
#include <iterator>
#include <string>

namespace
{
	//	選べる画面比率
	struct aspect_item
	{
		const char* label;
		float ratio;
	};
	constexpr aspect_item aspect_items[] =
	{
		{ "Game Resolution",	-1.0f },
		{ "16:9",				16.0f / 9.0f },
		{ "16:10",				16.0f / 10.0f },
		{ "4:3",				4.0f / 3.0f },
		{ "21:9",				21.0f / 9.0f },
		{ "Free",				0.0f },
	};
}

editor_viewport::editor_viewport(std::shared_ptr<render_job_executor> render_executor)
: renderer(std::make_unique<world_renderer>(std::move(render_executor)))
{
}

editor_viewport::~editor_viewport()
{
	finalize();
}

bool editor_viewport::initialize(int width, int height)
{
	return resize(width, height);
}

void editor_viewport::finalize()
{
	renderer.reset();
	output_texture.reset();
}

bool editor_viewport::resize(int width, int height)
{
	if (width <= 0 || height <= 0 || !renderer)
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
	if (!texture_factory::create_texture2d(created, "editor_viewport_output", texture2d_desc))
		return false;

	//	描画情報解放
	const bool initialized = output_texture != nullptr;
	renderer->release_targets();
	output_texture = created;
	if (!initialized)
		return renderer->initialize(render_target_view(output_texture));
	return renderer->create_targets(render_target_view(output_texture));
}

void editor_viewport::collect(world& target, const debug_view_settings& debug_settings)
{
	if (!renderer || !output_texture)
		return;

	renderer->collect(target);
	renderer->collect_debug_primitives(target, debug_settings);
}

void editor_viewport::submit_render_jobs(world& target, float elapsed_time)
{
	if (!renderer || !output_texture)
		return;

	renderer->submit_render_jobs(target, elapsed_time);
}

bool editor_viewport::imgui_render(engine_context& context, const ImVec2& position, const ImVec2& size, const std::function<bool(const ImVec2& image_position, const ImVec2& image_size)>& overlay)
{
	if (!output_texture)
		return false;

	constexpr float min_size = 16.0f;
	if (size.x < min_size || size.y < min_size)
		return false;

	//	選んだ比率で窓に入る一番大きな範囲を求める
	float fit_width = size.x;
	float fit_height = size.y;
	const float aspect = get_aspect_ratio();
	if (aspect > 0.0f)
	{
		if (fit_width / fit_height > aspect)
			fit_width = fit_height * aspect;
		else
			fit_height = fit_width / aspect;
	}

	//	窓の大きさに描画先を合わせる
	const int width = std::max(static_cast<int>(fit_width), 2);
	const int height = std::max(static_cast<int>(fit_height), 2);
	if (output_texture->get_width() != width || output_texture->get_height() != height)
		resize(width, height);

	//	レターボックス・ピラーボックス
	const ImVec2 image_size(static_cast<float>(width), static_cast<float>(height));
	const ImVec2 image_position(std::floor(position.x + (size.x - image_size.x) * 0.5f), std::floor(position.y + (size.y - image_size.y) * 0.5f));
	ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), IM_COL32(0, 0, 0, 255));
	ImGui::SetCursorScreenPos(image_position);
	ImGui::Image(imgui_texture_keeper::instance()->keep(output_texture), image_size);

	//	マウスを使っている間はワールドに渡さない
	const bool overlay_uses_mouse = overlay && overlay(image_position, image_size);

	//	画像の上にカーソルがあるかを調べる
	bool hovered = false;
	if (!overlay_uses_mouse)
	{
		ImGui::SetCursorScreenPos(image_position);
		ImGui::InvisibleButton("##view", image_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
	}
	context.set_viewport_rect({ image_position.x, image_position.y, image_size.x, image_size.y });
	if (hovered)
		input::pass_mouse_through_ui();
	return hovered;
}

void editor_viewport::imgui_menu()
{
	if (!aspect_variable)
		return;

	if (!ImGui::BeginMenu("Aspect"))
		return;

	const int current = aspect_variable->get();
	for (int index = 0; index < static_cast<int>(std::size(aspect_items)); ++index)
	{
		std::string label = aspect_items[index].label;
		if (aspect_items[index].ratio < 0.0f)
		{
			const engine_config_data::windows_data& windows = engine_config_data::instance()->windows;
			label += " (" + std::to_string(windows.screen_width) + "x" + std::to_string(windows.screen_height) + ")";
		}
		if (ImGui::MenuItem(label.c_str(), nullptr, current == index))
			aspect_variable->set(index);
	}
	ImGui::EndMenu();
}

float editor_viewport::get_aspect_ratio() const
{
	if (!aspect_variable)
		return 0.0f;

	const int index = std::clamp(aspect_variable->get(), 0, static_cast<int>(std::size(aspect_items)) - 1);
	if (aspect_items[index].ratio >= 0.0f)
		return aspect_items[index].ratio;

	//	ゲームの解像度の比率
	const engine_config_data::windows_data& windows = engine_config_data::instance()->windows;
	if (windows.screen_width == 0 || windows.screen_height == 0)
		return 0.0f;
	return static_cast<float>(windows.screen_width) / static_cast<float>(windows.screen_height);
}
#endif	//	defined(USE_IMGUI)
