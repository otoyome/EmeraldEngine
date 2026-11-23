#include "Editor/editor_gizmo.h"

#if defined(USE_IMGUI)
#include "ImGuizmo/ImGuizmo.h"

#include "Editor/editor_camera.h"
#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Object/spatial_object.h"

namespace
{
	//	行列と ImGuizmo の配列の変換
	void to_array(const math::matrixf& source, float* destination)
	{
		for (std::int32_t index = 0; index < 16; ++index)
			destination[index] = source[index];
	}

	math::matrixf from_array(const float* source)
	{
		return math::matrixf(
			source[0], source[1], source[2], source[3],
			source[4], source[5], source[6], source[7],
			source[8], source[9], source[10], source[11],
			source[12], source[13], source[14], source[15]);
	}
}

void editor_gizmo::imgui_menu()
{
	if (!ImGui::BeginMenu("Gizmo"))
		return;

	if (ImGui::MenuItem("Translate", "W", operation == operations::translate))
		operation = operations::translate;
	if (ImGui::MenuItem("Rotate", "E", operation == operations::rotate))
		operation = operations::rotate;
	if (ImGui::MenuItem("Scale", "R", operation == operations::scale))
		operation = operations::scale;
	ImGui::Separator();
	if (ImGui::MenuItem("Local", "Q", local))
		local = !local;
	ImGui::Separator();
	ImGui::MenuItem("Snap", nullptr, &snap);
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("Translate Snap", &snap_translate, 0.1f, 0.01f, 10000.0f, "%.2f");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("Rotate Snap", &snap_rotate, 0.1f, 0.1f, 180.0f, "%.1f");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("Scale Snap", &snap_scale, 0.01f, 0.01f, 10.0f, "%.2f");
	ImGui::EndMenu();
}

void editor_gizmo::handle_shortcuts()
{
	if (!ImGui::IsWindowHovered() || ImGui::GetIO().WantTextInput || ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGuizmo::IsUsing())
		return;

	if (ImGui::IsKeyPressed(ImGuiKey_W, false))
		operation = operations::translate;
	if (ImGui::IsKeyPressed(ImGuiKey_E, false))
		operation = operations::rotate;
	if (ImGui::IsKeyPressed(ImGuiKey_R, false))
		operation = operations::scale;
	if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
		local = !local;
}

bool editor_gizmo::manipulate(engine_context& context, const ImVec2& position, const ImVec2& size)
{
	handle_shortcuts();

	//	選んでいるオブジェクト
	const std::shared_ptr<game_object> object = context.get_editor().get_selected_object();
	if (!object || object->is_transient() || !object->check_type(spatial_object::type_name()))
		return false;
	spatial_object* target = static_cast<spatial_object*>(object.get());

	//	ビューポートを映しているエディターカメラの視点
	const std::shared_ptr<editor_camera_object> camera_object = context.get_objects().search_object<editor_camera_object>();
	if (!camera_object || !camera_object->get_camera())
		return false;
	view_parameters parameters = camera_object->get_camera()->get_parameters();

	//	縦横比は映している範囲に合わせる。
	parameters.screen_rect = math::float4(0.0f, 0.0f, size.x, size.y);
	parameters.clip_distance.y = parameters.clip_distance.x * 1.0e5f;
	parameters.hybrid_projection = false;

	//	カメラを原点にした座標で渡す
	float view[16], projection[16], model[16];
	to_array(math::matrixf::inverse_rigid_transformation(parameters.rotation.to_quaternion(), math::vector3f(0.0f, 0.0f, 0.0f)), view);
	to_array(parameters.make_projection_matrix(), projection);
	to_array(target->get_world().to_relative_matrix(parameters.position), model);

	//	毎フレームの初期化
	ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
	ImGuizmo::BeginFrame();
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(position.x, position.y, size.x, size.y);

	ImGuizmo::OPERATION gizmo_operation = ImGuizmo::TRANSLATE;
	float snap_values[3] = { snap_translate, snap_translate, snap_translate };
	switch (operation)
	{
		case operations::rotate:
		{
			gizmo_operation = ImGuizmo::ROTATE;
			snap_values[0] = snap_values[1] = snap_values[2] = snap_rotate;
			break;
		}
		case operations::scale:
		{
			gizmo_operation = ImGuizmo::SCALE;
			snap_values[0] = snap_values[1] = snap_values[2] = snap_scale;
			break;
		}
		default:
		{
			break;
		}
	}

	if (ImGuizmo::Manipulate(view, projection, gizmo_operation, local ? ImGuizmo::LOCAL : ImGuizmo::WORLD, model, nullptr, snap ? snap_values : nullptr))
	{
		//	カメラ基準の姿勢をワールドの姿勢に戻す
		math::transformd world(math::transformf::from_matrix(from_array(model)));
		world.set_translation(world.get_translation() + parameters.position);
		target->set_world(world);
	}
	return ImGuizmo::IsUsing() || ImGuizmo::IsOver();
}
#endif	//	defined(USE_IMGUI)
