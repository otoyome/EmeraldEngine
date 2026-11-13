#pragma once

#if defined(USE_IMGUI)
#include <memory>

#include "Texture/texture.h"

class world;
class world_renderer;
class engine_context;
class render_job_executor;
struct debug_view_settings;

//	エディターのビューポート
class editor_viewport
{
public:
	explicit editor_viewport(std::shared_ptr<render_job_executor> render_executor);
	~editor_viewport();

	editor_viewport(const editor_viewport&) = delete;
	editor_viewport& operator=(const editor_viewport&) = delete;

	bool initialize(int width, int height);
	void finalize();

	void collect(world& target, const debug_view_settings& debug_settings);
	void submit_render_jobs(world& target, float elapsed_time);

	bool imgui_render(engine_context& context, const ImVec2& position, const ImVec2& size);

	const texture_ptr& get_output_texture() const { return output_texture; }

private:
	bool resize(int width, int height);

private:
	texture_ptr output_texture;					//	描画先のテクスチャ
	std::unique_ptr<world_renderer> renderer;	//	ワールドレンダラー
};
#endif	//	defined(USE_IMGUI)
