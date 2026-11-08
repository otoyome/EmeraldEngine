#include "Process/boot_process.h"
#include "Process/game_process.h"
#include "framework.h"
#include "Texture/texture_factory.h"
#include "Graphics/render_data.h"

#include <algorithm>
#include <future>
#include <thread>

#include "Graphics/shader.h"
#include "Material/material_shader_map.h"
#include "Texture/texture.h"
#include "Texture/texture_defaults.h"
#include "Texture/texture_importer.h"
#include "Texture/texture_tracker.h"
#include "Graphics/font_data.h"
#include "Graphics/camera.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/clustered_lighting_system.h"
#include "Resource/resource_manager.h"
#include "Model/model_resource.h"
#include "Model/model_buffer_pool.h"
#include "Model/model_instance.h"
#include "Effect/effect_resource.h"
#include "Effect/effect_instance.h"
#include "Audio/audio_manager.h"
#include "Audio/sound_player.h"
#include "Input/input_action_system.h"

#include "Animation/ik_resource.h"

#include "scenes/load_shader_scene.h"

#include "Renderer/scene_renderer.h"
#include "Renderer/scene_renderer_forward.h"
#include "Renderer/scene_renderer_postprocess.h"
#include "Renderer/scene_renderer_display.h"
#include "Renderer/ui_renderer.h"
#include "Renderer/world_renderer.h"

#include "Object/object_manager.h"
#include "Object/world.h"
#include "Object/GameObjects/UI/ui_object.h"

#include "System/engine_config_data.h"
#include "System/game_config_data.h"

#if defined(USE_IMGUI)
#include "System/editor_config_data.h"
#include "Graphics/debug_primitive_collector.h"
#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"
#endif // defined(USE_IMGUI)
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"
#include "Editor/texture_editor.h"

#include "Editor/detail_profiler.h"
#include "System/console_manager.h"
#include "System/project_path.h"


bool boot_process::enter()
{
	framework& self = get_owner();

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

	//	プロジェクトのルートディレクトリを控える
	project_path::initialize();

	//	ゲームコンフィグ初期化
	{
		//	@remainder	内部でコンソールマネージャーに情報を受け渡しているだけ。
		game_config_data::create_instance();
	}

	//	描画関係初期化
	{
		render_data::create_instance(self.hwnd);
		if (!render_data::instance()->is_supported_device())
		{
			MessageBoxW(self.hwnd, L"この GPU は必要な機能(VS からの描画先配列番号の出力)に対応していないため起動できません。", L"起動エラー", MB_OK | MB_ICONERROR);
			return false;
		}
		texture_defaults::create_instance();
#if defined(USE_IMGUI)
		texture_tracker::create_instance();
		texture_importer::create_instance();
#endif	//	defined(USE_IMGUI)
		shader_manager::create_instance();
		pipeline_state_manager::create_instance();
		material_shader_map::create_instance();
		model_buffer_pool_manager::create_instance();
		asset_registry::create_instance();
		asset_registry::instance()->scan();
		resource_manager::create_instance();
		model_resource_manager::create_instance();
		ik_resource_manager::create_instance();	//	@remainder	これは不要では？
		font_data_manager::create_instance();
	}

	//	入力関係初期化
	{
		mouse::create_instance(self.hwnd);
		keyboard::create_instance();
		gamepad::create_instance();
		input::action_system::create_instance();
	}

	//	エフェクト関係
	{
		effect_resource_manager::create_instance();
	}

#if defined(USE_IMGUI)
	//	エディター/デバッグ関係
	{
		editor_config_data::create_instance();
		detail_profiler::create_instance();
		debug_primitive_manager::create_instance();
		editor_items::initialize();
	}
#endif // defined(USE_IMGUI)

	//	サウンド関係
	{
		audio_manager::create_instance();
		audio::sound_player::create_instance();
	}

	//	シーン関係
	{
		collision_filter_manager::create_instance();
	}

	//	各種バッファ用意
	if (!self.create_buffers())
		return false;

	//	当たり判定フィルター用意
	if (!self.create_collision_filters())
		return false;


	//	描画ジョブの実行者
	self.render_executor = std::make_shared<render_job_executor>(4);

	//	全描画コマンド処理後に呼び出される関数登録
	self.render_executor->register_post_rendering(
		[&self]
		{
#if defined(USE_IMGUI)
			//	ImGui描画
			{
				self.imgui_rendering_pass.activate(render_data::instance()->get_immediate_context().Get());
				render_data::instance()->get_immediate_context()->RSSetViewports(1, &self.imgui_viewport);
					
				ImGui::Render();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				self.imgui_rendering_pass.deactivate(render_data::instance()->get_immediate_context().Get());

				if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
				}

				//	ImGui の描画が終わったので、表示テクスチャを手放す
				imgui_texture_keeper::instance()->release();
			}
#endif // defined(USE_IMGUI)

			//	プレゼント
			UINT sync_interval{ engine_config_data::instance()->windows.vertical_sync };
			render_data::instance()->get_swap_chain()->Present(sync_interval, 0);
		});

	return true;
}

std::unique_ptr<framework_process> boot_process::next_process()
{
	return std::make_unique<game_process>(get_owner());
}
