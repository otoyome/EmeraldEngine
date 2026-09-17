#pragma once

#include "object/collision/collision_object.h"

#include "object/model_object.h"
#include "object/camera_object.h"

#include "object/game_constant.h"

#include "system/finite_state_machine.h"

#include "effect_resource.h"
#include "effect_instance.h"

//	ルンバ制御用基底オブジェクト
class roomba_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_object, game_object)

public:
	// 共通ステートId
	enum state_ids
	{
		state_idle,
		state_walk,
		state_dash,
		state_move_stop,

		state_common_max,
	};

	static constexpr std::string_view state_names[] =
	{
		"state_idle",
		"state_walk",
		"state_dash",
		"state_move_stop",
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

	//	判定形状情報
	struct bounding_descs
	{
		collision::bounding_object::types type = collision::bounding_object::types::box;
		bool is_follow_self = true;
		DirectX::XMFLOAT3 local_scale;
		math::rotator local_rotator;
		DirectX::XMFLOAT3 local_offset;
	};
	struct bounding_datas
	{
		bounding_descs desc;

		math::rotator rotator;
		DirectX::XMFLOAT3 position;
	};

	//	体力変動情報
	struct health_varing_descs
	{
		int value = 0; // 変動量
		bool defat_attack = true; // 撃破誘発技
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

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	const std::shared_ptr<model_object>& get_model_object() const;
	void set_model_object(std::shared_ptr<model_object> model);

	texture_ptr get_face_texture() const;
	void set_face_texture(texture_ptr face);

	DirectX::XMFLOAT4 get_face_uv() const;
	void set_face_uv(DirectX::XMFLOAT4 face_uv);

	const collision_object_ptr& get_push_box() const;

	const DirectX::XMFLOAT3& get_scale() const;
	void set_scale(const DirectX::XMFLOAT3& scale);

	math::rotator get_rotation() const;
	void set_rotation(math::rotator rotator);

	float get_rotation_speed() const;
	void set_rotation_speed(float rotation_speed);

	const DirectX::XMFLOAT3& get_position() const;
	void set_position(const DirectX::XMFLOAT3& position);

	const DirectX::XMFLOAT3& get_footstep_position() const;

	bool is_flag(flags flag) const { return system_utility::is_bitflag(flag_data, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(flag_data, 1 << static_cast<UINT>(flag), value); }

	DirectX::XMFLOAT3 get_velocity() const;
	void set_velocity(DirectX::XMFLOAT3 velocity);

	float get_gravity_scale() const;
	void set_gravity_scale(float gravity_scale);

	float get_step_offset() const;
	void set_step_offset(float step_offset);

	int get_health() const;
	void set_health(int health);
	int get_max_health() const;
	void set_max_health(int max_health);

	float get_elapsed_time_scale() const;
	void set_elapsed_time_scale(float elapsed_time_scale);

	DirectX::XMFLOAT3 get_focus_point() const;

	void add_move_vector(DirectX::XMFLOAT2 direction, float accelaration);

	void set_turn_vector(DirectX::XMFLOAT2 direction);
	DirectX::XMFLOAT2 get_turn_vector() const;

	void add_impulse(DirectX::XMFLOAT3 delta_velocity);

	float get_max_move_speed() const;
	void set_max_move_speed(float max_move_speed);

	float get_breaking_deceleration() const;
	void set_breaking_deceleration(float breaking_deceleration);

	float get_air_drag_coefficient() const;

public:	//	入力情報設定
	struct action_datas
	{
		DirectX::XMFLOAT3 move = { 0, 0, 0 }; // 移動方向
		bool dash = false; // ダッシュ
		bool attract = false; // 吸収

		bool use_turn_vector = false; // 向きベクトルを別で指定する
		DirectX::XMFLOAT2 turn_vector = { 0, 0 }; // 向き方向
	};
	void input_action_data(action_datas action_data);

public:	//	ステート関係
	std::string_view get_current_state() const;
	bool check_same_state(std::string_view state_name) const;
	bool change_state(std::string_view state_id);

	bool register_change_state_callback(std::string_view state_id, std::string_view identifier, call_back_change_state callback);
	bool unregister_change_state_callback(std::string_view state_id, std::string_view identifier);

private:	//	内部ステート系
	template<typename Type, typename ... Arguments>
	bool register_state(const Arguments ... arguments)
	{
		return state_machine.register_state<Type, Arguments...>(arguments...);
	}

	void register_states();

	bool input_action(float elapsed_time);
	bool input_move(float elapsed_time, move_mode mode);

	void enter_idle();
	void update_idle(float elapsed_time);
	void exit_idle();

	void enter_walk();
	void update_walk(float elapsed_time);
	void exit_walk();

	void enter_dash();
	void update_dash(float elapsed_time);
	void exit_dash();

	void enter_move_stop();
	void update_move_stop(float elapsed_time);
	void exit_move_stop();

private:	//	内部エフェクト系
	void register_effects();
	bool register_effect(size_t effect_id, std::string_view file_path);
	std::weak_ptr<effect_instance> play_effect(size_t effect_id);
	void adjustment_velocity(DirectX::XMFLOAT2& move, float adjust_move_velocity_rate);
	bool calculate_position(DirectX::XMFLOAT3 velocity, bool vertical, DirectX::XMFLOAT3& out_position, DirectX::XMFLOAT3& out_normal, float* out_friction = nullptr);

private:	//	姿勢制御
	void update_rotation(float elapsed_time);
	void update_vertical_velocity(float elapsed_time);
	void update_vertical_move(float elapsed_time);
	void update_horizontal_velocity(float elapsed_time);
	void update_horizontal_move(float elapsed_time);

private:
	void generate_box_data(std::vector<bounding_inner_datas>& datas, const bounding_datas& bounding_data);

	virtual void on_push_box_enter(collision_object::data data);
	virtual void on_push_box_stay(collision_object::data data);

private:
	std::shared_ptr<model_object> model;
	int flag_data; // ビットフラグ情報
	int health; // 体力
	int max_health; // 最大体力
	float elapsed_time_scale; // 時間スケール
	action_datas action_data; // 入力情報

	finite_state_machine state_machine; // ステートマシン
	call_back_change_state_map on_change_states; // ステート変更時呼び出し関数群

	collision_object_ptr push_box; // 押し合い判定
	std::string collision_filter_name; // コリジョンフィルター名

	DirectX::XMFLOAT2 move_vector; // 移動方向
	DirectX::XMFLOAT2 turn_front_vector; // 回転方向
	DirectX::XMFLOAT3 velocity; // 速度(m/s)
	DirectX::XMFLOAT3 world_gravity; // ワールド重力(m/s2)
	float accelaration; // 加速量(m/s2)
	float breaking_deceleration; // 減速量(m/s2)
	float max_move_speed; // 最大速度(m/s)
	float rotation_speed; // 回転速度
	float roll_value; // 傾き量

	float gravity_scale; // 重力スケール
	float slope_rate; // 坂道補正係数
	float step_offset; // 段差補正
	float slope_limit; // 傾斜角度が一定以上なら進めないようにする
	float ground_friction; // 地面摩擦係数
	DirectX::XMFLOAT3 ground_normal; // 地面法線

	DirectX::XMFLOAT3 frame_gravity; // 1Fの重力(deltaの方が良いか？)

	std::unordered_map<size_t, effect_resource_ptr> effect_resources; // エフェクトリソース
	std::vector<effect_instance_ptr> effect_instances; // エフェクトインスタンス
};
