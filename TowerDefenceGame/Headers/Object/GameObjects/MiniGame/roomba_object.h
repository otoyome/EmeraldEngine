#pragma once

#include "object/collision/collision_object.h"

#include "object/model_object.h"
#include "object/camera_object.h"

#include "object/game_constant.h"

#include "system/finite_state_machine.h"

#include "effect_resource.h"
#include "effect_instance.h"

#include "Audio/audio_manager.h"

//	ルンバ制御用基底オブジェクト
class roomba_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(roomba_object, spatial_object)

public:
	// 共通ステートId
	enum class states
	{
		active,
		dead,
	};

	//	移動段階
	enum class move_mode
	{
		walk,
		dash,
	};

	//	状態定義フラグ
	enum class flags
	{
		movable,			//	移動可能フラグ
		on_ground,			//	接地フラグ
		invisible,			//	無敵フラグ
	};

	// エフェクトId
	enum class effect_id
	{
		dash_dust,
		spark,
		spark_smoke,
		dead,
	};

	//	判定形状情報
	struct bounding_descs
	{
		collision::bounding_object::types type = collision::bounding_object::types::box;
		bool is_follow_self = true;
		math::float3 local_scale;
		math::rotatorf local_rotator;
		math::float3 local_offset;
	};
	struct bounding_datas
	{
		bounding_descs desc;

		math::rotatorf rotator;
		math::float3 position;
	};

private:
	struct bounding_inner_datas
	{
		bool is_follow_self = false;
		collision_object_ptr object;
	};

	//	ステート変更時呼び出し
	using call_back_change_state = std::function<void(roomba_object*)>;
	using call_back_change_state_inner_map = std::unordered_map<std::string, call_back_change_state>;
	using call_back_change_state_map = std::unordered_map<std::string, call_back_change_state_inner_map>;

public:
	roomba_object(std::string_view name, std::shared_ptr<model_object> model, std::string_view collision_filter_name);
	~roomba_object() override;

	void fixed_update(float elapsed_time) override;
	void post_update(float elapsed_time) override;

	void on_finalize() override;
	void on_deactive() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	const std::shared_ptr<model_object>& get_model_object() const;
	void set_model_object(std::shared_ptr<model_object> model);

	texture_ptr get_face_texture() const;
	void set_face_texture(texture_ptr face);

	math::float4 get_face_uv() const;
	void set_face_uv(math::float4 face_uv);

	const collision_object_ptr& get_push_box() const;

	math::float3 get_scale() const;
	void set_scale(const math::float3& scale);

	math::rotatorf get_rotation() const;
	void set_rotation(math::rotatorf rotator);

	float get_rotation_speed() const;
	void set_rotation_speed(float rotation_speed);

	math::float3 get_position() const;
	void set_position(const math::float3& position);

	const math::float3& get_footstep_position() const;

	bool is_flag(flags flag) const { return system_utility::is_bitflag(flag_data, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(flag_data, 1 << static_cast<UINT>(flag), value); }

	math::float3 get_velocity() const;
	void set_velocity(math::float3 velocity);

	float get_gravity_scale() const;
	void set_gravity_scale(float gravity_scale);

	float get_step_offset() const;
	void set_step_offset(float step_offset);

	int get_wet_value() const;
	void set_wet_value(int wet_value);
	int get_max_wet_value() const;
	void set_max_wet_value(int max_wet_value);
	int get_heat_value() const;
	void set_heat_value(int heat_value);
	int get_max_heat_value() const;
	void set_max_heat_value(int max_heat_value);
	float get_wet_rate() const;
	float get_heat_rate() const;

	float get_attract_range() const;

	float get_elapsed_time_scale() const;
	void set_elapsed_time_scale(float elapsed_time_scale);

	math::float3 get_focus_point() const;

	void add_move_vector(math::float2 direction);

	void set_turn_vector(math::float2 direction);
	math::float2 get_turn_vector() const;

public:	//	速度系
	void add_force(math::float3 force);
	void add_impulse(math::float3 delta_velocity);

	float get_max_speed(move_mode mode) const;
	float get_current_accelaration_force() const;
	float get_accelaration_force(move_mode mode) const;

private:
	float get_current_braking_force() const;
	float get_current_friction() const;


	float calculate_drag_coefficient() const;
	float calculate_frontal_area() const;

public:	//	入力情報設定
	struct action_datas
	{
		math::float3 move = { 0, 0, 0 }; // 移動方向
		bool dash = false; // ダッシュ
		bool brake = false; // ブレーキ

		bool use_turn_vector = false; // 向きベクトルを別で指定する
		math::float2 turn_vector = { 0, 0 }; // 向き方向
	};
	void input_action_data(action_datas action_data);

public:	//	ステート関係
	std::string_view get_current_state() const;
	bool check_same_state(std::string_view state_name) const;
	bool check_same_state(states state) const;
	bool change_state(std::string_view state_id);
	bool change_state(states state);

	bool register_change_state_callback(std::string_view state_id, std::string_view identifier, call_back_change_state callback);
	bool unregister_change_state_callback(std::string_view state_id, std::string_view identifier);

public:	//	当たり関係
	void on_wet(float rate);

private:	//	内部ステート系
	template<typename Type, typename ... Arguments>
	bool register_state(const Arguments ... arguments)
	{
		return state_machine.register_state<Type, Arguments...>(arguments...);
	}

	void register_states();

	void update_values(float elapsed_time);

	void enter_active();
	void update_active(float elapsed_time);
	void exit_active();

	void enter_dead();
	void update_dead(float elapsed_time);
	void exit_dead();

private:	//	エフェクト系
	void register_effects();
	void update_effects(float elapsed_time);
	void unregister_effects();
	bool register_effect(size_t effect_id, std::string_view file_path);
	std::weak_ptr<effect_instance> play_effect(size_t effect_id);

private:	//	サウンド系
	void update_sounds();
	void stop_sounds();

private:	//	姿勢制御
	void update_rotation(float elapsed_time);
	void update_velocity(float elapsed_time);
	void update_move(float elapsed_time);

	bool calculate_position(math::float3 velocity, float elapsed_time, math::vector3d& out_position, math::float3& out_normal, float* out_friction = nullptr);

private:
	void generate_box_data(std::vector<bounding_inner_datas>& datas, const bounding_datas& bounding_data);

	virtual void on_push_box_enter(collision_object::data data);
	virtual void on_push_box_stay(collision_object::data data);

private:
	std::shared_ptr<model_object> model;
	int flag_data; // ビットフラグ情報

	int wet_value; // 濡れ量
	int max_wet_value; // 最大濡れ量
	int heat_value; // 熱量
	int max_heat_value; // 最大熱量

	float elapsed_time_scale; // 時間スケール
	action_datas action_data; // 入力情報

	finite_state_machine state_machine; // ステートマシン
	call_back_change_state_map on_change_states; // ステート変更時呼び出し関数群

	collision_object_ptr push_box; // 押し合い判定
	std::string collision_filter_name; // コリジョンフィルター名

	math::float2 move_vector; // 移動方向
	math::float2 turn_front_vector; // 回転方向
	math::float3 velocity; // 速度(m/s)
	math::float3 world_gravity; // ワールド重力(m/s2)
	float world_air_density;	// 空気密度(kg/m^3)
	float gravity_scale; // 重力スケール

	float rotation_speed; // 回転速度

	float velocity_tilt_pitch; // 速度ピッチ
	float velocity_tilt_roll; // 速度ロール
	float ground_tilt_pitch; // 地形ピッチ
	float ground_tilt_roll; // 地形ロール
	float death_spin_angle;	//	死亡後の回転蓄積角度
	math::rotatorf death_spin_rotator;	//	死亡時に固定する向き(進行方向)

	float slope_rate; // 坂道補正係数
	float step_offset; // 段差補正
	float slope_limit; // 傾斜角度が一定以上なら進めないようにする
	float ground_friction; // 地面摩擦係数
	math::float3 ground_normal; // 地面法線

	std::unordered_map<size_t, effect_resource_ptr> effect_resources; // エフェクトリソース
	std::vector<effect_instance_ptr> effect_instances; // エフェクトインスタンス

	effect_instance_ptr dash_effect_instance;
	effect_instance_ptr spark_effect_instance;
	effect_instance_ptr spark_smoke_effect_instance;
	size_t dash_effect_instance_node_index[2];

	bool last_dash_input = false;	//	直前の固定更新のダッシュ入力
	audio::sound_handle accelerate_sound;	//	加速音
	audio::sound_handle slide_sound;		//	横滑り音
};
