#pragma once

#include "object/gameobjects/MiniGame/roomba_object.h"

//	ルンバ用コントローラーオブジェクト
class roomba_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_controller_object, game_object)

private:
	enum class modes
	{
		normal,
	};

public:
	std::shared_ptr<roomba_object> get_controll_object() const;
	math::float3 get_focus_point() const;

	bool get_input_purmit_flag() const;
	void set_input_purmit_flag(bool input_stop);

public:
	roomba_controller_object(std::string_view name, std::shared_ptr<roomba_object> controll_object, std::shared_ptr<camera_object> camera);
	~roomba_controller_object() override;

	void fixed_update(float elapsed_time) override;

private:
	math::float3 get_move_vector();

	void on_finalize() override;

private:
	float work_time;
	bool input_purmit_flag;
	std::shared_ptr<camera_object> camera;
	std::shared_ptr<roomba_object> controll_object;

	modes mode;
	roomba_object::action_datas action_datas[system_units::frame_per_sec];
};
