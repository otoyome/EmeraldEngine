#include "Renderer/scene_renderer.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/deferred_lighting_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"

#include "PipelineState/post_process_pipeline_state.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#endif // defined(USE_IMGUI)

#include "Renderer/scene_renderer_common.h"

namespace
{
    void compute_bounding_sphere(DirectX::XMVECTOR* corners, UINT size, DirectX::XMVECTOR& out_center, float& out_radius)
    {
        DirectX::XMVECTOR sum = DirectX::XMVectorZero();
        for (UINT index = 0; index < size; ++index)
        {
            sum = DirectX::XMVectorAdd(sum, corners[index]);
        }

        DirectX::XMVECTOR center = DirectX::XMVectorScale(sum, 1.0f / 8.0f);

        float radius_sq = 0.0f;
        for (UINT index = 0; index < size; ++index)
        {
            float d_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(corners[index], center)));
            radius_sq = std::max(radius_sq, d_sq);
        }

        out_center = center;
        out_radius = sqrtf(radius_sq);
    }

    void compute_frustum_bounding_sphere(float ndc_near, float ndc_far,
                                          DirectX::CXMMATRIX inverse_view_projection_transform,
                                          DirectX::XMVECTOR& out_center, float& out_radius)
    {
        DirectX::XMVECTOR corners[8] =
        {
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(-1.0f, -1.0f, ndc_near, 1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(+1.0f, -1.0f, ndc_near, 1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(+1.0f, +1.0f, ndc_near, 1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(-1.0f, +1.0f, ndc_near, 1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(-1.0f, -1.0f, ndc_far,  1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(+1.0f, -1.0f, ndc_far,  1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(+1.0f, +1.0f, ndc_far,  1.0f), inverse_view_projection_transform),
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(-1.0f, +1.0f, ndc_far,  1.0f), inverse_view_projection_transform),
        };
		compute_bounding_sphere(corners, 8, out_center, out_radius);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
depth_prepass_renderer::depth_prepass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
}

depth_prepass_renderer::~depth_prepass_renderer()
{
}

void depth_prepass_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass, *renderer_context.meshlet_culling_system);
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass, *renderer_context.meshlet_culling_system);
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->opaque, material::surface_paths::depth_write_pass);
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->mask, material::surface_paths::depth_write_pass);

		rendering_pass.deactivate(context.device_context);
	}, "depth_pre_pass", renderer_context, get_render_basis_sort_order());
}

void depth_prepass_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	描画パス生成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gbuffer_renderer::gbuffer_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	シェーダー準備
	pipeline_state_manager::instance()->generate_pipeline_state<skybox_pipeline_state>("skybox", &skybox_pipeline_states);
	skybox_pipeline_states->set_domain(pipeline_state_base::domains::system);
}

gbuffer_renderer::~gbuffer_renderer()
{
}

void gbuffer_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass, *renderer_context.meshlet_culling_system);
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass, *renderer_context.meshlet_culling_system);
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->opaque, material::surface_paths::g_buffer_pass);
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->mask, material::surface_paths::g_buffer_pass);

		//	スカイライト処理
		for (const auto& data : context.render_data->light_container.light_probes)
		{
			const light_probe* light_probe = data.get();
			if (!light_probe->is_flag(light_probe::flags::active) || !light_probe->is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			context.diffuse_ibl_texture = light_probe->get_diffuse_ibl_texture();
			context.specluar_ibl_texture = light_probe->get_specular_ibl_texture();
			context.lut_ggx_texture = light_probe->get_lut_ggx_texture();

			//	間接光の情報を設定
			light_probes probe;
			probe.position.x = light_probe->get_position().x;
			probe.position.y = light_probe->get_position().y;
			probe.position.z = light_probe->get_position().z;
			probe.inner_area.x = light_probe->get_inner_range().x;
			probe.inner_area.y = light_probe->get_inner_range().y;
			probe.inner_area.z = light_probe->get_inner_range().z;
			probe.outer_area.x = light_probe->get_outer_range().x;
			probe.outer_area.y = light_probe->get_outer_range().y;
			probe.outer_area.z = light_probe->get_outer_range().z;
			probe.color = light_probe->get_color();
			context.deferred_light_constant = {};
			context.deferred_light_constant.light_data = light_data_utility::convert_light_data(probe);
			context.deferred_light_constant.light_attribute = light_probe->get_attribute();

			//	全画面描画
			skybox_pipeline_states->begin(context);
			scene_renderer_commons::draw(context, skybox_pipeline_states, 4);
			skybox_pipeline_states->end(context);
			break;
		}

		rendering_pass.deactivate(context.device_context);
	}, "gbuffer_pass", renderer_context, get_render_basis_sort_order());
}

void gbuffer_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	GBufferパス生成
	{
		FLOAT clear_color[][4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	base_color
			{ 0.0f, 0.0f, 0.0f, 1.0f },	//	normal_depth
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	parameter
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	velocity
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	unique_data
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	shade_color [エンジン改造:G-Buffer]
			{ 0.0f, 0.0f, 0.0f, 0.0f },	//	ilm_color [エンジン改造:G-Buffer]
		};
		rendering_pass.render_targets.clear();
		for (int i = 0; i < static_cast<int>(common_pipeline_states::gbuffer_id::max); ++i)
		{
			rendering_pass.render_targets.emplace_back(render_targets->g_buffer_textures[i]);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color_value[0] = clear_color[i][0];
			rtv.clear_color_value[1] = clear_color[i][1];
			rtv.clear_color_value[2] = clear_color[i][2];
			rtv.clear_color_value[3] = clear_color[i][3];
		}
		{
			//	emissive用にシーンバッファ追加
			rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color_value[0] = 0.0f;
			rtv.clear_color_value[1] = 0.0f;
			rtv.clear_color_value[2] = 0.0f;
			rtv.clear_color_value[3] = 0.0f;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_desc.Texture2D.MipSlice = 0;
		rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
		rendering_pass.depth_stencil.clear_depth = false;
	}

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gbuffer_decal_renderer::gbuffer_decal_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	//	書き込み用デフォルトプリミティブ
	cube = std::make_unique<geometric_cube>(render_data::instance()->get_device().Get());

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		{
			buffer_desc.ByteWidth = sizeof(common_pipeline_states::decal_primitive_constants);
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, decal_primitive_primitive_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	ステンシル書き込みシェーダー取得
	pipeline_state_manager* instance = pipeline_state_manager::instance();
	instance->generate_pipeline_state<deferred_decal_stencil_write_pipeline_state>("deferred_decal_stencil_write", &stencil_write_pipeline_state);
}

gbuffer_decal_renderer::~gbuffer_decal_renderer()
{
}

void gbuffer_decal_renderer::render(renderer_context& renderer_context)
{
	if (renderer_context.render_decal_indices->draw_list.empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	法線/深度コピー
		{
			Microsoft::WRL::ComPtr<ID3D11Resource> destination, source;
			render_targets->work_buffer_rgba32float_texture->get_shader_resource_view()->GetResource(destination.ReleaseAndGetAddressOf());
			render_targets->g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)]->get_shader_resource_view()->GetResource(source.ReleaseAndGetAddressOf());
			context.device_context->CopyResource(destination.Get(), source.Get());
		}

		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	デカール
		{
			//	カメラ球体
			collision::bounding_sphere sphere;
			{
				DirectX::XMFLOAT3 center;
				center.x = context.scene_constant.camera_position.x;
				center.y = context.scene_constant.camera_position.y;
				center.z = context.scene_constant.camera_position.z;
				sphere = collision::bounding_sphere(center, context.scene_constant.camera_clip_distance.x * 1.01f);
			}

			//	必要なテクスチャを設定
			{
				context.g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)] = render_targets->work_buffer_rgba32float_texture;
				context.g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::unique_data)] = render_targets->g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::unique_data)];
			}

			//	デカール描画
			for (auto& decal_index : renderer_context.render_decal_indices->draw_list)
			{
				auto& decal = context.render_data->decal_container.decal_datas[decal_index];

				//	バッファ情報生成
				buffer_datas buffer_data;
				{
					DirectX::XMMATRIX W = DirectX::XMMatrixScaling(decal.scale.x, decal.scale.y, decal.scale.z)
										* DirectX::XMMatrixRotationQuaternion(system_utility::to_xmvector(decal.rotation.to_quaternion()))
										* DirectX::XMMatrixTranslation(decal.position.x, decal.position.y, decal.position.z);
					//	プリミティブバッファ更新
					{
						common_pipeline_states::decal_primitive_constants dpc;
						DirectX::XMStoreFloat4x4(&dpc.decal_transform, W);
						auto front = system_utility::to_xmfloat(decal.rotation.up());
						dpc.decal_direction.x = front.x;
						dpc.decal_direction.y = front.y;
						dpc.decal_direction.z = front.z;
						dpc.decal_direction.w = 0;
						DirectX::XMStoreFloat4x4(&dpc.decal_inverse_transform, DirectX::XMMatrixInverse(nullptr, W));
						context.device_context->UpdateSubresource(decal_primitive_primitive_buffer.Get(), 0, 0, &dpc, 0, 0);
					}
					buffer_data.primitive_buffer = decal_primitive_primitive_buffer;
				}

				//	マテリアル更新
				decal.material.material->update_parameters(context);

				//	ステンシル書き込み
				{
					stencil_write_pipeline_state->begin(context);
					stencil_write_pipeline_state->update_parameters(context, *decal.material.material, &buffer_data);
					scene_renderer_commons::draw_geometric_primitive(context, *cube);
					stencil_write_pipeline_state->end(context);
				}

				//	デカール書き込み
				auto pass = sphere.intersection(&decal.box) ? material::decal_paths::fullscreen : material::decal_paths::primitive;
				if (auto pipeline_state = decal.material.material->get_pipeline_state(pass))
				{
					pipeline_state->begin(context);
					if (pass == material::decal_paths::fullscreen)
					{
						context.is_force_material_both_draw = true;
						pipeline_state->update_parameters(context, *decal.material.material, &buffer_data);
						scene_renderer_commons::draw(context, pipeline_state, 4);
						context.is_force_material_both_draw = false;
					}
					else
					{
						pipeline_state->update_parameters(context, *decal.material.material, &buffer_data);
						scene_renderer_commons::draw_geometric_primitive(context, *cube);
					}
					pipeline_state->end(context);
				}
			}

			rendering_pass.deactivate(context.device_context);
		}

		context.g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)] = render_targets->g_buffer_textures[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)];
	}, "gbuffer_decal_pass", renderer_context, get_render_basis_sort_order());
}

void gbuffer_decal_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	GBufferパス生成
	{
		rendering_pass.render_targets.clear();
#if 01
		{
			rendering_pass.render_targets.emplace_back(render_targets->g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::base_color)]);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color = false;
		}
		{
			rendering_pass.render_targets.emplace_back(render_targets->g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::normal_depth)]);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color = false;
		}
#else
		FLOAT clear_color[][4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },

			{ 0.0f, 0.0f, 0.0f, 0.0f },
		};
		for (int i = 0; i < static_cast<int>(common_pipeline_states::gbuffer_id::max); ++i)
		{
			rendering_pass.render_targets.emplace_back(render_targets->g_buffer_textures[i]);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color = false;
		}
#endif
		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_desc.Texture2D.MipSlice = 0;
		rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
		rendering_pass.depth_stencil.clear_depth = false;
		rendering_pass.depth_stencil.clear_stencil = true;
	}

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
deffered_lighting_renderer::deffered_lighting_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, lighting_pass_viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
, shadow_map_pass_viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture;
	if (texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture))
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
	//	シャドウマップ生成
	if (texture = std::static_pointer_cast<texture2d>(render_targets->shadowmap_depth_texture))
	{
		resize_shadow_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}

	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	ディファードレンダリング系シェーダー準備
	{
		static constexpr const char* shader_names[] =
		{
			"deferred_indirect_lighting",
			"deferred_directional_lighting",
			"deferred_point_lighting",
			"deferred_spot_lighting",
		};
		//	
#if defined(USE_CUSTOM_PIPELINE)
		deferred_lighting_material = std::make_shared<material>();
		deferred_lighting_material->set_domain(material::domains::system);
		deferred_lighting_material->set_flag(material::flags::two_sided, true);
		deferred_lighting_material->set_blend_mode(material::blend_modes::addative);	//	加算合成

		static constexpr std::string_view vs_path_list[] =
		{
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
		};
		static constexpr std::string_view ps_path_list[] =
		{
			"Shaders/DeferredRendering/deferred_rendering_indirect_ps.hlsl",
			"Shaders/DeferredRendering/deferred_rendering_directional_ps.hlsl",
			"Shaders/DeferredRendering/deferred_rendering_point_ps.hlsl",
			"Shaders/DeferredRendering/deferred_rendering_spot_ps.hlsl",
		};
		static constexpr material::system_paths system_paths_list[] =
		{
			material::system_paths::deferred_indirect,
			material::system_paths::deferred_directional,
			material::system_paths::deferred_point,
			material::system_paths::deferred_spot,
		};
			
		//	deferred_indirect_lighting
		for (UINT index = 0; index < ARRAYSIZE(shader_names); ++ index)
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>(shader_names[index], &pso);
			pso->set_domain(pipeline_state_base::domains::system);
			pso->set_flag(pipeline_state_base::flags::hidden, false);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = vs_path_list[index];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = ps_path_list[index];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
			deferred_lighting_material->set_pipeline_state(system_paths_list[index], shader_names[index]);
		}
#else
		instance->generate_pipeline_state<deferred_indirect_lighting_pipeline_state>(shader_names[0], &indirect_lighting_pipeline_state);
		instance->generate_pipeline_state<deferred_directional_lighting_pipeline_state>(shader_names[1], &directional_lighting_pipeline_state);
		instance->generate_pipeline_state<deferred_point_lighting_pipeline_state>(shader_names[2], &point_lighting_pipeline_state);
		instance->generate_pipeline_state<deferred_spot_lighting_pipeline_state>(shader_names[3], &spot_lighting_pipeline_state);
#endif
	}
}

deffered_lighting_renderer::~deffered_lighting_renderer()
{
}

void deffered_lighting_renderer::render(renderer_context& context)
{
	//	描画番号
	register_index = 0;

	//	間接光
	render_indirect(context);

	//	ライティング処理
	for (const auto& data : context.render_data->light_container.lights)
	{
		const light* light = data.get();
		if (!light->is_flag(light::flags::active))
			continue;

		switch (light->get_light_type())
		{
			case light_type::directional:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				if (light->is_flag(light::flags::use_shadow))
				{
					render_directional_shadow_map(*light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_directional(*light, shadow_map_constant, context);
				break;
			}
			case light_type::point:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				if (light->is_flag(light::flags::use_shadow))
				{
					render_point_shadow_map(*light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_point(*light, shadow_map_constant, context);
				break;
			}
			case light_type::spot:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				if (light->is_flag(light::flags::use_shadow))
				{
					render_spot_shadow_map(*light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_spot(*light, shadow_map_constant, context);
				break;
			}
		}
	}
}

void deffered_lighting_renderer::render_indirect(renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	AOバッファ生成
		{
		}

		//	描画先設定
		lighting_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);

		//	間接光
		for (const auto& data : context.render_data->light_container.light_probes)
		{
			const light_probe* light_probe = data.get();
			if (!light_probe->is_flag(light_probe::flags::active))
				continue;

			//	間接光の情報を設定
			light_probes probe;
			probe.position.x = light_probe->get_position().x;
			probe.position.y = light_probe->get_position().y;
			probe.position.z = light_probe->get_position().z;
			probe.inner_area.x = light_probe->get_inner_range().x;
			probe.inner_area.y = light_probe->get_inner_range().y;
			probe.inner_area.z = light_probe->get_inner_range().z;
			probe.outer_area.x = light_probe->get_outer_range().x;
			probe.outer_area.y = light_probe->get_outer_range().y;
			probe.outer_area.z = light_probe->get_outer_range().z;
			probe.color = light_probe->get_color();
			context.deferred_light_constant = {};
			context.deferred_light_constant.light_data = light_data_utility::convert_light_data(probe);
			context.deferred_light_constant.light_attribute = light_probe->get_attribute();

			if (light_probe->is_flag(light_probe::flags::unbind))
			{
				//	全画面描画
#if defined(USE_CUSTOM_PIPELINE)
				//	IBLテクスチャ設定
				deferred_lighting_material->set_texture("diffuse_iem", light_probe->get_diffuse_ibl_texture());
				deferred_lighting_material->set_texture("specular_pmrem", light_probe->get_specular_ibl_texture());
				deferred_lighting_material->set_texture("lut_ggx", light_probe->get_lut_ggx_texture());

				auto pipeline_state =
					deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_indirect);

				pipeline_state->begin(context);
				pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
				scene_renderer_commons::draw(context, pipeline_state, 4);
				pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
				//	IBLテクスチャ設定
				context.diffuse_ibl_texture = light_probe->get_diffuse_ibl_texture();
				context.specluar_ibl_texture = light_probe->get_specular_ibl_texture();
				context.lut_ggx_texture = light_probe->get_lut_ggx_texture();

				indirect_lighting_pipeline_state->begin(context);
				scene_renderer_commons::draw(context, indirect_lighting_pipeline_state, 4);
				indirect_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
			}
			else
			{
				//	範囲制限をかける
			}
		}

		lighting_pass.deactivate(context.device_context);
	}, "render_indirect_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_directional(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		//	コンテキストに情報登録
		context.deferred_light_constant.light_data = light.convert_light_data();
		context.deferred_light_constant.light_attribute = light.get_attribute();
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
#if defined(USE_CUSTOM_PIPELINE)
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_directional);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			scene_renderer_commons::draw(context, pipeline_state, 4);
			pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
			directional_lighting_pipeline_state->begin(context);
			scene_renderer_commons::draw(context, directional_lighting_pipeline_state, 4);
			directional_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_directional_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_directional_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	DirectX::XMFLOAT4 rotation = system_utility::to_xmfloat(light.get_rotation().to_quaternion());
	DirectX::XMFLOAT3 position = light.get_position();
	DirectX::XMMATRIX CameraTransformMatrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&context.scene_constant.view_transform));
	DirectX::XMMATRIX LightTransformMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) * 
												DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMMATRIX InverseProjectionTransform;
	InverseProjectionTransform = DirectX::XMLoadFloat4x4(&context.scene_constant.inverse_projection_transform);

	DirectX::XMVECTOR LightRight, LightUp, LightFront, LightPosition;
	LightRight = LightTransformMatrix.r[0];
	LightUp = LightTransformMatrix.r[1];
	LightFront = LightTransformMatrix.r[2];
	LightPosition = LightTransformMatrix.r[3];

	//	クリップ距離を算出
	float near_clip, far_clip;
	{
		DirectX::XMVECTOR NearPosition, FarPosition;
		NearPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), InverseProjectionTransform);
		FarPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), InverseProjectionTransform);
		near_clip = DirectX::XMVectorGetZ(NearPosition);
		far_clip = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(FarPosition, NearPosition)));
	}

	//	シーンのAABBを算出
	DirectX::XMFLOAT3 scene_min = {FLT_MAX, FLT_MAX, FLT_MAX}, scene_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
	{
		auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
		auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();
		for (auto& draw_index : context.render_data->mesh_container.shadow_datas)
		{
			auto draw_data = draw_data_ptr + draw_index;
			auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
			scene_min = system_utility::to_xmfloat(math::min(system_utility::to_math(buffer_data->bounds_min), system_utility::to_math(scene_min)));
			scene_max = system_utility::to_xmfloat(math::max(system_utility::to_math(buffer_data->bounds_max), system_utility::to_math(scene_max)));
		}
	}
	const bool has_scene_aabb = (scene_min.x <= scene_max.x) && (scene_min.y <= scene_max.y) && (scene_min.z <= scene_max.z);

	//	ライト空間上でのシーンAABBのZ範囲を取得
	float scene_min_z = 0.0f, scene_max_z = 0.0f;
	if (has_scene_aabb)
	{
		DirectX::XMVECTOR Min = DirectX::XMLoadFloat3(&scene_min);
		DirectX::XMVECTOR Max = DirectX::XMLoadFloat3(&scene_max);
		DirectX::XMVECTOR Center = DirectX::XMVectorScale(DirectX::XMVectorAdd(Min, Max), 0.5f);
		DirectX::XMVECTOR Extent = DirectX::XMVectorScale(DirectX::XMVectorSubtract(Max, Min), 0.5f);
		float center_proj = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center, LightFront));
		float extent_proj = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Extent, DirectX::XMVectorAbs(LightFront)));
		scene_min_z = center_proj - extent_proj;
		scene_max_z = center_proj + extent_proj;
	}

	//	CSM用に遠クリップ面に対して距離制限をかける
	float projection_near_clip, projection_far_clip;
	{
		projection_near_clip = 0.0f;
		projection_far_clip = scene_renderer_commons::convert_linear_depth_to_projection_depth(light.get_csm_distance_factor(), far_clip / near_clip);

		//	クリップ距離を再算出
		DirectX::XMVECTOR NearPosition, FarPosition;
		NearPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, projection_near_clip, 0.0f), InverseProjectionTransform);
		FarPosition = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, projection_far_clip, 0.0f), InverseProjectionTransform);
		far_clip = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(FarPosition, NearPosition)));
	}

	//	シャドウマップ情報算出
	shadow_map_constant.shadow_map_count = directional_shadow_map_count;
	shadow_map_constant.shadow_attenuation = light.get_shadow_attenuation();
	shadow_map_constant.shadow_mask = 0;
	collision::frustum_culling frustum_culling;
	{
		DirectX::XMMATRIX InverseViewProjectionTransform;
		InverseViewProjectionTransform = DirectX::XMLoadFloat4x4(&context.scene_constant.inverse_view_projection_transform);
		//	視錐台を囲むライト空間上の行列を生成
		DirectX::XMMATRIX LightViewMatrix, LightProjectionMatrix;
		float top_min_distance, top_max_distance;	//	最大深度の範囲
		{
			//	AABBを囲む球体を算出
			DirectX::XMVECTOR Center;
			float radius;
			if (has_scene_aabb)
			{
				DirectX::XMVECTOR scene_corners[8];
				scene_corners[0] = DirectX::XMVectorSet(scene_min.x, scene_min.y, scene_min.z, 1.0f);
				scene_corners[1] = DirectX::XMVectorSet(scene_max.x, scene_min.y, scene_min.z, 1.0f);
				scene_corners[2] = DirectX::XMVectorSet(scene_min.x, scene_max.y, scene_min.z, 1.0f);
				scene_corners[3] = DirectX::XMVectorSet(scene_max.x, scene_max.y, scene_min.z, 1.0f);
				scene_corners[4] = DirectX::XMVectorSet(scene_min.x, scene_min.y, scene_max.z, 1.0f);
				scene_corners[5] = DirectX::XMVectorSet(scene_max.x, scene_min.y, scene_max.z, 1.0f);
				scene_corners[6] = DirectX::XMVectorSet(scene_min.x, scene_max.y, scene_max.z, 1.0f);
				scene_corners[7] = DirectX::XMVectorSet(scene_max.x, scene_max.y, scene_max.z, 1.0f);
				compute_bounding_sphere(scene_corners, 8, Center, radius);
			}
			else
			{
				//	フラスタムから生成
				compute_frustum_bounding_sphere(projection_near_clip, projection_far_clip, InverseViewProjectionTransform, Center, radius);
			}

			//	視線行列算出
			{
				LightPosition = DirectX::XMVectorSubtract(Center, DirectX::XMVectorScale(LightFront, radius));
				LightTransformMatrix.r[0] = DirectX::XMVectorSetW(LightRight, 0.0f);
				LightTransformMatrix.r[1] = DirectX::XMVectorSetW(LightUp, 0.0f);
				LightTransformMatrix.r[2] = DirectX::XMVectorSetW(LightFront, 0.0f);
				LightTransformMatrix.r[3] = DirectX::XMVectorSetW(LightPosition, 1.0f);
				LightViewMatrix = DirectX::XMMatrixInverse(nullptr, LightTransformMatrix);
			}

			//	投影行列生成
			{
				top_min_distance = 0.0f;
				top_max_distance = top_min_distance + radius * 2.0f;
				LightProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(-radius, +radius, -radius, +radius, top_max_distance, top_min_distance);
			}

			//	フラスタムカリング生成
			{
				DirectX::XMFLOAT4X4 frustum_culling_light_view_projection_transform;
				DirectX::XMStoreFloat4x4(&frustum_culling_light_view_projection_transform,
										  LightViewMatrix * LightProjectionMatrix);
				frustum_culling.create_planes(frustum_culling_light_view_projection_transform);
				frustum_culling.create_frustum_corners(DirectX::XMMatrixInverse(nullptr,
														DirectX::XMLoadFloat4x4(&frustum_culling_light_view_projection_transform)));
			}
		}

		//	PSSM
		std::vector<float> split_distance;
		split_distance.resize(directional_shadow_map_count + 1);
		{
			float split_weight = light.get_pssm_split_weight();
			float weight = math::lerp(0.1f, 0.9f, split_weight);
			float divN = far_clip / near_clip, subN = far_clip - near_clip;

			UINT index = 0;
			split_distance[index]	= near_clip;
			for (++index; index < directional_shadow_map_count; ++index)
			{
				float spliit = static_cast<float>(index) / static_cast<float>(directional_shadow_map_count);
				float fLog = near_clip * pow(divN, spliit);
				float fUni = near_clip + subN * spliit;
				split_distance[index]	= fLog * weight + fUni * ( 1.0f - weight);
			}
			split_distance[index]	= far_clip;
		}

		//	視錐台を距離で分割してライトビュープロジェクション行列を生成
		for (UINT index = 0; index < directional_shadow_map_count; ++index)
		{
			//	AABBを囲む球体を算出（半径はカメラの回転に依存せず、near/far/FOV/aspectのみで決まる＝Stableな値）
			DirectX::XMVECTOR Center;
			float radius;
			compute_frustum_bounding_sphere(scene_renderer_commons::convert_linear_depth_to_projection_depth(split_distance[index + 0] / far_clip, far_clip / near_clip),
											scene_renderer_commons::convert_linear_depth_to_projection_depth(split_distance[index + 1] / far_clip, far_clip / near_clip),
											InverseViewProjectionTransform, Center, radius);
			radius *= 1.0001f;

			//	各行列調整
			{
				//	Stable CSM
				float texel_size_x = (radius * 2.0f) / shadow_map_pass_viewport.Width;
				float texel_size_y = (radius * 2.0f) / shadow_map_pass_viewport.Height;
				float center_x = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center, LightRight));
				float center_y = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center, LightUp));
				float center_z = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Center, LightFront));
				float X = floorf(center_x / texel_size_x) * texel_size_x;
				float Y = floorf(center_y / texel_size_y) * texel_size_y;

				//	near側はcasterを含めるためAABB最小値まで延長、far側はレシーバー範囲
				float receiver_near_z = center_z - radius;
				float receiver_far_z  = center_z + radius;
				float near_z = receiver_near_z;
				float far_z  = receiver_far_z;
				if (has_scene_aabb)
				{
					near_z = std::min(receiver_near_z, scene_min_z);
				}
				else
				{
					near_z -= (top_max_distance - top_min_distance);
				}

				DirectX::XMVECTOR Origin = DirectX::XMVectorAdd(
					DirectX::XMVectorAdd(DirectX::XMVectorScale(LightRight, X), DirectX::XMVectorScale(LightUp, Y)),
					DirectX::XMVectorScale(LightFront, near_z));

				LightTransformMatrix.r[0] = DirectX::XMVectorSetW(LightRight, 0.0f);
				LightTransformMatrix.r[1] = DirectX::XMVectorSetW(LightUp, 0.0f);
				LightTransformMatrix.r[2] = DirectX::XMVectorSetW(LightFront, 0.0f);
				LightTransformMatrix.r[3] = DirectX::XMVectorSetW(Origin, 1.0f);
				DirectX::XMMATRIX LightViewMatrix = DirectX::XMMatrixInverse(nullptr, LightTransformMatrix);

				//	投影行列生成
				float ortho_near = 0.0f;
				float ortho_far  = std::max(far_z - near_z, 0.0001f);
				DirectX::XMMATRIX LightProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(-radius, +radius, -radius, +radius, ortho_far, ortho_near);
				DirectX::XMStoreFloat4x4(&shadow_map_constant.light_view_projection_transform[index], LightViewMatrix * LightProjectionMatrix);
			}

			{
				float split_near_clip = split_distance[index + 0];
				float split_far_clip = split_distance[index + 1];
				shadow_map_constant.shadow_split_distance[index].x = split_near_clip;
				shadow_map_constant.shadow_split_distance[index].y = split_near_clip;
				shadow_map_constant.shadow_split_distance[index].z = split_far_clip;
				shadow_map_constant.shadow_split_distance[index].w = split_far_clip;

				//	バイアス納入
				auto& bias_data = light.get_shadow_bias(index);
				float depth_bias, slope_depth_bias;
				depth_bias = bias_data.max_depth_bias / directional_shadow_map_count;
				depth_bias *= 0.5f;
				depth_bias *= bias_data.depth_bias;
				slope_depth_bias	= bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
				shadow_map_constant.shadow_bias[index].x = std::max(0.0f, depth_bias);
				shadow_map_constant.shadow_bias[index].y = std::max(0.0f, depth_bias * slope_depth_bias);
				shadow_map_constant.shadow_bias[index].z = bias_data.max_slope_depth_bias;
				shadow_map_constant.shadow_bias[index].w = 1.0f / bias_data.sigma_factor; // 分散値
			}
		}
	}

	job_executor.execute([this, frustum_culling, shadow_map_constant](graphics_context& context)
	{
		shadow_map_pass.activate(context.device_context);

		//	定数バッファ情報設定
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &shadow_map_pass_viewport);

		//	モデル類描画
		context.is_shadow_rendering = true;
		scene_renderer_commons::model_render_shadow_batched(context, context.render_data->mesh_container.shadow_datas, frustum_culling, material::surface_paths::shadow_write_pass);
		//scene_renderer_commons::particle_render(context, context.render_data->effect_container.sprite_trail_shadow_datas, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		shadow_map_pass.deactivate(context.device_context);
	}, "render_directional_shadow_map_pass", context, get_render_basis_sort_order() + register_index++, true);
}

void deffered_lighting_renderer::render_point(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		context.deferred_light_constant.light_data = light.convert_light_data();
		context.deferred_light_constant.light_attribute = light.get_attribute();
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
#if defined(USE_CUSTOM_PIPELINE)
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_point);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			scene_renderer_commons::draw(context, pipeline_state, 4);
			pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
			point_lighting_pipeline_state->begin(context);
			scene_renderer_commons::draw(context, point_lighting_pipeline_state, 4);
			point_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_point_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_point_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	DirectX::XMFLOAT3 position = light.get_position();
	DirectX::XMVECTOR position_vector = DirectX::XMLoadFloat3(&position);
	float range = light.get_range();

	//	キューブマップ6面分の視線方向・上方向(0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z)
	static const DirectX::XMFLOAT3 cube_face_directions[6] =
	{
		{ +1.0f,  0.0f,  0.0f },
		{ -1.0f,  0.0f,  0.0f },
		{  0.0f, +1.0f,  0.0f },
		{  0.0f, -1.0f,  0.0f },
		{  0.0f,  0.0f, +1.0f },
		{  0.0f,  0.0f, -1.0f },
	};
	static const DirectX::XMFLOAT3 cube_face_ups[6] =
	{
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 0.0f, +1.0f },
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 1.0f,  0.0f },
	};

	DirectX::XMMATRIX ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.0f, range, 1.0f);

	//	定数バッファ設定
	shadow_map_constant.shadow_map_count = 6;
	shadow_map_constant.shadow_attenuation = light.get_shadow_attenuation();
	shadow_map_constant.shadow_mask = 0;

	for (UINT face = 0; face < 6; ++face)
	{
		DirectX::XMVECTOR direction_vector = DirectX::XMLoadFloat3(&cube_face_directions[face]);
		DirectX::XMVECTOR up_vector = DirectX::XMLoadFloat3(&cube_face_ups[face]);
		DirectX::XMMATRIX ViewMatrix = DirectX::XMMatrixLookToLH(position_vector, direction_vector, up_vector);
		DirectX::XMStoreFloat4x4(&shadow_map_constant.light_view_projection_transform[face], ViewMatrix * ProjectionMatrix);

		//	面選択はワールド方向で行うため、カスケード用の深度バンドは常に真になる値にしておく
		shadow_map_constant.shadow_split_distance[face].x = -1.0f;
		shadow_map_constant.shadow_split_distance[face].y = -1.0f;
		shadow_map_constant.shadow_split_distance[face].z = FLT_MAX;
		shadow_map_constant.shadow_split_distance[face].w = FLT_MAX;

		//	バイアス処理
		auto& bias_data = light.get_shadow_bias(face);
		float depth_bias, slope_depth_bias;
		depth_bias = bias_data.max_depth_bias / math::max(0.01f, range);
		depth_bias *= 0.5f;
		depth_bias *= bias_data.depth_bias;
		slope_depth_bias = bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
		shadow_map_constant.shadow_bias[face].x = std::max(0.0f, depth_bias);
		shadow_map_constant.shadow_bias[face].y = std::max(0.0f, depth_bias * slope_depth_bias);
		shadow_map_constant.shadow_bias[face].z = bias_data.max_slope_depth_bias;
		shadow_map_constant.shadow_bias[face].w = bias_data.sigma_factor; // 分散値
	}

	job_executor.execute([this, shadow_map_constant, position, range](graphics_context& context)
	{
		shadow_map_pass.activate(context.device_context);

		context.shadow_map_constant = shadow_map_constant;

		context.device_context->RSSetViewports(1, &shadow_map_pass_viewport);

		context.is_shadow_rendering = true;
		scene_renderer_commons::model_render_shadow_batched(context, context.render_data->mesh_container.shadow_datas, position, range, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		shadow_map_pass.deactivate(context.device_context);
	}, "render_point_shadow_map_pass", context, get_render_basis_sort_order() + register_index++, true);
}



void deffered_lighting_renderer::render_spot(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		context.deferred_light_constant.light_data = light.convert_light_data();
		context.deferred_light_constant.light_attribute = light.get_attribute();
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
#if defined(USE_CUSTOM_PIPELINE)
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_spot);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			scene_renderer_commons::draw(context, pipeline_state, 4);
			pipeline_state->end(context);
#else	//	defined(USE_CUSTOM_PIPELINE)
			spot_lighting_pipeline_state->begin(context);
			scene_renderer_commons::draw(context, spot_lighting_pipeline_state, 4);
			spot_lighting_pipeline_state->end(context);
#endif	//	defined(USE_CUSTOM_PIPELINE)
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_spot_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_spot_shadow_map(const light& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	auto reserve_scene_constant = context.scene_constant;

	DirectX::XMFLOAT4 rotation = system_utility::to_xmfloat(light.get_rotation().to_quaternion());
	DirectX::XMFLOAT3 position = light.get_position();
	DirectX::XMMATRIX TransformMatrix, ViewMatrix, ProjectionMatrix;
	TransformMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation))
					* DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	ViewMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);
	ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(light.get_outer_cone() * 2.0f, 1.0f, light.get_range(), 1.0f);

	//	定数バッファ情報設定
	shadow_map_constant.shadow_map_count = 1;
	shadow_map_constant.shadow_attenuation = light.get_shadow_attenuation();
	shadow_map_constant.shadow_split_distance[0].x = -1.0f;
	shadow_map_constant.shadow_split_distance[0].y = -1.0f;
	shadow_map_constant.shadow_split_distance[0].z = FLT_MAX;
	shadow_map_constant.shadow_split_distance[0].w = FLT_MAX;

	//	バイアス納入
	{
		//	ライトの範囲で処理
		float near_clip = 0.1f;
		float far_clip = near_clip + light.get_range();

		auto& bias_data = light.get_shadow_bias(0);
		float depth_bias, slope_depth_bias;
		depth_bias = bias_data.max_depth_bias / directional_shadow_map_count;
		depth_bias *= 0.5f;
		depth_bias *= bias_data.depth_bias;
		slope_depth_bias	= bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
		shadow_map_constant.shadow_bias[0].x = std::max(0.0f, depth_bias);
		shadow_map_constant.shadow_bias[0].y = std::max(0.0f, depth_bias * slope_depth_bias);
		shadow_map_constant.shadow_bias[0].z = bias_data.max_slope_depth_bias;
		shadow_map_constant.shadow_bias[0].w = bias_data.sigma_factor; // 分散値
	}

	shadow_map_constant.shadow_mask = 0;
	DirectX::XMStoreFloat4x4(&shadow_map_constant.light_view_projection_transform[0], ViewMatrix * ProjectionMatrix);

	//	モデル類描画
	collision::frustum_culling frustum_culling;
	{
		frustum_culling.create_planes(shadow_map_constant.light_view_projection_transform[0]);
		frustum_culling.create_frustum_corners(DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&context.scene_constant.view_projection_transform)));
	}

	job_executor.execute([this, shadow_map_constant, frustum_culling](graphics_context& context)
	{
		shadow_map_pass.activate(context.device_context);

		context.shadow_map_constant = shadow_map_constant;

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &shadow_map_pass_viewport);

		context.is_shadow_rendering = true;
		scene_renderer_commons::model_render_shadow_batched(context, context.render_data->mesh_container.shadow_datas, frustum_culling, material::surface_paths::shadow_write_pass);
		//scene_renderer_commons::particle_render(context, context.render_data->effect_container.sprite_trail_shadow_datas, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		shadow_map_pass.deactivate(context.device_context);
	}, "render_spot_shadow_map_pass", context, get_render_basis_sort_order() + register_index++, true);
	context.scene_constant = reserve_scene_constant;
}

void deffered_lighting_renderer::resize_buffer(int width, int height)
{
	if (lighting_pass_viewport.Width == static_cast<float>(width) && lighting_pass_viewport.Height == static_cast<float>(height))
		return;

	//	ライティングパス生成
	{
		lighting_pass.render_targets.clear();
		lighting_pass.render_targets.emplace_back(render_targets->scene_color_texture);
		lighting_pass.render_targets.back().clear_color = false;
		lighting_pass.render_targets.back().clear_color_value[0] = 0.0f;
		lighting_pass.render_targets.back().clear_color_value[1] = 0.0f;
		lighting_pass.render_targets.back().clear_color_value[2] = 0.0f;
		lighting_pass.render_targets.back().clear_color_value[3] = 1.0f;

		// ビューポートの設定
		lighting_pass_viewport.TopLeftX = 0.0f;
		lighting_pass_viewport.TopLeftY = 0.0f;
		lighting_pass_viewport.Width = static_cast<float>(width);
		lighting_pass_viewport.Height = static_cast<float>(height);
		lighting_pass_viewport.MinDepth = 0.0f;
		lighting_pass_viewport.MaxDepth = 1.0f;
	}
}

void deffered_lighting_renderer::resize_shadow_buffer(int width, int height)
{
	if (shadow_map_pass_viewport.Width == static_cast<float>(width) && shadow_map_pass_viewport.Height == static_cast<float>(height))
		return;

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	render_targets->shadowmap_depth_texture->get_shader_resource_view()->GetDesc(&srv_desc);
	//	シャドウマップパス生成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	depth_stencil_view_desc.Texture2DArray.ArraySize = srv_desc.Texture2DArray.ArraySize;
	depth_stencil_view_desc.Texture2DArray.FirstArraySlice = 0;
	depth_stencil_view_desc.Texture2DArray.MipSlice = 0;
	shadow_map_pass.depth_stencil = depth_stencil_view(render_targets->shadowmap_depth_texture, &depth_stencil_view_desc);

	// ビューポートの設定
	shadow_map_pass_viewport.TopLeftX = 0.0f;
	shadow_map_pass_viewport.TopLeftY = 0.0f;
	shadow_map_pass_viewport.Width = static_cast<float>(width);
	shadow_map_pass_viewport.Height = static_cast<float>(height);
	shadow_map_pass_viewport.MinDepth = 0.0f;
	shadow_map_pass_viewport.MaxDepth = 1.0f;
}

#if defined(USE_IMGUI)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
debug_draw_pass_renderer::debug_draw_pass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	resize_buffer(0, 0);

	static constexpr std::string_view pso_names[] =
	{
		"debug_draw_line_0",
		"debug_draw_line_1",
	};
	//	パイプラインステート生成
	if (auto psm = pipeline_state_manager::instance())
	{
		int index;
		index = static_cast<int>(debug_draw_primitive_container::line_buffer_id::normal);
		psm->generate_pipeline_state<debug_draw_line_pipeline_state>(pso_names[index], &debug_draw_line_pipeline_states[index]);
		debug_draw_line_pipeline_states[index]->set_domain(pipeline_state_base::domains::system);

		index = static_cast<int>(debug_draw_primitive_container::line_buffer_id::disable_depth_test);
		psm->generate_pipeline_state<debug_draw_line_disable_depth_test_pipeline_state>(pso_names[index], &debug_draw_line_pipeline_states[index]);
		debug_draw_line_pipeline_states[index]->set_domain(pipeline_state_base::domains::system);
	}
}

debug_draw_pass_renderer::~debug_draw_pass_renderer()
{
}

void debug_draw_pass_renderer::render(renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	ライン描画
		for (UINT index = 0; index < static_cast<int>(debug_draw_primitive_container::line_buffer_id::max); ++index)
		{
			debug_draw_line_pipeline_states[index]->begin(context);
			if (!context.render_data->debug_draw_container.line_buffer_datas[index].color_meshs.empty())
			{
				auto& mesh = context.render_data->debug_draw_container.line_buffer_datas[index].color_meshs[0];
				auto it = mesh.vertex_buffer_views->begin();
				UINT stride = static_cast<UINT>(it->second.stride_in_bytes);
				UINT offset = static_cast<UINT>(it->second.offset());
				context.set_vertex_buffers(0, 1, it->second.buffer.GetAddressOf(), &stride, &offset);

				context.device_context->Draw(static_cast<UINT>(it->second.count()), 0);
			}
			debug_draw_line_pipeline_states[index]->end(context);
		}
		rendering_pass.deactivate(context.device_context);
	}, "debug_draw_pass", context, get_render_basis_sort_order());
}

void debug_draw_pass_renderer::resize_buffer(int width, int height)
{
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
	rendering_pass.render_targets.back().clear_color = false;
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	DXGI_SWAP_CHAIN_DESC desc;
	render_data::instance()->get_swap_chain()->GetDesc(&desc);

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(desc.BufferDesc.Width);
	viewport.Height = static_cast<float>(desc.BufferDesc.Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
#endif // defined(USE_IMGUI)
