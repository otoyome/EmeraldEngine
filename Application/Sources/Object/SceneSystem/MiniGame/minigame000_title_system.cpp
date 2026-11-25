#include "Object/SceneSystem/MiniGame/minigame000_title_system.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

#include "Input/mouse.h"
#include "Object/asset_reference_field.h"
#include "Object/game_object_serializer.h"
#include "Object/Graphics/camera_object.h"
#include "Object/System/locator_object.h"
#include "Scenes/scene_instance.h"

minigame000_title_system::minigame000_title_system()
: next_scene()
, camera_focus_tag("Title Camera Focus")
, camera_distance(system_units::convert_m_to_unit(15.0f))
, camera_pitch(math::angle::to_radian(30.0f))
, camera_rotate_speed(math::angle::to_radian(6.0f))
, next_scene_operation()
, next_scene_resource()
, decide_action("minigame000.title.decide", { keyboard::key_states::enter, keyboard::key_states::space, mouse::key_states::left, gamepad::key_states::a, gamepad::key_states::start })
, connections()
{
}

minigame000_title_system::~minigame000_title_system()
{
}

void minigame000_title_system::on_begin()
{
	state = states::input_wait;
	elapsed = 0.0f;
	decided = false;

	//	タイトルではマウスを固定しない
	if (mouse::is_valid())
	{
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	次のシーンを先に読んでおく
	if (!next_scene.is_empty())
		next_scene_operation = resource_manager::instance()->load_async(next_scene);
	DEBUG_CHECK_ERROR_CATEGORY(!next_scene_operation, core::logger::categorys::system, "minigame000_title_system next scene is empty");

	//	決定入力購読
	connections += decide_action.subscribe(get_broadcaster(), [this](const input::action_event& event)
	{
		if (event.phase == input::action_phases::pressed)
			decided = true;
	});
}

void minigame000_title_system::on_end()
{
	connections.disconnect_all();

	if (next_scene_operation)
	{
		next_scene_operation->cancel();
		next_scene_operation.reset();
	}
	next_scene_resource.reset();
	state = states::input_wait;
	decided = false;

	camera.reset();
	camera_focus.reset();
}

void minigame000_title_system::pre_update(float elapsed_time)
{
	update_next_scene_loading();
	update_camera(elapsed_time);

	switch (state)
	{
		case states::input_wait:
		{
			//	開始直後の入力は捨てる
			elapsed += elapsed_time;
			if (elapsed < input_delay)
			{
				decided = false;
				break;
			}

			if (decided)
			{
				decided = false;
				state = states::load_wait;
			}
			break;
		}
		case states::load_wait:
		{
			//	読込中
			if (next_scene_operation)
				break;

			//	読み込めていれば切り替える
			if (change_scene(next_scene_resource))
			{
				state = states::changed;
				break;
			}

			//	切り替えられなければ入力待ちに戻す
			DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_title_system change scene failed");
			state = states::input_wait;
			break;
		}
		case states::changed:
		{
			break;
		}
	}
}

#if defined(USE_IMGUI)
void minigame000_title_system::imgui_render()
{
	asset_reference_field::imgui_combo("next_scene", next_scene, resource::asset_types::scene);
	ImGui::DragFloat("input_delay", &input_delay, 0.01f, 0.0f, 10.0f);

	ImGui::SeparatorText("camera");
	ImGui::InputTextUtf8("camera_focus_tag", camera_focus_tag);
	ImGui::DragFloat("camera_distance", &camera_distance, 0.1f, 0.0f, FLT_MAX);
	ImGui::SliderAngle("camera_pitch", &camera_pitch, -89.0f, 89.0f);
	ImGui::SliderAngle("camera_rotate_speed", &camera_rotate_speed, -180.0f, 180.0f);

	ImGui::SeparatorText("state");
	ImGui::Text("state : %.*s", static_cast<int>(magic_enum::enum_name(state).size()), magic_enum::enum_name(state).data());
	ImGui::Text("elapsed : %.2f", elapsed);
	ImGui::Text("next scene loading : %s", next_scene_operation ? "true" : "false");
	ImGui::Text("next scene loaded : %s", next_scene_resource ? "true" : "false");
	ImGui::Text("camera : %s", camera.expired() ? "(none)" : "found");
	ImGui::Text("camera focus : %s", camera_focus.expired() ? "(none)" : "found");
	ImGui::Text("camera yaw : %.1f", math::angle::to_degree(camera_yaw));
}
#endif	//	defined(USE_IMGUI)

template<class Archive>
void minigame000_title_system::serialize_fields(Archive& archive, std::uint32_t version)
{
	switch (version)
	{
		case 1:
		{
			archive(CEREAL_NVP(next_scene),
					CEREAL_NVP(input_delay));
			break;
		}
		case 2:
		{
			archive(CEREAL_NVP(next_scene),
					CEREAL_NVP(input_delay),
					CEREAL_NVP(camera_focus_tag),
					CEREAL_NVP(camera_distance),
					CEREAL_NVP(camera_pitch),
					CEREAL_NVP(camera_rotate_speed));
			break;
		}
	}
}

void minigame000_title_system::serialize(cereal::JSONOutputArchive& archive) const
{
	game_object_serializer::save_fields(archive, "minigame000_title_system", *this);
}

void minigame000_title_system::deserialize(cereal::JSONInputArchive& archive)
{
	game_object_serializer::load_fields(archive, "minigame000_title_system", *this);
}

void minigame000_title_system::update_next_scene_loading()
{
	if (!next_scene_operation || !next_scene_operation->is_done())
		return;

	const resource::load_operation_ptr<scene_resource> operation = std::move(next_scene_operation);
	next_scene_operation.reset();

	if (!operation->is_succeeded() || !operation->get_resource())
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_title_system next scene load failed[%s] %s",
			std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		return;
	}
	next_scene_resource = operation->get_resource();
}

void minigame000_title_system::update_camera(float elapsed_time)
{
	//	カメラとフォーカス点を探す
	std::shared_ptr<camera_object> current_camera = camera.lock();
	std::shared_ptr<locator_object> focus = camera_focus.lock();
	if (!current_camera || !focus)
	{
		const scene_instance& scene = get_scene();
		if (!focus)
		{
			std::vector<std::shared_ptr<locator_object>> locators;
			locator_object::collect(scene, camera_focus_tag, locators);
			if (!locators.empty())
			{
				focus = locators.front();
				camera_focus = focus;
			}
		}
		if (!current_camera)
		{
			//	シーン内で一番手前のカメラを使う
			for (const std::weak_ptr<game_object>& weak_object : scene.get_objects())
			{
				const game_object_ptr object = weak_object.lock();
				if (!object || !object->check_type(camera_object::type_name()) || !object->is_active())
					continue;

				std::shared_ptr<camera_object> found = std::static_pointer_cast<camera_object>(object);
				if (!current_camera || found->get_view_order() > current_camera->get_view_order())
					current_camera = found;
			}
			camera = current_camera;

			//	見つけた時点のカメラの向きから回り始める
			if (current_camera)
				camera_yaw = current_camera->get_world_euler().y;
		}

		if (!current_camera || !focus)
			return;
	}

	//	斜め上からフォーカス点を見ながら回る
	camera_yaw = math::angle::normalize(camera_yaw + static_cast<double>(camera_rotate_speed) * elapsed_time);
	const math::rotatord rotation(static_cast<double>(camera_pitch), camera_yaw, 0.0);
	const math::vector3d position = focus->get_world_translation() - rotation.front() * static_cast<double>(camera_distance);
	current_camera->set_world_euler(rotation);
	current_camera->set_world_translation(position);
}

SCENE_SYSTEM_REGISTER(minigame000_title_system);
