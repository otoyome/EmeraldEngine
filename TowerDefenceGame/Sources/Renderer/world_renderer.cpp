#include "Renderer/world_renderer.h"
#include "Texture/texture_factory.h"
#include "Graphics/Core/render_data.h"

#include <algorithm>

#include "Model/model_buffer_pool.h"
#include "Model/model_instance.h"
#include "Effect/effect_instance.h"
#include "Graphics/Culling/occlusion_culling_system.h"
#include "Graphics/Lighting/clustered_lighting_system.h"

#include "Renderer/scene_renderer.h"
#include "Renderer/scene_renderer_forward.h"
#include "Renderer/scene_renderer_postprocess.h"
#include "Renderer/scene_renderer_display.h"
#include "Renderer/ui_renderer.h"
#include "Renderer/occlusion_culling_renderer.h"

#include "Object/object_manager.h"
#include "Object/world.h"
#include "Object/GameObjects/UI/ui_object.h"

#include "System/engine_config_data.h"
#include "System/console_manager.h"
#include "Editor/detail_profiler.h"

#if defined(USE_IMGUI)
#include "Graphics/Debug/debug_primitive_collector.h"
#endif // defined(USE_IMGUI)

namespace
{
	//	view_stateの破棄フレーム数
	constexpr uint64_t scene_view_state_keep_frames = 60;

	static constexpr scene_view_state_key view_state_key = scene_view_state_cache::make_key("main");

	//	マテリアルの定数バッファ等を一気に更新&GPUへ転送
	void upload_material_constant_buffers(const render_data_context& data)
	{
		gpu_upload_list& upload_list = render_data::instance()->get_upload_list();

		auto upload = [&upload_list](material_instance* target)
		{
			if (!target)
				return;

			target->sync_from_resource();
			target->flush_constant_buffers(upload_list);
		};

		for (auto& material_data : data.mesh_container.material_datas)
			upload(material_data.material);
		for (auto& sprite_trail : data.effect_container.sprite_trail_draw_datas)
			upload(sprite_trail.material_data.material);
		for (auto& decal : data.decal_container.decal_datas)
			upload(decal.material.material);
		for (auto& sprite_data : data.sprite_container.sprite_datas)
			upload(&sprite_data.sprite->get_material());
		if (const auto& volume = data.post_process_volume_container.selected_volume)
		{
			for (auto& postprocess_material : volume->get_materials())
				upload(postprocess_material.get());
		}
	}

#if defined(USE_IMGUI)
	//	描画関係のデバッグ設定
	console_variable<bool> freeze_culling_variable("r.culling.freeze", false, "カリング結果を固定する（メイン視点）");
	console_variable<bool> visualize_meshlets_variable("r.meshlet.visualize", false, "メッシュレット単位で色分け表示する（メイン視点）");
#endif	//	defined(USE_IMGUI)
}

world_renderer::world_renderer(std::shared_ptr<render_job_executor> render_executor)
: render_executor(std::move(render_executor))
{
}

world_renderer::~world_renderer()
{
}

bool world_renderer::initialize(render_target_view output_target)
{
	render_targets = std::make_shared<scene_render_targets>();
	if (!create_targets(std::move(output_target)))
		return false;

	create_renderers();
	return true;
}

bool world_renderer::create_targets(render_target_view output_target)
{
	//	中身を作り直す
	*render_targets = scene_render_targets{};

	//	出力先
	output_target.clear_color = false;
	render_targets->output.render_targets.emplace_back(std::move(output_target));

	const int width = static_cast<int>(render_targets->output.get_size().x);
	const int height = static_cast<int>(render_targets->output.get_size().y);
	if (!create_scene_targets(width, height))
		return false;

	//	各レンダラーの対応
	for (auto& render : scene_renderers)
		render->resize_buffer(width, height);
	for (auto& render : ui_renderers)
		render->resize_buffer(width, height);
	return true;
}

void world_renderer::release_targets()
{
	if (render_targets)
		*render_targets = scene_render_targets{};
}

bool world_renderer::create_scene_targets(int scene_width, int scene_height)
{
	//	UI描画用のシーン定数バッファ生成(viewport_rectのみ使用するため他の項目は未使用)
	{
		HRESULT hr;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		//	viewport_rectは変化しないので初回のみアップロードしておく
		common_pipeline_states::scene_constants ui_scene_constant{};
		ui_scene_constant.viewport_rect = { 0.0f, 0.0f, static_cast<float>(scene_width), static_cast<float>(scene_height) };
		D3D11_SUBRESOURCE_DATA initial_data{};
		initial_data.pSysMem = &ui_scene_constant;
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &initial_data, ui_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	メイン視点のシーン定数バッファ生成
	{
		HRESULT hr;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = scene_width;
		texture2d_desc.Height = scene_height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		//	シーンテクスチャ生成
		{
			texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			if (!texture_factory::create_texture2d(render_targets->scene_color_texture, common_pipeline_states::scene_color_map_name, texture2d_desc))
			{
				return false;
			}
		}

		//	深度ステンシルバッファを生成
		{
			texture2d_desc.Format = common_pipeline_states::scene_depth_texture_format;
			texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

			D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
			shader_resource_view_desc.Format = common_pipeline_states::scene_depth_shader_resource_view_format;
			shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
			shader_resource_view_desc.Texture2D.MipLevels = 1;
			if (!texture_factory::create_texture2d(render_targets->scene_depth_stencil_texture, common_pipeline_states::scene_depth_map_name, texture2d_desc, &shader_resource_view_desc))
			{
				return false;
			}
		}

		//	GBuffer生成
		{
			texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			DXGI_FORMAT formats[] =
			{
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	BaseColor
				DXGI_FORMAT_R16G16B16A16_UNORM,		//	NormalTangent
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	Parameters
				DXGI_FORMAT_R16G16_FLOAT,			//	Velocity
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	CustomData0

				DXGI_FORMAT_R8G8B8A8_UNORM,			//	CustomData1
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	CustomData2
			};
			for (int index = 0; index < static_cast<int>(magic_enum::enum_count<common_pipeline_states::gbuffer_id>()); ++index)
			{
				texture_ptr texture;
				texture2d_desc.Format = formats[index];
				if (texture_factory::create_texture2d(texture,
																	 common_pipeline_states::gbuffer_names[index],
																	 texture2d_desc))
				{
					render_targets->g_buffer_textures.emplace_back(texture);
				}
			}
		}
	}

	//	シャドウマップ準備
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_buffer{};
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = 2048;
		texture2d_desc.Height = 2048;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 6;
		texture2d_desc.Format = common_pipeline_states::shadow_map_texture_format;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = common_pipeline_states::shadow_map_shader_resource_view_format;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		shader_resource_view_desc.Texture2DArray.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2DArray.MipLevels = 1;
		shader_resource_view_desc.Texture2DArray.FirstArraySlice = 0;
		shader_resource_view_desc.Texture2DArray.ArraySize = texture2d_desc.ArraySize;

		if (!texture_factory::create_texture2d(render_targets->shadowmap_depth_texture, common_pipeline_states::shadow_maps_name, texture2d_desc, &shader_resource_view_desc))
		{
			return false;
		}
	}

	//	ワークバッファ準備
	D3D11_TEXTURE2D_DESC ssr_texture2d_desc{};
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = scene_width;
		texture2d_desc.Height = scene_height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		ssr_texture2d_desc = texture2d_desc;	//	現時点の物をコピっておく

		static constexpr std::string_view WorkColorName = "Work Scene Color ";
		texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->work_buffer_scene_textures); ++index)
		{
			std::string name = std::string(WorkColorName) + std::to_string(index);
			if (!texture_factory::create_texture2d(render_targets->work_buffer_scene_textures[index], name, texture2d_desc))
			{
				return false;
			}
		}

		static constexpr std::string_view WorkRGBA32FloatName = "Work RGBA32 Float";
		texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		if (!texture_factory::create_texture2d(render_targets->work_buffer_rgba32float_texture, WorkRGBA32FloatName, texture2d_desc))
		{
			return false;
		}

		//	デカールで G-Buffer の法線/接線を読むためのコピー先
		static constexpr std::string_view WorkNormalTangentName = "Work GBuffer Normal Tangent";
		texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		if (!texture_factory::create_texture2d(render_targets->work_buffer_gbuffer_normal_tangent_texture, WorkNormalTangentName, texture2d_desc))
		{
			return false;
		}

		static constexpr std::string_view WorkHalfColorName = "Work Half Scene Color ";
		texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texture2d_desc.Width = scene_width / 2;
		texture2d_desc.Height = scene_height / 2;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->work_buffer_half_scene_textures); ++index)
		{
			std::string name = std::string(WorkHalfColorName) + std::to_string(index);
			if (!texture_factory::create_texture2d(render_targets->work_buffer_half_scene_textures[index], name, texture2d_desc))
			{
				return false;
			}
		}
	}

	//	SSRデノイズ用情報
	{
		static constexpr std::string_view SSRAccumateTextureName = "SSR Accumate Texture ";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->ssr_accumlate_textures); ++index)
		{
			std::string name = std::string(SSRAccumateTextureName) + std::to_string(index);
			if (!texture_factory::create_texture2d(render_targets->ssr_accumlate_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
		static constexpr std::string_view SSRCounterTextureName = "SSR Counter Texture";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->ssr_counter_textures); ++index)
		{
			std::string name = std::string(SSRCounterTextureName) + std::to_string(index);
			if (!texture_factory::create_texture2d(render_targets->ssr_counter_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
	}

	return true;
}

void world_renderer::create_renderers()
{
	//	シーンレンダラー登録
	{
		scene_renderers.emplace_back(std::make_unique<depth_prepass_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<occlusion_culling_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<gbuffer_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<gbuffer_decal_renderer>(render_executor, render_targets));
		//	SSAO
		scene_renderers.emplace_back(std::make_unique<deffered_lighting_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<screen_space_reflection_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<before_dof_forward_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<depth_of_field_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<before_dof_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_dof_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_dof_forward_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_translucency_post_process_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<motion_blur_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_motion_blur_forward_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<before_bloom_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<exposure_renderer>(render_executor, render_targets));
		//	@tool ブルーム・レンズゴーストは作り直すまで実行しない
		//scene_renderers.emplace_back(std::make_unique<bloom_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<tone_mapping_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_tone_mapping_post_process_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<anti_alias_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<display_mapping_renderer>(render_executor, render_targets));

#if defined(USE_IMGUI)
		scene_renderers.emplace_back(std::make_unique<debug_draw_pass_renderer>(render_executor, render_targets));
#endif // defined(USE_IMGUI)

		//	ソート
		std::sort(scene_renderers.begin(), scene_renderers.end(),
			[](std::unique_ptr<renderer>& l, std::unique_ptr<renderer>& r)
			{
				return l->get_domain() < r->get_domain();
			});
	}

	//	UIレンダラーの追加
	{
		ui_renderers.emplace_back(std::make_unique<ui_renderer>(render_executor, render_targets));
	}

	//	プロファイラー表示用のレンダラー名設定
	for (auto& render : scene_renderers)
		DETAIL_PROFILE_SET_RENDERER_NAME(*render);
	for (auto& render : ui_renderers)
		DETAIL_PROFILE_SET_RENDERER_NAME(*render);
}

void world_renderer::collect(world& target)
{
	render_data = std::make_shared<render_data_context>();
	particle_indices = std::make_shared<render_particle_indices>();

	DETAIL_PROFILE_CPU_SCOPE("render.collect_render_data");

	//	視点情報を取得
	view_selection selection;
	camera_cut = false;
	const camera_ptr game_camera = target.find_view_camera();

	//	スクリーン矩形は出力先に合わせる
	const math::uint2 output_size = render_targets->output.get_size();
	const math::float4 screen_rect = { 0.0f, 0.0f, static_cast<float>((std::max)(output_size.x, 1u)), static_cast<float>((std::max)(output_size.y, 1u)) };
	if (game_camera)
		game_camera->get_parameters().screen_rect = screen_rect;

	//	別のカメラに変わったか判断
	if (selector.select(game_camera.get(), selection))
	{
		current_view_parameters = selection.parameters;
		camera_cut = selection.camera_cut;
	}
	current_view_parameters.screen_rect = screen_rect;

	//	相対描画用の描画原点
	const math::vector3d render_origin = scene_view::compute_render_origin(current_view_parameters, engine_config_data::instance()->rendering.camera_relative_rendering);
	render_data->render_origin = render_origin;

	//	ライト情報収集
	{
		//	メイン視点の視錐台と距離・画面上の大きさで間引く
		const scene_view collect_view = scene_view::from_parameters(current_view_parameters, render_origin);
		light_view_info view_info;
		view_info.position = current_view_parameters.position;
		view_info.tan_half_fov_y = std::tan(current_view_parameters.fov_y * 0.5f);
		view_info.frustum = &collect_view.get_frustum_culling();
		target.collect_lights(render_data->light_container, render_origin, view_info);
	}

	//	ポストプロセスボリューム取得
	target.collect_post_process_volumes(render_data->post_process_volume_container, current_view_parameters.position);

	//	メッシュ取得
	target.collect_meshes(render_data->mesh_container, render_origin);

	//	エフェクトコンテナ取得
	target.get_effects().get_container(render_data->effect_container, render_data->decal_container, render_origin);

	//	UIオブジェクトコンテナ収集
	{
		render_data->sprite_container.sprite_datas.clear();

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
			render_data->sprite_container.sprite_datas.emplace_back(sprite_data);
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
				render_data->sprite_container.sprite_datas.emplace_back(sprite_data);
			}
			++sort_key;
		}

		//	ソート
		std::sort(render_data->sprite_container.sprite_datas.begin(), render_data->sprite_container.sprite_datas.end(),
			[](const sprite_datas& l, const sprite_datas& r)
			{
				if (l.sort_order < r.sort_order)
					return l.sort_key != r.sort_key;
				return false;
			});
	}

	//	パーティクルの描画振り分け
	build_particle_render_lists(*render_data, *particle_indices);

	//	モデルの頂点/インデックスバッファ確保分をGPUへ転送
	model_buffer_pool_manager::instance()->flush_uploads();

	//	マテリアルの定数バッファをGPUへ転送
	upload_material_constant_buffers(*render_data);
}

#if defined(USE_IMGUI)
void world_renderer::collect_debug_primitives(world& target, const debug_view_settings& debug_settings)
{
	//	デバッグ情報取得
	build_debug_primitives(target, *render_data, debug_settings);
	target.get_debug_primitives().get_container(render_data->debug_draw_container, render_data->render_origin);
}

void world_renderer::build_debug_primitives(world& target, const render_data_context& render_data, const debug_view_settings& debug_settings)
{
	engine_context& engine = target.get_context();

	//	衝突判定のデバッグ表示
	if (debug_settings.collision)
	{
		target.get_objects().get_world().debug_primitive_draw(engine);
	}

	//	モデルAABBのデバッグ表示
	if (debug_settings.mesh_aabb)
	{
		math::float4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
		math::float4 color = {1.0f, 0.0f, 0.0f, 1.0f};
		const math::double3 render_origin = render_data.render_origin.to_double3();
		for (auto& buffer_data : render_data.mesh_container.draw_items)
		{
			math::float3 scale((buffer_data.bounds_max.x - buffer_data.bounds_min.x) * 0.5f, (buffer_data.bounds_max.y - buffer_data.bounds_min.y) * 0.5f, (buffer_data.bounds_max.z - buffer_data.bounds_min.z) * 0.5f);
			math::float3 center(buffer_data.bounds_min.x + scale.x, buffer_data.bounds_min.y + scale.y, buffer_data.bounds_min.z + scale.z);

			//	AABB は描画原点からの相対座標
			const math::double3 world_center(render_origin.x + center.x, render_origin.y + center.y, render_origin.z + center.z);
			QUEUE_DEBUG_PRIMITIVE_BOX(engine, world_center, rotation, scale, color, true);
		}
	}

	const bool display_grid_xz = debug_settings.grid_xz;
	const bool display_grid_xy = debug_settings.grid_xy;
	const bool display_grid_yz = debug_settings.grid_yz;
	const int display_grid_length = debug_settings.grid_length;
	if (!display_grid_xz && !display_grid_xy && !display_grid_yz)
		return;

	float grid_length = system_units::convert_m_to_unit(static_cast<float>(display_grid_length));
	for (int index = -display_grid_length; index <= display_grid_length; ++index)
	{
		float len = system_units::convert_m_to_unit(static_cast<float>(index));
		if (display_grid_xz)
		{
			math::float4 line_color = math::float4(1.0f, 0.0f, 1.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(len, 0.0f, -grid_length), math::float3(len, 0.0f, +grid_length), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(-grid_length, 0.0f, len), math::float3(+grid_length, 0.0f, len), line_color, true);
		}
		if (display_grid_xy)
		{
			math::float4 line_color = math::float4(1.0f, 1.0f, 0.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(len, -grid_length, 0.0f), math::float3(len, +grid_length, 0.0f), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(-grid_length, len, 0.0f), math::float3(+grid_length, len, 0.0f), line_color, true);
		}
		if (display_grid_yz)
		{
			math::float4 line_color = math::float4(0.0f, 1.0f, 1.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(0.0f, len, -grid_length), math::float3(0.0f, len, +grid_length), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(0.0f, -grid_length, len), math::float3(0.0f, +grid_length, len), line_color, true);
		}
	}
}
#endif	//	defined(USE_IMGUI)

void world_renderer::submit_render_jobs(world& target, float elapsed_time)
{
	if (!render_data)
		return;

	//	メイン視点で描画
	{
		const scene_view_state_ptr& view_state = view_states.acquire(view_state_key, render_frame_index);
#if defined(USE_IMGUI)
		view_state->set_freeze_culling(freeze_culling_variable.get());
		view_state->set_debug_visualize_meshlets(visualize_meshlets_variable.get());
#endif	//	defined(USE_IMGUI)
		const scene_view_ptr target_view = std::make_shared<const scene_view>(scene_view::from_parameters(current_view_parameters, render_data->render_origin));
		view_state->begin_frame(*target_view, camera_cut);

		//	レンダラーコンテキスト準備
		renderer_context context = {};
		{
			context.render_data = render_data;
			context.render_mesh_indices = std::make_shared<render_mesh_indices>();
			context.render_particle_indices = particle_indices;
			context.render_decal_indices = std::make_shared<render_decal_indices>();
			prepare_renderer_context(context, target_view, view_state, scene_constant_buffer, render_data, particle_indices, total_time, total_time + elapsed_time);
		}

		//	オクルージョンカリング結果を読み出す
		std::unordered_map<UINT, uint8_t> occlusion_visible;
		if (const auto& occlusion = view_state->get_occlusion_culling())
			occlusion->read_last_frame(occlusion_visible);

		//	描画メッシュ／デカールのカリング・ソート
		{
			DETAIL_PROFILE_CPU_SCOPE("render.build_render_lists");
			build_render_lists(*render_data, *view_state, target_view->get_frustum_culling(), occlusion_visible,
				*context.render_mesh_indices, *context.render_decal_indices);
		}

		//	シーン構築
		{
			effect_manager* effects = &target.get_effects();
			render_executor->execute([context, effects](graphics_context& context)
			{
				//	シーン定数バッファ更新
				context.device_context->UpdateSubresource(context.scene_constant_buffer.Get(), 0, 0, &context.scene_constant, 0, 0);

				//	エフェクト類のGPU側の更新
				context.debug_begin_event(L"update_effect_gpu");
				effects->update_gpu(context, context.scene_constant.time_data.x - context.scene_constant.time_data.y);
				context.debug_end_event();

			}, "scene_constantract_pass", context, static_cast<UINT>(renderer_domain::scene_constantract_pass), true);
		}

		//	描画
		{
			DETAIL_PROFILE_CPU_SCOPE("render.scene_renderers_submit");
			for (auto& render : scene_renderers)
			{
				DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_submit, render->get_profile_name());
				render->render(context);
			}
		}

		//	描画終了処理
		view_state->end_frame(*target_view);
	}

	//	UI描画
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

	//	時間加算
	total_time += elapsed_time;

	//	その他開放
	view_states.release_unused(render_frame_index, scene_view_state_keep_frames);
	++render_frame_index;

	render_data.reset();
	particle_indices.reset();
}

void world_renderer::build_particle_render_lists(const render_data_context& render_data, render_particle_indices& particle_indices)
{
	for (auto& particle_index : render_data.effect_container.sprite_trail_color_datas)
	{
		auto& material = render_data.effect_container.sprite_trail_draw_datas[particle_index].material_data.material;

		//	番号登録
		switch (material->get_blend_mode())
		{
			case material_resource::blend_modes::opaque:
			{
				particle_indices.opaque.push_back(particle_index);
				break;
			}
			case material_resource::blend_modes::mask:
			{
				particle_indices.mask.push_back(particle_index);
				break;
			}
			case material_resource::blend_modes::translucent:
			case material_resource::blend_modes::addative:
			{
				int index = static_cast<int>(material->get_translucency_path());
				particle_indices.forward[index].push_back(particle_index);
				break;
			}
		}
	}
}

void world_renderer::prepare_renderer_context(
	renderer_context& context,
	const scene_view_ptr& target_view,
	const scene_view_state_ptr& target_state,
	const Microsoft::WRL::ComPtr<ID3D11Buffer>& scene_constant_buffer,
	const std::shared_ptr<render_data_context>& render_data,
	const std::shared_ptr<render_particle_indices>& particle_indices,
	float previous_time, float current_time)
{
	//	シーン定数バッファ設定
	{
		context.scene_constant_buffer = scene_constant_buffer;
		context.scene_constant.time_data.x = current_time;
		context.scene_constant.time_data.y = previous_time;
#if defined(USE_IMGUI)
		context.scene_constant.time_data.z = target_state->is_debug_visualize_meshlets() ? 1.0f : 0.0f;
#endif	//	defined(USE_IMGUI)

		target_view->fill_scene_constants(context.scene_constant, target_state.get());

		//	ディスプレイマッピング情報
		context.scene_constant.rendering_gamut = static_cast<UINT>(engine_config_data::instance()->get_active_rendering_gamut());
		context.scene_constant.output_curve = static_cast<UINT>(display_mapping_renderer::output_curves::srgb);
		context.scene_constant.peak_nits = sdr_reference_white_nits;
	}

	//	描画視点情報と描画制御情報設定
	{
		context.current_view = target_view;
		context.current_view_state = target_state;
	}
}

void world_renderer::build_render_lists(
	const render_data_context& render_data,
	scene_view_state& target_state,
	const collision::frustum_culling& frustum_culling,
	const std::unordered_map<UINT, uint8_t>& occlusion_visible,
	render_mesh_indices& mesh_indices,
	render_decal_indices& decal_indices)
{
	const auto& mesh_container = render_data.mesh_container;
	const auto* draw_data_ptr = mesh_container.draw_datas.data();
	const auto* material_data_ptr = mesh_container.material_datas.data();
	const auto* buffer_data_ptr = mesh_container.draw_items.data();
	const auto& color_datas = mesh_container.color_datas;
	const std::size_t count = color_datas.size();

	//	ソート情報
	auto& lengths = mesh_indices.lengths;
	lengths.assign(mesh_container.draw_items.size(), 0.0f);
	sort_pipeline_keys.assign(mesh_container.draw_items.size(), std::uintptr_t(0));
	const auto& near_plane = frustum_culling.get_plane(collision::frustum_culling::surface_near);

	//	描画判定
	auto draw_classify = [&](std::size_t list_index, cull_bucket& bucket)
	{
		const std::size_t mesh_index = color_datas[list_index];
		const auto* draw_data = draw_data_ptr + mesh_index;
		const auto* material = (material_data_ptr + draw_data->material_index)->material;
		const auto* buffer_data = buffer_data_ptr + draw_data->buffer_index;

		//	カリング処理
		if (system_utility::is_bitflag(buffer_data->system_flags, mesh_draw_item::flags::culling))
		{
			//	フラスタムカリング
			bool visible = frustum_culling.to_aabb(buffer_data->bounds_min, buffer_data->bounds_max);

			//	オクルージョンカリング
			if (visible)
			{
				auto it = occlusion_visible.find(buffer_data->unique_id);
				if (it != occlusion_visible.end() && it->second == 0)
					visible = false;
			}

#if defined(USE_IMGUI)
			//	判定結果を過去のもので固定
			visible = target_state.resolve_debug_visible(buffer_data->unique_id, visible);
#endif	//	defined(USE_IMGUI)
			if (!visible)
				return;
		}
		//	カメラからの距離算出
		math::float3 center;
		center.x = (buffer_data->bounds_min.x + buffer_data->bounds_max.x) * 0.5f;
		center.y = (buffer_data->bounds_min.y + buffer_data->bounds_max.y) * 0.5f;
		center.z = (buffer_data->bounds_min.z + buffer_data->bounds_max.z) * 0.5f;
		lengths[mesh_index] = near_plane.calculate_distance(center);

		//	番号登録
		switch (material->get_blend_mode())
		{
			case material_resource::blend_modes::opaque:
				sort_pipeline_keys[mesh_index] = reinterpret_cast<std::uintptr_t>(material->get_pipeline_state(material_resource::surface_paths::g_buffer_pass).get());
				bucket.opaque.push_back(mesh_index);
				break;
			case material_resource::blend_modes::mask:
				sort_pipeline_keys[mesh_index] = reinterpret_cast<std::uintptr_t>(material->get_pipeline_state(material_resource::surface_paths::g_buffer_pass).get());
				bucket.mask.push_back(mesh_index);
				break;
			case material_resource::blend_modes::translucent:
			case material_resource::blend_modes::addative:
			{
				int path = static_cast<int>(material->get_translucency_path());
				bucket.forward[path].push_back(mesh_index);
				break;
			}
		}
	};

	//	仕分け
	{
		constexpr std::size_t min_items_per_thread = 512;
		const std::size_t worker_slots = parallel_executor.max_workers();
		if (cull_buckets.size() < worker_slots)
			cull_buckets.resize(worker_slots);
		for (auto& bucket : cull_buckets)
			bucket.clear();

		if (count < min_items_per_thread * 2 || worker_slots <= 1)
		{
			for (std::size_t i = 0; i < count; ++i)
				draw_classify(i, cull_buckets[0]);
		}
		else
		{
			parallel_executor.parallel_for(count, min_items_per_thread,
				[&](std::size_t worker_index, std::size_t begin, std::size_t end)
				{
					cull_bucket& bucket = cull_buckets[worker_index];
					for (std::size_t i = begin; i < end; ++i)
						draw_classify(i, bucket);
				});
		}
	}

	std::size_t total_opaque = 0;
	std::size_t total_mask = 0;
	std::size_t total_forward[render_mesh_indices::forward_size] = {};
	for (auto& bucket : cull_buckets)
	{
		total_opaque += bucket.opaque.size();
		total_mask += bucket.mask.size();
		for (int path = 0; path < render_mesh_indices::forward_size; ++path)
			total_forward[path] += bucket.forward[path].size();
	}

	mesh_indices.opaque.reserve(total_opaque);
	mesh_indices.mask.reserve(total_mask);
	for (int path = 0; path < render_mesh_indices::forward_size; ++path)
		mesh_indices.forward[path].reserve(total_forward[path]);
	for (auto& bucket : cull_buckets)
	{
		mesh_indices.opaque.insert(mesh_indices.opaque.end(), bucket.opaque.begin(), bucket.opaque.end());
		mesh_indices.mask.insert(mesh_indices.mask.end(), bucket.mask.begin(), bucket.mask.end());
		for (int path = 0; path < render_mesh_indices::forward_size; ++path)
			mesh_indices.forward[path].insert(mesh_indices.forward[path].end(), bucket.forward[path].begin(), bucket.forward[path].end());
	}

	//	同一パイプラインステートで固まる用に優先しつつ、
	//	不透明・マスク手前から距離でソート
	auto pipeline_then_near = [this, &lengths](std::size_t left_index, std::size_t right_index)
	{
		if (sort_pipeline_keys[left_index] != sort_pipeline_keys[right_index])
			return sort_pipeline_keys[left_index] < sort_pipeline_keys[right_index];
		return lengths[left_index] < lengths[right_index];
	};
	std::sort(mesh_indices.opaque.begin(), mesh_indices.opaque.end(), pipeline_then_near);
	std::sort(mesh_indices.mask.begin(), mesh_indices.mask.end(), pipeline_then_near);

	//	半透明・加算は奥から距離ソート
	auto far_to_near = [&lengths](std::size_t left_index, std::size_t right_index)
	{
		return lengths[left_index] > lengths[right_index];
	};
	for (auto& indices : mesh_indices.forward)
		std::sort(indices.begin(), indices.end(), far_to_near);

	//	デカール
	const auto& decal_datas = render_data.decal_container.decal_datas;
	decal_indices.draw_list.reserve(decal_datas.size());
	for (UINT decal_index = 0; decal_index < decal_datas.size(); ++decal_index)
	{
		//	フラスタムカリング
		if (!frustum_culling.to_box(decal_datas[decal_index].box))
			continue;

		decal_indices.draw_list.emplace_back(decal_index);
	}
}
