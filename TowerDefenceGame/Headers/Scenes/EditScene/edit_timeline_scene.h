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
	//	@tool オブジェクトを1つ、空（kind::actor・モデル未設定）の状態で追加する。中身は object_kind コンボで切り替える
	void add_object();
	void remove_object(std::size_t object_index);
	void rebuild_object_models();

	//	@tool kind::actor から他の種類へ切り替えたときはモデルの登録を、kind::effect から他の種類へ切り替えたときは
	//	@tool 再生中エフェクトの登録をそれぞれ解除する（データ自体は保持したまま、シーン上の実体だけ片付ける）
	void on_object_kind_changed(std::size_t object_index, timeline_resource::object_entry::kind previous_kind);

	//	@tool ImSequencer は GetItemCount()==0 のとき Sequencer() が何も描画せず return するため
	//	@tool （ライブラリ側の仕様）、最初の1件を追加した際もシーケンサー自体の「+」が掴めない。
	//	@tool そのための代替ボタン用エントリポイント。type は sequence_interface::Add と同じ意味
	//	@tool （[0,objects.size()):該当オブジェクトへアニメーション追加 / [objects.size(),objects.size()*2):該当オブジェクトへ配置区間追加）
	void add_entry_and_select(int type);

	void new_timeline();
	void load_timeline();
	void save_timeline();

	void advance_playback(float elapsed_time);
	void apply_timeline_state();
	void clear_active_effects();
	void clear_object_models();

	void imgui_render_objects();
	void imgui_render_transport();
	void imgui_render_sequencer();
	void imgui_render_selected_entry();

private:
	//	ImSequencer 用インターフェース。
	//	@tool ImSequencer は整数フレームの直接ポインタ(int**)でしか扱えないため、
	//	@tool 本体（timeline の各エントリ）は秒を持ったまま、表示用に frame_cache へ変換してから渡し、
	//	@tool Sequencer() 呼び出し後に秒へ書き戻す（sync_from_entries / sync_to_entries）
	struct sequence_interface : public ImSequencer::SequenceInterface
	{
		edit_timeline_scene* owner = nullptr;
		mutable std::string label_cache;
		mutable std::string type_name_cache;

		//	@tool flat index <-> フレームキャッシュ
		std::vector<std::pair<int, int>> frame_cache;
		void sync_from_entries();	//	timeline(秒) -> frame_cache(整数フレーム)
		void sync_to_entries();	//	frame_cache(整数フレーム、編集反映) -> timeline(秒)

		//	@tool kind::actor のオブジェクトが持つアニメーション／配置(transform)の flat index を解決するヘルパー
		bool resolve_animation(int flat_index, std::size_t& out_object_index, std::size_t& out_entry_index) const;
		std::size_t total_animation_count() const;

		bool resolve_transform(int flat_index, std::size_t& out_object_index, std::size_t& out_entry_index) const;
		std::size_t total_transform_count() const;

		//	@tool kind::camera / kind::effect / kind::event のオブジェクトは、そのオブジェクト自身が1行ぶんのデータを持つ
		bool resolve_camera(int flat_index, std::size_t& out_object_index) const;
		std::size_t total_camera_count() const;

		bool resolve_effect(int flat_index, std::size_t& out_object_index) const;
		std::size_t total_effect_count() const;

		bool resolve_event(int flat_index, std::size_t& out_object_index) const;
		std::size_t total_event_count() const;

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
	std::shared_ptr<editor_camera> editor_camera_object;	//	区間外はこれが唯一直接main_cameraを制御する
	std::shared_ptr<post_process_volume_object> post_process_volume;
	std::shared_ptr<editor_items::context> editor_context;

private:
	timeline_resource timeline;

	std::vector<std::shared_ptr<model_object>> object_models;	//	@tool kind::actor のオブジェクトのみ有効な値が入る（他は nullptr）
	std::vector<int> last_active_animation_per_object;

	//	再生状態
	bool is_playing = false;
	bool loop_playback = true;
	float current_time = 0.0f;	//	@tool 再生位置（秒）。これが本体
	float previous_time = 0.0f;	//	@tool 直前フレームの再生位置（秒）。イベント通過判定に使う
	int current_frame = 0;		//	@tool ImSequencer とのやりとり専用の一時値（整数固定の変換単位で毎フレーム消える）

	//	相対中のエフェクトインスタンス
	std::unordered_map<size_t, effect_instance_ptr> active_effects;

	//	シーケンサー選択状態
	int selected_entry = -1;
	bool sequencer_expanded = true;
	int sequencer_first_frame = 0;

	sequence_interface sequencer_interface;
};
#endif // defined(USE_IMGUI)
