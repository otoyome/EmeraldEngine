#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "Framework/app_window.h"
#include "Editor/imgui_present.h"
#include "Renderer/render_system.h"
#include "System/engine_world.h"

struct ImGuiContext;
struct ImPlotContext;
struct ImFontAtlas;

//	@tool エディター用の別ウィンドウ（ネイティブのウィンドウ・ImGui コンテキスト・ワールド・視点の描画先）
//	@tool ワールドのシーンは窓の中の ImGui のウィンドウ「View」に映し、編集用のウィンドウは窓の中にドッキングできる
//	@tool 全てメインスレッドから呼ぶ。ImGui の描画データは複製して、提出スレッドで描いて表示する
class editor_window
{
public:
	//	@tool ワールドを作った直後に、そのワールドを処理中にして呼ぶ（シーンの読み込みなど）
	using world_setup = std::function<void()>;

public:
	//	@tool name はワールド・視点の名前、ini_name は ImGui の配置を保存するファイル名（同じ種類の窓で共有する）
	editor_window(std::string_view name, std::string_view ini_name, render_system& renderer, ImFontAtlas* shared_fonts);
	~editor_window();

	editor_window(const editor_window&) = delete;
	editor_window& operator=(const editor_window&) = delete;
	editor_window(editor_window&&) noexcept = delete;
	editor_window& operator=(editor_window&&) noexcept = delete;

	//	@tool ウィンドウ・ImGui コンテキスト・ワールドを作って表示する
	bool create(const wchar_t* title, int width, int height, const world_setup& setup);

	//	@tool 閉じる操作がされたか（破棄は描画中のフレームを待ってから行う）
	bool is_close_requested() const { return close_requested; }

	//	@tool ウィンドウの大きさに合わせて表示先と視点の描画先を作り直す（更新の最初に呼ぶ）
	void update_targets();
	//	@tool ワールドの画面が入力を受け付けるかを決める（ImGui の判定は前のフレームの物を使う）
	void update_world_viewport();
	//	@tool ImGui のフレームを始めて、ウィンドウを組み立てる
	void build_windows();
	//	@tool 視点を抽出中のフレームに足す（最小化中・View が隠れている間は描かない）
	void add_view(render_system& renderer);
	//	@tool ImGui のフレームを締め、表示する場合は描画データを返す
	std::optional<imgui_present::window_draw> end_frame();

	//	@tool このウィンドウのワールドを処理中にして呼ぶ
	template<class Function>
	void with_world(Function& function)
	{
		engine_world::scope world_scope(*world);
		function();
	}

private:
	//	@tool このウィンドウの ImGui・ImPlot のコンテキストを現在にする（抜けると元に戻す）
	class imgui_scope
	{
	public:
		explicit imgui_scope(const editor_window& window);
		~imgui_scope();
		imgui_scope(const imgui_scope&) = delete;
		imgui_scope& operator=(const imgui_scope&) = delete;
	private:
		ImGuiContext* previous_context;
		ImPlotContext* previous_plot_context;
	};

	bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	std::string name;
	std::string ini_file_name;	//	@tool ImGui は文字列を参照し続けるので持っておく
	render_system& renderer;
	ImFontAtlas* shared_fonts = nullptr;

	app_window window;
	ImGuiContext* imgui_context = nullptr;
	ImPlotContext* plot_context = nullptr;
	std::unique_ptr<engine_world> world;

	//	@tool ウィンドウの表示先
	imgui_present::present_target_ptr present_target;
	//	@tool 視点の描画先（View の大きさで作り直す）
	render_system::view_target_ptr view_target;
	uint32_t target_generation = 0;
	view_parameters view;
	bool camera_cut = true;

	bool close_requested = false;
	bool minimized = false;
	//	@tool View が表示されているか（隠れている間は描かない）
	bool view_visible = false;
	//	@tool View の描画領域の大きさ（描画先はこの大きさで作り直す）
	int requested_width = 0;
	int requested_height = 0;
	//	@tool マウスが View の上にあるか・View の上で押したボタンを押し続けているか
	bool hovered = false;
	bool captured = false;
};
#endif	//	defined(USE_IMGUI)
