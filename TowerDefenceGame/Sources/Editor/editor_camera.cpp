#include "Editor/editor_camera.h"
#include "Object/engine_context.h"

#if	defined(USE_IMGUI)
#include "Object/object_manager.h"
#include "System/console_manager.h"

namespace
{
	//	エディターカメラの設定
	console_variable<float> move_speed_variable("editor.camera.move_speed", system_units::convert_m_to_unit(0.2f) * 0.5f, "エディターカメラの移動速度（シーンの速度倍率が掛かる）", console_flags::persist);
	console_variable<float> rotate_speed_variable("editor.camera.rotate_speed", 0.0125f * 0.5f, "エディターカメラの回転速度（シーンの速度倍率が掛かる）", console_flags::persist);
}

editor_camera_object::editor_camera_object(std::string_view name, const math::vector3d& position, const math::rotatorf& rotation, float speed_scale)
: camera_object(name)
, rotation(rotation)
, position(position)
, rotation_speed(rotate_speed_variable.get() * speed_scale)
, move_speed(move_speed_variable.get() * speed_scale)
{
	apply_view(nullptr);
}

editor_camera_object::~editor_camera_object()
{
}

void editor_camera_object::post_update(float elapsed_time)
{
}

void editor_camera_object::on_active()
{
	base::on_active();

	if (auto game_camera = find_game_camera())
	{
		const view_parameters& parameters = game_camera->get_camera()->get_parameters();
		position = parameters.position;
		rotation = parameters.rotation;
	}
}

std::shared_ptr<camera_object> editor_camera_object::find_game_camera() const
{
	std::shared_ptr<camera_object> result;
	for (const auto& object : get_context().get_objects().search_objects<camera_object>())
	{
		if (!object->is_active() || object->check_type(editor_camera_object::type_name()))
			continue;
		if (!object->get_camera()->is_active())
			continue;
		if (!result || object->get_view_order() > result->get_view_order())
			result = object;
	}
	return result;
}

void editor_camera_object::apply_view(const view_parameters* base)
{
	//	視点姿勢を視点パラメーターに反映反映
	auto& parameters = get_camera()->get_parameters();
	if (base)
		parameters = *base;
	parameters.rotation = rotation;
	parameters.position = position;
}

void editor_camera_object::debug_update(float elapsed_time)
{
	//	ImGui の窓に映している上でだけ入力を受け付ける
	const engine_context& context = get_context();
	const bool accepts_input = context.is_embedded() ? context.is_viewport_hovered() : !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
	if (accepts_input)
		handle_input(elapsed_time);

	if (is_auto_rotate)
	{
		rotation.y += math::angle::to_radian(1.0f * elapsed_time);
	}

	auto game_camera = find_game_camera();
	apply_view(game_camera ? &game_camera->get_camera()->get_parameters() : nullptr);

	if (accepts_input && input::get_key_state(mouse::key_states::left) != input::state::none)
	{
		//	座標情報を送る
		mouse_position_data data;
		if (context.is_embedded())
		{
			//	映している画像の中の座標にする
			const math::float4& rect = context.get_viewport_rect();
			const ImVec2 mouse_position = ImGui::GetIO().MousePos;
			data.position.x = mouse_position.x - rect.x;
			data.position.y = mouse_position.y - rect.y;
			data.viewport_size = { rect.z, rect.w };
		}
		else
		{
			auto mouse_state = mouse::instance()->get_state();
			data.position.x = static_cast<float>(mouse_state.position.position.x);
			data.position.y = static_cast<float>(mouse_state.position.position.y);

			//	マウス座標はクライアント座標なので、クライアント領域の大きさを基準にする
			const math::int2 client_size = mouse::instance()->get_client_size();
			data.viewport_size = { static_cast<float>((std::max)(client_size.x, 1)), static_cast<float>((std::max)(client_size.y, 1)) };
		}
		messenger& target = get_context().get_objects().get_messenger();
		if (input::get_key_state(mouse::key_states::left) == input::state::down)
			target.send_data(left_click_down_identifier, &data);
		else if (input::get_key_state(mouse::key_states::left) == input::state::hold)
			target.send_data(left_click_hold_identifier, &data);
		else if (input::get_key_state(mouse::key_states::left) == input::state::release)
			target.send_data(left_click_release_identifier, &data);
	}
}

void editor_camera_object::handle_input(float elapsed_time)
{
	//	移動は単精度の移動量を求めてから倍精度の座標に足す
	auto add_position = [this](const math::float3& delta)
	{
		position.x += delta.x;
		position.y += delta.y;
		position.z += delta.z;
	};

	//	キーボード操作
	if (input::get_key_state(mouse::key_states::right) == input::state::hold)
	{
		int x = 0, y = 0, z = 0;
		if (input::get_key_state(keyboard::key_states::w) == input::state::hold)
		{
			++z;
		}
		if (input::get_key_state(keyboard::key_states::s) == input::state::hold)
		{
			--z;
		}
		if (input::get_key_state(keyboard::key_states::d) == input::state::hold)
		{
			++x;
		}
		if (input::get_key_state(keyboard::key_states::a) == input::state::hold)
		{
			--x;
		}
		if (input::get_key_state(keyboard::key_states::e) == input::state::hold)
		{
			++y;
		}
		if (input::get_key_state(keyboard::key_states::q) == input::state::hold)
		{
			--y;
		}

		int wheel = static_cast<int>(input::get_key_value(mouse::axis_states::wheel));
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
	{
		math::float2 move;
		move.x = input::get_key_value(mouse::axis_states::x);
		move.y = input::get_key_value(mouse::axis_states::y);
		int wheel = static_cast<int>(input::get_key_value(mouse::axis_states::wheel));
		move.x *= math::step(1.0f, fabsf(move.x));
		move.y *= math::step(1.0f, fabsf(move.y));
		if (input::get_key_state(mouse::key_states::right) == input::state::hold)
		{
			float speed = rotation_speed * system_units::convert_sec_to_fps(elapsed_time);
			rotation.x += move.y * speed;
			rotation.y += move.x * (rotation.up().y < 0 ? -speed : speed);
		}

		if (input::get_key_state(mouse::key_states::middle) == input::state::hold ||
			(input::get_key_state(mouse::key_states::right) != input::state::hold && wheel != 0))
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
	}
}

#endif	//	defined(USE_IMGUI)
