#include "Editor/editor_window.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imPlot/imPlot.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include "Input/mouse.h"
#include "Input/keyboard.h"
#include "System/world_viewport.h"
#include "Scenes/scene_base.h"
#include "Object/object_manager.h"

namespace
{
	//	@tool エディター用のウィンドウのクラス名
	constexpr const wchar_t* editor_window_class_name = L"SampleEngineEditorWindow";
}

editor_window::imgui_scope::imgui_scope(const editor_window& window)
	: previous_context(ImGui::GetCurrentContext())
	, previous_plot_context(ImPlot::GetCurrentContext())
{
	ImGui::SetCurrentContext(window.imgui_context);
	ImPlot::SetCurrentContext(window.plot_context);
}

editor_window::imgui_scope::~imgui_scope()
{
	ImGui::SetCurrentContext(previous_context);
	ImPlot::SetCurrentContext(previous_plot_context);
}

editor_window::editor_window(std::string_view name, std::string_view ini_name, render_system& renderer, ImFontAtlas* shared_fonts)
	: name(name)
	, ini_file_name(ini_name)
	, renderer(renderer)
	, shared_fonts(shared_fonts)
{
}

editor_window::~editor_window()
{
	//	@tool 呼び出し側で描画中のフレームが終わるのを待ってから破棄する
	window.clear_message_hooks();

	if (imgui_context)
	{
		{
			//	@tool ワールド（シーンの終了処理）は、この窓の ImGui を現在にして破棄する
			imgui_scope scope(*this);
			world.reset();
			ImGui_ImplWin32_Shutdown();
			ImPlot::DestroyContext(plot_context);
		}
		ImGui::DestroyContext(imgui_context);
		imgui_context = nullptr;
		plot_context = nullptr;
	}
	world.reset();

	if (present_target)
		present_target->release();
}

bool editor_window::create(const wchar_t* title, int width, int height, const world_setup& setup)
{
	if (!window.create(GetModuleHandleW(nullptr), editor_window_class_name, width, height, app_window::kinds::tool, title))
		return false;
	window.set_close_handler([this] { close_requested = true; });
	window.add_message_hook([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) { return handle_message(hwnd, msg, wparam, lparam); });

	//	@tool ImGui のコンテキスト（フォントはメインと共有する。描画はメインの描画バックエンドで行う）
	{
		imgui_context = ImGui::CreateContext(shared_fonts);
		ImGuiContext* previous_context = ImGui::GetCurrentContext();
		ImPlotContext* previous_plot_context = ImPlot::GetCurrentContext();
		ImGui::SetCurrentContext(imgui_context);
		plot_context = ImPlot::CreateContext();
		ImPlot::SetCurrentContext(plot_context);

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.IniFilename = ini_file_name.c_str();
		//	@tool メインの DX11 バックエンドが VtxOffset に対応しているので、大きな描画リストも描ける
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		ImGui_ImplWin32_Init(window.get_handle());
		ImGui::StyleColorsDark();

		ImGui::SetCurrentContext(previous_context);
		ImPlot::SetCurrentContext(previous_plot_context);
	}

	//	@tool 表示先
	present_target = imgui_present::present_target::create(window.get_handle(), static_cast<UINT>(width), static_cast<UINT>(height));

	//	@tool ワールド（入力はこのウィンドウに結び付ける）
	{
		imgui_scope scope(*this);
		world = std::make_unique<engine_world>(name);
		engine_world::scope world_scope(*world);
		mouse::instance()->set_window(window.get_handle());
		keyboard::instance()->set_window(window.get_handle());
		world_viewport::instance()->set_in_window(true);
		world_viewport::instance()->set_accepts_input(false, false);
		if (setup)
			setup();
	}

	ShowWindow(window.get_handle(), SW_SHOW);
	return true;
}

bool editor_window::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (imgui_context)
	{
		imgui_scope scope(*this);
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
			return true;
	}
	if (world)
	{
		engine_world::scope world_scope(*world);
		if (mouse::is_valid() && mouse::instance()->window_proccess(hwnd, msg, wparam, lparam))
			return true;
	}
	return false;
}

void editor_window::update_targets()
{
	int width = 0;
	int height = 0;
	minimized = IsIconic(window.get_handle()) || !window.get_client_size(width, height) || width <= 0 || height <= 0;
	if (minimized)
		return;

	//	@tool 表示先の大きさを合わせる（描画中のフレームがバックバッファを使い終わってから）
	if (present_target && present_target->swap_chain &&
		(present_target->width != static_cast<UINT>(width) || present_target->height != static_cast<UINT>(height)))
	{
		renderer.flush_rendering();
		present_target->resize(static_cast<UINT>(width), static_cast<UINT>(height));
	}

	//	@tool マウスの座標は描画領域の大きさで求める
	{
		engine_world::scope world_scope(*world);
		mouse::instance()->set_viewport_size({ width, height });
	}

	//	@tool View の大きさが変わったら視点の描画先を作り直す
	if (requested_width <= 0 || requested_height <= 0)
		return;
	if (view_target && view_target->width == requested_width && view_target->height == requested_height)
		return;

	//	@tool 視点の状態（履歴・カリング結果など）は大きさに依存するので、作り直す度に別の物を使う
	const std::string target_name = name + "#" + std::to_string(++target_generation);
	view_target = renderer.create_view_target(target_name, requested_width, requested_height);
	camera_cut = true;
}

void editor_window::update_world_viewport()
{
	//	@tool View の上にマウスがある間・そこで押したボタンを押し続けている間だけ、ワールドが入力を受け取る
	const bool accepts = view_visible && (hovered || captured);
	engine_world::scope world_scope(*world);
	world_viewport::instance()->set_accepts_input(accepts, accepts);
}

void editor_window::build_windows()
{
	imgui_scope scope(*this);
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//	@tool ウィンドウ全体をドッキング領域にし、View は最初だけ中央に置く
	const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	view_visible = ImGui::Begin("View");
	ImGui::PopStyleVar();
	hovered = false;
	if (view_visible)
	{
		//	@tool 描画先は描画領域の大きさで作る（次のフレームの更新で作り直す）
		const ImVec2 available = ImGui::GetContentRegionAvail();
		requested_width = (std::max)(1, static_cast<int>(available.x));
		requested_height = (std::max)(1, static_cast<int>(available.y));

		if (view_target && view_target->output.texture)
		{
			//	@tool この窓の ImGui はビューポートを使わないので、画面の位置はそのままクライアント座標になる
			const ImVec2 position = ImGui::GetCursorScreenPos();
			ImGui::Image(view_target->output.texture->get_shader_resource_view().Get(), ImVec2(static_cast<float>(view_target->width), static_cast<float>(view_target->height)));
			hovered = ImGui::IsItemHovered();

			engine_world::scope world_scope(*world);
			world_viewport::instance()->set_rect({ position.x, position.y, static_cast<float>(view_target->width), static_cast<float>(view_target->height) });
		}
	}
	ImGui::End();

	//	@tool View の上で押したボタンを押している間は、View の外へ出てもワールドが入力を受け取る
	const bool any_down = ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle);
	const bool any_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
	if (!any_down)
		captured = false;
	else if (hovered && any_clicked)
		captured = true;

	//	@tool ワールドのシーン・オブジェクトの編集用のウィンドウ
	{
		engine_world::scope world_scope(*world);
		if (scene_manager::is_valid())
			scene_manager::instance()->imgui_render();
		if (object_manager::is_valid())
			object_manager::instance()->imgui_render();
	}
}

void editor_window::add_view(render_system& renderer)
{
	if (minimized || !view_visible || !view_target)
		return;
	engine_world::scope world_scope(*world);
	renderer.add_view(view_target, view, camera_cut, true);
}

std::optional<imgui_present::window_draw> editor_window::end_frame()
{
	imgui_scope scope(*this);
	ImGui::Render();
	if (minimized || !present_target || !present_target->swap_chain || !present_target->render_target_view)
		return std::nullopt;

	imgui_present::window_draw draw;
	draw.target = present_target;
	draw.draw_data = imgui_present::copy_draw_data(*ImGui::GetDrawData());
	draw.clear = true;
	return draw;
}
#endif	//	defined(USE_IMGUI)
