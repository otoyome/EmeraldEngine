#include "Process/shader_compile_process.h"
#include "Process/game_process.h"
#include "framework.h"

#include <algorithm>

#include "Graphics/global_shader.h"
#include "Material/material_shader_map.h"
#include "System/engine_config_data.h"

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

	//	進捗のうちグローバルシェーダーが占める割合(残りはマテリアル)
	constexpr float global_shader_progress_rate = 0.9f;
}

shader_compile_process::~shader_compile_process()
{
	if (worker.joinable())
		worker.join();
}

bool shader_compile_process::enter()
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

void shader_compile_process::build()
{
	//	グローバルシェーダー
	const std::size_t thread_count = std::max(std::thread::hardware_concurrency(), 2u) - 1;
	succeeded = global_shader_base::compile_all(thread_count,
		[this](std::size_t done, std::size_t total)
		{
			splash.set_progress(global_shader_progress_rate * static_cast<float>(done) / static_cast<float>(std::max<std::size_t>(total, 1)));
		});

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
		splash.set_progress(global_shader_progress_rate + (1.0f - global_shader_progress_rate) * static_cast<float>(domain_index) / static_cast<float>(std::size(material_domains)));
	}

	finished = true;
}

void shader_compile_process::exit()
{
	//	途中で終わるときは作成が済むのを待つ
	if (worker.joinable())
		worker.join();

	splash.close();
}

std::unique_ptr<framework_process> shader_compile_process::next_process()
{
	if (!finished)
		return nullptr;

	if (worker.joinable())
		worker.join();

	const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
	if (!succeeded)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "shader compile process failed. [%.2f sec]", elapsed.count());
		PostQuitMessage(0);
		return nullptr;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader compile process finished. [%zu shaders, %.2f sec]", global_shader_base::get_list().size(), elapsed.count());

	//	スプラッシュウィンドウを閉じて、メインのウィンドウを出す
	splash.close();
	framework& self = get_owner();
	ShowWindow(self.hwnd, SW_SHOW);
	SetForegroundWindow(self.hwnd);

	return std::make_unique<game_process>(get_owner());
}
