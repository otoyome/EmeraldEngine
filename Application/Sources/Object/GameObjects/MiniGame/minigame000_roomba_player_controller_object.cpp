#include "object/gameobjects/MiniGame/minigame000_roomba_player_controller_object.h"

#include "Object/Graphics/camera_object.h"
#include "Scenes/scene_manager.h"

minigame000::roomba_player_controller_object::roomba_player_controller_object(std::string_view name)
: base(name)
, camera()
, input_purmit_flag(true)

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
}

minigame000::roomba_player_controller_object::~roomba_player_controller_object()
{
}

void minigame000::roomba_player_controller_object::on_initialize()
{
	base::on_initialize();

	//	“ü—Íw“Ç
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

void minigame000::roomba_player_controller_object::on_finalize()
{
	connections.disconnect_all();
	camera.reset();

	base::on_finalize();
}

#if defined(USE_IMGUI)
void minigame000::roomba_player_controller_object::imgui_render_core()
{
	base::imgui_render_core();

	ImGui::Checkbox("input_purmit_flag", &input_purmit_flag);

	ImGui::SeparatorText("input");
	ImGui::Text("move : %.2f %.2f", move_x, move_y);
	ImGui::Text("dash : %s", dash_pressed ? "true" : "false");
	ImGui::Text("brake : %s", brake_pressed ? "true" : "false");

	const std::shared_ptr<camera_object> target = camera.lock();
	const core::cache_string camera_name(target ? target->get_name() : std::string_view("(none)"));
	ImGui::Text("camera : %s", camera_name.get<core::utf8_cache>().data());
}
#endif	//	defined(USE_IMGUI)

bool minigame000::roomba_player_controller_object::make_command(std::uint32_t tick, roomba_input_command& out_command)
{
	out_command = {};
	out_command.tick = tick;
	if (input_purmit_flag)
	{
		out_command.move = get_move_vector();
		out_command.dash = dash_pressed;
		out_command.brake = brake_pressed;
	}
	return true;
}

math::float3 minigame000::roomba_player_controller_object::get_move_vector() const
{
	float y_axis = move_y;
	float x_axis = move_x;
	if (y_axis == 0.0f && x_axis == 0.0f)
		return math::float3(0, 0, 0);

	//	“|‚µ‚½—Ê‚ğc‚·
	const float input_length = std::min(sqrtf(x_axis * x_axis + y_axis * y_axis), 1.0f);

	//	ƒJƒƒ‰Šî€‚ÌˆÚ“®•ûŒü‚ğæ“¾
	float camera_right_x = 1.0f;
	float camera_right_z = 0.0f;
	float camera_front_x = 0.0f;
	float camera_front_z = 1.0f;
	if (const std::shared_ptr<camera_object> target = camera.lock())
	{
		camera_right_x = static_cast<float>(target->get_world().get_right().x);
		camera_right_z = static_cast<float>(target->get_world().get_right().z);
		float camera_right_length = sqrtf(camera_right_x * camera_right_x + camera_right_z * camera_right_z);
		if (camera_right_length > 0.0f)
		{
			camera_right_x /= camera_right_length;
			camera_right_z /= camera_right_length;
		}

		camera_front_x = static_cast<float>(target->get_world().get_front().x);
		camera_front_z = static_cast<float>(target->get_world().get_front().z);
		float camera_front_length = sqrtf(camera_front_x * camera_front_x + camera_front_z * camera_front_z);
		if (camera_front_length > 0.0f)
		{
			camera_front_x	/= camera_front_length;
			camera_front_z	/= camera_front_length;
		}
	}

	//	is•ûŒüZo
	math::float3 vector;
	vector.x = (camera_right_x * x_axis) + (camera_front_x * y_axis);
	vector.y = 0.0f;
	vector.z = (camera_right_z * x_axis) + (camera_front_z * y_axis);
	float vector_length = sqrtf(vector.x * vector.x + vector.z * vector.z);
	if (vector_length <= FLT_EPSILON)
		return math::float3(0, 0, 0);

	vector.x *= input_length / vector_length;
	vector.z *= input_length / vector_length;
	return vector;
}