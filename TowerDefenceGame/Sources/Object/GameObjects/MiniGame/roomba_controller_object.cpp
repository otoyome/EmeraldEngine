#include "object/gameobjects/MiniGame/roomba_controller_object.h"

#include "object/game_constant.h"

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

DirectX::XMFLOAT3 roomba_controller_object::get_focus_point() const
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

roomba_controller_object::roomba_controller_object(std::string_view name, std::shared_ptr<roomba_object> controll_object, std::shared_ptr<camera_object> camera)
: game_object(name)
, work_time(0)
, input_purmit_flag(true)
, camera(camera)
, controll_object(controll_object)
, mode(modes::default)
, action_datas()
{
}

roomba_controller_object::~roomba_controller_object()
{
}

void roomba_controller_object::fixed_update(float elapsed_time)
{
	//	€–S”jŠüˆ—
	//if (dead_state != dead_states::none)
	//{
	//	if (dead_state == dead_states::call)
	//		dead_process();
	//	return;
	//}

	//	“ü—Í‹L˜^
	auto keyboard_state = keyboard::instance()->get_state();
	auto mouse_state = mouse::instance()->get_state();
	switch (mode)
	{
		case modes::default:
		{
			for (size_t index = ARRAYSIZE(action_datas) - 1; index > 0; --index)
			{
				action_datas[index] = action_datas[index - 1];
			}
			action_datas[0].move = get_move_vector();
			action_datas[0].dash = keyboard_state.is_key_state(keyboard::key_states::shift) == input::state::hold ||
								   mouse_state.is_key_state(mouse::key_states::left) == input::state::hold;
			action_datas[0].brake = keyboard_state.is_key_state(keyboard::key_states::ctrl) == input::state::hold ||
								   mouse_state.is_key_state(mouse::key_states::right) == input::state::hold;

			//	ƒAƒNƒVƒ‡ƒ“î•ñˆø‚«“n‚µ
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

DirectX::XMFLOAT3 roomba_controller_object::get_move_vector()
{
	auto keyboard_state = keyboard::instance()->get_state();
	int y_axis  = (keyboard_state.is_key_state(keyboard::key_states::w) == input::state::hold)
				- (keyboard_state.is_key_state(keyboard::key_states::s) == input::state::hold);
	int x_axis  = (keyboard_state.is_key_state(keyboard::key_states::d) == input::state::hold)
				- (keyboard_state.is_key_state(keyboard::key_states::a) == input::state::hold);
	if (y_axis == 0 && x_axis == 0)
		return DirectX::XMFLOAT3(0, 0, 0);

	float camera_right_x = camera->get_right().x;
	float camera_right_z = camera->get_right().z;
	float camera_right_length = sqrtf(camera_right_x * camera_right_x + camera_right_z * camera_right_z);
	if (camera_right_length > 0.0f)
	{
		camera_right_x /= camera_right_length;
		camera_right_z /= camera_right_length;
	}

	float camera_front_x = camera->get_front().x;
	float camera_front_z = camera->get_front().z;
	float camera_front_length = sqrtf(camera_front_x * camera_front_x + camera_front_z * camera_front_z);
	if (camera_front_length > 0.0f)
	{
		camera_front_x	/= camera_front_length;
		camera_front_z	/= camera_front_length;
	}

	DirectX::XMFLOAT3 vector;
	vector.x = (camera_right_x * x_axis) + (camera_front_x * y_axis);
	vector.y = 0.0f;
	vector.z = (camera_right_z * x_axis) + (camera_front_z * y_axis);
	float vector_length = sqrtf(vector.x * vector.x + vector.z * vector.z);
	vector.x /= vector_length;
	vector.z /= vector_length;
	return vector;
}

void roomba_controller_object::on_finalize()
{
	object_manager::instance()->unregister_object(shared_from_this());
	controll_object.reset();
}
