#pragma once

#include "Scenes/scene_system.h"

#if defined(USE_IMGUI)
#include <unordered_map>

#include "Animation/timeline_resource.h"

#include "Object/Graphics/model_object.h"
#include "Object/Graphics/camera_object.h"
#include "Object/Graphics/post_process_volume_object.h"

#include "Effect/effect_resource.h"
#include "Effect/effect_instance.h"

#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"

#include "ImGuizmo/ImSequencer.h"

//	タイムライン編集シーン
class edit_timeline_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(edit_timeline_system, scene_system)

public:
	edit_timeline_system();
	edit_timeline_system(const edit_timeline_system&) = delete;
	edit_timeline_system(edit_timeline_system&&) = delete;
	~edit_timeline_system() override;

protected:
	void on_begin() override { initialize_core(); }
	void on_end() override { uninitialize_core(); }
	void pre_update(float elapsed_time) override { update_core(elapsed_time); }
	void imgui_render() override { imgui_render_core(); }

	bool initialize_core();
	bool uninitialize_core();
	void update_core(float elapsed_time);
	void imgui_render_core();

private:
	void add_object();
	void remove_object(std::size_t object_index);
	void rebuild_object_models();

	void on_object_kind_changed(std::size_t object_index, timeline_resource::object_entry::kind previous_kind);

	void add_entry_and_select(int type);

	void new_timeline();
	void load_timeline();
	void open_timeline(const std::string& file_path);
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
	struct sequence_interface : public ImSequencer::SequenceInterface
	{
		edit_timeline_system* owner = nullptr;
		mutable std::string label_cache;
		mutable std::string type_name_cache;

		std::vector<std::pair<int, int>> frame_cache;

		void sync_from_entries();
		void sync_to_entries();

		bool resolve_animation(int flat_index, std::size_t& out_object_index, std::size_t& out_entry_index) const;
		std::size_t total_animation_count() const;

		bool resolve_transform(int flat_index, std::size_t& out_object_index, std::size_t& out_entry_index) const;
		std::size_t total_transform_count() const;

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
	const view_parameters* find_current_view_parameters() const;

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<editor_camera_object> editor_camera;
	std::shared_ptr<editor_items::context> editor_context;

	std::vector<std::shared_ptr<model_object>> object_models;
	std::vector<int> last_active_animation_per_object;
	std::unordered_map<size_t, effect_instance_ptr> active_effects;

private:
	timeline_resource timeline;

	//	再生状態
	bool is_playing = false;
	bool loop_playback = true;
	float current_time = 0.0f;
	float previous_time = 0.0f;
	int current_frame = 0;

private:	//	シーケンサー関係
	int selected_entry = -1;
	bool sequencer_expanded = true;
	int sequencer_first_frame = 0;

	sequence_interface sequencer_interface;
};
#endif // defined(USE_IMGUI)
