#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class app_window;
class render_system;
class editor_host;
class world;
class world_renderer;
class ui_world_renderer;
class framework_process;

//	フレームワーク
class framework
{
	friend class core_process;
	friend class graphics_process;
	friend class input_process;
	friend class audio_process;
	friend class asset_watch_process;
	friend class editor_process;
	friend class boot_shader_compile_process;
	friend class shader_compile_process;
	friend class game_process;

public:
	static constexpr const TCHAR* APPLICATION_NAME = _T("Sample Engine");

public:
	explicit framework(app_window& window);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run();

private:
	bool initialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
	bool uninitialize();

private:
	bool add_process(std::unique_ptr<framework_process> process);
	bool advance_process();

private:
	//	プロセス
	struct process_entry
	{
		std::unique_ptr<framework_process> process;
		std::uint64_t entered_index = 0;
	};

private:
	app_window& window;
	CONST HWND hwnd;
	std::unique_ptr<render_system> renderer;	//	1フレームの描画の流れ
	std::vector<process_entry> processes;	//	積んだプロセス
	std::uint64_t entered_count = 0;	//	積んだ回数

	std::unique_ptr<world> main_world;	//	メインのワールド
	std::unique_ptr<world_renderer> main_renderer;	//	メインのワールドの描画
	std::unique_ptr<world> boot_world;	//	読み込み画面のワールド
	std::unique_ptr<ui_world_renderer> boot_renderer;	//	読み込み画面のワールドの描画

#if defined(USE_IMGUI)
	std::unique_ptr<editor_host> editor;	//	エディター機能
#endif	//	defined(USE_IMGUI)

private:	//	その他
	high_resolution_timer tictoc;
};
