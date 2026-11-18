#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>

#include "Texture/texture.h"

class world;
class world_renderer;
class engine_context;
class render_job_executor;
struct debug_view_settings;
template<class T> class console_variable;

//	エディターのビューポート
class editor_viewport
{
public:
	//	選んでいる画面比率
	float get_aspect_ratio() const;

	//	描画テクスチャ
	const texture_ptr& get_output_texture() const { return output_texture; }

public:
	explicit editor_viewport(std::shared_ptr<render_job_executor> render_executor);
	~editor_viewport();

	editor_viewport(const editor_viewport&) = delete;
	editor_viewport& operator=(const editor_viewport&) = delete;

	bool initialize(int width, int height);
	void finalize();

	void collect(world& target, const debug_view_settings& debug_settings);
	void submit_render_jobs(world& target, float elapsed_time);

	//	ImGui描画
	bool imgui_render(engine_context& context, const ImVec2& position, const ImVec2& size, const std::function<bool(const ImVec2& image_position, const ImVec2& image_size)>& overlay = nullptr);

	//	画面比率を保存する変数
	void set_aspect_variable(console_variable<int>* variable) { aspect_variable = variable; }

	//	画面比率を選ぶメニュー
	void imgui_menu();

private:
	bool resize(int width, int height);

private:
	texture_ptr output_texture;					//	描画先のテクスチャ
	std::unique_ptr<world_renderer> renderer;	//	ワールドレンダラー
	console_variable<int>* aspect_variable = nullptr;	//	画面比率の選択
};
#endif	//	defined(USE_IMGUI)
