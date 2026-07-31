#include "Renderer/scene_renderer_forward.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/deferred_lighting_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"

#include "PipelineState/post_process_pipeline_state.h"

namespace
{
	void draw(const graphics_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0)
	{
		buffer_datas buffer_data;
		buffer_data.vertex_count = vertex_count;
		context.device_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.device_context->IASetPrimitiveTopology(primitive_topology);
		context.device_context->Draw(vertex_count, vertex_offset);
	}

	void draw_indexed(const graphics_context& context,
					  float mesh_length,
					  const buffer_datas& buffer_data,
					  graphics_pipeline_state_ptr pipeline_state)
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views = nullptr;
		const buffer_view* index_buffer_view = nullptr;
		D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		if (context.is_shadow_rendering)
		{
			vertex_buffer_views = buffer_data.shadow_mesh.vertex_buffer_views;
			index_buffer_view = buffer_data.shadow_mesh.index_buffer_view;
			primitive_topology = buffer_data.shadow_mesh.primitive_topology;
		}
		else
		{
			//	LOD対応
			UINT lod_level = buffer_data.color_meshs.size();
			float lod_range_min = buffer_data.lod_range_rate.x * context.scene_constant.camera_clip_distance.x;
			float lod_range_max = buffer_data.lod_range_rate.y * context.scene_constant.camera_clip_distance.y;
			float lod_rate = (mesh_length - lod_range_min) / (lod_range_max - lod_range_min);
			auto lod_mesh_index = math::lerp(0u, lod_level - 1, math::saturate(lod_rate));
			auto* draw_mesh = &buffer_data.color_meshs[lod_mesh_index];
			vertex_buffer_views = draw_mesh->vertex_buffer_views;
			index_buffer_view = draw_mesh->index_buffer_view;
			primitive_topology = draw_mesh->primitive_topology;
		}

		//	頂点ストリーム更新
		UINT stream_index = 0;
		for (auto& semantic : pipeline_state->get_semantic_list())
		{
			auto it = vertex_buffer_views->find(semantic.first);
			if (it != vertex_buffer_views->end())
			{
				auto& buffer_data = it->second;
				UINT offset = static_cast<UINT>(buffer_data.offset());
				UINT stride = static_cast<UINT>(buffer_data.stride_in_bytes);
				context.device_context->IASetVertexBuffers(stream_index, 1, buffer_data.buffer.GetAddressOf(), &stride, &offset);
			}
			else
			{
				//	空のオブジェクトを納入しておく
				UINT offset = 0;
				UINT stride = 0;
				ID3D11Buffer* buffer = nullptr;
				context.device_context->IASetVertexBuffers(stream_index, 1, &buffer, &stride, &offset);
			}
			++stream_index;
		}

		context.device_context->IASetIndexBuffer(index_buffer_view->buffer.Get(), index_buffer_view->format,
													static_cast<UINT>(index_buffer_view->offset()));
		context.device_context->IASetPrimitiveTopology(primitive_topology);

		context.device_context->DrawIndexed(static_cast<UINT>(index_buffer_view->count()), 0, 0);

	}

	void model_render(graphics_context& context, const std::vector<std::size_t>& mesh_indices, const std::vector<float>& mesh_lengths)
	{
		auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
		auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
		auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

		//	モデル類描画
		graphics_pipeline_state_ptr current_pipeline_state;
		for (auto& draw_index : mesh_indices)
		{
			auto draw_data = draw_data_ptr + draw_index;
			auto material_data = material_data_ptr + draw_data->material_index;

			if (auto pipeline_state = material_data->material->get_pipeline_state(material::surface_paths::forward_pass))
			{
				if (pipeline_state != current_pipeline_state)
				{
					if (current_pipeline_state)
						current_pipeline_state->end(context);

					current_pipeline_state = pipeline_state;
					current_pipeline_state->begin(context);
				}

				auto buffer_data = buffer_data_ptr + draw_data->buffer_index;

				material_data->material->update_parameters(context);
				current_pipeline_state->update_parameters(context, *material_data->material, buffer_data);
				draw_indexed(context, mesh_lengths[draw_index], *buffer_data, current_pipeline_state);
			}
		}
		if (current_pipeline_state)
			current_pipeline_state->end(context);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
before_dof_forward_renderer::before_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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
}

before_dof_forward_renderer::~before_dof_forward_renderer()
{
}

void before_dof_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::before_dof);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass : before_dof", renderer_context, get_render_basis_sort_order());
}

void before_dof_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
after_dof_forward_renderer::after_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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
}

after_dof_forward_renderer::~after_dof_forward_renderer()
{
}

void after_dof_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::after_dof);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass : after_dof", renderer_context, get_render_basis_sort_order());
}

void after_dof_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
after_motion_blur_forward_renderer::after_motion_blur_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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
}

after_motion_blur_forward_renderer::~after_motion_blur_forward_renderer()
{
}

void after_motion_blur_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::after_motion_blur);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass: after_motion_blur", renderer_context, get_render_basis_sort_order());
}

void after_motion_blur_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
particle_effect_renderer::particle_effect_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
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

	//	パーティクル準備
	{
		//	統合して使えるコンピュートパーティクルシステム生成
		UINT emitter_max = 128;
		UINT emitter_data_size = sizeof(DirectX::XMFLOAT4X4) * 6;
		UINT particle_max = 100000;
		UINT particle_data_size = sizeof(DirectX::XMFLOAT4) * 32;
		particle_system = std::make_unique<compute_particle_system>(render_data::instance()->get_device().Get(),
			emitter_max,
			emitter_data_size,
			particle_max,
			particle_data_size);

		//	テスト用にスプライトパーティクルエミッター登録
		{
			particle_system_item::emitter_resource emitter_resource;
			//	エミッター更新アイテムの最大登録アイテムサイズ(1Fの更新における)
			emitter_resource.system.max_update_item_size =
				sizeof(particle_system_item::emitter::common) +
				sizeof(particle_system_item::emitter::sprite_animation) +
				sizeof(particle_system_item::emitter::velocity);

			//	エミッター毎のパーティクル生成最大サイズ
			emitter_resource.system.total_particle_count = 0x8FFF;

			//	パーティクル型サイズ
			emitter_resource.system.particle_type_size =
				sizeof(particle_system_item::particle::common) +
				sizeof(particle_system_item::particle::velocity);

			//	エミッター毎の描画タイプ
			emitter_resource.render.renderer_type = particle_system_item::renderer_types::sprite;
			emitter_resource.render.sampler_state = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR_CLAMP);
			
			texture_manager::instance()->load_texture_from_file(emitter_resource.render.texture, "Assets/Resources/Textures/Explosion01_5x5.png");

			//	アイテムはリソース内で管理できると吉
			emitter_datas[Bomb].emitter_index = particle_system->register_emitter(render_data::instance()->get_device().Get(), emitter_resource);
		}

		//	テスト用にリボンパーティクルエミッター登録
		{
			particle_system_item::emitter_resource emitter_resource;
			//	エミッター更新アイテムの最大登録アイテムサイズ(1Fの更新における)
			emitter_resource.system.max_update_item_size =
				sizeof(particle_system_item::emitter::common) +
				sizeof(particle_system_item::emitter::trail) +
				sizeof(particle_system_item::emitter::velocity);

			//	エミッター毎のパーティクル生成最大サイズ
			emitter_resource.system.total_particle_count = 0x1FFF;

			//	パーティクル型サイズ
			emitter_resource.system.particle_type_size =
				sizeof(particle_system_item::particle::common) +
				sizeof(particle_system_item::particle::trail_header) +
				sizeof(particle_system_item::particle::trail_body) * TRAIL_BODY_MAX +
				sizeof(particle_system_item::particle::velocity);

			//	エミッター毎の描画タイプ
			emitter_resource.render.renderer_type = particle_system_item::renderer_types::ribon;
			emitter_resource.render.trail_count = TRAIL_BODY_MAX;
			emitter_resource.render.sampler_state = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR_CLAMP);

			texture_manager::instance()->load_texture_from_file(emitter_resource.render.texture, "Assets/Resources/Textures/trail.png");

			//	アイテムはリソース内で管理できると吉
			emitter_datas[Trail].emitter_index = particle_system->register_emitter(render_data::instance()->get_device().Get(), emitter_resource);
			emitter_datas[Trail].translation.y = 30;
		}
	}
}

particle_effect_renderer::~particle_effect_renderer()
{
}

void particle_effect_renderer::render(renderer_context& renderer_context)
{
	//	爆発処理
	if (emitter_datas[Bomb].emitter_index != 0xFFFFFFFF)
	{
		auto& emitter_data = emitter_datas[Bomb];
		particle_system_item::emitter::common common_data;
		{
			//	パーティクル生成情報
			common_data.emitter_state;

			//	エミッター姿勢情報
			common_data.scale = emitter_data.scale;
			DirectX::XMStoreFloat4(&common_data.rotation,
				DirectX::XMQuaternionRotationRollPitchYaw(emitter_data.rotation.x, emitter_data.rotation.y, emitter_data.rotation.z));
			common_data.translation = emitter_data.translation;

			//	パーティクル発生情報
			common_data.spawn_life_time_range = { 10.5f, 20.5f };
			common_data.spawn_scale_range = { 30.0f, 60.0f };
			if ((::GetAsyncKeyState('X') & 0x8000))
				common_data.spawn_count = 40;
			else
				common_data.spawn_count = 0;

			common_data.spawn_area_min = { -40, 0, -40 };
			common_data.spawn_area_max = { +40, 0, +40 };
			common_data.gravity = { 0.0f, -9.8f, 0.0f };
			common_data.gravity = { 0.0f, 0.0f, 0.0f };
		}

		//	エミッター更新情報登録
		compute_particle_system::update_emitter_data data;

		//	各種アイテム登録
		{
			UINT offset = 0;
			//	スプライトアニメーションアイテム登録
			{
				particle_system_item::emitter::sprite_animation sprite_animation;
				sprite_animation.split_count = { 5, 5 };
				sprite_animation.increment_time = 1.0f / 2;
				UINT write_position = static_cast<UINT>(data.item_binary.size());
				UINT write_size = sizeof(particle_system_item::emitter::sprite_animation);
				data.item_binary.resize(write_position + write_size);
				memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
					&sprite_animation, sizeof(sprite_animation));
				common_data.active_items |= 1 << particle_system_item::emitter::sprite_animation::data_id;
			}

			//	速度アイテム登録
			{
				particle_system_item::emitter::velocity velocity;
				velocity.angle_range = { 0.5f, 1.5f };
				velocity.direction.x = static_cast<float>(rand() % 100 - 50) * 0.01f;
				velocity.direction.y = static_cast<float>(rand() % 100 - 50) * 0.01f;
				velocity.direction.z = static_cast<float>(rand() % 100 - 50) * 0.01f;
				float length = 0;
				length = sqrtf(velocity.direction.x * velocity.direction.x +
					velocity.direction.y * velocity.direction.y +
					velocity.direction.z * velocity.direction.z);
				velocity.direction.x /= length;
				velocity.direction.y /= length;
				velocity.direction.z /= length;
				velocity.power_range = { 2.5f, 5.0f };
				velocity.acceleration.x = static_cast<float>(rand() % 100 - 50) * 0.1f;
				velocity.acceleration.y = static_cast<float>(rand() % 100 - 50) * 0.1f;
				velocity.acceleration.z = static_cast<float>(rand() % 100 - 50) * 0.1f;
				length = sqrtf(velocity.acceleration.x * velocity.acceleration.x +
					velocity.acceleration.y * velocity.acceleration.y +
					velocity.acceleration.z * velocity.acceleration.z);
				velocity.acceleration.x /= length;
				velocity.acceleration.y /= length;
				velocity.acceleration.z /= length;
				if (emitter_data.emitter_index == 0)
				{
					velocity.angle_range = { 0.0f, 0.5f };
					velocity.power_range = { 0, 0.1f };
				}

				UINT write_position = static_cast<UINT>(data.item_binary.size());
				UINT write_size = sizeof(particle_system_item::emitter::velocity);
				data.item_binary.resize(write_position + write_size);
				memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
					&velocity, sizeof(velocity));
				common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
			}
		}
		data.common = common_data;

		//	エミッター更新アイテム登録
		particle_system->update_emitter(emitter_data.emitter_index, data);
	}

	//	軌跡処理
	if (emitter_datas[Trail].emitter_index != 0xFFFFFFFF)
	{
		auto& emitter_data = emitter_datas[Trail];
		particle_system_item::emitter::common common_data;
		{
			//	パーティクル生成情報
			common_data.emitter_state;

			//	エミッター姿勢情報
			common_data.scale = emitter_data.scale;
			DirectX::XMStoreFloat4(&common_data.rotation,
				DirectX::XMQuaternionRotationRollPitchYaw(emitter_data.rotation.x, emitter_data.rotation.y, emitter_data.rotation.z));
			common_data.translation = emitter_data.translation;

			//	パーティクル発生情報
			common_data.spawn_life_time_range = { 10.5f, 20.5f };
			common_data.spawn_scale_range = { 0.15f, 0.5f };
			if ((::GetAsyncKeyState('Z') & 0x8000))
				common_data.spawn_count = 40;
			else
				common_data.spawn_count = 0;

			common_data.spawn_area_min = { -40, 0, -40 };
			common_data.spawn_area_max = { +40, 0, +40 };
			common_data.gravity = { 0.0f, -9.8f, 0.0f };
		}

		//	エミッター更新情報登録
		compute_particle_system::update_emitter_data data;

		//	各種アイテム登録
		{
			UINT offset = 0;

			//	軌跡アイテム登録
			{
				particle_system_item::emitter::trail trail;
				trail.max = TRAIL_BODY_MAX;

				UINT write_position = static_cast<UINT>(data.item_binary.size());
				UINT write_size = sizeof(particle_system_item::emitter::trail);
				data.item_binary.resize(write_position + write_size);
				memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
					&trail, sizeof(trail));
				common_data.active_items |= 1 << particle_system_item::emitter::trail::data_id;
			}

			//	速度アイテム登録
			{
				particle_system_item::emitter::velocity velocity;
				velocity.angle_range = { 0.5f, 1.5f };
				velocity.direction.x = static_cast<float>(rand() % 1000 - 500) * 0.001f;
				velocity.direction.y = static_cast<float>(rand() % 1000 - 500) * 0.001f;
				velocity.direction.z = static_cast<float>(rand() % 1000 - 500) * 0.001f;
				float length = 0;
				length = sqrtf(velocity.direction.x * velocity.direction.x +
					velocity.direction.y * velocity.direction.y +
					velocity.direction.z * velocity.direction.z);
				velocity.direction.x /= length;
				velocity.direction.y /= length;
				velocity.direction.z /= length;
				velocity.power_range = { 2.5f, 5.0f };
				velocity.acceleration.x = static_cast<float>(rand() % 1000 - 500) * 0.01f;
				velocity.acceleration.y = static_cast<float>(rand() % 1000 - 500) * 0.01f;
				velocity.acceleration.z = static_cast<float>(rand() % 1000 - 500) * 0.01f;
				length = sqrtf(
					velocity.acceleration.x * velocity.acceleration.x +
					velocity.acceleration.y * velocity.acceleration.y +
					velocity.acceleration.z * velocity.acceleration.z);
				velocity.acceleration.x /= length;
				velocity.acceleration.y /= length;
				velocity.acceleration.z /= length;
				if (emitter_data.emitter_index == 0)
				{
					velocity.angle_range = { 0.0f, 0.5f };
					velocity.power_range = { 0, 0.1f };
				}

				UINT write_position = static_cast<UINT>(data.item_binary.size());
				UINT write_size = sizeof(particle_system_item::emitter::velocity);
				data.item_binary.resize(write_position + write_size);
				memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
					&velocity, sizeof(velocity));
				common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
			}
		}
		data.common = common_data;

		//	エミッター更新アイテム登録
		particle_system->update_emitter(emitter_data.emitter_index, data);
	}

	job_executor.execute([this](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	パーティクルシステム更新
		particle_system->update(context,
			context.scene_constant.time_data.x - context.scene_constant.time_data.y);

		//	パーティクルシステム描画
		particle_system->render(context);

		rendering_pass.deactivate(context.device_context);
	}, "effect_translucent", renderer_context, get_render_basis_sort_order(), true);
}

void particle_effect_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}