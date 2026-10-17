#include "custom_pipeline_state.h"

#include "render_data.h"
#include "Graphics/material_render_states.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
graphics_custom_pipeline_state::graphics_custom_pipeline_state()
: graphics_pipeline_state()
{
	//	@tool ステートはマテリアルの設定から描画毎に選ぶ（共有のステートを生成時に作っておく）
	render_state_mode = render_state_modes::material;
	material_render_states::instance();
}

graphics_custom_pipeline_state::~graphics_custom_pipeline_state()
{
}

void graphics_custom_pipeline_state::prepare()
{
	//	各種データ類更新
	update_register_datas();
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<vertex_shader>& shader)
{
	if (shaders.vs == shader)
		return;

#if defined(USE_IMGUI)
	if (shaders.vs)
		shaders.vs->unregister_reload_callback(vs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	shaders.vs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (shaders.vs)
		vs_reload_callback_id = shaders.vs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<hull_shader>& shader)
{
	if (shaders.hs == shader)
		return;

#if defined(USE_IMGUI)
	if (shaders.hs)
		shaders.hs->unregister_reload_callback(hs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	shaders.hs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (shaders.hs)
		hs_reload_callback_id = shaders.hs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<domain_shader>& shader)
{
	if (shaders.ds == shader)
		return;

#if defined(USE_IMGUI)
	if (shaders.ds)
		shaders.ds->unregister_reload_callback(ds_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	shaders.ds = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (shaders.ds)
		ds_reload_callback_id = shaders.ds->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<geometry_shader>& shader)
{
	if (shaders.gs == shader)
		return;

#if defined(USE_IMGUI)
	if (shaders.gs)
		shaders.gs->unregister_reload_callback(gs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	shaders.gs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (shaders.gs)
		gs_reload_callback_id = shaders.gs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<pixel_shader>& shader)
{
	if (shaders.ps == shader)
		return;

#if defined(USE_IMGUI)
	if (shaders.ps)
		shaders.ps->unregister_reload_callback(ps_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	shaders.ps = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (shaders.ps)
		ps_reload_callback_id = shaders.ps->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::generate_input_resource_descs(std::vector<input_resource_desc>& out_data)
{
	if (dirty_input_resource_datas)
	{
		dirty_input_resource_datas = false;
		input_resource_desces.clear();
		input_resource_desc resource_descs[static_cast<int>(arrangement::max)];
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::vertex)], shaders.vs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::hull)], shaders.hs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::domain)], shaders.ds.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::geometry)], shaders.gs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::pixel)], shaders.ps.get());
		for (auto& resource_desc : resource_descs)
		{
			input_resource_desces.emplace_back(resource_desc);
		}
	}
	out_data = input_resource_desces;

	//	登録位置リスト更新
	update_register_datas();
}

void graphics_custom_pipeline_state::update_register_datas()
{
	if (dirty_input_resource_datas)
	{
		dirty_input_resource_datas = false;
		input_resource_desces.clear();
		input_resource_desc resource_descs[static_cast<int>(arrangement::max)];
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::vertex)], shaders.vs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::hull)], shaders.hs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::domain)], shaders.ds.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::geometry)], shaders.gs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::pixel)], shaders.ps.get());
		for (auto& resource_desc : resource_descs)
		{
			input_resource_desces.emplace_back(resource_desc);
		}
	}

	if (dirty)
	{
		dirty = false;

		//	@tool シェーダーのリフレクションから割り当て配置を作り直す
		layout.clear();
		shader_base* shader_list[static_cast<int>(arrangement::max)] =
		{
			shaders.vs.get(),
			shaders.hs.get(),
			shaders.ds.get(),
			shaders.gs.get(),
			shaders.ps.get()
		};
		for (UINT index = 0; index < ARRAYSIZE(shader_list); ++index)
		{
			if (!shader_list[index])
				continue;

			const shader_stage stage = static_cast<shader_stage>(index);
			for (auto& cbv : input_resource_desces[index].cbvs)
			{
				layout.add_reflected_constant_buffer(stage, cbv.register_index, cbv.name);
			}
			for (auto& rv : input_resource_desces[index].rvs)
			{
				if (rv.is_srvs())
				{
					layout.add_reflected_shader_resource(stage, rv.register_index, rv.name);
				}
				else if (rv.is_sampler())
				{
					layout.add_reflected_sampler(stage, rv.register_index, rv.name);
				}
			}
		}
		layout.finalize_reflection();

		semantic_list.clear();
		if (shaders.vs)
		{
			for (auto& element_desc : shaders.vs->get_input_element_desc())
			{
				semantic_list.emplace_back(std::make_pair(semantic(element_desc.SemanticName, element_desc.SemanticIndex), graphics_utility::get_byte_offset_from_format(element_desc.Format)));
			}
		}

	}
}

#if defined(USE_IMGUI)
void graphics_custom_pipeline_state::on_update_core(const shader_base* shader_base)
{
	//	更新が入った時に各種再生成
	dirty = true;
	dirty_input_resource_datas = true;
}
#endif	//	defined(USE_IMGUI)

