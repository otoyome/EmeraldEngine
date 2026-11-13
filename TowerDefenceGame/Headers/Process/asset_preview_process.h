#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Process/framework_process.h"
#include "System/cache_string.h"
#include "Texture/texture.h"
#include "Renderer/debug_view_settings.h"
#include "Scenes/scene_system.h"

class world;
class world_renderer;
struct render_pass;
class scene_manager;
class engine_context;

//	アセットプレビュープロセス
class asset_preview_process : public framework_process
{
public:
	//	シーンオープン関数
	using scene_opener = std::function<void(scene_manager& scenes)>;

	//	パネルレイアウト
	struct panel_layout
	{
		float left_width = 280.0f;
		float right_width = 360.0f;
		float bottom_height = 220.0f;
	};

public:
	asset_preview_process(framework& owner, const cache_string& file_path, std::string_view window_kind, scene_opener open_scene);
	~asset_preview_process() override;

	std::string_view get_key() const override { return key; }

	bool is_closed() const override { return !open; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
	void render(float elapsed_time) override;
	void submit_render_jobs(float elapsed_time) override;
	void post_render() override;
	void imgui_render() override;

private:
	bool resize_output(int width, int height);

	//	レイアウト描画
	bool imgui_render_layout(engine_context& context);

	//	パネル描画
	void imgui_render_panel(scene_system::panel_regions region, const char* id, const ImVec2& position, const ImVec2& size);

public:
	//	キー生成
	static std::string make_key(std::string_view window_kind, const cache_string& file_path);

private:
	//	境目のドラッグで大きさを変える
	static void imgui_splitter(const char* id, const ImVec2& position, const ImVec2& size, bool vertical, float sign, float& value);

private:
	cache_string file_path;		//	開いたファイル
	bool open = true;			//	窓が開いているか
	std::string window_kind;	//	窓の種類
	std::string key;			//	二重に開かないためのキー
	scene_opener open_scene;	//	自分のワールドにシーンを登録する処理

	std::unique_ptr<world> preview_world;		//	ワールド
	texture_ptr output_texture;					//	描画先のテクスチャ（ImGui で表示する）
	std::unique_ptr<world_renderer> renderer;	//	ワールドレンダラー

private:
	static std::unordered_map<std::string, panel_layout> panel_layouts;	//	レイアウトパネルリスト
};
#endif	//	defined(USE_IMGUI)
