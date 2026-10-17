#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

#if defined(USE_IMGUI)
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "imtimeline/ImTimeline.h"
#endif

#include "system/parallel_for_executor.h"
#include "render_pass.h"

#include "Scenes/scene_base.h"
#include "System/engine_world.h"
#include "Renderer/render_job_executor.h"
#include "Renderer/frame_graph.h"
#include "Renderer/scene_render_features.h"
#include "Renderer/scene_render_targets.h"
#include "camera.h"
#include "graphics/scene_view.h"
#include "graphics/scene_view_state.h"

namespace collision { class frustum_culling; }
struct render_data_context;
class render_command_list;
class effect_manager;

class framework
{
public:
	static constexpr const TCHAR* APPLICATION_NAME = _T("Sample Engine");

public:
	framework(HWND hwnd);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run();

	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	bool initialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
	bool uninitialize();

	bool create_buffers();
	bool create_collision_filters();

	void prepare_renderer_context(
		renderer_context& context,
		const scene_view_ptr& target_view,
		const scene_view_state_ptr& target_state,
		const Microsoft::WRL::ComPtr<ID3D11Buffer>& scene_constant_buffer,
		const std::shared_ptr<render_data_context>& render_data,
		const std::shared_ptr<render_particle_indices>& particle_indices,
		float previous_time, float current_time);

	void build_render_lists(
		const render_data_context& render_data,
		scene_view_state& target_state,
		const collision::frustum_culling& frustum_culling,
		const std::unordered_map<UINT, uint8_t>& occlusion_visible,
		render_mesh_indices& mesh_indices,
		render_decal_indices& decal_indices);

	void build_particle_render_lists(
		const render_data_context& render_data,
		render_particle_indices& particle_indices);

private:	//	@tool 描画スレッド・提出スレッド
#if defined(USE_IMGUI)
	//	@tool ImGui の描画データの複製（提出スレッドで描くため。描画リストは複製が所有する）
	struct imgui_draw_snapshot
	{
		ImDrawData draw_data;
		//	@tool 描画リストが参照するテクスチャ（提出スレッドで描き終わるまで解放させない）
		std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
		~imgui_draw_snapshot();
	};
#endif	//	defined(USE_IMGUI)

	//	@tool 1つの視点の描画先（シーン用のレンダーターゲット・前のフレームから引き継ぐ情報・定数バッファ）
	//	@tool 作り直しはメインスレッドで行う。描画中のフレームは shared_ptr で古い物を持ち続ける
	struct view_target
	{
		std::string name;
		scene_view_state_key state_key = 0;
		int width = 0;
		int height = 0;
		std::shared_ptr<scene_render_targets> render_targets;
		Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
	};
	using view_target_ptr = std::shared_ptr<view_target>;

	//	@tool 1つの視点の受け渡し情報（1つのワールドを1つの視点から描く）
	struct view_packet
	{
		view_target_ptr target;
		std::shared_ptr<render_data_context> render_data;
		std::shared_ptr<render_particle_indices> particle_indices;
		view_parameters parameters;
		bool camera_cut = false;
		//	@tool 抽出したワールドのエフェクト管理（GPU 側の更新に使う）
		effect_manager* effects = nullptr;
	};

	//	1フレーム分の受け渡し情報
	struct frame_packet
	{
		uint64_t frame_index = 0;
		float elapsed_time = 0.0f;
		float previous_time = 0.0f;
		//	描く視点
		std::vector<view_packet> views;
		bool single_thread = false;
		bool freeze_culling = false;
		bool visualize_meshlets = false;
		UINT sync_interval = 0;
		engine_config_data::antialias_data::modes antialias_mode = engine_config_data::antialias_data::modes::NONE;
#if defined(USE_IMGUI)
		std::shared_ptr<imgui_draw_snapshot> imgui_draw_data;
		//	@tool 画面キャプチャの際に保存する、モデル編集用の画面
		std::string view_capture_path;
		texture_ptr view_capture_texture;
#endif	//	defined(USE_IMGUI)

		//	@tool 描画スレッドが作る提出物
		std::shared_ptr<render_command_list> command_list;
	};
	using frame_packet_ptr = std::shared_ptr<frame_packet>;

	//	@tool メインスレッド：描画に必要な情報を抽出する
	frame_packet_ptr extract_frame(float elapsed_time);
	//	@tool 描画スレッド：描画パスを組み立てて記録を始める（packet.submission を作る）
	void render_frame(frame_packet& packet);
	//	@tool 提出スレッド：提出物を順に実行して表示する（immediate context の排他は呼び出し側で取る）
	void submit_frame(frame_packet& packet);

	//	@tool フレームを描画側へ渡す（同時に処理するフレーム数を超える場合は待つ）
	void dispatch_frame(frame_packet_ptr packet);
	//	@tool 渡したフレームが全て提出し終わるまで待つ（メインスレッドからのみ有効）
	void flush_rendering();
	//	@tool 直前に渡したフレームのエフェクト更新（GPU）が終わるまで待つ（ゲームの更新前に呼ぶ）
	void wait_effect_update();

	void start_render_threads();
	void stop_render_threads();
	void render_thread_main();
	void submit_thread_main();

	//	@tool 同時に処理するフレーム数（抽出済みで提出が終わっていないフレーム数の上限）
	static constexpr UINT max_frames_in_flight = 2;

	std::thread::id main_thread_id;
	bool threaded_rendering = false;
	bool render_threads_running = false;
	std::thread render_thread;
	std::thread submit_thread;
	std::mutex frame_mutex;
	std::condition_variable frame_condition;
	std::deque<frame_packet_ptr> render_queue;
	std::deque<frame_packet_ptr> submit_queue;
	UINT frames_in_flight = 0;
	uint64_t dispatched_frame_index = 0;
	uint64_t submitted_frame_index = 0;
	uint64_t effect_updated_frame_index = 0;
	float total_time = 0.0f;
	//	@tool 提出し終わったフレーム（破棄はメインスレッドで行う）
	std::vector<frame_packet_ptr> retired_packets;

	//	@tool 処理中のワールドから1つの視点の描画情報を抽出する（parameters・camera_cut は選んだ視点で更新する）
	view_packet extract_view(const view_target_ptr& target, view_parameters& parameters, bool& camera_cut, bool in_window);

	//	@tool 視点の描画先を作る（ImGui で表示する出力テクスチャを持つ。描画中のフレームは古い描画先を持ち続ける）
	view_target_ptr create_view_target(std::string_view name, int width, int height);
	//	@tool シーン用のレンダーターゲットを作る（register_names はテクスチャ管理に名前で登録するか）
	bool create_scene_render_targets(scene_render_targets& targets, int scene_width, int scene_height, bool register_names);

	//	@tool ゲームのワールド
	std::unique_ptr<engine_world> main_world;
	//	@tool メイン画面の描画先
	view_target_ptr main_view_target;

	//	@tool ワールド1つ分の更新（処理中のワールドに対して行う）
	void update_world(float elapsed_time);
	//	@tool 全てのワールドで順に処理する（メイン → ImGui のウィンドウに映すワールド）
	template<class Function>
	void for_each_world(Function&& function)
	{
		function();
#if defined(USE_IMGUI)
		if (model_editor && model_editor->world)
		{
			engine_world::scope world_scope(*model_editor->world);
			function();
		}
#endif	//	defined(USE_IMGUI)
	}
	//	@tool 各ワールドの画面が入力を受け付けるかを決める（ImGui の判定は前のフレームの物を使う）
	void update_world_viewports();

#if defined(USE_IMGUI)
	//	@tool モデル編集用のワールド（メイン画面はゲームのワールドを映し続け、こちらは ImGui のドッキングウィンドウに描く）
	struct editor_world_view
	{
		std::unique_ptr<engine_world> world;
		view_target_ptr target;
		uint32_t target_generation = 0;
		view_parameters view;
		bool camera_cut = true;
		//	@tool ウィンドウが表示されているか（隠れている間は描かない）
		bool visible = false;
		//	@tool ウィンドウの描画領域の大きさ（描画先はこの大きさで作り直す）
		int requested_width = 0;
		int requested_height = 0;
		//	@tool マウスが画面の上にあるか・画面の上で押したボタンを押し続けているか
		bool hovered = false;
		bool captured = false;
	};
	std::unique_ptr<editor_world_view> model_editor;
	bool model_editor_open_requested = false;
	bool model_editor_close_requested = false;

	//	@tool モデル編集用のワールドの生成・破棄・描画先の作り直し（更新の最初に行う）
	void update_model_editor_world();
	//	@tool モデル編集用のワールドを映すウィンドウ
	void imgui_model_editor_window();
#endif	//	defined(USE_IMGUI)

private:
	CONST HWND hwnd;
	std::shared_ptr<scene_render_targets> render_targets;
	//	@tool 描画機能ごとの共有資源（全視点で共有する）
	std::unique_ptr<scene_rendering::scene_render_features> scene_features;
	std::shared_ptr<render_job_executor> render_executor;
	//	@tool フレームのレンダーグラフ（各レンダラーが描画パスを追加する）
	std::shared_ptr<frame_graph> frame_render_graph;

	//	描画用の一部情報
	render_pass back_buffer_clear_rendering_pass;

	//	UI描画用のシーン定数バッファ(viewport_rectのみ使用)
	Microsoft::WRL::ComPtr<ID3D11Buffer> ui_scene_constant_buffer;

private:
	//	メイン視点情報
	static constexpr scene_view_state_key main_scene_view_state_key = scene_view_state_cache::make_key("main");
	view_parameters main_view_parameters;
	bool main_view_camera_cut = false;
	Microsoft::WRL::ComPtr<ID3D11Buffer> main_scene_constant_buffer;

	scene_view_state_cache scene_view_states;
	uint64_t render_frame_index = 0;

private:	//	描画情報振り分け
	struct cull_bucket
	{
		std::vector<std::size_t> opaque;
		std::vector<std::size_t> mask;
		std::vector<std::size_t> forward[render_mesh_indices::forward_size];
		void clear()
		{
			opaque.clear();
			mask.clear();
			for (auto& indices : forward)
				indices.clear();
		}
	};
	std::vector<cull_bucket> cull_buckets;
	std::vector<std::uintptr_t> sort_pipeline_keys;
	parallel_for_executor parallel_executor;

private:	//	その他
	float wheel{ 0 };
	high_resolution_timer tictoc;

#if defined(USE_IMGUI)
	//	ImGui用
	render_pass imgui_rendering_pass;
	D3D11_VIEWPORT imgui_viewport;

	//	ログ系
	logger::categorys display_log_category = logger::categorys::max;
	UINT display_log_flags = (1 << static_cast<UINT>(logger::levels::error)) | (1 << static_cast<UINT>(logger::levels::warning));
	bool is_log_descending_order = false;

	void imgui_initialize();
	void imgui_terminate();
	void imgui_engine_config_window();
	void imgui_log_window();
	void imgui_profiler_window();
	void imgui_model_buffer_pool_window();
	void imgui_memory_allocator_window();
	void imgui_textures_window();
	void imgui_scene_change();

	void imgui_console_window();
	void imgui_console_commands_window();

	void build_debug_primitives(const render_data_context& render_data);
#endif // defined(USE_IMGUI)
};

