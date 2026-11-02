#include "Renderer/scene_renderer.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/skybox_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"
#include "PipelineState/post_process_pipeline_state.h"

#include "graphics/clustered_lighting_system.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#endif // defined(USE_IMGUI)

#include "Renderer/scene_renderer_common.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
    void compute_bounding_sphere(const math::vector3f* corners, UINT size, math::vector3f& out_center, float& out_radius)
    {
        math::vector3f sum = math::vector3f::zero();
        for (UINT index = 0; index < size; ++index)
        {
            sum = sum + corners[index];
        }

        math::vector3f center = sum * (1.0f / 8.0f);

        float radius_sq = 0.0f;
        for (UINT index = 0; index < size; ++index)
        {
            float d_sq = (corners[index] - center).square_length();
            radius_sq = std::max(radius_sq, d_sq);
        }

        out_center = center;
        out_radius = sqrtf(radius_sq);
    }

    void compute_frustum_bounding_sphere(float ndc_near, float ndc_far,
                                          const math::matrixf& inverse_view_projection_transform,
                                          math::vector3f& out_center, float& out_radius)
    {
        const math::vector3f corners[8] =
        {
            math::vector3f::transform_coord(math::vector3f(-1.0f, -1.0f, ndc_near), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(+1.0f, -1.0f, ndc_near), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(+1.0f, +1.0f, ndc_near), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(-1.0f, +1.0f, ndc_near), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(-1.0f, -1.0f, ndc_far), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(+1.0f, -1.0f, ndc_far), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(+1.0f, +1.0f, ndc_far), inverse_view_projection_transform),
            math::vector3f::transform_coord(math::vector3f(-1.0f, +1.0f, ndc_far), inverse_view_projection_transform),
        };
		compute_bounding_sphere(corners, 8, out_center, out_radius);
    }

	//	シャドウキャッシュキー生成
	std::uint64_t compute_shadow_signature(const light_render_data& light, const render_data_context& render_data, bool& out_dynamic)
	{
		std::uint64_t hash = 1469598103934665603ull;
		auto mix = [&hash](std::uint64_t value) { hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2); };
		auto mix_quantized = [&mix](double value, double scale) { mix(static_cast<std::uint64_t>(std::llround(value * scale))); };
		auto mix_float = [&mix](float value)
		{
			std::uint32_t bits;
			std::memcpy(&bits, &value, sizeof(bits));
			mix(bits);
		};

		//	ライト
		const math::vector3d& origin = render_data.render_origin;
		mix(static_cast<std::uint64_t>(light.type));
		mix_quantized(origin.x + light.position.x, 100.0);
		mix_quantized(origin.y + light.position.y, 100.0);
		mix_quantized(origin.z + light.position.z, 100.0);
		mix_quantized(light.direction.x, 10000.0);
		mix_quantized(light.direction.y, 10000.0);
		mix_quantized(light.direction.z, 10000.0);
		mix_float(light.range);
		mix_float(light.outer_cone);
		for (const auto& bias : light.bias_data)
		{
			mix_float(bias.depth_bias);
			mix_float(bias.slope_depth_bias_scale);
			mix_float(bias.max_depth_bias);
			mix_float(bias.slope_depth_bias);
			mix_float(bias.max_slope_depth_bias);
			mix_float(bias.sigma_factor);
		}

		//	影響範囲内のメッシュ
		const auto& mesh_container = render_data.mesh_container;
		const float range_square = light.range * light.range;
		for (auto draw_index : mesh_container.shadow_datas)
		{
			const auto& draw_data = mesh_container.draw_datas[draw_index];
			const auto& buffer_data = mesh_container.buffer_datas[draw_data.buffer_index];

			const float dx = (std::max)((std::max)(buffer_data.bounds_min.x - light.position.x, light.position.x - buffer_data.bounds_max.x), 0.0f);
			const float dy = (std::max)((std::max)(buffer_data.bounds_min.y - light.position.y, light.position.y - buffer_data.bounds_max.y), 0.0f);
			const float dz = (std::max)((std::max)(buffer_data.bounds_min.z - light.position.z, light.position.z - buffer_data.bounds_max.z), 0.0f);
			if (dx * dx + dy * dy + dz * dz > range_square)
				continue;

			if (!system_utility::is_bitflag(buffer_data.system_flags, buffer_datas::flags::static_mesh))
				out_dynamic = true;

			mix(buffer_data.unique_id);
			const math::matrixf world(buffer_data.world_transform);
			for (UINT row = 0; row < 3; ++row)
			{
				const math::vector3f axis = world.to_vector3(row);
				mix_quantized(axis.x, 10000.0);
				mix_quantized(axis.y, 10000.0);
				mix_quantized(axis.z, 10000.0);
			}
			const math::vector3f translation = world.to_vector3(3);
			mix_quantized(origin.x + translation.x, 100.0);
			mix_quantized(origin.y + translation.y, 100.0);
			mix_quantized(origin.z + translation.z, 100.0);
		}
		return hash;
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
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass, *renderer_context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass, *renderer_context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->opaque, material::particle_paths::depth_write_pass);
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->mask, material::particle_paths::depth_write_pass);

		rendering_pass.deactivate(context.device_context);
	}, "depth_pre_pass", renderer_context, get_render_basis_sort_order());
}

void depth_prepass_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	描画パス生成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = common_pipeline_states::scene_depth_stencil_view_format;
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
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass, *renderer_context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::model_render_indirect(context, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass, *renderer_context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->opaque, material::particle_paths::g_buffer_pass);
		scene_renderer_commons::particle_render(context, renderer_context.render_particle_indices->mask, material::particle_paths::g_buffer_pass);

		//	スカイライト処理
		for (const auto& light_probe : context.render_data->light_container.light_probes)
		{
			if (!light_probe.is_flag(light_probe::flags::active) || !light_probe.is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			context.diffuse_ibl_texture = light_probe.diffuse_ibl_texture;
			context.specluar_ibl_texture = light_probe.specular_ibl_texture;
			context.lut_ggx_texture = light_probe.lut_ggx_texture;

			//	間接光の情報を設定
			context.deferred_light_probe_constant = {};
			context.deferred_light_probe_constant.light_probe_data = light_probe.data;
			context.deferred_light_probe_constant.light_probe_attribute = light_probe.attribute;

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
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	base_color
			{ 0.0f, 0.0f, 0.0f, common_pipeline_states::gbuffer_max_linear_depth },	//	normal_depth
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	parameter
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	velocity
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	unique_data
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	shade_color [エンジン改造:G-Buffer]
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	ilm_color [エンジン改造:G-Buffer]
		};
		rendering_pass.render_targets.clear();
		for (int i = 0; i < static_cast<int>(magic_enum::enum_count<common_pipeline_states::gbuffer_id>()); ++i)
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
		depth_stencil_view_desc.Format = common_pipeline_states::scene_depth_stencil_view_format;
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
				math::float3 center;
				center.x = context.scene_constant.camera_position.x;
				center.y = context.scene_constant.camera_position.y;
				center.z = context.scene_constant.camera_position.z;
				sphere = collision::bounding_sphere(math::double3(center), context.scene_constant.camera_clip_distance.x * 1.01f);
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
					const math::matrixf W = math::matrixf::scaling(decal.scale.x, decal.scale.y, decal.scale.z)
										* math::matrixf::rotation_quaternion(decal.rotation.to_quaternion())
										* math::matrixf::translation(decal.position.x, decal.position.y, decal.position.z);
					//	プリミティブバッファ更新
					{
						common_pipeline_states::decal_primitive_constants dpc;
						dpc.decal_transform = W.to_float4x4();
						auto front = decal.rotation.up().to_float3();
						dpc.decal_direction.x = front.x;
						dpc.decal_direction.y = front.y;
						dpc.decal_direction.z = front.z;
						dpc.decal_direction.w = 0;
						dpc.decal_inverse_transform = W.inverted().to_float4x4();
						context.device_context->UpdateSubresource(decal_primitive_primitive_buffer.Get(), 0, 0, &dpc, 0, 0);
					}
					buffer_data.primitive_buffer = decal_primitive_primitive_buffer;
				}

				//	マテリアル更新

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
		for (int i = 0; i < static_cast<int>(magic_enum::enum_count<common_pipeline_states::gbuffer_id>()); ++i)
		{
			rendering_pass.render_targets.emplace_back(render_targets->g_buffer_textures[i]);
			auto& rtv = rendering_pass.render_targets.back();
			rtv.clear_color = false;
		}
#endif
		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = common_pipeline_states::scene_depth_stencil_view_format;
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

	//	影を落とす点光源/スポットライトにシャドウマップアトラスを割り当てる
	struct atlas_slot
	{
		UINT first_slice = 0;
		bool needs_render = true;
	};
	std::unordered_map<const light_render_data*, atlas_slot> atlas_slots;
	atlas.begin_frame();
	if (shadow_atlas::is_enable())
	{
		struct atlas_candidate
		{
			const light_render_data* light;
			float priority;
		};
		std::vector<atlas_candidate> candidates;
		const math::vector3f camera(context.scene_constant.camera_position.x, context.scene_constant.camera_position.y, context.scene_constant.camera_position.z);
		for (const auto& light : context.render_data->light_container.lights)
		{
			if (!light.is_flag(light::flags::active) || !light.is_flag(light::flags::use_shadow) || clustered_lighting_system::is_target(light))
				continue;
			if (light.type != light_type::point && light.type != light_type::spot)
				continue;
			const float distance = (math::vector3f(light.position) - camera).length();
			candidates.push_back({ &light, light.range / (std::max)(distance, light.range * 0.1f) });
		}
		std::stable_sort(candidates.begin(), candidates.end(), [](const atlas_candidate& l, const atlas_candidate& r) { return l.priority > r.priority; });

		const UINT max_lights = shadow_atlas::get_max_lights();
		for (const auto& candidate : candidates)
		{
			if (atlas.get_allocated_lights() >= max_lights)
				break;

			bool dynamic = false;
			const std::uint64_t signature = compute_shadow_signature(*candidate.light, *context.render_data, dynamic);
			atlas_slot slot;
			if (atlas.acquire(candidate.light->id, candidate.light->type == light_type::point ? 6u : 1u, signature, dynamic, slot.first_slice, slot.needs_render))
				atlas_slots[candidate.light] = slot;
		}
	}
	atlas.end_frame();

	//	ライティング処理
	for (const auto& light : context.render_data->light_container.lights)
	{
		if (!light.is_flag(light::flags::active))
			continue;

		//	クラスタードライティング対象は後でまとめて描画する
		if (clustered_lighting_system::is_target(light))
			continue;

		switch (light.type)
		{
			case light_type::directional:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				if (light.is_flag(light::flags::use_shadow))
				{
					render_directional_shadow_map(light, shadow_map_constant, context);
				}

				//	ライティング処理
				render_directional(light, shadow_map_constant, context);
				break;
			}
			case light_type::point:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				const auto atlas_it = atlas_slots.find(&light);
				const int atlas_slice = (atlas_it != atlas_slots.end()) ? static_cast<int>(atlas_it->second.first_slice) : -1;
				const bool render_shadow = (atlas_it == atlas_slots.end()) || atlas_it->second.needs_render;
				if (light.is_flag(light::flags::use_shadow))
				{
					render_point_shadow_map(light, shadow_map_constant, context, atlas_slice, render_shadow);
				}

				//	ライティング処理
				render_point(light, shadow_map_constant, context, atlas_slice >= 0);
				break;
			}
			case light_type::spot:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				const auto atlas_it = atlas_slots.find(&light);
				const int atlas_slice = (atlas_it != atlas_slots.end()) ? static_cast<int>(atlas_it->second.first_slice) : -1;
				const bool render_shadow = (atlas_it == atlas_slots.end()) || atlas_it->second.needs_render;
				if (light.is_flag(light::flags::use_shadow))
				{
					render_spot_shadow_map(light, shadow_map_constant, context, atlas_slice, render_shadow);
				}

				//	ライティング処理
				render_spot(light, shadow_map_constant, context, atlas_slice >= 0);
				break;
			}
		}
	}

	//	影無しの点光源/スポットライト
	render_clustered(context);
}

void deffered_lighting_renderer::render_clustered(renderer_context& context)
{
	if (!context.current_view_state)
		return;

	const std::shared_ptr<clustered_lighting_system> clustered_lighting = context.current_view_state->get_clustered_lighting();
	if (!clustered_lighting)
		return;

	//	対象ライトの収集とバッファ準備はメインスレッドで行う
	clustered_lighting->setup_frame(*context.render_data, context.scene_constant);

	job_executor.execute([this, clustered_lighting](graphics_context& context)
	{
		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < magic_enum::enum_count<common_pipeline_states::gbuffer_id>(); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	クラスター構築
		clustered_lighting->build(context);
		if (clustered_lighting->get_light_count() == 0 && !clustered_lighting_system::is_visualize())
			return;

		//	描画先設定
		lighting_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);

		//	クラスター内のライトでライティング
		clustered_lighting->render(context);

		lighting_pass.deactivate(context.device_context);
	}, "render_clustered_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_indirect(renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < magic_enum::enum_count<common_pipeline_states::gbuffer_id>(); ++index)
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
		for (const auto& light_probe : context.render_data->light_container.light_probes)
		{
			if (!light_probe.is_flag(light_probe::flags::active))
				continue;

			//	間接光の情報を設定
			context.deferred_light_probe_constant = {};
			context.deferred_light_probe_constant.light_probe_data = light_probe.data;
			context.deferred_light_probe_constant.light_probe_attribute = light_probe.attribute;

			if (light_probe.is_flag(light_probe::flags::unbind))
			{
				//	全画面描画
				//	IBLテクスチャ設定
				deferred_lighting_material->set_texture("diffuse_iem", light_probe.diffuse_ibl_texture);
				deferred_lighting_material->set_texture("specular_pmrem", light_probe.specular_ibl_texture);
				deferred_lighting_material->set_texture("lut_ggx", light_probe.lut_ggx_texture);

				auto pipeline_state =
					deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_indirect);

				pipeline_state->begin(context);
				pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
				scene_renderer_commons::draw(context, pipeline_state, 4);
				pipeline_state->end(context);
			}
			else
			{
				//	範囲制限をかける
			}
		}

		lighting_pass.deactivate(context.device_context);
	}, "render_indirect_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_directional(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	job_executor.execute([this, light, shadow_map_constant](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		//	コンテキストに情報登録
		context.deferred_light_constant.light_data = light.data;
		context.shadow_map_texture = render_targets->shadowmap_depth_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < magic_enum::enum_count<common_pipeline_states::gbuffer_id>(); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_directional);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			scene_renderer_commons::draw(context, pipeline_state, 4);
			pipeline_state->end(context);
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_directional_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_directional_shadow_map(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context)
{
	math::float4 rotation = light.rotation.to_quaternion().to_float4();
	math::matrixf CameraTransformMatrix = math::matrixf(context.scene_constant.view_transform).inverted();
	math::matrixf LightTransformMatrix = math::matrixf::rotation_quaternion(math::quaternionf(rotation));
	math::matrixf InverseProjectionTransform;
	InverseProjectionTransform = math::matrixf(context.scene_constant.inverse_projection_transform);

	math::vector3f LightRight, LightUp, LightFront, LightPosition;
	LightRight = LightTransformMatrix.to_vector3(0);
	LightUp = LightTransformMatrix.to_vector3(1);
	LightFront = LightTransformMatrix.to_vector3(2);
	LightPosition = LightTransformMatrix.to_vector3(3);

	const math::vector3d& render_origin = context.render_data->render_origin;
	const double origin_x = math::vector3d::dot(render_origin, math::vector3d(LightRight));
	const double origin_y = math::vector3d::dot(render_origin, math::vector3d(LightUp));

	//	クリップ距離を算出
	float near_clip = context.scene_constant.camera_clip_distance.x;
	float camera_far_clip = context.scene_constant.camera_clip_distance.y;

	//	シーンのAABBを算出
	math::float3 scene_min = {FLT_MAX, FLT_MAX, FLT_MAX}, scene_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
	{
		auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
		auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();
		for (auto& draw_index : context.render_data->mesh_container.shadow_datas)
		{
			auto draw_data = draw_data_ptr + draw_index;
			auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
			scene_min = math::min(math::vector3f(buffer_data->bounds_min), math::vector3f(scene_min)).to_float3();
			scene_max = math::max(math::vector3f(buffer_data->bounds_max), math::vector3f(scene_max)).to_float3();
		}
	}
	const bool has_scene_aabb = (scene_min.x <= scene_max.x) && (scene_min.y <= scene_max.y) && (scene_min.z <= scene_max.z);

	//	ライト空間上でのシーンAABBのZ範囲を取得
	float scene_min_z = 0.0f, scene_max_z = 0.0f;
	if (has_scene_aabb)
	{
		math::vector3f Min(scene_min);
		math::vector3f Max(scene_max);
		math::vector3f Center = (Min + Max) * 0.5f;
		math::vector3f Extent = (Max - Min) * 0.5f;
		float center_proj = math::vector3f::dot(Center, LightFront);
		float extent_proj = math::vector3f::dot(Extent, math::abs(LightFront));
		scene_min_z = center_proj - extent_proj;
		scene_max_z = center_proj + extent_proj;
	}

	//	CSM用に遠クリップ面に対して距離制限をかける
	float far_clip = std::max(near_clip, std::min(light.csm_distance, camera_far_clip));
	float projection_near_clip = scene_renderer_commons::convert_view_depth_to_projection_depth(near_clip, near_clip, camera_far_clip);
	float projection_far_clip = scene_renderer_commons::convert_view_depth_to_projection_depth(far_clip, near_clip, camera_far_clip);

	//	シャドウマップ情報算出
	shadow_map_constant.shadow_map_count = directional_shadow_map_count;
	shadow_map_constant.shadow_attenuation = light.shadow_attenuation;
	shadow_map_constant.shadow_mask = 0;
	collision::frustum_culling frustum_culling;
	{
		math::matrixf InverseViewProjectionTransform;
		InverseViewProjectionTransform = math::matrixf(context.scene_constant.inverse_view_projection_transform);
		//	視錐台を囲むライト空間上の行列を生成
		math::matrixf LightViewMatrix, LightProjectionMatrix;
		float top_min_distance, top_max_distance;	//	最大深度の範囲
		{
			//	AABBを囲む球体を算出
			math::vector3f Center;
			float radius;
			if (has_scene_aabb)
			{
				math::vector3f scene_corners[8];
				scene_corners[0] = math::vector3f(scene_min.x, scene_min.y, scene_min.z);
				scene_corners[1] = math::vector3f(scene_max.x, scene_min.y, scene_min.z);
				scene_corners[2] = math::vector3f(scene_min.x, scene_max.y, scene_min.z);
				scene_corners[3] = math::vector3f(scene_max.x, scene_max.y, scene_min.z);
				scene_corners[4] = math::vector3f(scene_min.x, scene_min.y, scene_max.z);
				scene_corners[5] = math::vector3f(scene_max.x, scene_min.y, scene_max.z);
				scene_corners[6] = math::vector3f(scene_min.x, scene_max.y, scene_max.z);
				scene_corners[7] = math::vector3f(scene_max.x, scene_max.y, scene_max.z);
				compute_bounding_sphere(scene_corners, 8, Center, radius);
			}
			else
			{
				//	フラスタムから生成
				compute_frustum_bounding_sphere(projection_near_clip, projection_far_clip, InverseViewProjectionTransform, Center, radius);
			}

			//	視線行列算出
			{
				LightPosition = Center - (LightFront * radius);
				LightTransformMatrix = math::matrixf(math::vector4f(LightRight, 0.0f), math::vector4f(LightUp, 0.0f), math::vector4f(LightFront, 0.0f), math::vector4f(LightPosition, 1.0f));
				LightViewMatrix = LightTransformMatrix.inverted();
			}

			//	投影行列生成
			{
				top_min_distance = 0.0f;
				top_max_distance = top_min_distance + radius * 2.0f;
				LightProjectionMatrix = math::matrixf::orthographic_offset_center(-radius, +radius, -radius, +radius, top_min_distance, top_max_distance);
			}

			//	フラスタムカリング生成
			{
				math::float4x4 frustum_culling_light_view_projection_transform;
				frustum_culling_light_view_projection_transform = (LightViewMatrix * LightProjectionMatrix).to_float4x4();
				frustum_culling.create_planes(frustum_culling_light_view_projection_transform);
				frustum_culling.create_frustum_corners(math::matrixf(frustum_culling_light_view_projection_transform).inverted());
			}
		}

		//	PSSM
		std::vector<float> split_distance;
		split_distance.resize(directional_shadow_map_count + 1);
		{
			float split_weight = light.pssm_split_weight;
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
			//	AABBを囲む球体を算出
			math::vector3f Center;
			float radius;
			compute_frustum_bounding_sphere(scene_renderer_commons::convert_view_depth_to_projection_depth(split_distance[index + 0], near_clip, camera_far_clip),
											scene_renderer_commons::convert_view_depth_to_projection_depth(split_distance[index + 1], near_clip, camera_far_clip),
											InverseViewProjectionTransform, Center, radius);
			radius *= 1.0001f;

			//	各行列調整
			{
				//	Stable CSM
				float texel_size_x = (radius * 2.0f) / shadow_map_pass_viewport.Width;
				float texel_size_y = (radius * 2.0f) / shadow_map_pass_viewport.Height;
				float center_x = math::vector3f::dot(Center, LightRight);
				float center_y = math::vector3f::dot(Center, LightUp);
				float center_z = math::vector3f::dot(Center, LightFront);

				//	絶対座標でスナップしてから描画原点からの相対値に戻す
				const double snapped_x = std::floor((origin_x + center_x) / texel_size_x) * texel_size_x;
				const double snapped_y = std::floor((origin_y + center_y) / texel_size_y) * texel_size_y;
				float X = static_cast<float>(snapped_x - origin_x);
				float Y = static_cast<float>(snapped_y - origin_y);

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

				math::vector3f Origin = ((LightRight * X) + (LightUp * Y)) + (LightFront * near_z);

				LightTransformMatrix = math::matrixf(math::vector4f(LightRight, 0.0f), math::vector4f(LightUp, 0.0f), math::vector4f(LightFront, 0.0f), math::vector4f(Origin, 1.0f));
				math::matrixf LightViewMatrix = LightTransformMatrix.inverted();

				//	投影行列生成
				float ortho_near = 0.0f;
				float ortho_far  = std::max(far_z - near_z, 0.0001f);
				const math::matrixf LightProjectionMatrix = math::matrixf::orthographic_offset_center(-radius, +radius, -radius, +radius, ortho_near, ortho_far);
				shadow_map_constant.light_view_projection_transform[index] = (LightViewMatrix * LightProjectionMatrix).to_float4x4();
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
		for (UINT index = 0; index < magic_enum::enum_count<common_pipeline_states::gbuffer_id>(); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &shadow_map_pass_viewport);

		//	モデル類描画
		context.is_shadow_rendering = true;
		scene_renderer_commons::model_render_shadow_batched(context, context.render_data->mesh_container.shadow_datas, frustum_culling, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		shadow_map_pass.deactivate(context.device_context);
	}, "render_directional_shadow_map_pass", context, get_render_basis_sort_order() + register_index++, true);
}

void deffered_lighting_renderer::render_point(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, bool use_atlas)
{
	//	シャドウマップアトラスに描いた場合はアトラスを参照する
	const texture_ptr shadow_texture = use_atlas ? atlas.get_texture() : render_targets->shadowmap_depth_texture;
	job_executor.execute([this, light, shadow_map_constant, shadow_texture](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		context.deferred_light_constant.light_data = light.data;
		context.shadow_map_texture = shadow_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < magic_enum::enum_count<common_pipeline_states::gbuffer_id>(); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_point);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			scene_renderer_commons::draw(context, pipeline_state, 4);
			pipeline_state->end(context);
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_point_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::begin_shadow_target(graphics_context& context, ID3D11DepthStencilView* atlas_depth_stencil_view)
{
	if (atlas_depth_stencil_view)
	{
		context.device_context->OMSetRenderTargets(0, nullptr, atlas_depth_stencil_view);
		context.device_context->ClearDepthStencilView(atlas_depth_stencil_view, D3D11_CLEAR_DEPTH, common_pipeline_states::depth_clear_value, 0);
	}
	else
	{
		shadow_map_pass.activate(context.device_context);
	}
}

void deffered_lighting_renderer::end_shadow_target(graphics_context& context, ID3D11DepthStencilView* atlas_depth_stencil_view)
{
	if (atlas_depth_stencil_view)
		context.device_context->OMSetRenderTargets(0, nullptr, nullptr);
	else
		shadow_map_pass.deactivate(context.device_context);
}
void deffered_lighting_renderer::render_point_shadow_map(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, int atlas_slice, bool render_shadow)
{
	math::float3 position = light.position;
	math::vector3f position_vector(position);
	float range = light.range;

	//	キューブマップ6面分の視線方向・上方向(0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z)
	static const math::float3 cube_face_directions[6] =
	{
		{ +1.0f,  0.0f,  0.0f },
		{ -1.0f,  0.0f,  0.0f },
		{  0.0f, +1.0f,  0.0f },
		{  0.0f, -1.0f,  0.0f },
		{  0.0f,  0.0f, +1.0f },
		{  0.0f,  0.0f, -1.0f },
	};
	static const math::float3 cube_face_ups[6] =
	{
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 0.0f, +1.0f },
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 1.0f,  0.0f },
	};

	const math::matrixf ProjectionMatrix = math::matrixf::perspective_fov(math::constants<float>::pi_half, 1.0f, 1.0f, range);

	//	定数バッファ設定
	shadow_map_constant.shadow_map_count = 6;
	shadow_map_constant.shadow_attenuation = light.shadow_attenuation;
	shadow_map_constant.shadow_mask = 0;
	shadow_map_constant.shadow_slice_offset = (atlas_slice >= 0) ? static_cast<UINT>(atlas_slice) : 0;

	for (UINT face = 0; face < 6; ++face)
	{
		math::vector3f direction_vector(cube_face_directions[face]);
		math::vector3f up_vector(cube_face_ups[face]);
		const math::matrixf ViewMatrix = math::matrixf::look_at(position_vector, position_vector + direction_vector, up_vector);
		shadow_map_constant.light_view_projection_transform[face] = (ViewMatrix * ProjectionMatrix).to_float4x4();

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

	//	影のキャッシュが使える場合は描き直さない
	if (!render_shadow)
		return;

	//	シャドウマップアトラスに描く場合は割り当てられたスライス範囲に書き込む
	ID3D11DepthStencilView* atlas_depth_stencil_view = (atlas_slice >= 0) ? atlas.get_depth_stencil_view(static_cast<UINT>(atlas_slice), 6) : nullptr;
	const D3D11_VIEWPORT atlas_viewport = atlas.get_viewport();
	job_executor.execute([this, shadow_map_constant, position, range, atlas_depth_stencil_view, atlas_viewport](graphics_context& context)
	{
		begin_shadow_target(context, atlas_depth_stencil_view);

		context.shadow_map_constant = shadow_map_constant;

		context.device_context->RSSetViewports(1, atlas_depth_stencil_view ? &atlas_viewport : &shadow_map_pass_viewport);

		context.is_shadow_rendering = true;
		scene_renderer_commons::model_render_shadow_batched(context, context.render_data->mesh_container.shadow_datas, position, range, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		end_shadow_target(context, atlas_depth_stencil_view);
	}, "render_point_shadow_map_pass", context, get_render_basis_sort_order() + register_index++, true);
}



void deffered_lighting_renderer::render_spot(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, bool use_atlas)
{
	//	シャドウマップアトラスに描いた場合はアトラスを参照する
	const texture_ptr shadow_texture = use_atlas ? atlas.get_texture() : render_targets->shadowmap_depth_texture;
	job_executor.execute([this, light, shadow_map_constant, shadow_texture](graphics_context& context)
	{
		//	描画先設定
		lighting_pass.activate(context.device_context);

		context.deferred_light_constant.light_data = light.data;
		context.shadow_map_texture = shadow_texture;
		context.shadow_map_constant = shadow_map_constant;

		//	G-Bufferテクスチャ設定
		for (UINT index = 0; index < magic_enum::enum_count<common_pipeline_states::gbuffer_id>(); ++index)
		{
			context.g_buffer_textures[index] = render_targets->g_buffer_textures[index];
		}

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &lighting_pass_viewport);
		{
			auto pipeline_state =
				deferred_lighting_material->get_pipeline_state(material::system_paths::deferred_spot);

			pipeline_state->begin(context);
			pipeline_state->update_parameters(context, *deferred_lighting_material, nullptr);
			scene_renderer_commons::draw(context, pipeline_state, 4);
			pipeline_state->end(context);
		}
		lighting_pass.deactivate(context.device_context);
	}, "render_spot_pass", context, get_render_basis_sort_order() + register_index++);
}

void deffered_lighting_renderer::render_spot_shadow_map(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, int atlas_slice, bool render_shadow)
{
	auto reserve_scene_constant = context.scene_constant;

	math::float4 rotation = light.rotation.to_quaternion().to_float4();
	math::float3 position = light.position;
	math::matrixf TransformMatrix, ViewMatrix, ProjectionMatrix;
	TransformMatrix = math::matrixf::rotation_quaternion(math::quaternionf(rotation))
					* math::matrixf::translation(position.x, position.y, position.z);
	ViewMatrix = TransformMatrix.inverted();
	ProjectionMatrix = math::matrixf::perspective_fov(light.outer_cone * 2.0f, 1.0f, 1.0f, light.range);

	//	定数バッファ情報設定
	shadow_map_constant.shadow_map_count = 1;
	shadow_map_constant.shadow_attenuation = light.shadow_attenuation;
	shadow_map_constant.shadow_split_distance[0].x = -1.0f;
	shadow_map_constant.shadow_split_distance[0].y = -1.0f;
	shadow_map_constant.shadow_split_distance[0].z = FLT_MAX;
	shadow_map_constant.shadow_split_distance[0].w = FLT_MAX;

	//	バイアス納入
	{
		//	ライトの範囲で処理
		float near_clip = 0.1f;
		float far_clip = near_clip + light.range;

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
	//	シャドウマップアトラスの先頭スライス
	shadow_map_constant.shadow_slice_offset = (atlas_slice >= 0) ? static_cast<UINT>(atlas_slice) : 0;
	shadow_map_constant.light_view_projection_transform[0] = (ViewMatrix * ProjectionMatrix).to_float4x4();

	//	モデル類描画
	collision::frustum_culling frustum_culling;
	{
		frustum_culling.create_planes(shadow_map_constant.light_view_projection_transform[0]);
		//	シャドウマップアトラスではカメラに依存しないよう、ライト自身の視錐台の頂点を使う
		if (atlas_slice >= 0)
			frustum_culling.create_frustum_corners(math::matrixf(shadow_map_constant.light_view_projection_transform[0]).inverted());
		else
			frustum_culling.create_frustum_corners(math::matrixf(context.scene_constant.view_projection_transform).inverted());
	}

	//	影のキャッシュが使える場合は描き直さない
	if (!render_shadow)
		return;

	//	シャドウマップアトラスに描く場合は割り当てられたスライス範囲に書き込む
	ID3D11DepthStencilView* atlas_depth_stencil_view = (atlas_slice >= 0) ? atlas.get_depth_stencil_view(static_cast<UINT>(atlas_slice), 1) : nullptr;
	const D3D11_VIEWPORT atlas_viewport = atlas.get_viewport();
	job_executor.execute([this, shadow_map_constant, frustum_culling, atlas_depth_stencil_view, atlas_viewport](graphics_context& context)
	{
		begin_shadow_target(context, atlas_depth_stencil_view);

		context.shadow_map_constant = shadow_map_constant;

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, atlas_depth_stencil_view ? &atlas_viewport : &shadow_map_pass_viewport);

		context.is_shadow_rendering = true;
		scene_renderer_commons::model_render_shadow_batched(context, context.render_data->mesh_container.shadow_datas, frustum_culling, material::surface_paths::shadow_write_pass);
		context.is_shadow_rendering = false;

		end_shadow_target(context, atlas_depth_stencil_view);
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
	depth_stencil_view_desc.Format = common_pipeline_states::shadow_map_depth_stencil_view_format;
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
		for (UINT index = 0; index < magic_enum::enum_count<debug_draw_primitive_container::line_buffer_id>(); ++index)
		{
			debug_draw_line_pipeline_states[index]->begin(context);
			if (!context.render_data->debug_draw_container.line_buffer_datas[index].color_meshs.empty())
			{
				auto& mesh = context.render_data->debug_draw_container.line_buffer_datas[index].color_meshs[0];
				auto it = mesh.vertex_buffer_views->begin();
				context.set_vertex_streams(1, it->second.srv.GetAddressOf());

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
	depth_stencil_view_desc.Format = common_pipeline_states::scene_depth_stencil_view_format;
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
