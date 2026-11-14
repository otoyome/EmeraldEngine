#include "Process/boot_process.h"
#include "Process/game_process.h"
#include "Process/scene_editor_process.h"
#include "framework.h"

#include <algorithm>

#include "Graphics/Pipeline/global_shader.h"
#include "Material/material_shader_map.h"
#include "System/engine_config_data.h"
#include "Resource/asset_registry.h"

namespace
{
	//	既定とエラー用のパイプラインを作るマテリアルのドメイン
	constexpr material_shader_map::domains material_domains[] =
	{
		material_shader_map::domains::surface,
		material_shader_map::domains::deferred_decal,
		material_shader_map::domains::post_process,
		material_shader_map::domains::user_interface,
		material_shader_map::domains::particle,
	};

	constexpr float global_shader_progress_rate = 0.9f;

	constexpr float asset_scan_progress_rate = 0.05f;
}

boot_process::~boot_process()
{
	if (worker.joinable())
		worker.join();
}

bool boot_process::enter()
{
	start_time = std::chrono::steady_clock::now();

	//	スプラッシュウィンドウ
	const std::wstring& splash_image_path = engine_config_data::instance()->application.splash_image_path.get<utf16_cache>();
	splash.show(GetModuleHandleW(nullptr), splash_image_path.c_str());
	splash.set_text(L"シェーダーをコンパイルしています");

	//	作成は別スレッドで行う
	worker = std::thread([this] { build(); });
	return true;
}

void boot_process::build()
{
	scan_assets();
	succeeded = compile_global_shaders();
	prepare_material_pipelines();

	finished = true;
}

void boot_process::set_progress_after_scan(float rate)
{
	splash.set_progress(asset_scan_progress_rate + (1.0f - asset_scan_progress_rate) * rate);
}

void boot_process::scan_assets()
{
	splash.set_text(L"アセットを調べています");
	asset_registry::instance()->scan({ asset_registry::root_directory, asset_registry::engine_root_directory });
	splash.set_progress(asset_scan_progress_rate);
}

bool boot_process::compile_global_shaders()
{
	//	グローバルシェーダー
	splash.set_text(L"シェーダーをコンパイルしています");
	const std::size_t thread_count = std::max(std::thread::hardware_concurrency(), 2u) - 1;
	return global_shader_base::compile_all(thread_count,
		[this](std::size_t done, std::size_t total)
		{
			set_progress_after_scan(global_shader_progress_rate * static_cast<float>(done) / static_cast<float>(std::max<std::size_t>(total, 1)));
		});
}

void boot_process::prepare_material_pipelines()
{
	//	マテリアルの既定とエラー用のパイプライン
	splash.set_text(L"マテリアルを準備しています");
	material_shader_map* shader_map = material_shader_map::instance();
	std::size_t domain_index = 0;
	for (material_shader_map::domains domain : material_domains)
	{
		if (material_shader_map::is_supported_domain(domain))
		{
			for (UINT pass = 0; pass < material_shader_map::get_pass_count(domain); ++pass)
			{
				material_shader_map::request request;
				request.shader_path = material_shader_map::get_default_shader_path(domain);
				request.domain = domain;
				request.pass = pass;
				request.pixel = false;
				shader_map->get_pipeline_state(request);
				shader_map->get_failed_pipeline_state(domain, pass);
			}
		}
		++domain_index;
		set_progress_after_scan(global_shader_progress_rate + (1.0f - global_shader_progress_rate) * static_cast<float>(domain_index) / static_cast<float>(std::size(material_domains)));
	}
}

void boot_process::exit()
{
	//	途中で終わるときは作成が済むのを待つ
	if (worker.joinable())
		worker.join();

	splash.close();
}

std::unique_ptr<framework_process> boot_process::next_process()
{
	if (!finished)
		return nullptr;

	if (worker.joinable())
		worker.join();

	const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
	if (!succeeded)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "boot process failed. [%.2f sec]", elapsed.count());
		PostQuitMessage(0);
		return nullptr;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::shader, "boot process finished. [%zu shaders, %.2f sec]", global_shader_base::get_list().size(), elapsed.count());

	//	スプラッシュウィンドウを閉じて、メインのウィンドウを出す
	splash.close();
	framework& self = get_owner();
	ShowWindow(self.hwnd, SW_SHOW);
	SetForegroundWindow(self.hwnd);
#if defined(USE_IMGUI)
	self.startup_finished = true;

	//	エディターでは編集用のプロセスから始める
	if (self.editor)
		return std::make_unique<scene_editor_process>(self);
#endif	//	defined(USE_IMGUI)

	return std::make_unique<game_process>(get_owner());
}
