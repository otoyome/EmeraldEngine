#pragma once

#include "scenes/scene_base.h"

#if defined(USE_IMGUI)
#include <unordered_map>

#include "timeline_resource.h"

#include "object/model_object.h"
#include "object/camera_object.h"
#include "object/post_process_volume_object.h"

#include "effect_resource.h"
#include "effect_instance.h"

#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"

#include "ImGuizmo/ImSequencer.h"

//	タイムライン編集シーン
class edit_timeline_scene : public scene_base
{
	SCENE_DECRLEAR(edit_timeline_scene, scene_base);

public:
	edit_timeline_scene();
	edit_timeline_scene(const edit_timeline_scene&) = delete;
	edit_timeline_scene(edit_timeline_scene&&) = delete;
	~edit_timeline_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	void add_actor();
	void remove_actor(std::size_t actor_index);
	void rebuild_actor_models();

	//	@tool ImSequencer は GetItemCount()==0 のとき Sequencer() が何も描画せず即 return するため
	//	@tool （ライブラリ側の仕様）、最初の1件を追加する手段がシーケンサー内の「+」だけだと詰む。
	//	@tool そのための代替ボタン用エントリポイント。type は sequence_interface::Add と同じ意味
	//	@tool （[0,actors.size()):該当アクターへアニメーション追加 / [actors.size(),actors.size()*2):該当アクターへ配置区間追加 /
	//	@tool  actors.size()*2:エフェクト追加 / actors.size()*2+1:カメラ追加）
	void add_entry_and_select(int type);

	void new_timeline();
	void load_timeline();
	void save_timeline();

	void advance_playback(float elapsed_time);
	void apply_timeline_state();
	void clear_active_effects();
	void clear_actor_models();

	void imgui_render_actors();
	void imgui_render_transport();
	void imgui_render_sequencer();
	void imgui_render_selected_entry();

private:
	//	ImSequencer 用インターフェース。
	//	@tool ImSequencer は整数フレームの直接ポインタ(int**)でしかやり取りできないため、
	//	@tool 本体（timeline の各エントリ）は秒で持ったまま、表示用に frame_cache へ変換してから渡し、
	//	@tool Sequencer() 呼び出し後に秒へ書き戻す（sync_from_entries / sync_to_entries）
	struct sequence_interface : public ImSequencer::SequenceInterface
	{
		edit_timeline_scene* owner = nullptr;
		mutable std::string label_cache;
		mutable std::string type_name_cache;

		//	@tool flat index <-> フレームキャッシュ
		std::vector<std::pair<int, int>> frame_cache;
		void sync_from_entries();	//	timeline(秒) -> frame_cache(整数フレーム)
		void sync_to_entries();	//	frame_cache(整数フレーム、編集反映後) -> timeline(秒)

		bool resolve_animation(int flat_index, std::size_t& out_actor_index, std::size_t& out_entry_index) const;
		std::size_t total_animation_count() const;

		//	@tool アニメーションと同じ考え方で、配置(transform)区間を flat index から解決するヘルパー
		bool resolve_transform(int flat_index, std::size_t& out_actor_index, std::size_t& out_entry_index) const;
		std::size_t total_transform_count() const;

		int GetFrameMin() const override;
		int GetFrameMax() const override;
		int GetItemCount() const override;
		int GetItemTypeCount() const override;
		const char* GetItemTypeName(int type_index) const override;
		const char* GetItemLabel(int index) const override;
		void Get(int index, int** start, int** end, int* type, unsigned int* color) override;
		void Add(int type) override;
		void Del(int index) override;
		void Duplicate(int index) override;
	};

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<editor_camera> editor_camera_object;	//	区間外はこれが自由視点でmain_cameraを制御する
	std::shared_ptr<post_process_volume_object> post_process_volume;
	std::shared_ptr<editor_items::context> editor_context;

private:
	timeline_resource timeline;

	std::vector<std::shared_ptr<model_object>> actor_models;
	std::vector<int> last_active_animation_per_actor;

	//	再生状態
	bool is_playing = false;
	bool loop_playback = true;
	float current_time = 0.0f;	//	@tool 再生位置（秒）。これが本体
	int current_frame = 0;		//	@tool ImSequencer とのやり取り専用の一時値（内部固定の変換単位で毎フレーム同期する）

	//	発火中のエフェクトインスタンス
	std::unordered_map<size_t, effect_instance_ptr> active_effects;

	//	シーケンサー選択状態
	int selected_entry = -1;
	bool sequencer_expanded = true;
	int sequencer_first_frame = 0;

	sequence_interface sequencer_interface;
};
#endif // defined(USE_IMGUI)
