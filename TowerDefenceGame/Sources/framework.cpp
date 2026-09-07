#include "framework.h"

#include <algorithm>
#include <future>
#include <thread>

#include "shader.h"
#include "texture.h"
#include "font_data.h"
#include "camera.h"
#include "model_resource.h"
#include "model_instance.h"
#include "effect_resource.h"
#include "effect_instance.h"

#include "ik_resource.h"

#include "scenes/load_shader_scene.h"

#include "Renderer/scene_renderer.h"
#include "Renderer/scene_renderer_forward.h"
#include "Renderer/scene_renderer_postprocess.h"

#include "Object/object_manager.h"

#include "System/engine_config_data.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#include "Editor/editor_items.h"
#endif // defined(USE_IMGUI)

framework::framework(HWND hwnd) : hwnd(hwnd)
{
	logger::create_instance();
}

framework::~framework()
{
	logger::delete_instance();
}

int framework::run()
{
	MSG msg{};

	//	ウィンドウ名設定
	SetWindowTextW(hwnd, APPLICATION_NAME);

	if (!initialize())
	{
		return uninitialize();
	}

	imgui_initialize();

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			tictoc.tick();
			calculate_frame_stats();
			update(tictoc.time_interval());
			render(tictoc.time_interval());
		}
	}

	imgui_terminate();

	BOOL fullscreen{};
	render_data::instance()->get_swap_chain()->GetFullscreenState(&fullscreen, 0);
	if (fullscreen)
	{
		render_data::instance()->get_swap_chain()->SetFullscreenState(FALSE, 0);
	}

	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

LRESULT framework::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif

	if (mouse::instance()->window_proccess(hwnd, msg, wparam, lparam)) { return true; }

	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);

		EndPaint(hwnd, &ps);
		break;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_CREATE:
	{
		break;
	}
	case WM_KEYDOWN:
	{
		if (wparam == VK_ESCAPE)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
		break;
	}
	case WM_ENTERSIZEMOVE:
	{
		tictoc.stop();
		break;
	}
	case WM_EXITSIZEMOVE:
	{
		tictoc.start();
		break;
	}
	case WM_MOUSEWHEEL:
	{
		wheel = GET_WHEEL_DELTA_WPARAM(wparam);
		break;
	}
	case WM_MOVE:
	{
		engine_config_data::instance()->windows.window_left = (int)(short)LOWORD(lparam);
		engine_config_data::instance()->windows.window_top = (int)(short)HIWORD(lparam);
		break;
	}
	case WM_WINDOWPOSCHANGED:
	{
		WINDOWPOS* pos = (WINDOWPOS*)lparam;
		engine_config_data::instance()->windows.window_left = pos->x;
		engine_config_data::instance()->windows.window_top = pos->y;
		break;
	}

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

bool framework::initialize()
{
	HRESULT hr{ S_OK };

	//	COMオブジェクト初期化
	{
		hr = CoInitializeEx(NULL, 0);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			return false;
		}
	}

	//	文字コード変換器所持
	system_utility::initialize_charset_converter();

	render_data::create_instance(hwnd);
	mouse::create_instance(hwnd);
	keyboard::create_instance();
	texture_manager::create_instance();
	font_data_manager::create_instance();
	shader_manager::create_instance();
	pipeline_state_manager::create_instance();
	light_manager::create_instance();
	post_process_volume_manager::create_instance();
	camera_manager::create_instance();
	model_resource_manager::create_instance();
	model_instance_manger::create_instance();
	effect_resource_manager::create_instance();
	effect_manager::create_instance();
	ik_resource_manager::create_instance();
#if defined(USE_IMGUI)
	debug_primitive_manager::create_instance();
#endif // defined(USE_IMGUI)
#if defined(USE_IMGUI)
	editor_items::initialize();
#endif // defined(USE_IMGUI)

	collision_filter_manager::create_instance();
	object_manager::create_instance();
	scene_manager::create_instance();

	//	各種バッファ用意
	render_targets = std::make_shared<scene_render_targets>();
	if (!create_buffers())
		return false;

	//	当たり判定フィルター用意
	if (!create_collision_filters())
		return false;

	//	オクルージョンカリング生成
	occlusion_culling = std::make_shared<occlusion_culling_system>();

	//	シーンレンダラー登録
	{
		render_executor = std::make_shared<render_job_executor>(4);

		scene_renderers.emplace_back(std::make_unique<depth_prepass_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<occlusion_culling_renderer>(render_executor, occlusion_culling, render_targets));
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
		scene_renderers.emplace_back(std::make_unique<bloom_and_tone_mapping_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_tone_mapping_post_process_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<anti_alias_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<finalpass_renderer>(render_executor, render_targets));

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
		//ui_renderers.emplace_back(std::make_unique<imgui_draw_pass_renderer>(render_executor, render_targets));
	}

	//	全描画コマンド処理後に呼び出される関数登録
	render_executor->register_post_rendering(
		[this]
		{
#if defined(USE_IMGUI)
			//	ImGui描画
			{
				imgui_rendering_pass.activate(render_data::instance()->get_immediate_context().Get());
				render_data::instance()->get_immediate_context()->RSSetViewports(1, &imgui_viewport);
					
				ImGui::Render();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				imgui_rendering_pass.deactivate(render_data::instance()->get_immediate_context().Get());

				if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
				}
			}
#endif // defined(USE_IMGUI)

			//	プレゼント
			UINT sync_interval{ engine_config_data::instance()->windows.vertical_sync };
			render_data::instance()->get_swap_chain()->Present(sync_interval, 0);
		});

	//	初回シーン登録
	scene_manager::instance()->change_scene<load_shader_scene>();
	return true;
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
#if defined(USE_IMGUI)
	//	CPU側の更新時間計測開始
	stationed_cpu_timer.begin();

	//	各種データの更新チェック
	if (shader_manager::is_valid())
	{
		shader_manager::instance()->check_shader_update_and_recompile();
	}
	if (texture_manager::is_valid())
	{
		texture_manager::instance()->check_update_and_reload();
	}

#endif //	defined(USE_IMGUI)

	//	入力関係の更新
	if (mouse::is_valid())
	{
		mouse::instance()->update_state();
	}
	if (keyboard::is_valid())
	{
		keyboard::instance()->update_state();
	}

	//	シーン内の更新
	if (scene_manager::is_valid())
	{
		scene_manager::instance()->updates(elapsed_time);
	}

	//	オブジェクト更新
	if (object_manager::is_valid())
	{
		object_manager::instance()->updates(elapsed_time);
	}

	//	エフェクト関係更新
	if (effect_manager::is_valid())
	{
		effect_manager::instance()->update(elapsed_time);
	}

#if defined(USE_IMGUI)
	//	CPU側の更新時間計測終了
	{
		stationed_cpu_timer.end();

		auto& timer_data = stationed_cpu_stamp_datas[static_cast<UINT>(timer::update)].datas;
		for (size_t index = 0; index < timer_data.size() - 1; ++index)
		{
			timer_data[index] = timer_data[index + 1];
		}
		timer_data.back() = stationed_cpu_timer.get_result();
	}

	//	GPUスナップショット
	if ((::GetAsyncKeyState(VK_SHIFT) & 0x8000) && (::GetAsyncKeyState(VK_F5) & 0x8000))
	{
		is_gpu_snap_shot = true;
	}
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	editor_items::begin_contexts();

	//	メインウィンドウ
	imgui_main_window();

	//	シーン情報表示
	if (scene_manager::is_valid())
	{
		scene_manager::instance()->imgui_render();
	}

	//	オブジェクト情報表示
	object_manager::instance()->imgui_render();

	//	ログ情報
	imgui_log_window();

	//	エンジンの設定情報
	imgui_engine_config_window();

	//	GPU計測時間表示
	imgui_profiler_window(elapsed_time);

	//	描画系情報
	imgui_render_infos_window();

	//	テクスチャリスト情報
	imgui_textures_window();

	//	シーン変更
	imgui_scene_change();
#endif // defined(USE_IMGUI)
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
#if defined(USE_IMGUI)
	//	CPU側の描画時間計測開始
	stationed_cpu_timer.begin();
#endif //	defined(USE_IMGUI)

	//	描画前処理
	if (object_manager::is_valid())
	{
		object_manager::instance()->pre_render();
	}

	//	描画用の情報収集
	auto render_data = std::make_shared<render_data_context>();
	auto render_particle_indices = std::make_shared<::render_particle_indices>();
	{
		//	ライト情報収集
		light_manager::instance()->get_container(render_data->light_container);

#if defined(USE_IMGUI)
		//	デバッグ情報取得
		plane_line_draw();
		debug_primitive_manager::instance()->get_container(render_data->debug_draw_container);
#endif // defined(USE_IMGUI)

		//	ポストプロセスボリューム取得
		post_process_volume_manager::instance()->get_container(render_data->post_process_volume_container);

		//	メッシュ取得
		model_instance_manger::instance()->get_container(render_data->mesh_container);

		//	エフェクトコンテナ取得
		effect_manager::instance()->get_container(render_data->effect_container, render_data->decal_container);

		//	パーティクルの描画振り分け
		for (auto& particle_index : render_data->effect_container.sprite_trail_color_datas)
		{
			auto& material = render_data->effect_container.sprite_trail_draw_datas[particle_index].material_data.material;

			//	番号登録
			switch (material->get_blend_mode())
			{
				case material::blend_modes::opaque:
				{
					render_particle_indices->opaque.push_back(particle_index);
					break;
				}
				case material::blend_modes::mask:
				{
					render_particle_indices->mask.push_back(particle_index);
					break;
				}
				case material::blend_modes::translucent:
				case material::blend_modes::addative:
				{
					int index = static_cast<int>(material->get_translucency_path());
					render_particle_indices->forward[index].push_back(particle_index);
					break;
				}
			}
		}
	}

	//	バッファ描画
	{
#if defined(USE_IMGUI)
		//	GPUのスナップショット
		gpu_snap_shot_begin();
#endif	//	defined(USE_IMGUI)

		//	レンダラー開始処理
		render_executor->begin();

		//	描画準備
		{
			renderer_context context = {};
			context.render_data = render_data;
			render_executor->execute([this, context](graphics_context& context)
			{
				back_buffer_clear_rendering_pass.activate(context.device_context);

				//	各モデルの定数バッファを更新
				auto position_semantic = semantic("POSITION", 0);
				auto normal_semantic = semantic("NORMAL", 0);
				auto tangent_semantic = semantic("TANGENT", 0);
				for (auto& buffer_data : context.render_data->mesh_container.buffer_datas)
				{
					auto& current_matrices = buffer_data.transform_matrices[static_cast<int>(buffer_datas::transform_matrix_index::current)];
					auto& previous_matrices = buffer_data.transform_matrices[static_cast<int>(buffer_datas::transform_matrix_index::previous)];

					//	定数バッファ更新
					ID3D11Buffer* constant_buffer = buffer_data.primitive_buffer.Get();
					common_pipeline_states::primitive_constants pcb = {};
					pcb.primitive_parameter.x = buffer_data.color_meshs[0].vertex_buffer_views->find(tangent_semantic) != buffer_data.color_meshs[0].vertex_buffer_views->end();
					pcb.primitive_parameter.y = 0;
					pcb.primitive_parameter.z = 0;
					pcb.primitive_parameter.w = 0;
					for (UINT index = 0; index < buffer_data.color_meshs[0].morph_meshes.size(); ++index)
					{
						auto& morph = buffer_data.color_meshs[0].morph_meshes[index];
						if (morph.vertex_buffer_views->find(position_semantic) != morph.vertex_buffer_views->end())
						{
							float* weight = &pcb.position_morph[pcb.primitive_parameter.y / 4].x;
							weight[pcb.primitive_parameter.y % 4] = morph.weight;
							pcb.primitive_parameter.y++;
						}
						if (morph.vertex_buffer_views->find(normal_semantic) != morph.vertex_buffer_views->end())
						{
							float* weight = &pcb.normal_morph[pcb.primitive_parameter.z / 4].x;
							weight[pcb.primitive_parameter.z % 4] = morph.weight;
							pcb.primitive_parameter.z++;
						}
					}

					DEBUG_CHECK_WARNING_CATEGORY(common_pipeline_states::mesh_bone_max < previous_matrices.size() || common_pipeline_states::mesh_bone_max < current_matrices.size(),
						logger::categorys::model, "primitive_constant_buffer bone over.");
					UINT max_current_transforms = std::min(common_pipeline_states::mesh_bone_max, static_cast<UINT>(current_matrices.size()));
					UINT max_previous_transforms = std::min(common_pipeline_states::mesh_bone_max, static_cast<UINT>(previous_matrices.size()));
					memcpy_s(pcb.bone_transforms, sizeof(pcb.bone_transforms),
						current_matrices.data(), max_current_transforms * sizeof(DirectX::XMFLOAT4X4));
					memcpy_s(pcb.previous_bone_transforms, sizeof(pcb.previous_bone_transforms),
						previous_matrices.data(), max_previous_transforms * sizeof(DirectX::XMFLOAT4X4));
					context.device_context->UpdateSubresource(constant_buffer, 0, 0, &pcb, 0, 0);
				}

				back_buffer_clear_rendering_pass.deactivate(context.device_context);
			}, "begin_draw_pass", context, static_cast<UINT>(renderer_domain::begin_pass));
		}

		static float total_time = 0;

		//	カメラ分描画(仮)
		std::unordered_map<UINT, uint8_t> occlusion_visible;
		if (occlusion_culling)
			occlusion_culling->read_last_frame(occlusion_visible);

		camera_container camera_container;
		camera_manager::instance()->get_container(camera_container);
		for (auto& camera : camera_container.cameras)
		{
			//	レンダラーコンテキスト準備
			renderer_context context = {};
			context.render_data = render_data;
			context.render_mesh_indices = std::make_shared<render_mesh_indices>();
			context.render_particle_indices = render_particle_indices;
			context.render_decal_indices = std::make_shared<render_decal_indices>();
			prepare_render_context(context, camera, render_data, render_particle_indices, total_time, total_time + elapsed_time);

			//	描画メッシュ／デカールのカリング・ソート
			build_render_lists(*render_data, camera.camera->get_frustum_culling(), occlusion_visible,
				*context.render_mesh_indices, *context.render_decal_indices);

			//	シーン構築
			{
				render_executor->execute([this, context](graphics_context& context)
				{
					context.device_context->UpdateSubresource(context.scene_constant_buffer.Get(), 0, 0, &context.scene_constant, 0, 0);

					//	エフェクト類のGPU側の更新
					context.debug_begin_event(L"update_effect_gpu");
					effect_manager::instance()->update_gpu(context, context.scene_constant.time_data.x - context.scene_constant.time_data.y);
					context.debug_end_event();

				}, "scene_constantract_pass", context, static_cast<UINT>(renderer_domain::scene_constantract_pass), true);
			}

			//	描画
			for (auto& render : scene_renderers)
			{
				render->render(context);
			}
		}

		//	UI描画
		{
			renderer_context context = {};
			context.scene_constant_buffer = nullptr;
			context.render_data = render_data;
			context.render_mesh_indices = std::make_shared<render_mesh_indices>();
			for (auto& render : ui_renderers)
			{
				render->render(context);
			}
		}

		//	描画コマンドを実行
		render_executor->render();

		//	時間加算
		total_time += elapsed_time;

		//	描画コマンド実行終了処理
		render_executor->end();

#if defined(USE_IMGUI)
		//	スナップショットの情報を取得
		gpu_snap_shot_end();
#endif //	defined(USE_IMGUI)
	}

	//	描画後処理
	if (object_manager::is_valid())
	{
		object_manager::instance()->post_render();
	}

#if defined(USE_IMGUI)
	//	CPU側の描画時間計測終了
	{
		stationed_cpu_timer.end();

		auto& timer_data = stationed_cpu_stamp_datas[static_cast<UINT>(timer::render)].datas;
		for (size_t index = 0; index < timer_data.size() - 1; ++index)
		{
			timer_data[index] = timer_data[index + 1];
		}
		timer_data.back() = stationed_cpu_timer.get_result();
	}
#endif //	defined(USE_IMGUI)
}

void framework::prepare_render_context(
	renderer_context& context,
	const camera_container::camera_data& camera,
	const std::shared_ptr<render_data_context>& render_data,
	const std::shared_ptr<render_particle_indices>& particle_indices,
	float previous_time, float current_time)
{
	context.scene_constant_buffer = camera.constant_buffer;

	//	シーン定数バッファ設定
	{
		context.scene_constant.time_data.x = current_time;
		context.scene_constant.time_data.y = previous_time;

		context.scene_constant.camera_position.x = camera.camera->get_position().x;
		context.scene_constant.camera_position.y = camera.camera->get_position().y;
		context.scene_constant.camera_position.z = camera.camera->get_position().z;
		context.scene_constant.camera_position.w = camera.camera->get_fov_y();

		context.scene_constant.camera_right.x = camera.camera->get_right().x;
		context.scene_constant.camera_right.y = camera.camera->get_right().y;
		context.scene_constant.camera_right.z = camera.camera->get_right().z;
		context.scene_constant.camera_right.w = tanf(camera.camera->get_fov_y() * 0.5f);

		context.scene_constant.camera_up.x = camera.camera->get_up().x;
		context.scene_constant.camera_up.y = camera.camera->get_up().y;
		context.scene_constant.camera_up.z = camera.camera->get_up().z;
		context.scene_constant.camera_up.w = 0.0f;

		context.scene_constant.camera_direction.x = camera.camera->get_front().x;
		context.scene_constant.camera_direction.y = camera.camera->get_front().y;
		context.scene_constant.camera_direction.z = camera.camera->get_front().z;
		context.scene_constant.camera_direction.w = 0.0f;

		context.scene_constant.viewport_rect = camera.camera->get_screen_rect();

		context.scene_constant.camera_clip_distance.x = camera.camera->get_clip_distance().x;
		context.scene_constant.camera_clip_distance.y = camera.camera->get_clip_distance().y;
		context.scene_constant.camera_clip_distance.z = camera.camera->get_clip_distance().x * camera.camera->get_clip_distance().y;
		context.scene_constant.camera_clip_distance.w = camera.camera->get_clip_distance().y - camera.camera->get_clip_distance().x;

		context.scene_constant.z_buffer_parameteres.y = camera.camera->get_clip_distance().y / camera.camera->get_clip_distance().x;
		context.scene_constant.z_buffer_parameteres.x = 1.0f - context.scene_constant.z_buffer_parameteres.y;
		context.scene_constant.z_buffer_parameteres.z = context.scene_constant.z_buffer_parameteres.x / camera.camera->get_clip_distance().y;
		context.scene_constant.z_buffer_parameteres.w = context.scene_constant.z_buffer_parameteres.y / camera.camera->get_clip_distance().y;

		context.scene_constant.view_transform = camera.camera->get_view_matrix();
		context.scene_constant.projection_transform = camera.camera->get_projection_matrix();
		context.scene_constant.view_projection_transform = camera.camera->get_view_projection_matrix();

		DirectX::XMMATRIX View, Projection, ViewProjection;
		View = DirectX::XMLoadFloat4x4(&context.scene_constant.view_transform);
		Projection = DirectX::XMLoadFloat4x4(&context.scene_constant.projection_transform);
		ViewProjection = DirectX::XMLoadFloat4x4(&context.scene_constant.view_projection_transform);
		DirectX::XMStoreFloat4x4(&context.scene_constant.inverse_view_transform, DirectX::XMMatrixInverse(nullptr, View));
		DirectX::XMStoreFloat4x4(&context.scene_constant.inverse_projection_transform, DirectX::XMMatrixInverse(nullptr, Projection));
		DirectX::XMStoreFloat4x4(&context.scene_constant.inverse_view_projection_transform, DirectX::XMMatrixInverse(nullptr, ViewProjection));

		context.scene_constant.previous_view_projection_transform = camera.camera->get_previous_view_projection_matrix();
	}
}

void framework::build_render_lists(
	const render_data_context& render_data,
	const collision::frustum_culling& frustum_culling,
	const std::unordered_map<UINT, uint8_t>& occlusion_visible,
	render_mesh_indices& mesh_indices,
	render_decal_indices& decal_indices)
{
	const auto& mesh_container = render_data.mesh_container;
	const auto* draw_data_ptr = mesh_container.draw_datas.data();
	const auto* material_data_ptr = mesh_container.material_datas.data();
	const auto* buffer_data_ptr = mesh_container.buffer_datas.data();
	const auto& color_datas = mesh_container.color_datas;
	const std::size_t count = color_datas.size();

	//	ソート情報
	auto& lengths = mesh_indices.lengths;
	lengths.assign(mesh_container.buffer_datas.size(), 0.0f);
	sort_pipeline_keys.assign(mesh_container.buffer_datas.size(), std::uintptr_t(0));
	const auto& near_plane = frustum_culling.get_plane(collision::frustum_culling::surface_near);

	auto classify = [&](std::size_t list_index, cull_bucket& bucket)
	{
		const std::size_t mesh_index = color_datas[list_index];
		const auto* draw_data = draw_data_ptr + mesh_index;
		const auto* material = (material_data_ptr + draw_data->material_index)->material;
		const auto* buffer_data = buffer_data_ptr + draw_data->buffer_index;

		//	カリング処理
		if (system_utility::is_bitflag(buffer_data->system_flags, buffer_datas::flags::culling))
		{
			//	フラスタムカリング
			if (!frustum_culling.to_aabb(buffer_data->bounds_min, buffer_data->bounds_max))
				return;

			//	オクルージョンカリング（1F前の結果を利用）
			if (system_utility::is_bitflag(buffer_data->system_flags, buffer_datas::flags::static_mesh))
			{
				auto it = occlusion_visible.find(buffer_data->unique_id);
				if (it != occlusion_visible.end() && it->second == 0)
					return;
			}
		}

		//	カメラからの距離算出（AABB 中心）
		DirectX::XMFLOAT3 center;
		center.x = (buffer_data->bounds_min.x + buffer_data->bounds_max.x) * 0.5f;
		center.y = (buffer_data->bounds_min.y + buffer_data->bounds_max.y) * 0.5f;
		center.z = (buffer_data->bounds_min.z + buffer_data->bounds_max.z) * 0.5f;
		lengths[mesh_index] = near_plane.calculate_distance(center);

		//	番号登録
		switch (material->get_blend_mode())
		{
			case material::blend_modes::opaque:
				sort_pipeline_keys[mesh_index] = reinterpret_cast<std::uintptr_t>(material->get_pipeline_state(material::surface_paths::g_buffer_pass).get());
				bucket.opaque.push_back(mesh_index);
				break;
			case material::blend_modes::mask:
				sort_pipeline_keys[mesh_index] = reinterpret_cast<std::uintptr_t>(material->get_pipeline_state(material::surface_paths::g_buffer_pass).get());
				bucket.mask.push_back(mesh_index);
				break;
			case material::blend_modes::translucent:
			case material::blend_modes::addative:
			{
				int path = static_cast<int>(material->get_translucency_path());
				bucket.forward[path].push_back(mesh_index);
				break;
			}
		}
	};

	constexpr std::size_t min_items_per_thread = 512;
	const std::size_t worker_slots = parallel_executor.max_workers();
	if (cull_buckets.size() < worker_slots)
		cull_buckets.resize(worker_slots);
	for (auto& bucket : cull_buckets)
		bucket.clear();

	if (count < min_items_per_thread * 2 || worker_slots <= 1)
	{
		for (std::size_t i = 0; i < count; ++i)
			classify(i, cull_buckets[0]);
	}
	else
	{
		parallel_executor.parallel_for(count, min_items_per_thread,
			[&](std::size_t worker_index, std::size_t begin, std::size_t end)
			{
				cull_bucket& bucket = cull_buckets[worker_index];
				for (std::size_t i = begin; i < end; ++i)
					classify(i, bucket);
			});
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


bool framework::uninitialize()
{
	scene_manager::delete_instance();
	if (object_manager::is_valid())
	{
		object_manager::instance()->finalize();
		object_manager::delete_instance();
	}
	collision_filter_manager::delete_instance();

#if defined(USE_IMGUI)
	editor_items::terminate();
#endif // defined(USE_IMGUI)
#if defined(USE_IMGUI)
	debug_primitive_manager::delete_instance();
#endif // defined(USE_IMGUI)
	ik_resource_manager::delete_instance();
	effect_manager::delete_instance();
	effect_resource_manager::delete_instance();
	model_instance_manger::delete_instance();
	model_resource_manager::delete_instance();
	camera_manager::delete_instance();
	post_process_volume_manager::delete_instance();
	light_manager::delete_instance();
	pipeline_state_manager::delete_instance();
	shader_manager::delete_instance();
	font_data_manager::delete_instance();
	texture_manager::delete_instance();
	keyboard::delete_instance();
	mouse::delete_instance();
	render_data::delete_instance();

	//	文字コード変換器破棄
	system_utility::uninitialize_charset_converter();

	//	COMオブジェクト破棄
	CoUninitialize();
	return true;
}

void framework::calculate_frame_stats()
{
#if defined(_DEBUG) || defined(USE_IMGUI)
	if (++frames, (tictoc.time_stamp() - current_time) >= 1.0f)
	{
		display_frame = static_cast<float>(frames);

		frames = 0;
		previous_time = current_time;
		current_time += 1.0f;
	}
#endif	//	defined(_DEBUG) || defined(USE_IMGUI)
}

bool framework::create_buffers()
{
	render_targets = std::make_shared<scene_render_targets>();
	int scene_width = SCREEN_WIDTH;
	int scene_height = SCREEN_HEIGHT;

	//	バックバッファクリア用
	{
		back_buffer_clear_rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
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
			if (!texture_manager::instance()->generate_texture2d(render_targets->scene_color_texture, common_pipeline_states::scene_color_map_name, texture2d_desc))
			{
				return false;
			}
		}

		//	深度ステンシルバッファを生成
		{
			texture2d_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
			texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

			D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
			shader_resource_view_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
			shader_resource_view_desc.Texture2D.MipLevels = 1;
			if (!texture_manager::instance()->generate_texture2d(render_targets->scene_depth_stencil_texture, common_pipeline_states::scene_depth_map_name, texture2d_desc, &shader_resource_view_desc))
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
				DXGI_FORMAT_R32G32B32A32_FLOAT,		//	Normal_Depth
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	Parameters
				DXGI_FORMAT_R32G32B32A32_FLOAT,		//	Velocity
				DXGI_FORMAT_R32G32B32A32_UINT,		//	UniqueData

				DXGI_FORMAT_R8G8B8A8_UNORM,			//	[エンジン改造:G-Buffer]
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	[エンジン改造:G-Buffer]
			};
			for (int index = 0; index < static_cast<int>(common_pipeline_states::gbuffer_id::max); ++index)
			{
				texture_ptr texture;
				texture2d_desc.Format = formats[index];
				if (texture_manager::instance()->generate_texture2d(texture,
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
		texture2d_desc.Width = 4096;
		texture2d_desc.Height = 4096;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 6;
		texture2d_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = DXGI_FORMAT_R32_FLOAT;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		shader_resource_view_desc.Texture2DArray.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2DArray.MipLevels = 1;
		shader_resource_view_desc.Texture2DArray.FirstArraySlice = 0;
		shader_resource_view_desc.Texture2DArray.ArraySize = texture2d_desc.ArraySize;

		if (!texture_manager::instance()->generate_texture2d(render_targets->shadowmap_depth_texture, common_pipeline_states::shadow_maps_name, texture2d_desc, &shader_resource_view_desc))
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
			if (!texture_manager::instance()->generate_texture2d(render_targets->work_buffer_scene_textures[index], name, texture2d_desc))
			{
				return false;
			}
		}

		static constexpr std::string_view WorkRGBA32FloatName = "Work RGBA32 Float";
		texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		if (!texture_manager::instance()->generate_texture2d(render_targets->work_buffer_rgba32float_texture, WorkRGBA32FloatName, texture2d_desc))
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
			if (!texture_manager::instance()->generate_texture2d(render_targets->work_buffer_half_scene_textures[index], name, texture2d_desc))
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
			if (!texture_manager::instance()->generate_texture2d(render_targets->ssr_accumlate_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
		static constexpr std::string_view SSRCounterTextureName = "SSR Counter Texture";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->ssr_counter_textures); ++index)
		{
			std::string name = std::string(SSRCounterTextureName) + std::to_string(index);
			if (!texture_manager::instance()->generate_texture2d(render_targets->ssr_counter_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
	}

#if defined(USE_IMGUI)
	//	ImGui
	{
		imgui_rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
		imgui_rendering_pass.render_targets.back().clear_color = false;

		DXGI_SWAP_CHAIN_DESC desc;
		render_data::instance()->get_swap_chain()->GetDesc(&desc);

		// ビューポートの設定
		imgui_viewport.TopLeftX = 0.0f;
		imgui_viewport.TopLeftY = 0.0f;
		imgui_viewport.Width = static_cast<float>(desc.BufferDesc.Width);
		imgui_viewport.Height = static_cast<float>(desc.BufferDesc.Height);
		imgui_viewport.MinDepth = 0.0f;
		imgui_viewport.MaxDepth = 1.0f;
	}
#endif // defined(USE_IMGUI)

	return true;
}

bool framework::create_collision_filters()
{
	//	プレイヤー用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("player");
		filter->set_category(collision_category::player);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, true);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	プレイヤー攻撃用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("player_attack");
		filter->set_category(collision_category::player_attack);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, true);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, false);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	エネミー用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("enemy");
		filter->set_category(collision_category::enemy);
		filter->set_flag(collision_category::player, true);
		filter->set_flag(collision_category::player_attack, true);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	エネミー攻撃用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("enemy_attack");
		filter->set_category(collision_category::enemy_attack);
		filter->set_flag(collision_category::player, true);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	ステージ用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("stage");
		filter->set_category(collision_category::stage);
		filter->set_flag(collision_category::player, true);
		filter->set_flag(collision_category::player_attack, true);

		filter->set_flag(collision_category::enemy, true);
		filter->set_flag(collision_category::enemy_attack, true);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, true);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	カメラ用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("camera");
		filter->set_category(collision_category::camera);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	非表示中用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("invisible");
		filter->set_category(collision_category::invisible);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	IK用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("ik");
		filter->set_category(collision_category::ik);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}

	return true;
}
