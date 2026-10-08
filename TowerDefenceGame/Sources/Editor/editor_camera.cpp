#include "Editor/editor_camera.h"

#if	defined(USE_IMGUI)

editor_camera::editor_camera(std::shared_ptr<camera_object> source)
: game_object("editor_camera")
, source(source)
, camera(std::make_shared<::camera>())

, execute(false)

, rotation(0, 0, 0)
, position(0, 0, 0)
{
	//	パラメーターコピー
	if (source)
		camera->get_parameters() = source->get_camera()->get_parameters();

	//	生成時点で視点の候補にする
	camera_manager::instance()->push_view_target(camera);
}

editor_camera::~editor_camera()
{
	if (camera_manager::is_valid())
	{
		camera_manager::instance()->remove_view_target(camera);
	}
}

void editor_camera::on_active()
{
	game_object::on_active();
	camera_manager::instance()->push_view_target(camera);
}

void editor_camera::on_deactive()
{
	game_object::on_deactive();
	camera_manager::instance()->remove_view_target(camera);
}

void editor_camera::pre_render()
{
	game_object::pre_render();

	//	視点姿勢を視点パラメーターに反映反映
	auto& parameters = camera->get_parameters();
	if (source)
		parameters = source->get_camera()->get_parameters();
	parameters.rotation = rotation;
	parameters.position = math::vector3d(position);
}

void editor_camera::debug_update(float elapsed_time)
{
	auto mouse_state = mouse::instance()->get_state();
	auto keyboard_state = keyboard::instance()->get_state();

	//	キーボード操作
	if (mouse_state.is_key_state(mouse::key_states::right) == input::state::hold)
	{
		int x = 0, y = 0, z = 0;
		if (keyboard_state.is_key_state(keyboard::key_states::w) == input::state::hold)
		{
			++z;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::s) == input::state::hold)
		{
			--z;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::d) == input::state::hold)
		{
			++x;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::a) == input::state::hold)
		{
			--x;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::e) == input::state::hold)
		{
			++y;
		}
		if (keyboard_state.is_key_state(keyboard::key_states::q) == input::state::hold)
		{
			--y;
		}

		int wheel = mouse::instance()->get_wheel_value();
		if (wheel != 0)
		{
			move_speed += move_speed * 0.001f * wheel;
		}

		float speed = move_speed * system_units::convert_sec_to_fps(elapsed_time);
		math::float3 front = rotation.front().to_float3();
		math::float3 right = rotation.right().to_float3();
		math::float3 up = rotation.up().to_float3();
		position.x += +front.x * speed * z + right.x * speed * x - up.x * speed * y;
		position.y += +front.y * speed * z + right.y * speed * x - up.y * speed * y;
		position.z += +front.z * speed * z + right.z * speed * x - up.z * speed * y;
	}

	// マウス操作
	if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
	{
		math::float2 move;
		move.x = static_cast<float>(mouse::instance()->get_move_cursol().x);
		move.y = static_cast<float>(mouse::instance()->get_move_cursol().y);
		int wheel = mouse::instance()->get_wheel_value();
		move.x *= math::step(1.0f, fabsf(move.x));
		move.y *= math::step(1.0f, fabsf(move.y));
		if (mouse_state.is_key_state(mouse::key_states::right) == input::state::hold)
		{
			float speed = rotation_speed * system_units::convert_sec_to_fps(elapsed_time);
			rotation.x += move.y * speed;
			rotation.y += move.x * (rotation.up().y < 0 ? -speed : speed);
		}

		if (mouse_state.is_key_state(mouse::key_states::middle) == input::state::hold ||
			(mouse_state.is_key_state(mouse::key_states::right) != input::state::hold && wheel != 0))
		{
			float speed = system_units::convert_m_to_unit(0.02f) * system_units::convert_sec_to_fps(elapsed_time);
			math::float3 front = rotation.front().to_float3();
			math::float3 right = rotation.right().to_float3();
			math::float3 up = rotation.up().to_float3();
			position.x += +front.x * speed * wheel - right.x * speed * move.x + up.x * speed * move.y;
			position.y += +front.y * speed * wheel - right.y * speed * move.x + up.y * speed * move.y;
			position.z += +front.z * speed * wheel - right.z * speed * move.x + up.z * speed * move.y;
		}

		if (mouse_state.is_key_state(mouse::key_states::left) != input::state::none)
		{
			//	座標情報を送る
			mouse_position_data data;
			data.position.x = static_cast<float>(mouse_state.position.position.x);
			data.position.y = static_cast<float>(mouse_state.position.position.y);
			if (mouse_state.is_key_state(mouse::key_states::left) == input::state::down)
				object_manager::instance()->get_messenger().send_data(left_click_down_identifier, &data);
			else if (mouse_state.is_key_state(mouse::key_states::left) == input::state::hold)
				object_manager::instance()->get_messenger().send_data(left_click_hold_identifier, &data);
			else if (mouse_state.is_key_state(mouse::key_states::left) == input::state::release)
				object_manager::instance()->get_messenger().send_data(left_click_release_identifier, &data);
		}
	}

	if (is_auto_rotate)
	{
		rotation.y += math::angle::to_radian(1.0f * elapsed_time);
	}
}

void editor_camera::on_execute()
{
	rotation = math::rotatorf(source->get_spatial()->get_world_rotation().to_float4());
	position = source->get_spatial()->get_world_translation().to_float3();
}

#endif	//	defined(USE_IMGUI)
