#pragma once

#include "camera.h"
#include "object/game_object.h"

#include "object/gameobjects/MiniGame/roomba_controller_object.h"

#include "animation_resource.h"

//	ルンバ制御用オブジェクト
class roomba_camera_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_camera_controller_object, game_object)

public:
	// ステート
	enum class states
	{
		entry,

		free,

		clear_performance,
	};

public:
	std::shared_ptr<camera_object> get_camera_object() const {return camera;}

	const math::rotatorf& get_rotation() const;
	void set_rotation(const math::rotatorf& rotator);

	const math::float3& get_position() const;
	void set_position(const math::float3& position);

	void set_input_purmit_flag(bool flag);

	//	カメラ側のアニメーションが進行可能判定フラグ
	bool is_camera_transition_wait();

public:
	roomba_camera_controller_object(std::string_view name, std::shared_ptr<roomba_controller_object> player_controller, std::shared_ptr<camera_object> camera);
	~roomba_camera_controller_object() override;

	void fixed_update(float elapsed_time) override;
	void update(float elapsed_time) override;

	void on_finalize() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

	void change_state(states state);

private:
	void fixed_update_entry(float elapsed_time);
	void fixed_update_default(float elapsed_time);

	bool play_animation(std::string_view animation_name, bool loop);
	void update_animation(float time);
	bool is_playing_animation();
	float calculate_animation_rate();

private:
	std::shared_ptr<roomba_controller_object> player_controller;
	std::shared_ptr<camera_object> camera;

	states state; // モード
	int sub_state; // サブステート
	float work_float; // ワーク変数
	bool input_purmit_flag; // 入力許可フラグ

	float minimum_distance; // 最低距離
	float current_distance; // 現在距離
	float maximum_distance; // 最大距離
	float game_distance_scale; // ゲーム側からの距離補正係数
	float game_distance_scale_inner;
	float game_distance_scale_rate;

	math::rotatorf disengage_direction; // カメラを離す方向
	float update_disengage_direction_inner_rate; // 方向適応割合
	float update_disengage_direction_rate; // 方向適応割合

	math::float3 focus; // 注視点座標
	float focus_distance; // 注視点距離
	float focus_limit_range; // 注視点移動制限範囲
	float update_focus_rate_inner; // 注視点更新係数

	float update_fov_rate_inner; // 視野角更新係数
	float dead_wait_time;
	math::float3 dead_focus;
};
