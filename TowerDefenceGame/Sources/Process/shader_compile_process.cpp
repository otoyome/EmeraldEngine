#include "Process/shader_compile_process.h"
#include "Process/game_process.h"
#include "framework.h"
#include "Graphics/render_data.h"

#include "Material/material_shader_map.h"
#include "Object/object_manager.h"
#include "Object/world.h"
#include "Object/GameObjects/UI/ui_image.h"
#include "Renderer/ui_world_renderer.h"
#include "Renderer/world_renderer.h"
#include "Renderer/render_system.h"

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

	constexpr int step_count = static_cast<int>(std::size(material_domains)) + 1;

	constexpr float progress_bar_margin = 0.1f;
	constexpr float progress_bar_y = 0.85f;
	constexpr float progress_bar_height = 8.0f;
}

shader_compile_process::~shader_compile_process()
{
	if (worker.joinable())
		worker.join();
}

bool shader_compile_process::enter()
{
	framework& self = get_owner();

	start_time = std::chrono::steady_clock::now();

	//	読み込み画面
	{
		const math::float4 rect =
		{
			SCREEN_WIDTH * progress_bar_margin,
			SCREEN_HEIGHT * progress_bar_y,
			SCREEN_WIDTH * (1.0f - progress_bar_margin * 2.0f),
			progress_bar_height,
		};

		auto background = std::make_shared<ui_image>("Progress Bar Background");
		background->set_screen_rect(rect);
		background->set_color(math::float4{ 0.2f, 0.2f, 0.2f, 1.0f });
		background->set_draw_mode(ui_image::draw_modes::single);
		background->set_sort_order(0);
		self.boot_world->get_objects().register_object(background);

		progress_bar = std::make_shared<ui_image>("Progress Bar");
		progress_bar->set_screen_rect(rect);
		progress_bar->set_color(math::float4{ 1.0f, 1.0f, 1.0f, 1.0f });
		progress_bar->set_draw_mode(ui_image::draw_modes::filled);
		progress_bar->set_fill_method(ui_image::fill_methods::axis);
		progress_bar->set_fill_origin(ui_image::fill_origins::left);
		progress_bar->set_fill_amount(0.0f);
		progress_bar->set_sort_order(1);
		self.boot_world->get_objects().register_object(progress_bar);
	}

	//	作成は別スレッドで行い、メインスレッドは読み込み画面を描き続ける
	std::shared_ptr<render_output> output = render_output::create_back_buffer();
	worker = std::thread([this, output] { build(output); });
	return true;
}

void shader_compile_process::build(std::shared_ptr<render_output> output)
{
	framework& self = get_owner();

	//	マテリアルの既定とエラー用のパイプライン
	material_shader_map* shader_map = material_shader_map::instance();
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
		++finished_steps;
	}

	//	メインのワールドの描画
	created_renderer = std::make_unique<world_renderer>(self.renderer->get_executor(), output);
	succeeded = created_renderer->initialize();
	++finished_steps;

	finished = true;
}

void shader_compile_process::exit()
{
	framework& self = get_owner();

	//	途中で終わるときは作成が済むのを待つ
	if (worker.joinable())
		worker.join();

	progress_bar.reset();
	self.boot_renderer.reset();
	self.boot_world.reset();
}

void shader_compile_process::update(float elapsed_time)
{
	framework& self = get_owner();

	progress_bar->set_fill_amount(get_progress());

	self.boot_world->get_objects().updates(elapsed_time);
}

void shader_compile_process::render(float elapsed_time)
{
	framework& self = get_owner();

	//	描画前処理
	self.boot_world->get_objects().pre_render();

	//	描画用の情報収集
	self.boot_renderer->collect(*self.boot_world);

}

void shader_compile_process::submit_render_jobs(float elapsed_time)
{
	framework& self = get_owner();

	self.boot_renderer->submit_render_jobs(*self.boot_world, elapsed_time);
}

void shader_compile_process::post_render()
{
	framework& self = get_owner();

	self.boot_world->get_objects().post_render();
}

#if defined(USE_IMGUI)
void shader_compile_process::imgui_render()
{
	if (ImGui::Begin("Shader Compile"))
	{
		const std::string text = std::to_string(finished_steps.load()) + " / " + std::to_string(step_count);
		ImGui::ProgressBar(get_progress(), ImVec2(-1.0f, 0.0f), text.c_str());
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)

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
	DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader compile process finished. [%.2f sec]", elapsed.count());

	//	作ったメインのワールドの描画をフレームワークに渡す
	get_owner().main_renderer = std::move(created_renderer);
	return std::make_unique<game_process>(get_owner());
}

float shader_compile_process::get_progress() const
{
	return static_cast<float>(finished_steps.load()) / static_cast<float>(step_count);
}
