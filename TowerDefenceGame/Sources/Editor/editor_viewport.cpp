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

bool editor_viewport::imgui_render(engine_context& context, const ImVec2& position, const ImVec2& size)
{
	if (!output_texture)
		return false;

	//	窓の大きさに描画先を合わせる
	const int width = std::max(static_cast<int>(size.x), 1);
	const int height = std::max(static_cast<int>(size.y), 1);
	if (output_texture->get_width() != width || output_texture->get_height() != height)
		resize(width, height);

	const ImVec2 image_size(static_cast<float>(width), static_cast<float>(height));
	ImGui::SetCursorScreenPos(position);
	ImGui::Image(imgui_texture_keeper::instance()->keep(output_texture), image_size);

	//	画像の上にカーソルがあるかを調べる
	ImGui::SetCursorScreenPos(position);
	ImGui::InvisibleButton("##view", image_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
	context.set_viewport_rect({ position.x, position.y, image_size.x, image_size.y });
	if (hovered)
		input::pass_mouse_through_ui();
	return hovered;
}
#endif	//	defined(USE_IMGUI)
