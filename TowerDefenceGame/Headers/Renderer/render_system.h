#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <deque>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "system/parallel_for_executor.h"
#include "System/engine_config_data.h"

#include "Renderer/render_job_executor.h"
#include "Renderer/frame_graph.h"
#include "Renderer/scene_render_features.h"
#include "camera.h"
#include "graphics/scene_view.h"
#include "graphics/scene_view_state.h"

namespace collision { class frustum_culling; }
struct view_render_data;
class render_command_list;
class effect_manager;

//	@tool 描画の流れをまとめる（メインスレッドの抽出 → 描画スレッドの組み立て → 提出スレッドの実行と表示）
//	@tool 描画は常に描画スレッドと提出スレッドで行う。抽出の関数（begin_frame・add_view・end_frame など）はメインスレッドから呼ぶ
class render_system
{
public:
	//	@tool 1つの視点の描画先（シーン用のレンダーターゲット・前のフレームから引き継ぐ情報・定数バッファ）
	//	@tool 作り直しはメインスレッドで行う。描画中のフレームは shared_ptr で古い物を持ち続ける
	//	@tool シーン用のテクスチャはフレームグラフの一時テクスチャ、前のフレームから引き継ぐ情報は scene_view_state（state_key で引く）が持つ
	struct view_target
	{
		std::string name;
		scene_view_state_key state_key = 0;
		int width = 0;
		int height = 0;
		scene_rendering::scene_output output;
		Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
	};
	using view_target_ptr = std::shared_ptr<view_target>;

	//	@tool 提出スレッドで、表示の直前にバックバッファへ重ねて描く処理（ImGui）
	using overlay_function = std::function<void(ID3D11DeviceContext* immediate_context, ID3D11RenderTargetView* back_buffer)>;
	//	@tool 抽出の途中でデバッグ表示を積む処理（メインスレッド。描画情報の抽出後、デバッグ表示の取得前に呼ぶ）
	using debug_primitive_builder = std::function<void(const view_render_data& render_data)>;

public:
	render_system();
	~render_system();

	render_system(const render_system&) = delete;
	render_system& operator=(const render_system&) = delete;
	render_system(render_system&&) noexcept = delete;
	render_system& operator=(render_system&&) noexcept = delete;

	//	@tool 描画に使う資源を作る（メインスレッド。呼んだスレッドをメインスレッドとして扱う）
	bool initialize();

	//	@tool 描画スレッドと提出スレッドを開始・停止する（停止は渡したフレームが全て提出し終わるのを待つ）
	void start_render_threads();
	void stop_render_threads();

	//	@tool フレームの抽出を始める（提出し終わったフレームを破棄する）
	void begin_frame(float elapsed_time);
	//	@tool 処理中のワールドから1つの視点を抽出して、抽出中のフレームに足す（先頭がメイン画面。parameters・camera_cut は選んだ視点で更新する）
	void add_view(const view_target_ptr& target, view_parameters& parameters, bool& camera_cut, bool in_window);
	//	@tool 抽出中のフレームに、表示の直前に重ねて描く処理を設定する
	void set_overlay(overlay_function overlay);
	//	@tool 抽出したフレームを描画側へ渡す（同時に処理するフレーム数を超える場合は待つ）
	void end_frame();

	//	@tool 渡したフレームが全て提出し終わるまで待つ（メインスレッドからのみ有効）
	void flush_rendering();
	//	@tool 直前に渡したフレームのエフェクト更新（GPU）が終わるまで待つ（ゲームの更新前に呼ぶ）
	void wait_effect_update();

	//	@tool 視点の描画先を作る（ImGui で表示する出力テクスチャを持つ。描画中のフレームは古い描画先を持ち続ける）
	view_target_ptr create_view_target(std::string_view name, int width, int height);
	//	@tool メイン画面（バックバッファ）の描画先
	const view_target_ptr& get_main_view_target() const { return main_view_target; }

	void set_debug_primitive_builder(debug_primitive_builder builder) { debug_primitives = std::move(builder); }
	//	@tool フレームのレンダーグラフ（直前のフレームのパス一覧の表示に使う）
	frame_graph* get_frame_graph() const { return frame_render_graph.get(); }

private:
	//	@tool 1つの視点の受け渡し情報（1つのワールドを1つの視点から描く）
	struct view_packet
	{
		view_target_ptr target;
		std::shared_ptr<view_render_data> render_data;
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
		bool freeze_culling = false;
		bool visualize_meshlets = false;
		UINT sync_interval = 0;
		engine_config_data::antialias_data::modes antialias_mode = engine_config_data::antialias_data::modes::NONE;
		//	@tool 提出スレッドで、表示の直前にバックバッファへ重ねて描く処理（破棄はメインスレッドで行う）
		overlay_function overlay;
#if defined(USE_IMGUI)
		//	@tool 画面キャプチャの際に保存する、モデル編集用の画面
		std::string view_capture_path;
		texture_ptr view_capture_texture;
#endif	//	defined(USE_IMGUI)

		//	@tool 描画スレッドが作る提出物
		std::shared_ptr<render_command_list> command_list;
	};
	using frame_packet_ptr = std::shared_ptr<frame_packet>;

	bool create_buffers();

	void prepare_renderer_context(
		renderer_context& context,
		const scene_view_ptr& target_view,
		const scene_view_state_ptr& target_state,
		const Microsoft::WRL::ComPtr<ID3D11Buffer>& scene_constant_buffer,
		const std::shared_ptr<view_render_data>& render_data,
		const std::shared_ptr<render_particle_indices>& particle_indices,
		float previous_time, float current_time);

	void build_render_lists(
		const view_render_data& render_data,
		scene_view_state& target_state,
		const collision::frustum_culling& frustum_culling,
		const std::unordered_map<UINT, uint8_t>& occlusion_visible,
		render_mesh_indices& mesh_indices,
		render_decal_indices& decal_indices);

	void build_particle_render_lists(
		const view_render_data& render_data,
		render_particle_indices& particle_indices);

	//	@tool 処理中のワールドから1つの視点の描画情報を抽出する（parameters・camera_cut は選んだ視点で更新する）
	view_packet extract_view(const view_target_ptr& target, view_parameters& parameters, bool& camera_cut, bool in_window);
	//	@tool 描画スレッド：描画パスを組み立てて記録を始める（packet.submission を作る）
	void render_frame(frame_packet& packet);
	//	@tool 提出スレッド：提出物を順に実行して表示する（immediate context の排他は呼び出し側で取る）
	void submit_frame(frame_packet& packet);

	//	@tool フレームを描画側へ渡す（同時に処理するフレーム数を超える場合は待つ）
	void dispatch_frame(frame_packet_ptr packet);

	void render_thread_main();
	void submit_thread_main();

private:	//	@tool 描画スレッド・提出スレッド
	//	@tool 同時に処理するフレーム数（抽出済みで提出が終わっていないフレーム数の上限）
	static constexpr UINT max_frames_in_flight = 2;

	std::thread::id main_thread_id;
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
	//	@tool 抽出中のフレーム（begin_frame から end_frame まで）
	frame_packet_ptr extracting_packet;
	debug_primitive_builder debug_primitives;

private:
	//	@tool 方向光源と、アトラスに入らない点光源・スポットライトのシャドウマップ（全視点で共有する）
	texture_ptr shadow_map_texture;
	//	@tool 描画機能ごとの共有資源（全視点で共有する）
	std::unique_ptr<scene_rendering::scene_render_features> scene_features;
	std::shared_ptr<render_job_executor> render_executor;
	//	@tool フレームのレンダーグラフ（各レンダラーが描画パスを追加する）
	std::shared_ptr<frame_graph> frame_render_graph;

	//	描画用の一部情報
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> back_buffer_view;	//	@tool バックバッファの描画先（フレームの最初のクリアと ImGui の描画に使う）

	//	UI描画用のシーン定数バッファ(viewport_rectのみ使用)
	Microsoft::WRL::ComPtr<ID3D11Buffer> ui_scene_constant_buffer;

private:
	//	メイン視点情報
	static constexpr scene_view_state_key main_scene_view_state_key = scene_view_state_cache::make_key("main");
	Microsoft::WRL::ComPtr<ID3D11Buffer> main_scene_constant_buffer;
	//	@tool メイン画面の描画先
	view_target_ptr main_view_target;

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
};
