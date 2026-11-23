#include "object/gameobjects/MiniGame/roomba_controller_object.h"

#include "Input/input_action.h"
#include "Scenes/scene_manager.h"

namespace
{
	constexpr std::string_view SearchLockOnTarget = "SEARCH_LOCKON_TARGET";
	constexpr std::string_view ControllObjectDead = "CONTROLL_OBJECT_DEAD";
	constexpr std::string_view ControllObjectDeadComplete = "CONTROLL_OBJECT_DEAD_COMPLETE";

	constexpr std::string_view OnStaggerEnterId = "STAGGER_ENTER_CALLBACK";
}

std::shared_ptr<roomba_object> roomba_controller_object::get_controll_object() const
{
	return controll_object;
}

math::float3 roomba_controller_object::get_focus_point() const
{
	return controll_object->get_position();
}

bool roomba_controller_object::get_input_purmit_flag() const
{
	return input_purmit_flag;
}

void roomba_controller_object::set_input_purmit_flag(bool input_purmit_flag)
{
	this->input_purmit_flag = input_purmit_flag;
}

roomba_controller_object::roomba_controller_object(engine_context& context, std::string_view name, std::shared_ptr<roomba_object> controll_object, std::shared_ptr<camera_object> camera)
: game_object(name)
, work_time(0)
, input_purmit_flag(true)
, camera(camera)
, controll_object(controll_object)
, mode(modes::normal)
, action_datas()
, dash_action("roomba.dash", { keyboard::key_states::shift, mouse::key_states::left, gamepad::key_states::rt })
, brake_action("roomba.brake", { keyboard::key_states::ctrl, mouse::key_states::right, gamepad::key_states::lt })
, move_x_action("roomba.move_x", {
	{ keyboard::key_states::d, +1.0f },
	{ keyboard::key_states::a, -1.0f },
	{ gamepad::axis_states::left_x, 1.0f },
})
, move_y_action("roomba.move_y", {
	{ keyboard::key_states::w, +1.0f },
	{ keyboard::key_states::s, -1.0f },
	{ gamepad::axis_states::left_y, 1.0f },
})
, dash_pressed(false)
, brake_pressed(false)
, move_x(0.0f)
, move_y(0.0f)
{
	//	ì¸óÕçwì«
	set_context(context);
	core::signals::broadcaster& broadcaster = get_context().get_scenes().get_broadcaster();
	connections += dash_action.subscribe(broadcaster, [this](const input::action_event& event)
	{
		dash_pressed = event.phase == input::action_phases::pressed;
	});
	connections += brake_action.subscribe(broadcaster, [this](const input::action_event& event)
	{
		brake_pressed = event.phase == input::action_phases::pressed;
	});
	connections += move_x_action.subscribe(broadcaster, [this](const input::axis_event& event)
	{
		move_x = event.value;
	});
	connections += move_y_action.subscribe(broadcaster, [this](const input::axis_event& event)
	{
		move_y = event.value;
	});
}

roomba_controller_object::~roomba_controller_object()
{
}

void roomba_controller_object::fixed_update(float elapsed_time)
{
	//	ì¸óÕãLò^
	switch (mode)
	{
		case modes::normal:
		{
			for (size_t index = ARRAYSIZE(action_datas) - 1; index > 0; --index)
			{
				action_datas[index] = action_datas[index - 1];
			}
			action_datas[0].move = get_move_vector();
			action_datas[0].dash = dash_pressed;
			action_datas[0].brake = brake_pressed;

			//	ÉAÉNÉVÉáÉìèÓïÒà¯Ç´ìnÇµ
			static constexpr size_t check_frame = static_cast<size_t>(system_units::convert_sec_to_fps(0.1f) + 0.1f);
			static constexpr float frame_move_rate = 1.0f / static_cast<float>(check_frame);
			roomba_object::action_datas action_data = {};
			bool input_attack = false;
			for (size_t index = 0; index < check_frame; ++index)
			{
				action_data.move.x += action_datas[index].move.x * frame_move_rate;
				action_data.move.y += action_datas[index].move.y * frame_move_rate;
				action_data.move.z += action_datas[index].move.z * frame_move_rate;
				action_data.dash = action_datas[index].dash ? true : action_data.dash;
				action_data.brake = action_datas[index].brake ? true : action_data.brake;
			}

			//	
			if (input_purmit_flag)
			{
				controll_object->input_action_data(action_data);
			}
			break;
		}
	}
}

math::float3 roomba_controller_object::get_move_vector()
{
	float y_axis = move_y;
	float x_axis = move_x;
	if (y_axis == 0.0f && x_axis == 0.0f)
		return math::float3(0, 0, 0);

	//	ì|ÇµÇΩó ÇécÇ∑
	const float input_length = std::min(sqrtf(x_axis * x_axis + y_axis * y_axis), 1.0f);

	float camera_right_x = static_cast<float>(camera->get_world().get_right().x);
	float camera_right_z = static_cast<float>(camera->get_world().get_right().z);
	float camera_right_length = sqrtf(camera_right_x * camera_right_x + camera_right_z * camera_right_z);
	if (camera_right_length > 0.0f)
	{
		camera_right_x /= camera_right_length;
		camera_right_z /= camera_right_length;
	}

	float camera_front_x = static_cast<float>(camera->get_world().get_front().x);
	float camera_front_z = static_cast<float>(camera->get_world().get_front().z);
	float camera_front_length = sqrtf(camera_front_x * camera_front_x + camera_front_z * camera_front_z);
	if (camera_front_length > 0.0f)
	{
		camera_front_x	/= camera_front_length;
		camera_front_z	/= camera_front_length;
	}

	math::float3 vector;
	vector.x = (camera_right_x * x_axis) + (camera_front_x * y_axis);
	vector.y = 0.0f;
	vector.z = (camera_right_z * x_axis) + (camera_front_z * y_axis);
	float vector_length = sqrtf(vector.x * vector.x + vector.z * vector.z);
	vector.x *= input_length / vector_length;
	vector.z *= input_length / vector_length;
	return vector;
}

void roomba_controller_object::on_finalize()
{
	get_context().get_objects().unregister_object(shared_from_this());
	controll_object.reset();
}
