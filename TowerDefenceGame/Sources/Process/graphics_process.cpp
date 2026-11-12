#include "Process/graphics_process.h"
#include "framework.h"
#include "Graphics/render_data.h"

#include "Graphics/shader.h"
#include "Graphics/global_shader.h"
#include "Material/material_shader_map.h"
#include "Texture/texture_defaults.h"
#include "Texture/texture_importer.h"
#include "Texture/texture_tracker.h"
#include "Graphics/font_data.h"
#include "Resource/resource_manager.h"
#include "Model/model_resource.h"
#include "Model/model_buffer_pool.h"
#include "Effect/effect_resource.h"
#include "Animation/ik_resource.h"
#include "Renderer/render_system.h"
#include "Framework/display_settings.h"
#include "Framework/app_window.h"
#include "Editor/detail_profiler.h"

bool graphics_process::enter()
{
	framework& self = get_owner();

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
		asset_registry::instance()->scan({ asset_registry::root_directory, asset_registry::engine_root_directory });
		resource_manager::create_instance();
		model_resource_manager::create_instance();
		ik_resource_manager::create_instance();	//	@remainder	これは不要では？
		font_data_manager::create_instance();
	}

	//	エフェクト関係
	effect_resource_manager::create_instance();

	self.renderer = std::make_unique<render_system>(self.hwnd);

	display_settings::create_instance();
	if (const std::optional<display_settings::change_request> saved = display_settings::instance()->get_saved_request())
		apply_display(*saved);
	return true;
}

void graphics_process::exit()
{
	framework& self = get_owner();

	display_settings::delete_instance();
	self.renderer.reset();
	global_shader_base::release_all();

	//	描画関係
	{
		font_data_manager::delete_instance();
		ik_resource_manager::delete_instance();
		effect_resource_manager::delete_instance();
		resource_manager::delete_instance();
		asset_registry::delete_instance();
		model_resource_manager::delete_instance();
		model_buffer_pool_manager::delete_instance();
		material_shader_map::delete_instance();
		pipeline_state_manager::delete_instance();
		shader_manager::delete_instance();
#if defined(USE_IMGUI)
		texture_importer::delete_instance();
#endif	//	defined(USE_IMGUI)
		texture_defaults::delete_instance();
#if defined(USE_IMGUI)
		texture_tracker::delete_instance();
#endif	//	defined(USE_IMGUI)
		render_data::delete_instance();
	}
}

void graphics_process::update(float elapsed_time)
{
	//	解像度と表示の方法の変更
	if (display_settings::is_valid())
	{
		if (const std::optional<display_settings::change_request> request = display_settings::instance()->take_request())
			apply_display(*request);
	}

	//	リソースの構築と読込完了の通知
	if (resource_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.resource_manager");
		resource_manager::instance()->update();
	}
}

bool graphics_process::apply_display(const display_settings::change_request& request)
{
	framework& self = get_owner();
	display_settings& settings = *display_settings::instance();

	//	全プロセスの描画情報解放
	for (framework::process_entry& entry : self.processes)
		entry.process->release_display_resources();

	//	ウィンドウモードを変更
	self.window.apply_window_mode(request.mode, static_cast<int>(request.size.width), static_cast<int>(request.size.height));

	//	バックバッファリサイズ
	const bool succeeded = render_data::instance()->resize_back_buffer(request.size.width, request.size.height);
	if (!succeeded)
	{
		const display_settings::resolution& current = settings.get_current_resolution();
		self.window.apply_window_mode(settings.get_window_mode(), static_cast<int>(current.width), static_cast<int>(current.height));
	}

	//	全プロセスの描画情報生成
	for (framework::process_entry& entry : self.processes)
	{
		if (!entry.process->create_display_resources())
			DEBUG_LOG_CATEGORY(logger::categorys::system, "create_display_resources Failed");
	}

	if (succeeded)
		settings.set_current(request.size, request.mode);
	return succeeded;
}
