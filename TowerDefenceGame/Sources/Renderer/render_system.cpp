#include "Renderer/render_system.h"
#include "render_data.h"

#include <algorithm>
#include <thread>

#include "texture.h"
#include "camera.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/clustered_lighting_system.h"
#include "graphics/shadow_atlas.h"
#include "model_resource.h"
#include "model_buffer_pool.h"
#include "Graphics/material_constant_buffer_pool.h"
#include "model_instance.h"
#include "material.h"
#include "effect_resource.h"
#include "effect_instance.h"

#include "Renderer/scene_render_features.h"
#include "Renderer/scene_renderer_common.h"

#include "Object/object_manager.h"
#include "Object/GameObjects/UI/ui_object.h"

#include "System/engine_config_data.h"
#include "System/world_viewport.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif // defined(USE_IMGUI)

#include "Editor/detail_profiler.h"
#include "System/console_manager.h"

namespace
{
	//	view_stateの破棄フレーム数
	constexpr uint64_t scene_view_state_keep_frames = 60;

	//	@tool 描画先のビューを作る（texture が null の時はバックバッファ）
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> create_render_target_view(const texture_ptr& texture)
	{
		HRESULT hr = S_OK;
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		if (texture)
		{
			texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());
		}
		else
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
			hr = render_data::instance()->get_swap_chain()->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(back_buffer.GetAddressOf()));
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			resource = back_buffer;
		}

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
		hr = render_data::instance()->get_device()->CreateRenderTargetView(resource.Get(), nullptr, view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		return view;
	}

	//	@tool 視点の出力先の描画先とビューポートを作る（texture が null の時はバックバッファ）
	scene_rendering::scene_output make_scene_output(const texture_ptr& texture)
	{
		scene_rendering::scene_output output;
		output.texture = texture;
		output.view = create_render_target_view(texture);

		if (auto texture_2d = std::dynamic_pointer_cast<texture2d>(texture))
		{
			output.viewport.Width = static_cast<float>(texture_2d->get_desc().Width);
			output.viewport.Height = static_cast<float>(texture_2d->get_desc().Height);
		}
		else
		{
			DXGI_SWAP_CHAIN_DESC desc;
			render_data::instance()->get_swap_chain()->GetDesc(&desc);
			output.viewport.Width = static_cast<float>(desc.BufferDesc.Width);
			output.viewport.Height = static_cast<float>(desc.BufferDesc.Height);
		}
		output.viewport.MinDepth = 0.0f;
		output.viewport.MaxDepth = 1.0f;
		return output;
	}

	//	描画関係の設定
#if defined(USE_IMGUI)
	console_variable<bool> freeze_culling_variable("r.culling.freeze", false, "カリング結果を固定する（メイン視点）");
	console_variable<bool> visualize_meshlets_variable("r.meshlet.visualize", false, "メッシュレット単位で色分け表示する（メイン視点）");
#endif	//	defined(USE_IMGUI)
}

render_system::render_system() = default;

render_system::~render_system()
{
	stop_render_threads();
}

bool render_system::initialize()
{
	//	各種バッファ用意
	if (!create_buffers())
		return false;

	//	シーンレンダラー登録
	{
		//	描画コマンドマルチスレッド生成
		render_executor = std::make_shared<render_job_executor>(4);

		//	フレームグラフ生成
		frame_render_graph = std::make_shared<frame_graph>();

		//	描画機能ごとの共有リソース
		scene_features = std::make_unique<scene_rendering::scene_render_features>();

		main_view_target = std::make_shared<view_target>();
		main_view_target->name = "main";
		main_view_target->state_key = main_scene_view_state_key;
		main_view_target->width = SCREEN_WIDTH;
		main_view_target->height = SCREEN_HEIGHT;
		main_view_target->output = make_scene_output(nullptr);
		main_view_target->scene_constant_buffer = main_scene_constant_buffer;
	}

	//	@tool 描画側が参照している資源を書き換える前に、描画中のフレームを待てるようにする
	main_thread_id = std::this_thread::get_id();
	render_data::instance()->set_flush_rendering_function([this] { flush_rendering(); });
	render_data::instance()->set_retire_function([this](std::shared_ptr<const void> object) { retire_after_rendering(std::move(object)); });
	return true;
}

void render_system::begin_frame(float elapsed_time)
{
	//	描画コマンドの生成をし終わったフレームの描画データをメインスレッドで破棄
	{
		std::vector<frame_packet_ptr> finished_packets;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			finished_packets.swap(retired_packets);
		}
	}

	auto packet = std::make_shared<frame_packet>();
	packet->frame_index = ++dispatched_frame_index;
	packet->elapsed_time = elapsed_time;
	packet->previous_time = total_time;
	total_time += elapsed_time;
#if defined(USE_IMGUI)
	packet->freeze_culling = freeze_culling_variable.get();
	packet->visualize_meshlets = visualize_meshlets_variable.get();
#endif	//	defined(USE_IMGUI)
	packet->sync_interval = static_cast<UINT>(engine_config_data::instance()->windows.vertical_sync);
	packet->antialias_mode = engine_config_data::instance()->antialias.mode;
	//	@tool これより前に抽出したフレームは、このフレームより先に提出し終わるので、このフレームと一緒に破棄すればよい
	packet->retired_objects.swap(pending_retired_objects);
	extracting_packet = std::move(packet);
}

void render_system::retire_after_rendering(std::shared_ptr<const void> object)
{
	//	@tool 描画スレッドが動いていない時は描画中のフレームが無いので、ここですぐ破棄する
	if (!render_threads_running || std::this_thread::get_id() != main_thread_id)
		return;
	pending_retired_objects.emplace_back(std::move(object));
}

void render_system::add_view(const view_target_ptr& target, view_parameters& parameters, bool& camera_cut, bool in_window)
{
	_ASSERT_EXPR(extracting_packet, L"begin_frame が呼ばれていない");
	extracting_packet->views.emplace_back(extract_view(target, parameters, camera_cut, in_window));
}

void render_system::set_overlay(overlay_function overlay)
{
	_ASSERT_EXPR(extracting_packet, L"begin_frame が呼ばれていない");
	extracting_packet->overlay = std::move(overlay);
}

void render_system::end_frame()
{
	_ASSERT_EXPR(extracting_packet, L"begin_frame が呼ばれていない");
	dispatch_frame(std::move(extracting_packet));
	extracting_packet.reset();
}

void render_system::build_particle_render_lists(const view_render_data& render_data, render_particle_indices& particle_indices)
{
	for (auto& particle_index : render_data.effect_container.particle_color_items)
	{
		auto& material = render_data.effect_container.particle_draw_items[particle_index].material;

		//	番号登録
		switch (material->get_blend_mode())
		{
			case material::blend_modes::opaque:
			{
				particle_indices.opaque.push_back(particle_index);
				break;
			}
			case material::blend_modes::mask:
			{
				particle_indices.mask.push_back(particle_index);
				break;
			}
			case material::blend_modes::translucent:
			case material::blend_modes::addative:
			{
				int index = static_cast<int>(material->get_translucency_path());
				particle_indices.forward[index].push_back(particle_index);
				break;
			}
		}
	}
}

render_system::view_packet render_system::extract_view(const view_target_ptr& target, view_parameters& parameters, bool& camera_cut, bool in_window)
{
	//	@tool 処理中のワールドから、1つの視点の描画に必要な情報を抽出する
	auto render_data = std::make_shared<view_render_data>();
	auto render_particle_indices = std::make_shared<::render_particle_indices>();
	{
		DETAIL_PROFILE_CPU_SCOPE("render.collect_render_data");

		//	描画設定を取得
		clustered_lighting_system::capture_settings(render_data->settings.clustered_lighting);
		shadow_atlas::capture_settings(render_data->settings.shadow_atlas);

		//	視点の候補から今フレームのメイン視点を選ぶ
		view_selection selection;
		camera_cut = false;
		if (camera_manager::instance()->select_view(selection))
		{
			parameters = selection.parameters;
			camera_cut = selection.camera_cut;
		}

		//	@tool ImGui のウィンドウに映す場合は、投影をウィンドウの大きさに合わせる
		if (in_window)
			world_viewport::instance()->apply_screen_rect(parameters);

		//	相対描画用の描画原点
		const math::vector3d render_origin = scene_view::compute_render_origin(parameters, engine_config_data::instance()->rendering.camera_relative_rendering);
		render_data->render_origin = render_origin;

		//	ライト情報収集
		{
			//	メイン視点の視錐台と距離・画面上の大きさで間引く
			const scene_view collect_view = scene_view::from_parameters(parameters, render_origin);
			light_view_info view_info;
			view_info.position = parameters.position;
			view_info.tan_half_fov_y = std::tan(parameters.fov_y * 0.5f);
			view_info.frustum = &collect_view.get_frustum_culling();
			light_manager::instance()->get_container(render_data->light_container, render_origin, view_info);
		}

		//	ポストプロセスボリューム取得
		post_process_volume_manager::instance()->get_container(render_data->post_process_volume_container, parameters.position);

		//	メッシュ取得
		model_instance_manger::instance()->get_container(render_data->mesh_container, render_origin);

		//	エフェクトコンテナ取得
		effect_manager::instance()->get_container(render_data->effect_container, render_data->decal_container, render_origin);

		//	UIオブジェクトコンテナ収集
		//	@tool UI はメイン画面にだけ描く
		if (!in_window)
		{
			render_data->sprite_container.draw_items.clear();

			int sort_key = 0;
			auto ui_object_list = object_manager::instance()->search_objects<ui_object>();
			for (auto& ui_object : ui_object_list)
			{
				if (!ui_object->is_active())
					continue;

				sprite_draw_item sprite_data;
				sprite_data.sort_key = sort_key;
				sprite_data.sort_order = ui_object->get_sort_order();
				sprite_data.sprite = ui_object->get_sprite().make_render_proxy();
				render_data->sprite_container.draw_items.emplace_back(sprite_data);
				++sort_key;
			}

			auto ui_objects_list = object_manager::instance()->search_objects<ui_objects>();
			for (auto& ui_object : ui_objects_list)
			{
				if (!ui_object->is_active())
					continue;

				for (auto& sprite : ui_object->get_sprites())
				{
					sprite_draw_item sprite_data;
					sprite_data.sort_key = sort_key;
					sprite_data.sort_order = ui_object->get_sort_order();
					sprite_data.sprite = sprite.make_render_proxy();
					render_data->sprite_container.draw_items.emplace_back(sprite_data);
				}
				++sort_key;
			}

			//	ソート
			std::sort(render_data->sprite_container.draw_items.begin(), render_data->sprite_container.draw_items.end(),
				[](const sprite_draw_item& l, const sprite_draw_item& r)
				{
					if (l.sort_order < r.sort_order)
						return l.sort_key != r.sort_key;
					return false;
				});
		}

		//	パーティクルの描画振り分け
		build_particle_render_lists(*render_data, *render_particle_indices);

		//	モデルの頂点/インデックスバッファ確保分をGPUへ転送
		model_buffer_pool_manager::instance()->flush_uploads(render_data->upload_list);
		//	@tool 描画側が使うプールの SRV を写す（この後にプールが大きくなっても、このフレームは写した物を使う）
		scene_renderer_commons::capture_meshlet_attribute_srvs(render_data->mesh_container);

		//	@tool パイプラインステートの描画前準備（描画ジョブ内ではメンバを書き換えない）
		pipeline_state_manager::instance()->prepare_pipeline_states();

		//	@tool マテリアルの定数バッファ変更分を転送一覧に積む（描画ジョブ内では更新しない）
		material_constant_buffer_pool::instance()->flush_uploads(render_data->upload_list);

#if defined(USE_IMGUI)
		//	デバッグ情報取得
		if (debug_primitives)
			debug_primitives(*render_data);
		debug_primitive_manager::instance()->get_container(render_data->debug_draw_container, render_data->render_origin, render_data->upload_list);
#endif // defined(USE_IMGUI)
	}

	view_packet view;
	view.target = target;
	view.render_data = render_data;
	view.particle_indices = render_particle_indices;
	view.parameters = parameters;
	view.camera_cut = camera_cut;
	//	@tool 描画側はワールドの管理クラスを instance() で引かず、抽出時のワールドの物を使う
	view.effects = effect_manager::instance();
	return view;
}

void render_system::render_frame(frame_packet& packet)
{
	const float elapsed_time = packet.elapsed_time;
	const uint64_t frame_index = packet.frame_index;

	//	バッファ描画
	{
		//	レンダラー開始処理
		render_executor->begin_command_list();

		frame_render_graph->begin_frame();

		//	フレームの転送
		for (const view_packet& view : packet.views)
		{
			const std::shared_ptr<view_render_data>& render_data = view.render_data;
			renderer_context context = {};
			context.render_data = render_data;

			struct pass_data
			{
				std::shared_ptr<view_render_data> render_data;
			};
			frame_render_graph->add_pass<pass_data>("frame_upload_pass", context, [&](frame_graph::builder& builder, pass_data& data)
			{
				builder.use_immediate_context(false);
				data.render_data = render_data;
			},
			[](const pass_data& data, const frame_graph::resources&, render_job_context& context)
			{
				data.render_data->upload_list.upload(context.recorder);
				data.render_data->mesh_container.transform_uploads.upload(context.recorder);
			});
		}

		//	描画準備
		{
			renderer_context context = {};
			context.render_data = packet.views.front().render_data;
			struct pass_data {};
			frame_render_graph->add_pass<pass_data>("begin_draw_pass", context, [&](frame_graph::builder& builder, pass_data&)
			{
				//	@tool バックバッファ（グラフの外）をクリアする（クリアはグラフが描画先を設定する時に行う）
				frame_graph::color_attachment back_buffer;
				back_buffer.external_view = back_buffer_view;
				back_buffer.clear = true;
				builder.set_render_targets({ back_buffer });
			},
			[](const pass_data&, const frame_graph::resources&, render_job_context&)
			{
			});
		}

		for (std::size_t view_index = 0; view_index < packet.views.size(); ++view_index)
		{
			const view_packet& view = packet.views[view_index];
			const std::shared_ptr<view_render_data>& render_data = view.render_data;
			const std::shared_ptr<::render_particle_indices>& render_particle_indices = view.particle_indices;
			view_target& target = *view.target;

			//	@tool 視点で描画（視点ごとの状態・定数バッファ・レンダラー一式を使う）
			{
				const scene_view_state_ptr& state = scene_view_states.acquire(target.state_key, render_frame_index);
#if defined(USE_IMGUI)
				state->set_freeze_culling(packet.freeze_culling);
				state->set_debug_visualize_meshlets(packet.visualize_meshlets);
#endif	//	defined(USE_IMGUI)
				const scene_view_ptr scene_view_data = std::make_shared<const scene_view>(scene_view::from_parameters(view.parameters, render_data->render_origin));
				state->begin_frame(*scene_view_data, view.camera_cut);

				//	レンダラーコンテキスト準備
				renderer_context context = {};
				{
					context.render_data = render_data;
					context.render_mesh_indices = std::make_shared<render_mesh_indices>();
					context.render_particle_indices = render_particle_indices;
					context.render_decal_indices = std::make_shared<render_decal_indices>();
					prepare_renderer_context(context, scene_view_data, state, target.scene_constant_buffer, render_data, render_particle_indices, packet.previous_time, packet.previous_time + elapsed_time);
				}

				//	オクルージョンカリング結果を読み出す
				std::unordered_map<UINT, uint8_t> occlusion_visible;
				if (const auto& occlusion = state->get_occlusion_culling())
					occlusion->read_last_frame(occlusion_visible);

				//	クラスタードライティングのライト番号総数を読み出す
				if (const auto& clustered_lighting = state->get_clustered_lighting())
					clustered_lighting->read_statistics();

				//	描画メッシュ／デカールのカリング・ソート
				{
					DETAIL_PROFILE_CPU_SCOPE("render.build_render_lists");
					build_render_lists(*render_data, *state, scene_view_data->get_frustum_culling(), occlusion_visible,
						*context.render_mesh_indices, *context.render_decal_indices);
				}

				//	シーン構築
				//	@tool エフェクトの更新（GPU）はエフェクトのインスタンスを読むので、終わったらメインスレッドへ知らせる（最後の視点の分が終われば全て終わっている）
				//	@tool 記録中のジョブとは CPU 側の情報を取り合わないので、記録の完了を待たずに実行する
				{
					const bool notify_effect_update = view_index + 1 == packet.views.size();
					//	@tool エフェクトの更新完了の通知は render_system の同期用の情報を使う
					struct pass_data { std::function<void()> notify; effect_manager* effects = nullptr; };
					frame_render_graph->add_pass<pass_data>("scene_constantract_pass", context, [&](frame_graph::builder& builder, pass_data& data)
					{
						builder.use_immediate_context(false);
						data.effects = view.effects;
						if (notify_effect_update)
						{
							data.notify = [this, frame_index]
							{
								{
									std::lock_guard<std::mutex> lock(frame_mutex);
									effect_updated_frame_index = frame_index;
								}
								frame_condition.notify_all();
							};
						}
					},
					[](const pass_data& data, const frame_graph::resources&, render_job_context& context)
					{
						//	シーン定数バッファ更新
						context.recorder.update_subresource(context.scene_constant_buffer.Get(), 0, 0, &context.scene_constant, 0, 0);

						//	エフェクト類のGPU側の更新
						context.recorder.begin_event(L"update_effect_gpu");
						data.effects->update_gpu(context, context.scene_constant.time_data.x - context.scene_constant.time_data.y);
						context.recorder.end_event();

						if (data.notify)
							data.notify();
					});
				}

				//	描画
				{
					DETAIL_PROFILE_CPU_SCOPE("render.scene_renderers_issue");
					//	@tool 視点のシーンのパスを実行順に足す（共有資源と視点ごとの資源を使う）
					scene_rendering::scene_frame scene_frame;
					scene_frame.context = context;
					scene_frame.config.width = static_cast<UINT>(target.width);
					scene_frame.config.height = static_cast<UINT>(target.height);
					scene_frame.output = target.output;
					scene_frame.shadow_map = shadow_map_texture;
					//	@tool シーン色・深度・G-Buffer はこのフレームの一時テクスチャとして作る
					scene_frame.textures = scene_renderer_commons::create_scene_textures(*frame_render_graph, scene_frame.config, scene_frame.shadow_map, scene_frame.output.texture);
					scene_rendering::add_scene_passes(*frame_render_graph, scene_frame, *scene_features);
				}

				//	描画終了処理
				state->end_frame(*scene_view_data);
			}

			//	UI描画
			//	@tool UI はメイン画面（先頭の視点）にだけ描く
			if (view_index == 0)
			{
				renderer_context context = {};
				context.scene_constant_buffer = ui_scene_constant_buffer;
				context.render_data = render_data;
				context.render_mesh_indices = std::make_shared<render_mesh_indices>();
				DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_issue, "ui");
				frame_graph::owner_scope owner(*frame_render_graph, "ui");
				scene_rendering::add_ui_pass(*frame_render_graph, context, target.output, *scene_features);
			}
		}

		//	描画コマンドを実行
		{
			DETAIL_PROFILE_CPU_SCOPE("render.execute_commands");

			//	
			frame_render_graph->execute(*render_executor);

			//	生成されたコマンドリスト保存
			packet.command_list = render_executor->end_command_list();
		}
	}

	//	使われなくなった view_state を破棄
	scene_view_states.release_unused(render_frame_index, scene_view_state_keep_frames);
	++render_frame_index;
}

void render_system::submit_frame(frame_packet& packet)
{
	ID3D11DeviceContext* immediate_context = render_data::instance()->get_immediate_context().Get();

	//	@tool 提出物を順に実行する（記録が終わっていないものは待つ）
	{
		DETAIL_PROFILE_CPU_SCOPE("render.execute_command_list");
		packet.command_list->execute(immediate_context);
	}

	//	@tool バックバッファへ重ねて描く（ImGui）
	if (packet.overlay)
		packet.overlay(immediate_context, back_buffer_view.Get());

	//	プレゼント
	render_data::instance()->get_swap_chain()->Present(packet.sync_interval, 0);
}

void render_system::dispatch_frame(frame_packet_ptr packet)
{
	{
		std::unique_lock<std::mutex> lock(frame_mutex);
		frame_condition.wait(lock, [&] { return frames_in_flight < max_frames_in_flight; });
		++frames_in_flight;
		render_queue.emplace_back(std::move(packet));
	}
	frame_condition.notify_all();
}

void render_system::flush_rendering()
{
	//	@tool 描画スレッド・提出スレッドから呼ばれた場合は待たない（自分自身を待つことになるため）
	if (std::this_thread::get_id() != main_thread_id)
		return;

	std::unique_lock<std::mutex> lock(frame_mutex);
	frame_condition.wait(lock, [&] { return frames_in_flight == 0; });
}

void render_system::wait_effect_update()
{
	std::unique_lock<std::mutex> lock(frame_mutex);
	frame_condition.wait(lock, [&] { return effect_updated_frame_index >= dispatched_frame_index || submitted_frame_index >= dispatched_frame_index; });
}

void render_system::start_render_threads()
{
	if (render_threads_running)
		return;

	render_threads_running = true;
	render_thread = std::thread(&render_system::render_thread_main, this);
	submit_thread = std::thread(&render_system::submit_thread_main, this);
}

void render_system::stop_render_threads()
{
	if (!render_threads_running)
		return;

	flush_rendering();
	{
		std::lock_guard<std::mutex> lock(frame_mutex);
		render_threads_running = false;
	}
	frame_condition.notify_all();
	render_thread.join();
	submit_thread.join();
	retired_packets.clear();
	pending_retired_objects.clear();
}

void render_system::render_thread_main()
{
	while (true)
	{
		frame_packet_ptr packet;
		{
			std::unique_lock<std::mutex> lock(frame_mutex);
			frame_condition.wait(lock, [&] { return !render_queue.empty() || !render_threads_running; });
			if (render_queue.empty())
				break;
			packet = std::move(render_queue.front());
			render_queue.pop_front();
		}

		//	@tool 描画パスを組み立てて記録を始め、提出スレッドへ渡す
		render_frame(*packet);
		const uint64_t frame_index = packet->frame_index;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			submit_queue.emplace_back(std::move(packet));
		}
		frame_condition.notify_all();

		//	@tool レンダラーの状態は提出中のパスも参照するので、提出が終わってから次のフレームを組み立てる
		std::unique_lock<std::mutex> lock(frame_mutex);
		frame_condition.wait(lock, [&] { return submitted_frame_index >= frame_index; });
	}
}

void render_system::submit_thread_main()
{
	while (true)
	{
		frame_packet_ptr packet;
		{
			std::unique_lock<std::mutex> lock(frame_mutex);
			frame_condition.wait(lock, [&] { return !submit_queue.empty() || (!render_threads_running && render_queue.empty() && frames_in_flight == 0); });
			if (submit_queue.empty())
				break;
			packet = std::move(submit_queue.front());
			submit_queue.pop_front();
		}

		{
			std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
			submit_frame(*packet);
		}

		const uint64_t frame_index = packet->frame_index;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			//	@tool 描画データの破棄（モデルリソースなどのデストラクタ）はメインスレッドで行う
			retired_packets.emplace_back(std::move(packet));
			submitted_frame_index = frame_index;
			--frames_in_flight;
		}
		frame_condition.notify_all();
	}
}

void render_system::prepare_renderer_context(
	renderer_context& context,
	const scene_view_ptr& target_view,
	const scene_view_state_ptr& target_state,
	const Microsoft::WRL::ComPtr<ID3D11Buffer>& scene_constant_buffer,
	const std::shared_ptr<view_render_data>& render_data,
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
	}

	//	描画視点情報と描画制御情報設定
	{
		context.current_view = target_view;
		context.current_view_state = target_state;
	}
}

void render_system::build_render_lists(
	const view_render_data& render_data,
	scene_view_state& target_state,
	const collision::frustum_culling& frustum_culling,
	const std::unordered_map<UINT, uint8_t>& occlusion_visible,
	render_mesh_indices& mesh_indices,
	render_decal_indices& decal_indices)
{
	const auto& mesh_container = render_data.mesh_container;
	const auto* draw_item_ptr = mesh_container.draw_items.data();
	const auto& color_items = mesh_container.color_items;
	const std::size_t count = color_items.size();

	//	ソート情報
	auto& lengths = mesh_indices.lengths;
	lengths.assign(mesh_container.draw_items.size(), 0.0f);
	sort_pipeline_keys.assign(mesh_container.draw_items.size(), std::uintptr_t(0));
	const auto& near_plane = frustum_culling.get_plane(collision::frustum_culling::surface_near);

	//	描画判定
	auto draw_classify = [&](std::size_t list_index, cull_bucket& bucket)
	{
		const std::size_t mesh_index = color_items[list_index];
		const auto* draw_item = draw_item_ptr + mesh_index;
		const auto* material = draw_item->material.get();

		//	カリング処理
		if (system_utility::is_bitflag(draw_item->system_flags, mesh_draw_item::flags::culling))
		{
			//	フラスタムカリング
			bool visible = frustum_culling.to_aabb(draw_item->bounds_min, draw_item->bounds_max);

			//	オクルージョンカリング
			if (visible)
			{
				auto it = occlusion_visible.find(draw_item->unique_id);
				if (it != occlusion_visible.end() && it->second == 0)
					visible = false;
			}

#if defined(USE_IMGUI)
			//	判定結果を過去のもので固定
			visible = target_state.resolve_debug_visible(draw_item->unique_id, visible);
#endif	//	defined(USE_IMGUI)
			if (!visible)
				return;
		}
		//	カメラからの距離算出
		math::float3 center;
		center.x = (draw_item->bounds_min.x + draw_item->bounds_max.x) * 0.5f;
		center.y = (draw_item->bounds_min.y + draw_item->bounds_max.y) * 0.5f;
		center.z = (draw_item->bounds_min.z + draw_item->bounds_max.z) * 0.5f;
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
	const auto& decal_draw_items = render_data.decal_container.draw_items;
	decal_indices.draw_list.reserve(decal_draw_items.size());
	for (UINT decal_index = 0; decal_index < decal_draw_items.size(); ++decal_index)
	{
		//	フラスタムカリング
		if (!frustum_culling.to_box(decal_draw_items[decal_index].box))
			continue;

		decal_indices.draw_list.emplace_back(decal_index);
	}
}

bool render_system::create_buffers()
{
	int scene_width = SCREEN_WIDTH;
	int scene_height = SCREEN_HEIGHT;

	//	バックバッファクリア用
	//	@tool ImGui もこのビューに描く
	{
		back_buffer_view = create_render_target_view(nullptr);
	}

	//	UI描画用のシーン定数バッファ生成(viewport_rectのみ使用するため他の項目は未使用)
	{
		HRESULT hr;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, ui_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		//	viewport_rectは変化しないので初回のみアップロードしておく
		common_pipeline_states::scene_constants ui_scene_constant{};
		ui_scene_constant.viewport_rect = { 0.0f, 0.0f, static_cast<float>(scene_width), static_cast<float>(scene_height) };
		render_data::instance()->get_immediate_context()->UpdateSubresource(ui_scene_constant_buffer.Get(), 0, 0, &ui_scene_constant, 0, 0);
	}

	//	メイン視点のシーン定数バッファ生成
	{
		HRESULT hr;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, main_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	//	@tool シーン用のテクスチャ（色・深度・G-Buffer）はフレームグラフの一時テクスチャとして毎フレーム要求する（ここでは作らない）

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

		if (!texture_manager::instance()->generate_texture2d(shadow_map_texture, common_pipeline_states::shadow_maps_name, texture2d_desc, &shader_resource_view_desc))
		{
			return false;
		}
	}

	return true;
}

render_system::view_target_ptr render_system::create_view_target(std::string_view name, int width, int height)
{
	auto target = std::make_shared<view_target>();
	target->name = name;
	target->state_key = scene_view_state_cache::make_key(name);
	target->width = width;
	target->height = height;

	//	@tool シーン用のテクスチャはフレームグラフの一時テクスチャ、影はメイン画面の物を共有する（どちらも視点ごとに描き直す）

	//	@tool 最終的な出力先（ImGui で表示する。バックバッファと同じ形式）
	texture_ptr output_texture;
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (!texture_manager::instance()->generate_unregistered_texture2d(output_texture, texture2d_desc))
			return nullptr;
	}

	//	@tool 視点のシーン定数バッファ
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, target->scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return nullptr;
	}

	//	@tool 出力先の描画先を作り置く
	target->output = make_scene_output(output_texture);
	return target;
}
