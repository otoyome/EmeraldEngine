#include "Editor/editor_camera.h"

#if	defined(USE_IMGUI)
#include "Object/object_manager.h"
#include "System/console_manager.h"
#include "System/world_viewport.h"

namespace
{
	//	エディターカメラの設定
	console_variable<bool> enable_variable("editor.camera.enable", false, "エディターカメラで描画視点を上書きする", console_flags::persist);
	console_variable<float> move_speed_variable("editor.camera.move_speed", system_units::convert_m_to_unit(0.2f) * 0.5f, "エディターカメラの移動速度（シーンの速度倍率が掛かる）", console_flags::persist);
	console_variable<float> rotate_speed_variable("editor.camera.rotate_speed", 0.0125f * 0.5f, "エディターカメラの回転速度（シーンの速度倍率が掛かる）", console_flags::persist);

	console_command toggle_command("editor.camera.toggle", "エディターカメラの切り替え",
		[](const console_command::arguments&)
		{
			enable_variable.set(!enable_variable.get());
		});
}

editor_camera::editor_camera()
: camera(std::make_shared<::camera>())
, rotation(0, 0, 0)
, position(math::vector3d::zero())
, rotation_speed(rotate_speed_variable.get())
, move_speed(move_speed_variable.get())
{
}

editor_camera::~editor_camera()
{
}

void editor_camera::apply_view(const view_parameters* base)
{
	//	視点姿勢を視点パラメーターに反映反映
	auto& parameters = camera->get_parameters();
	if (base)
		parameters = *base;
	parameters.rotation = rotation;
	parameters.position = position;
}

void editor_camera::update(float elapsed_time)
{
	auto mouse_state = mouse::instance()->get_state();
	auto keyboard_state = keyboard::instance()->get_state();

	//	移動は単精度の移動量を求めてから倍精度の座標に足す
	auto add_position = [this](const math::float3& delta)
	{
		position.x += delta.x;
		position.y += delta.y;
		position.z += delta.z;
	};

	//	キーボード操作
	//	@tool 入力はこのワールドを映している画面が受け付けている時だけ使う
	if (world_viewport::instance()->accepts_keyboard() && mouse_state.is_key_state(mouse::key_states::right) == input::state::hold)
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
		add_position({
			+front.x * speed * z + right.x * speed * x - up.x * speed * y,
			+front.y * speed * z + right.y * speed * x - up.y * speed * y,
			+front.z * speed * z + right.z * speed * x - up.z * speed * y });
	}

	// マウス操作
	if (world_viewport::instance()->accepts_mouse())
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
			add_position({
				+front.x * speed * wheel - right.x * speed * move.x + up.x * speed * move.y,
				+front.y * speed * wheel - right.y * speed * move.x + up.y * speed * move.y,
				+front.z * speed * wheel - right.z * speed * move.x + up.z * speed * move.y });
		}

		if (mouse_state.is_key_state(mouse::key_states::left) != input::state::none)
		{
			//	座標情報を送る
			//	@tool 座標はこのワールドを映している画面の中の座標にする
			mouse_position_data data;
			data.position = world_viewport::instance()->to_view_position({ static_cast<float>(mouse_state.position.position.x), static_cast<float>(mouse_state.position.position.y) });
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

////////////////////////////////////////////////////////////////////////////////////////////////////////

editor_camera_manager::editor_camera_manager()
: camera(std::make_shared<editor_camera>())
{
}

editor_camera_manager::~editor_camera_manager()
{
	if (enabled)
		end_camera();
}

editor_camera_manager::scene_request_ptr editor_camera_manager::request(const math::vector3d& position, const math::rotatorf& rotation, float speed_scale)
{
	auto result = std::make_shared<scene_request>();
	requests.emplace_back(result);

	initial_position = position;
	initial_rotation = rotation;
	has_initial_pose = true;
	if (enabled)
	{
		camera->set_position(initial_position);
		camera->set_rotation(initial_rotation);
		has_initial_pose = false;
	}
	camera->set_rotation_speed(rotate_speed_variable.get() * speed_scale);
	camera->set_move_speed(move_speed_variable.get() * speed_scale);
	return result;
}

bool editor_camera_manager::is_user_enabled() const
{
	return enable_variable.get();
}

void editor_camera_manager::set_user_enabled(bool enabled)
{
	enable_variable.set(enabled);
}

bool editor_camera_manager::compute_enabled()
{
	requests.erase(std::remove_if(requests.begin(), requests.end(),
		[](const std::weak_ptr<scene_request>& request) { return request.expired(); }), requests.end());

	//	一時停止の要求が一つでもあれば無効
	for (auto& request : requests)
	{
		if (auto locked = request.lock(); locked && locked->is_suspended())
			return false;
	}
	return enable_variable.get() || !requests.empty();
}

void editor_camera_manager::begin_camera()
{
	if (has_initial_pose)
	{
		camera->set_position(initial_position);
		camera->set_rotation(initial_rotation);
		has_initial_pose = false;
	}
	else if (const view_parameters* game_view = camera_manager::instance()->get_game_view())
	{
		camera->set_position(game_view->position);
		camera->set_rotation(game_view->rotation);
	}
	camera->apply_view(camera_manager::instance()->get_game_view());
	camera_manager::instance()->set_override_view(camera->get_camera());
	enabled = true;
}

void editor_camera_manager::end_camera()
{
	if (camera_manager::is_valid())
		camera_manager::instance()->set_override_view(nullptr);
	enabled = false;
}

void editor_camera_manager::update(float elapsed_time)
{
	const bool next_enabled = compute_enabled();
	if (next_enabled && !enabled)
		begin_camera();
	else if (!next_enabled && enabled)
		end_camera();

	if (!enabled)
		return;

	camera->update(elapsed_time);
	camera->apply_view(camera_manager::instance()->get_game_view());
}

#endif	//	defined(USE_IMGUI)
