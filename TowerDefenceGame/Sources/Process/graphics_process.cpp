#include "Process/graphics_process.h"
#include "framework.h"
#include "Graphics/render_data.h"

#include "Graphics/shader.h"
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
		asset_registry::instance()->scan();
		resource_manager::create_instance();
		model_resource_manager::create_instance();
		ik_resource_manager::create_instance();	//	@remainder	これは不要では？
		font_data_manager::create_instance();
	}

	//	エフェクト関係
	effect_resource_manager::create_instance();

	self.renderer = std::make_unique<render_system>();
	return true;
}

void graphics_process::exit()
{
	framework& self = get_owner();

	self.renderer.reset();

	if (render_data::is_valid())
	{
		BOOL fullscreen{};
		render_data::instance()->get_swap_chain()->GetFullscreenState(&fullscreen, 0);
		if (fullscreen)
		{
			render_data::instance()->get_swap_chain()->SetFullscreenState(FALSE, 0);
		}
	}

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
	//	リソースの構築と読込完了の通知
	if (resource_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.resource_manager");
		resource_manager::instance()->update();
	}
}
