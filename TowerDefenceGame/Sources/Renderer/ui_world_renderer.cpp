#include "Renderer/ui_world_renderer.h"
#include "Graphics/render_data.h"

#include <algorithm>

#include "Renderer/ui_renderer.h"

#include "Object/object_manager.h"
#include "Object/world.h"
#include "Object/GameObjects/UI/ui_object.h"

#include "Editor/detail_profiler.h"

ui_world_renderer::ui_world_renderer(std::shared_ptr<render_job_executor> render_executor, std::shared_ptr<render_output> output)
: render_executor(std::move(render_executor))
, output(std::move(output))
{
}

ui_world_renderer::~ui_world_renderer()
{
}

bool ui_world_renderer::initialize()
{
	render_targets = std::make_shared<scene_render_targets>();
	output_generation = output->get_generation();
	if (!create_targets(output->get_width(), output->get_height()))
		return false;

	//	@tool UIレンダラーの追加
	ui_renderers.emplace_back(std::make_unique<ui_renderer>(render_executor, render_targets));
	for (auto& render : ui_renderers)
		DETAIL_PROFILE_SET_RENDERER_NAME(*render);
	return true;
}

bool ui_world_renderer::sync_output()
{
	if (output->get_generation() == output_generation)
		return true;

	output_generation = output->get_generation();
	if (!create_targets(output->get_width(), output->get_height()))
		return false;

	for (auto& render : ui_renderers)
		render->resize_buffer(output->get_width(), output->get_height());
	return true;
}

bool ui_world_renderer::create_targets(int width, int height)
{
	//	@tool 中身を作り直す(UI は出力先にしか描かない)
	*render_targets = scene_render_targets{};
	render_targets->output = output;

	//	@tool 出力先のクリア用
	output_clear_rendering_pass.render_targets.clear();
	output_clear_rendering_pass.render_targets.emplace_back(output->make_render_target_view());

	//	@tool UI描画用のシーン定数バッファ生成(viewport_rectのみ使用するため他の項目は未使用)
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		common_pipeline_states::scene_constants ui_scene_constant{};
		ui_scene_constant.viewport_rect = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
		D3D11_SUBRESOURCE_DATA initial_data{};
		initial_data.pSysMem = &ui_scene_constant;
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &initial_data, ui_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;
	}
	return true;
}

void ui_world_renderer::collect(world& target)
{
	if (!sync_output())
		return;

	render_data = std::make_shared<render_data_context>();

	DETAIL_PROFILE_CPU_SCOPE("render.collect_render_data");

	//	@tool UIオブジェクトコンテナ収集
	collect_sprites(target, render_data->sprite_container);

	//	@tool マテリアルの定数バッファをGPUへ転送し、使うパイプラインの結び付けを作り直す
	//	@tool 全体の作り直し(rebuild_pipeline_bindings)は別スレッドで作成中のパイプラインに触れるので行わない
	gpu_upload_list& upload_list = render_data::instance()->get_upload_list();
	for (auto& sprite_data : render_data->sprite_container.sprite_datas)
	{
		material_instance& material = sprite_data.sprite->get_material();
		material.sync_from_resource();
		material.flush_constant_buffers(upload_list);
		if (graphics_pipeline_state_ptr pipeline_state = material.get_pipeline_state())
			pipeline_state->rebuild_bindings();
	}
}

void ui_world_renderer::submit_render_jobs(world& target, float elapsed_time)
{
	if (!render_data)
		return;

	//	@tool 描画準備
	{
		renderer_context context = {};
		context.render_data = render_data;
		render_executor->execute([this](graphics_context& context)
		{
			//	@tool 画面クリア
			output_clear_rendering_pass.activate(context.device_context);

			output_clear_rendering_pass.deactivate(context.device_context);
		}, "begin_draw_pass", context, static_cast<UINT>(renderer_domain::begin_pass));
	}

	//	@tool UI描画
	{
		renderer_context context = {};
		context.scene_constant_buffer = ui_scene_constant_buffer;
		context.render_data = render_data;
		context.render_mesh_indices = std::make_shared<render_mesh_indices>();
		for (auto& render : ui_renderers)
		{
			DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_submit, render->get_profile_name());
			render->render(context);
		}
	}

	render_data.reset();
}

void ui_world_renderer::collect_sprites(world& target, sprite_container& container)
{
	container.sprite_datas.clear();

	int sort_key = 0;
	auto ui_object_list = target.get_objects().search_objects<ui_object>();
	for (auto& ui_object : ui_object_list)
	{
		if (!ui_object->is_active())
			continue;

		sprite_datas sprite_data;
		sprite_data.sort_key = sort_key;
		sprite_data.sort_order = ui_object->get_sort_order();
		sprite_data.sprite = &ui_object->get_sprite();
		container.sprite_datas.emplace_back(sprite_data);
		++sort_key;
	}

	auto ui_objects_list = target.get_objects().search_objects<ui_objects>();
	for (auto& ui_object : ui_objects_list)
	{
		if (!ui_object->is_active())
			continue;

		for (auto& sprite : ui_object->get_sprites())
		{
			sprite_datas sprite_data;
			sprite_data.sort_key = sort_key;
			sprite_data.sort_order = ui_object->get_sort_order();
			sprite_data.sprite = &sprite;
			container.sprite_datas.emplace_back(sprite_data);
		}
		++sort_key;
	}

	//	ソート
	std::sort(container.sprite_datas.begin(), container.sprite_datas.end(),
		[](const sprite_datas& l, const sprite_datas& r)
		{
			if (l.sort_order < r.sort_order)
				return l.sort_key != r.sort_key;
			return false;
		});
}
