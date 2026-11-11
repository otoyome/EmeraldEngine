#pragma once

#include "collision/collision.h"

#include "Animation/ik_resource.h"
#include "Animation/ik_solver.h"
#include "Model/model_instance.h"
#include "object/spatial_object.h"

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"
#endif	//	defined(USE_IMGUI)

//	モデル表示用オブジェクト
class model_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(model_object, spatial_object)

public:
	using call_back_root_animation_translation = std::function<void(const math::float3&, float)>;
	using call_back_root_animation_rotation = std::function<void(const math::float4&, float)>;

	//	FootIK方向
	static constexpr math::float3 foot_ik_direction = {0, -1, 0};

	//	IK情報
	struct ik_data
	{
		bool visible = false;
		bool initialized = false;	//	初回フレーム判定

		//	HandIK用
		math::float3 left_hand_target_position = {0, 0, 0};
		math::float3 right_hand_target_position = {0, 0, 0};

		//	AimIK用
		math::float3 aim_target_position;
	};

public:
	model_instance_ptr get_model_instance();
	ik_resource_ptr get_ik_resource();
	void set_ik_resource(ik_resource_ptr ik_resource);

	bool get_apply_root_animation_transform() const;
	void set_apply_root_animation_transform(bool flag);

public:
	model_object(std::string_view name);
	model_object(std::string_view name, std::string_view file_path);
	model_object(std::string_view name, model_resource_ptr resource);
	~model_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

	void fixed_update(float elapsed_time) override;
	void pre_render() override;

	void update_transform();
	void set_node_pose(size_t node_index, const model_instance::node_animation& node);

	bool register_root_animation_translation_callback(call_back_root_animation_translation callback);
	bool unregister_root_animation_translation_callback();

	bool register_root_animation_rotation_callback(call_back_root_animation_rotation callback);
	bool unregister_root_animation_rotation_callback();

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	void play_animation(size_t animation_index, bool loop, float blend_time = 0.2f);
	std::string_view get_playing_animation_name() const;

	size_t get_animation_index() const;
	size_t get_animation_index_from_name(std::string_view animation_name) const;

	float get_animation_end_time() const;
	float get_animation_time() const;
	void set_animation_time(float animation_time);
	float get_animation_speed() const;
	void set_animation_speed(float animation_speed);

	std::vector<std::string_view> get_animation_names() const;
	size_t get_animation_max() const;
	bool get_animation_loop() const;
	void set_animation_loop(bool flag);

public:
	size_t search_ik_behavior_index(std::string_view name) const;

	const std::vector<ik_data>& get_ik_behavior_datas() const { return ik_datas; }
	bool set_ik_behavior_datas(std::string_view name, const ik_data& data);
	bool set_ik_behavior_datas(size_t index, const ik_data& data);

private:
	void on_active() override;
	void on_deactive() override;
	void on_finalize() override;

private:
	void update_node_animation(const animation::data& animation_data, float current_time, float previous_time);
	void update_morph_animation(const animation::data& animation_data, float current_time, float previous_time);
	void update_ik();

	void load_model(std::string_view file_path);

private:
	model_instance_ptr model_instance;
	bool dirty = true;
	std::uint32_t applied_spatial_version = UINT32_MAX;

	bool apply_root_animation_transform = true;
	call_back_root_animation_translation on_root_animation_translation;
	call_back_root_animation_rotation on_root_animation_rotation;

	size_t animation_index = 0;
	bool animation_loop = false;
	float animation_time = 0;
	float animation_speed = 1.0f;

	float animation_blend_time = 0.0f;
	std::vector<model_instance::node_animation> current_node_animations;
	std::vector<model_instance::node_animation> previous_node_animations;
	std::vector<animation::morph> current_morph_animations;

	//	IK情報
	bool fixed_updated = false;
	bool animation_foot_ik = false;
	bool animation_hand_ik = false;
	bool animation_aim_ik = false;
	ik_solver_ptr ik_solver;
	std::vector<ik_data> ik_datas;

#if defined(USE_IMGUI)
	std::shared_ptr<editor_items::context> editor_context;
#endif	//	defined(USE_IMGUI)
};
