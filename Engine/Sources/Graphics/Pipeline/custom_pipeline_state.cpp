#include "Graphics/Pipeline/custom_pipeline_state.h"

#include "Graphics/Core/render_data.h"
#include "Graphics/Lighting/clustered_lighting_system.h"

namespace
{
	//  マテリアルフラグの対応表
	constexpr std::pair<material_resource::flags, UINT> convert_material_flags[] =
	{
		{material_resource::flags::indirect_lighting_off, common_pipeline_states::material_attribute_indirect_lighting_off},
		{material_resource::flags::shadow_receiver, common_pipeline_states::material_attribute_shadow_receiver},
		{material_resource::flags::decal_off, common_pipeline_states::material_attribute_decal_off},
	};

	//  テクスチャフラグの対応表
	constexpr std::pair<texture::flags, UINT> convert_texture_flags[] =
	{
		{texture::flags::srgb, common_pipeline_states::shader_resource_view_flags_srgb},
		{texture::flags::cube_map, common_pipeline_states::shader_resource_view_flags_cube_map},
		{texture::flags::gamut_rec709, common_pipeline_states::shader_resource_view_flags_gamut_rec709},
		{texture::flags::gamut_rec2020, common_pipeline_states::shader_resource_view_flags_gamut_rec2020},
	};

	//	近距離のライトを検索
	struct forward_light_data
	{
		common_pipeline_states::forward_light_constants light;

		//	IBL情報
		texture_ptr diffuse_ibl_texture;
		texture_ptr specluar_ibl_texture;
		texture_ptr lut_ggx_texture;
	};
	forward_light_data search_neighborhood_light(const graphics_context& context, const mesh_draw_item& buffer_data)
	{
		forward_light_data light_data = {};
		//	テキトーに近くのライトを詰める
		for (auto& light : context.render_data->light_container.lights)
		{
			//	クラスタードライティング対象はクラスターから参照するので詰めない
			if (context.clustered_lighting && clustered_lighting_system::is_target(light))
				continue;

			switch(light.type)
			{
				case light_type::directional:
				{
					if (light_data.light.light_counts.x >= common_pipeline_states::directional_light_count)
						continue;

					light_data.light.directional_light_data[light_data.light.light_counts.x] = light.data;
					++light_data.light.light_counts.x;
					break;
				}
				case light_type::point:
				{
					if (light_data.light.light_counts.y >= common_pipeline_states::point_light_count)
						continue;

					light_data.light.point_light_data[light_data.light.light_counts.y] = light.data;
					++light_data.light.light_counts.y;
					break;
				}
				case light_type::spot:
				{
					if (light_data.light.light_counts.z >= common_pipeline_states::spot_light_count)
						continue;

					light_data.light.spot_light_data[light_data.light.light_counts.z] = light.data;
					++light_data.light.light_counts.z;
					break;
				}
			}
		}

		//	IBL
		for (const auto& light_probe : context.render_data->light_container.light_probes)
		{
			if (!light_probe.is_flag(light_probe::flags::active) || !light_probe.is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			light_data.diffuse_ibl_texture = light_probe.diffuse_ibl_texture;
			light_data.specluar_ibl_texture = light_probe.specular_ibl_texture;
			light_data.lut_ggx_texture = light_probe.lut_ggx_texture;

			//	間接光の情報を設定
			light_data.light.light_probe_data = light_probe.data;
			break;
		}
		return light_data;
	}
}

typedef void(ID3D11DeviceContext::* set_constant_buffers)(
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	_In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers);

typedef void(ID3D11DeviceContext::* set_shader_resources)(
	_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	_In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews);

typedef void(ID3D11DeviceContext::* set_sampplers)(
	_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	_In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers);

namespace
{
	constexpr int shader_stage_count = 5;
	const set_constant_buffers set_constant_buffer_funcs[shader_stage_count] =
	{
		&ID3D11DeviceContext::VSSetConstantBuffers,
		&ID3D11DeviceContext::HSSetConstantBuffers,
		&ID3D11DeviceContext::DSSetConstantBuffers,
		&ID3D11DeviceContext::GSSetConstantBuffers,
		&ID3D11DeviceContext::PSSetConstantBuffers,
	};
	const set_shader_resources set_shader_resource_funcs[shader_stage_count] =
	{
		&ID3D11DeviceContext::VSSetShaderResources,
		&ID3D11DeviceContext::HSSetShaderResources,
		&ID3D11DeviceContext::DSSetShaderResources,
		&ID3D11DeviceContext::GSSetShaderResources,
		&ID3D11DeviceContext::PSSetShaderResources,
	};
	const set_sampplers set_samppler_funcs[shader_stage_count] =
	{
		&ID3D11DeviceContext::VSSetSamplers,
		&ID3D11DeviceContext::HSSetSamplers,
		&ID3D11DeviceContext::DSSetSamplers,
		&ID3D11DeviceContext::GSSetSamplers,
		&ID3D11DeviceContext::PSSetSamplers,
	};
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
graphics_custom_pipeline_state::graphics_custom_pipeline_state()
: graphics_pipeline_state()
{
}

graphics_custom_pipeline_state::~graphics_custom_pipeline_state()
{
}

void graphics_custom_pipeline_state::rebuild_bindings()
{
	//	各種データ類更新
	update_register_datas();
}

void graphics_custom_pipeline_state::begin(const graphics_context& context)
{
	_ASSERT_EXPR(!dirty && !dirty_input_resource_datas, L"graphics_custom_pipeline_state is dirty while rendering (rebuild_bindings was not called)");

	//	シェーダー設定
	context.device_context->VSSetShader(vs ? vs->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->IASetInputLayout(nullptr);
	context.device_context->HSSetShader(hs ? hs->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->DSSetShader(ds ? ds->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->GSSetShader(gs ? gs->get_shader().Get() : nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps ? ps->get_shader().Get() : nullptr, nullptr, 0);


	{
		//	シーン定数バッファ（1 フレーム不変）
		for (auto& cbv : index_map.cbv)
		{
			if (cbv.second.hash != common_pipeline_states::scene_constant_buffer_hash_key)
				continue;
			ID3D11Buffer* scene_cb = context.scene_constant_buffer.Get();
			for (UINT stage = 0; stage < magic_enum::enum_count<arrangement>(); ++stage)
			{
				if (cbv.second.ids[stage] == -1)
					continue;
				(context.device_context->*set_constant_buffer_funcs[stage])(cbv.second.ids[stage], 1, &scene_cb);
			}
		}

		if (context.render_data)
		{
			ID3D11ShaderResourceView* transform_views[2] =
			{
				context.render_data->mesh_container.primitive_header_srv.Get(),
				context.render_data->mesh_container.bone_matrix_srv.Get(),
			};
			const system_utility::hash_key transform_keys[2] =
			{
				common_pipeline_states::primitive_headers_hash_key,
				common_pipeline_states::bone_matrices_hash_key,
			};
			for (auto& srv : index_map.srv)
			{
				for (UINT slot = 0; slot < 2; ++slot)
				{
					if (srv.second.hash != transform_keys[slot])
						continue;
					for (UINT stage = 0; stage < magic_enum::enum_count<arrangement>(); ++stage)
					{
						if (srv.second.ids[stage] == -1)
							continue;
						(context.device_context->*set_shader_resource_funcs[stage])(srv.second.ids[stage], 1, &transform_views[slot]);
					}
				}
			}
		}
	}
}

void graphics_custom_pipeline_state::update_parameters(const graphics_context& context, const material_instance& material, const mesh_draw_item* buffer_data)
{
	const render_state_cache& state_cache = render_data::instance()->get_state_cache();

	//	ラスタライザーステート設定
	{
		bool is_two_sided = material.is_flag(material_resource::flags::two_sided);
		if (material.get_domain() == material_resource::domains::user_interface || context.is_force_material_both_draw)
		{
			is_two_sided = true;
		}

		bool is_wire = material.is_flag(material_resource::flags::wire);
		if (context.is_force_material_wire_draw)
		{
			is_wire = true;
		}

		render_state_cache::raster_state rs;
		if (is_wire)
		{
			rs = is_two_sided ? render_state_cache::raster_state::wire_two_sided : render_state_cache::raster_state::wire;
		}
		else
		{
			rs = is_two_sided ? render_state_cache::raster_state::two_sided : render_state_cache::raster_state::solid;
		}
		context.set_rasterizer_state(state_cache.get_rasterizer_state(rs).Get());
	}

	//	ブレンドステート&深度ステンシルステート設定
	{
		auto depth_stencil_state = render_state_cache::depth_stencil_state::depth_test_and_write;
		auto blend_state = render_state_cache::blend_state::opaque;
		auto stencil_ref_value = stencil_ref;

		if (context.is_shadow_rendering)
		{
			//	シャドウマップ生成時
			depth_stencil_state = render_state_cache::depth_stencil_state::depth_test_and_write;
			blend_state = render_state_cache::blend_state::opaque;
			stencil_ref_value = 0;
		}
		else if (material.get_domain() == material_resource::domains::deferred_decal)
		{
			//	デカール処理
			depth_stencil_state = render_state_cache::depth_stencil_state::decal;
			blend_state = render_state_cache::blend_state::opaque;
			stencil_ref_value = 0;
		}
		else
		{
			switch (material.get_blend_mode())
			{
				case material_resource::blend_modes::opaque:
				case material_resource::blend_modes::mask:
				{
					break;
				}
				case material_resource::blend_modes::translucent:
				{
					blend_state = render_state_cache::blend_state::translucent;
					depth_stencil_state = material.is_flag(material_resource::flags::disable_depth_test) ? render_state_cache::depth_stencil_state::depth_off : render_state_cache::depth_stencil_state::depth_test_only;
					break;
				}
				case material_resource::blend_modes::addative:
				{
					blend_state = render_state_cache::blend_state::addative;
					depth_stencil_state = material.is_flag(material_resource::flags::disable_depth_test) ? render_state_cache::depth_stencil_state::depth_off : render_state_cache::depth_stencil_state::depth_test_only;
					break;
				}
				//case material_resource::blend_modes::modulate:
				//{
				//	blend_state = blend_state::multiply;
				//	break;
				//}
			}
		}
		context.set_depth_stencil_state(state_cache.get_depth_stencil_state(depth_stencil_state).Get(), stencil_ref_value);
		context.set_blend_state(state_cache.get_blend_state(blend_state).Get());
	}

	bool parameter_buffer_used[magic_enum::enum_count<arrangement>()] = {};
	primitive_datas primitive_data = {};
	bool deferred_light_buffer_used = false;
	bool deferred_light_probe_buffer_used = false;
	bool forward_light_buffer_used = false;
	bool shadow_map_buffer_used = false;


	//	パラメーター設定
	{
		//	定数バッファビュー登録
		forward_light_data forward_light_data;
		for (auto& cbv : index_map.cbv)
		{
			system_utility::hash_key cbv_key = cbv.second.hash;
			if (cbv_key == common_pipeline_states::parameter_constant_buffer_hash_key)
			{
				//	特定のアイテムはcontextから
				for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
				{
					if (cbv.second.ids[index] == -1)
						continue;

					parameter_buffer_used[index] = true;
					ID3D11Buffer* constant_buffer = parameter_buffer_data[index].buffer.Get();
					(context.device_context->*set_constant_buffer_funcs[index])(cbv.second.ids[index], 1, &constant_buffer);
				}
				continue;
			}

			ID3D11Buffer* constant_buffer = nullptr;
			//	特定のアイテムはcontextから
			if (cbv_key == common_pipeline_states::scene_constant_buffer_hash_key)
			{
				continue;
			}
			else if (cbv_key == common_pipeline_states::shadow_map_constant_buffer_hash_key)
			{
				shadow_map_buffer_used = true;
				constant_buffer = shadow_map_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::deferred_light_constant_buffer_hash_key)
			{
				deferred_light_buffer_used = true;
				constant_buffer = deferred_light_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::deferred_light_probe_constant_buffer_hash_key)
			{
				deferred_light_probe_buffer_used = true;
				constant_buffer = deferred_light_probe_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::clustered_lighting_constant_buffer_hash_key)
			{
				constant_buffer = context.clustered_lighting ? context.clustered_lighting->get_constant_buffer() : nullptr;
			}
			else if (cbv_key == common_pipeline_states::forward_light_constant_buffer_hash_key)
			{
				if (!buffer_data)
					continue;

				if (!forward_light_buffer_used)
				{
					forward_light_data = search_neighborhood_light(context, *buffer_data);
				}
				forward_light_buffer_used = true;
				constant_buffer = forward_light_buffer_data.buffer.Get();
			}
			else if (cbv_key == common_pipeline_states::primitive_constant_buffer_hash_key ||
					 cbv_key == common_pipeline_states::decal_primitive_constant_buffer_hash_key)
			{
				primitive_data.use = true;
				for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
				{
					primitive_data.cbv_indices[index] = cbv.second.ids[index];
				}
				continue;
			}
			else
			{
				if (const material_resource::constant_buffer* material_constant_buffer = material.find_constant_buffer(cbv.first))
				{
					constant_buffer = material_constant_buffer->buffer.Get();
				}
			}

			for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
			{
				if (cbv.second.ids[index] == -1)
					continue;

				(context.device_context->*set_constant_buffer_funcs[index])(cbv.second.ids[index], 1, &constant_buffer);
			}
		}

		//	サンプラステート登録
		ID3D11SamplerState* sampler_state = render_data::instance()->get_state_cache().get_sampler_state(render_state_cache::SAMPLER_STATE::LINEAR).Get();
		for (auto& sampler : index_map.sampler)
		{
			for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
			{
				if (sampler.second.ids[index] == -1)
					continue;

				UINT register_index = sampler.second.ids[index];
				(context.device_context->*set_samppler_funcs[index])(register_index, 1, &sampler_state);
			}
		}

		//	シェーダーリソースビュー登録
		common_pipeline_states::parameter_constants parameter_constant_buffer[magic_enum::enum_count<arrangement>()];
		{
			for (auto& srv : index_map.srv)
			{
				system_utility::hash_key srv_key = srv.second.hash;
				const int paired_sampler_index = index_map.srv_sampler_indices[static_cast<size_t>(&srv - index_map.srv.data())];

				//	登録用関数
				auto register_srv_function = [&](const texture_ptr& srv_ptr)
				{
					if (!srv_ptr)
						return;

					Microsoft::WRL::ComPtr<ID3D11SamplerState> texture_sampler_state;
					if (paired_sampler_index >= 0)
					{
						texture_sampler_state = render_data::instance()->get_state_cache().get_sampler_state(srv_ptr->get_sampler_desc().to_d3d11_desc());
					}
					ID3D11SamplerState* texture_sampler = texture_sampler_state.Get();

					UINT srv_flag = 0;
					for (auto& flag : convert_texture_flags)
					{
						system_utility::set_bitflag(srv_flag, flag.second, srv_ptr->is_flag(flag.first));
					}

					ID3D11ShaderResourceView* shader_resource_view = srv_ptr ? srv_ptr->get_shader_resource_view().Get() : nullptr;

					for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
					{
						if (srv.second.ids[index] == -1)
							continue;

						UINT register_index = srv.second.ids[index];
						(context.device_context->*set_shader_resource_funcs[index])(register_index, 1, &shader_resource_view);
						int* shader_resource_view_index = &parameter_constant_buffer[index].shader_resource_view_index[register_index / 4].x;
						UINT* shader_resource_view_flags = &parameter_constant_buffer[index].shader_resource_view_flags[register_index / 4].x;
						shader_resource_view_index[register_index % 4] = 1;
						shader_resource_view_flags[register_index % 4] = srv_flag;

						//	同じステージの対になるサンプラーも設定する
						if (texture_sampler)
						{
							UINT sampler_register_index = index_map.sampler[paired_sampler_index].second.ids[index];
							if (sampler_register_index != -1)
							{
								(context.device_context->*set_samppler_funcs[index])(sampler_register_index, 1, &texture_sampler);
							}
						}
					}
				};
				//	クラスタードライティングのバッファ
				{
					ID3D11ShaderResourceView* clustered_srv = nullptr;
					bool is_clustered = true;
					if (srv_key == common_pipeline_states::clustered_lights_hash_key)
						clustered_srv = context.clustered_lighting ? context.clustered_lighting->get_light_srv() : nullptr;
					else if (srv_key == common_pipeline_states::cluster_counts_hash_key)
						clustered_srv = context.clustered_lighting ? context.clustered_lighting->get_count_srv() : nullptr;
					else if (srv_key == common_pipeline_states::cluster_offsets_hash_key)
						clustered_srv = context.clustered_lighting ? context.clustered_lighting->get_offset_srv() : nullptr;
					else if (srv_key == common_pipeline_states::cluster_light_indices_hash_key)
						clustered_srv = context.clustered_lighting ? context.clustered_lighting->get_index_srv() : nullptr;
					else
						is_clustered = false;

					if (is_clustered)
					{
						for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
						{
							if (srv.second.ids[index] == -1)
								continue;
							(context.device_context->*set_shader_resource_funcs[index])(srv.second.ids[index], 1, &clustered_srv);
						}
						continue;
					}
				}

				//	contextから設定系
				{
					bool is_updated = false;
					auto register_function = [&](const texture_ptr& srv_ptr)
					{
						register_srv_function(srv_ptr);
						is_updated = true;
					};

					for (UINT g_buffer_index = 0; g_buffer_index < ARRAYSIZE(common_pipeline_states::gbuffer_names); ++g_buffer_index)
					{
						if (srv_key == common_pipeline_states::gbuffer_hash_keys[g_buffer_index])
						{
							register_function(context.g_buffer_textures[g_buffer_index]);
							break;
						}
					}
					if (srv_key == common_pipeline_states::forward_only_diffuse_iem_hash_key)
					{
						register_function(forward_light_data.diffuse_ibl_texture);
					}
					else if (srv_key == common_pipeline_states::forward_only_specular_pmrem_hash_key)
					{
						register_function(forward_light_data.specluar_ibl_texture);
					}
					else if (srv_key == common_pipeline_states::forward_only_lut_ggx_hash_key)
					{
						register_function(forward_light_data.lut_ggx_texture);
					}
					else if (srv_key == common_pipeline_states::scene_color_map_hash_key)
					{
						register_function(context.scene_color_texture);
					}
					else if (srv_key == common_pipeline_states::gbuffer_depth_hash_key)
					{
						register_function(context.scene_depth_texture);
					}
					else if (srv_key == common_pipeline_states::scene_depth_map_hash_key)
					{
						register_function(context.scene_depth_texture);
					}
					else if (srv_key == common_pipeline_states::shadow_maps_hash_key)
					{
						register_function(context.shadow_map_texture);
					}

					if (is_updated)
						continue;
				}

				const std::size_t texture_count = material.get_texture_count(srv.first);
				for (std::size_t srv_index = 0; srv_index < texture_count; ++srv_index)
				{
					register_srv_function(material.find_texture(srv.first, srv_index));
				}
			}
		}

		//	マテリアルのパラメーター系登録
		{
			UINT material_flag = 0;
			for (auto& flag : convert_material_flags)
			{
				system_utility::set_bitflag(material_flag, flag.second, material.is_flag(flag.first));
			}
			for (UINT index = 0; index < magic_enum::enum_count<arrangement>(); ++index)
			{
				constant_buffer_data& data = parameter_buffer_data[index];
				if (!parameter_buffer_used[index])
					continue;

				//	定数バッファの設定
				auto& cb_data = parameter_constant_buffer[index];
				cb_data.material_shading_model = static_cast<UINT>(material.get_shading_model());
				cb_data.material_alpha_clip_threshold = material.get_alpha_clip_threshold();
				cb_data.material_flags = material_flag;
				cb_data.material_blend_mode = static_cast<UINT>(material.get_blend_mode());

				ID3D11Buffer* constant_buffer = data.buffer.Get();
				context.device_context->UpdateSubresource(constant_buffer, 0, 0, &cb_data, 0, 0);
			}
		}

		//	プリミティブ情報設定
		if (primitive_data.use && buffer_data)
		{
			for (UINT shader_index = 0; shader_index < magic_enum::enum_count<arrangement>(); ++shader_index)
			{
				UINT cbv_index = primitive_data.cbv_indices[shader_index];
				if (cbv_index == -1)
					continue;

				(context.device_context->*set_constant_buffer_funcs[shader_index])(cbv_index, 1, buffer_data->primitive_buffer.GetAddressOf());
			}
		}
		//	シャドウマップ情報設定
		if (shadow_map_buffer_used)
		{
			context.device_context->UpdateSubresource(shadow_map_buffer_data.buffer.Get(), 0, 0, &context.shadow_map_constant, 0, 0);
		}
		//	ライト情報設定
		if (deferred_light_buffer_used)
		{
			context.device_context->UpdateSubresource(deferred_light_buffer_data.buffer.Get(), 0, 0, &context.deferred_light_constant, 0, 0);
		}
		if (deferred_light_probe_buffer_used)
		{
			context.device_context->UpdateSubresource(deferred_light_probe_buffer_data.buffer.Get(), 0, 0, &context.deferred_light_probe_constant, 0, 0);
		}
		if (forward_light_buffer_used)
		{
			context.device_context->UpdateSubresource(forward_light_buffer_data.buffer.Get(), 0, 0, &forward_light_data.light, 0, 0);
		}
	}
}

void graphics_custom_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<vertex_shader>& shader)
{
	if (vs == shader)
		return;

#if defined(USE_IMGUI)
	if (vs)
		vs->unregister_reload_callback(vs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	vs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (vs)
		vs_reload_callback_id = vs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<hull_shader>& shader)
{
	if (hs == shader)
		return;

#if defined(USE_IMGUI)
	if (hs)
		hs->unregister_reload_callback(hs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	hs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (hs)
		hs_reload_callback_id = hs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<domain_shader>& shader)
{
	if (ds == shader)
		return;

#if defined(USE_IMGUI)
	if (ds)
		ds->unregister_reload_callback(ds_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	ds = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (ds)
		ds_reload_callback_id = ds->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<geometry_shader>& shader)
{
	if (gs == shader)
		return;

#if defined(USE_IMGUI)
	if (gs)
		gs->unregister_reload_callback(gs_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	gs = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (gs)
		gs_reload_callback_id = gs->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::set_shader(const std::shared_ptr<pixel_shader>& shader)
{
	if (ps == shader)
		return;

#if defined(USE_IMGUI)
	if (ps)
		ps->unregister_reload_callback(ps_reload_callback_id);
#endif	//	defined(USE_IMGUI)

	ps = shader;
	dirty = true;
	dirty_input_resource_datas = true;

#if defined(USE_IMGUI)
	if (ps)
		ps_reload_callback_id = ps->register_reload_callback([this](const shader_base* shader) { on_updates(shader); });
#endif	//	defined(USE_IMGUI)
}

void graphics_custom_pipeline_state::generate_input_resource_descs(std::vector<input_resource_desc>& out_data)
{
	if (dirty_input_resource_datas)
	{
		dirty_input_resource_datas = false;
		input_resource_desces.clear();
		input_resource_desc resource_descs[magic_enum::enum_count<arrangement>()];
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::vertex)], vs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::hull)], hs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::domain)], ds.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::geometry)], gs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::pixel)], ps.get());
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
	HRESULT hr;
	auto device = render_data::instance()->get_device().Get();

	if (dirty_input_resource_datas)
	{
		dirty_input_resource_datas = false;
		input_resource_desces.clear();
		input_resource_desc resource_descs[magic_enum::enum_count<arrangement>()];
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::vertex)], vs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::hull)], hs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::domain)], ds.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::geometry)], gs.get());
		generate_input_resource_desc(resource_descs[static_cast<int>(arrangement::pixel)], ps.get());
		for (auto& resource_desc : resource_descs)
		{
			input_resource_desces.emplace_back(resource_desc);
		}
	}

	if (dirty)
	{
		dirty = false;
		index_map.clear();
		shader_base* shaders[magic_enum::enum_count<arrangement>()] =
		{
			vs.get(),
			hs.get(),
			ds.get(),
			gs.get(),
			ps.get()
		};
		for (UINT index = 0; index < ARRAYSIZE(shaders); ++index)
		{
			if (!shaders[index])
				continue;

			for (auto& cbv : input_resource_desces[index].cbvs)
			{
				auto search_func = [&](const std::pair<std::string, index_map::indices>& data)
				{
					return data.first == cbv.name;
				};

				auto it = std::find_if(index_map.cbv.begin(), index_map.cbv.end(), search_func);
				if (it == index_map.cbv.end())
				{
					index_map.cbv.emplace_back(std::make_pair(cbv.name, index_map::indices()));
					it = std::find_if(index_map.cbv.begin(), index_map.cbv.end(), search_func);
				}

				it->second.ids[index] = cbv.register_index;
				it->second.hash = system_utility::generate_hash(cbv.name);
			}
			for (auto& rv : input_resource_desces[index].rvs)
			{
				auto search_func = [&](const std::pair<std::string, index_map::indices>& data)
				{
					return data.first == rv.name;
				};

				if (rv.is_srvs())
				{
					auto it = std::find_if(index_map.srv.begin(), index_map.srv.end(), search_func);
					if (it == index_map.srv.end())
					{
						index_map.srv.emplace_back(std::make_pair(rv.name, index_map::indices()));
						it = std::find_if(index_map.srv.begin(), index_map.srv.end(), search_func);
					}
					it->second.ids[index] = rv.register_index;
					it->second.hash = system_utility::generate_hash(rv.name);
				}
				else if (rv.is_sampler())
				{
					auto it = std::find_if(index_map.sampler.begin(), index_map.sampler.end(), search_func);
					if (it == index_map.sampler.end())
					{
						index_map.sampler.emplace_back(std::make_pair(rv.name, index_map::indices()));
						it = std::find_if(index_map.sampler.begin(), index_map.sampler.end(), search_func);
					}
					it->second.ids[index] = rv.register_index;
				}
			}
		}

		//	SRV毎に対になるサンプラーを探す
		{
			constexpr std::string_view sampler_suffix = "State";
			index_map.srv_sampler_indices.assign(index_map.srv.size(), -1);
			for (size_t srv_index = 0; srv_index < index_map.srv.size(); ++srv_index)
			{
				const std::string sampler_name = index_map.srv[srv_index].first + std::string(sampler_suffix);
				for (size_t sampler_index = 0; sampler_index < index_map.sampler.size(); ++sampler_index)
				{
					if (index_map.sampler[sampler_index].first == sampler_name)
					{
						index_map.srv_sampler_indices[srv_index] = static_cast<int>(sampler_index);
						break;
					}
				}
			}
		}

		//	必要な定数バッファを生成する
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;

			for (auto& cbv : index_map.cbv)
			{
				system_utility::hash_key cbv_key = system_utility::generate_hash(cbv.first);

				switch (cbv_key)
				{
					case	common_pipeline_states::parameter_constant_buffer_hash_key:
					{
						for (UINT index = 0; index < ARRAYSIZE(parameter_buffer_data); ++index)
						{
							if (parameter_buffer_data[index].buffer)
								continue;

							buffer_desc.ByteWidth = sizeof(common_pipeline_states::parameter_constants);
							hr = device->CreateBuffer(&buffer_desc, nullptr, parameter_buffer_data[index].buffer.ReleaseAndGetAddressOf());
							_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						}
						break;
					}
					case	common_pipeline_states::shadow_map_constant_buffer_hash_key:
					{
						if (shadow_map_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::shadow_map_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, shadow_map_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
					case	common_pipeline_states::deferred_light_constant_buffer_hash_key:
					{
						if (deferred_light_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::deferred_light_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, deferred_light_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
					case	common_pipeline_states::deferred_light_probe_constant_buffer_hash_key:
					{
						if (deferred_light_probe_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::deferred_light_probe_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, deferred_light_probe_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
					case	common_pipeline_states::forward_light_constant_buffer_hash_key:
					{
						if (forward_light_buffer_data.buffer)
							break;

						buffer_desc.ByteWidth = sizeof(common_pipeline_states::forward_light_constants);
						hr = device->CreateBuffer(&buffer_desc, nullptr, forward_light_buffer_data.buffer.ReleaseAndGetAddressOf());
						_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
						break;
					}
				}
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

bool graphics_custom_pipeline_state::is_failed()
{
	if ((vs && vs->is_failed()) ||
		(hs && hs->is_failed()) ||
		(ds && ds->is_failed()) ||
		(gs && gs->is_failed()) ||
		(ps && ps->is_failed()))
	{
		return true;
	}
	return false;
}
#endif	//	defined(USE_IMGUI)

