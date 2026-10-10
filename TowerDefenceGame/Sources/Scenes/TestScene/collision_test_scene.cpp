#include "scenes/TestScene/collision_test_scene.h"
#include "camera.h"

#include <cstdarg>
#include <cmath>

#include "object/light_object.h"
#include "object/camera_object.h"

#include "collision/sphere.h"
#include "collision/capsule.h"
#include "collision/box.h"
#include "collision/mesh.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	//	1メートル
	const float unit = system_units::convert_m_to_unit(1.0f);

	//	度数からクォータニオン
	math::float4 to_quaternion(const math::float3& degree)
	{
		math::float4 quaternion;
		quaternion = (math::quaternionf::rotation_xyz(math::angle::to_radian(degree.x), math::angle::to_radian(degree.y), math::angle::to_radian(degree.z))).to_float4();
		return quaternion;
	}

	math::float3 scale3(const math::float3& value, float scale)
	{
		return { value.x * scale, value.y * scale, value.z * scale };
	}

	math::float3 add3(const math::float3& a, const math::float3& b)
	{
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	math::float3 sub3(const math::float3& a, const math::float3& b)
	{
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	float dot3(const math::float3& a, const math::float3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	float length3(const math::float3& a)
	{
		return sqrtf(dot3(a, a));
	}

	math::float3 normalize3(const math::float3& a)
	{
		float length = length3(a);
		return length > 0.0f ? scale3(a, 1.0f / length) : a;
	}

	bool is_finite3(const math::float3& a)
	{
		return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
	}

	struct float_infomation
	{
		math::float3 point;
		math::float3 point2;
		math::float3 normal;
		float length;
	};

	float_infomation to_float_info(const collision::intersect_data::infomation& info)
	{
		return { math::float3(info.point), math::float3(info.point2), math::float3(info.normal), static_cast<float>(info.length) };
	}

	std::string text_format(const char* format_text, ...)
	{
		char buffer[512];
		va_list args;
		va_start(args, format_text);
		vsnprintf(buffer, sizeof(buffer), format_text, args);
		va_end(args);
		return buffer;
	}

	std::string to_string(const math::float3& value)
	{
		return text_format("(%.3f, %.3f, %.3f)", value.x, value.y, value.z);
	}

	//	床メッシュ生成
	std::shared_ptr<collision::bounding_mesh> create_floor_mesh(float half_x, float half_z)
	{
		std::vector<math::vector3f> positions =
		{
			math::vector3f(-half_x, 0.0f, -half_z),
			math::vector3f(-half_x, 0.0f, +half_z),
			math::vector3f(+half_x, 0.0f, +half_z),
			math::vector3f(+half_x, 0.0f, -half_z),
		};
		static constexpr UINT indices[] = { 0, 1, 2, 0, 2, 3 };

		std::vector<collision::bounding_mesh::triangle> triangles;
		for (size_t index = 0; index < std::size(indices); index += 3)
		{
			auto& triangle = triangles.emplace_back();
			triangle.a = indices[index + 0];
			triangle.b = indices[index + 1];
			triangle.c = indices[index + 2];

			//	事前計算
			auto p = positions.data();
			const auto& A = *(p + triangle.a);
			const auto& B = *(p + triangle.b);
			const auto& C = *(p + triangle.c);
			auto AB = B - A;
			auto BC = C - B;
			auto CA = A - C;
			triangle.ab = AB;
			triangle.bc = BC;
			triangle.ca = CA;
			triangle.normal = (math::vector3f::cross(AB, BC)).normalized();
			triangle.min = (math::min)(A, (math::min)(B, C));
			triangle.max = (math::max)(A, (math::max)(B, C));
			triangle.lab = collision::linef(collision::linef::types::segment, A.to_float3(), B.to_float3());
			triangle.lbc = collision::linef(collision::linef::types::segment, B.to_float3(), C.to_float3());
			triangle.lca = collision::linef(collision::linef::types::segment, C.to_float3(), A.to_float3());
		}
		return std::make_shared<collision::bounding_mesh>(positions, triangles, 0);
	}

#if defined(USE_IMGUI)
	//	形状描画
	void draw_shape(const collision::bounding_object& object, const math::float4& color, const math::float3& offset = { 0.0f, 0.0f, 0.0f })
	{
		math::float3 position = add3(object.get_position().to_float3(), offset);
		switch (object.get_type())
		{
			case collision::bounding_object::types::sphere:
			{
				auto& sphere = static_cast<const collision::bounding_sphere&>(object);
				QUEUE_DEBUG_PRIMITIVE_SPHERE(position, static_cast<float>(sphere.get_radius()), color, true);
				break;
			}
			case collision::bounding_object::types::capsule:
			{
				auto& capsule = static_cast<const collision::bounding_capsule&>(object);
				QUEUE_DEBUG_PRIMITIVE_CAPSULE(position, capsule.get_rotation().to_float4(), static_cast<float>(capsule.get_radius()), static_cast<float>(capsule.get_length()), color, true);
				break;
			}
			case collision::bounding_object::types::box:
			{
				QUEUE_DEBUG_PRIMITIVE_BOX(position, object.get_rotation().to_float4(), math::float3(object.get_local_scale()), color, true);
				break;
			}
			case collision::bounding_object::types::mesh:
			{
				auto& mesh = static_cast<const collision::bounding_mesh&>(object);
				math::matrixf Transform(mesh.get_transform_matrix());
				math::vector3f Offset(offset);
				auto& positions = mesh.get_positions();
				for (auto& triangle : mesh.get_triangles())
				{
					math::float3 points[3];
					const UINT indices[3] = { triangle.a, triangle.b, triangle.c };
					for (int index = 0; index < 3; ++index)
						points[index] = ((math::vector3f::transform_coord(math::vector3f(positions[indices[index]]), Transform)) + Offset).to_float3();
					QUEUE_DEBUG_PRIMITIVE_LINE(points[0], points[1], color, true);
					QUEUE_DEBUG_PRIMITIVE_LINE(points[1], points[2], color, true);
					QUEUE_DEBUG_PRIMITIVE_LINE(points[2], points[0], color, true);
				}
				break;
			}
		}
	}

	//	点と法線の描画
	void draw_point_normal(const math::float3& point, const math::float3& normal, float length, const math::float4& color)
	{
		QUEUE_DEBUG_PRIMITIVE_SPHERE(point, 0.03f * unit, color, false);
		QUEUE_DEBUG_PRIMITIVE_LINE(point, add3(point, scale3(normal, length)), color, false);
	}

	//	形状設定の編集
	void edit_shape(const char* label, collision_test_scene::shape_desc& desc)
	{
		ImGui::PushID(label);
		ImGui::Text("%s", label);
		static const char* type_names[] = { "sphere", "capsule", "box", "mesh(floor)" };
		int type = static_cast<int>(desc.type);
		if (ImGui::Combo("type", &type, type_names, IM_ARRAYSIZE(type_names)))
			desc.type = static_cast<collision_test_scene::shape_types>(type);
		ImGui::DragFloat3("position(m)", &desc.position.x, 0.01f);
		ImGui::DragFloat3("rotation(deg)", &desc.rotation.x, 0.5f);
		switch (desc.type)
		{
			case collision_test_scene::shape_types::sphere:
				ImGui::DragFloat("radius(m)", &desc.radius, 0.01f, 0.01f, 100.0f);
				break;
			case collision_test_scene::shape_types::capsule:
				ImGui::DragFloat("radius(m)", &desc.radius, 0.01f, 0.01f, 100.0f);
				ImGui::DragFloat("length(m)", &desc.length, 0.01f, 0.0f, 100.0f);
				break;
			case collision_test_scene::shape_types::box:
				ImGui::DragFloat3("half extent(m)", &desc.extent.x, 0.01f, 0.01f, 100.0f);
				break;
			case collision_test_scene::shape_types::mesh:
				ImGui::DragFloat2("half size xz(m)", &desc.extent.x, 0.01f, 0.01f, 100.0f);
				break;
		}
		ImGui::PopID();
	}

	//	判定結果の表示
	void show_intersect(const char* label, bool hit, bool hit_without_data, const collision::intersect_data& data)
	{
		ImGui::Text("%s : %s", label, hit ? "HIT" : "---");
		if (hit != hit_without_data)
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  mismatch with intersect=nullptr (%s)", hit_without_data ? "HIT" : "---");
		for (size_t index = 0; index < data.size(); ++index)
		{
			const auto info = to_float_info(data[index]);
			ImGui::Text("  [%zu] point %s", index, to_string(scale3(info.point, 1.0f / unit)).c_str());
			ImGui::Text("      normal %s  length %.4f m", to_string(info.normal).c_str(), info.length / unit);
		}
	}
#endif	//	defined(USE_IMGUI)

	//	交差テスト項目(メートル)
	struct overlap_case
	{
		const char* name;
		collision_test_scene::shape_desc a;
		collision_test_scene::shape_desc b;
		bool expect_hit;
		bool check_normal;
		math::float3 normal;	//	Aを押し出す向き
		float depth;				//	負なら確認しない
		bool check_opposite = true;	//	逆順の法線が反対向きか確認
	};

	//	キャストテスト項目(メートル)
	struct cast_case
	{
		const char* name;
		collision_test_scene::cast_desc cast;
		collision_test_scene::shape_desc target;
		bool expect_hit;
		float length;				//	負なら確認しない
		bool check_normal;
		math::float3 normal;
	};

	collision_test_scene::shape_desc make_sphere(math::float3 position, float radius)
	{
		collision_test_scene::shape_desc desc;
		desc.type = collision_test_scene::shape_types::sphere;
		desc.position = position;
		desc.radius = radius;
		return desc;
	}

	collision_test_scene::shape_desc make_capsule(math::float3 position, math::float3 rotation, float radius, float length)
	{
		collision_test_scene::shape_desc desc;
		desc.type = collision_test_scene::shape_types::capsule;
		desc.position = position;
		desc.rotation = rotation;
		desc.radius = radius;
		desc.length = length;
		return desc;
	}

	collision_test_scene::shape_desc make_box(math::float3 position, math::float3 rotation, math::float3 extent)
	{
		collision_test_scene::shape_desc desc;
		desc.type = collision_test_scene::shape_types::box;
		desc.position = position;
		desc.rotation = rotation;
		desc.extent = extent;
		return desc;
	}

	collision_test_scene::shape_desc make_floor(math::float3 position = { 0, 0, 0 }, math::float3 rotation = { 0, 0, 0 }, float half = 5.0f)
	{
		collision_test_scene::shape_desc desc;
		desc.type = collision_test_scene::shape_types::mesh;
		desc.position = position;
		desc.rotation = rotation;
		desc.extent = { half, half, half };
		return desc;
	}

	collision_test_scene::cast_desc make_cast(collision_test_scene::cast_types type, math::float3 start, math::float3 end, float radius = 0.5f, float length = 2.0f, math::float3 rotation = { 0, 0, 0 })
	{
		collision_test_scene::cast_desc desc;
		desc.type = type;
		desc.start = start;
		desc.end = end;
		desc.radius = radius;
		desc.length = length;
		desc.rotation = rotation;
		return desc;
	}
}

collision_test_scene::collision_test_scene()
: scene_base()
{
	shape_descs[0] = make_capsule({ 0.0f, 1.3f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 2.0f);
	shape_descs[1] = make_floor();
}

collision_test_scene::~collision_test_scene()
{
}

bool collision_test_scene::initialize_core()
{
	//	メインカメラ生成
	std::shared_ptr<camera_object> main_camera;
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(45.0f);
		main_camera->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 3.0, 10.0)));
		main_camera->set_world_euler(math::rotatord(0.2, math::constants<double>::pi, 0.0));
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

#if defined(USE_IMGUI)
	//	エディターカメラ生成
	{
		editor_camera_request = editor_camera_manager::instance()->request(
			system_units::convert_m_to_unit(math::vector3d(0.0, 3.0, 10.0)),
			{ 0.2f, math::constants<float>::pi, 0 },
			0.25f);
	}
#endif // defined(USE_IMGUI)

	//	平行光源生成
	{
		auto light = std::make_shared<directional_light_object>("平行光源");
		auto color = light->get_color();
		color.w = 1.0f;
		light->set_color(color);
		light->set_flag(light::flags::use_shadow, false);
		register_object(light);
	}

	return true;
}

void collision_test_scene::update_core(float elapsed_time)
{
	if (mode == 0)
		update_pair_test();
	else
		update_cast_test();
}

collision_test_scene::shape_desc collision_test_scene::to_unit(const shape_desc& desc)
{
	shape_desc result = desc;
	result.position = scale3(desc.position, unit);
	result.radius = desc.radius * unit;
	result.length = desc.length * unit;
	result.extent = scale3(desc.extent, unit);
	return result;
}

collision_test_scene::cast_desc collision_test_scene::to_unit(const cast_desc& desc)
{
	cast_desc result = desc;
	result.start = scale3(desc.start, unit);
	result.end = scale3(desc.end, unit);
	result.radius = desc.radius * unit;
	result.length = desc.length * unit;
	return result;
}

std::shared_ptr<collision::bounding_object> collision_test_scene::create_shape(const shape_desc& desc)
{
	math::float4 rotation = to_quaternion(desc.rotation);
	switch (desc.type)
	{
		case shape_types::sphere:
			return std::make_shared<collision::bounding_sphere>(math::double3(desc.position), desc.radius);
		case shape_types::capsule:
			return std::make_shared<collision::bounding_capsule>(math::double3(desc.position), math::double4(rotation), desc.radius, desc.length);
		case shape_types::box:
			return std::make_shared<collision::bounding_box>(math::double3(desc.extent), math::double4(rotation), math::double3(desc.position));
		case shape_types::mesh:
		{
			auto mesh = create_floor_mesh(desc.extent.x, desc.extent.y);
			mesh->set_rotation(math::double4(rotation));
			mesh->set_position(math::double3(desc.position));
			return mesh;
		}
	}
	return nullptr;
}

bool collision_test_scene::execute_cast(const cast_desc& desc, const collision::bounding_object& target, collision::intersect_data* intersect)
{
	switch (desc.type)
	{
		case cast_types::segment:
			return target.intersect_segment(math::double3(desc.start), math::double3(desc.end), intersect);
		case cast_types::sphere:
			return target.intersect_sphere_cast(math::double3(desc.start), math::double3(desc.end), desc.radius, intersect);
		case cast_types::capsule:
			return target.intersect_capsule_cast(math::double3(desc.start), math::double3(desc.end), desc.radius, desc.length, math::double4(to_quaternion(desc.rotation)), intersect);
	}
	return false;
}

void collision_test_scene::update_pair_test()
{
	auto a = create_shape(to_unit(shape_descs[0]));
	auto b = create_shape(to_unit(shape_descs[1]));
	if (!a || !b)
		return;

	intersect_ab.clear();
	intersect_ba.clear();
	hit_ab = a->intersection(b.get(), &intersect_ab);
	hit_ab_without_data = a->intersection(b.get(), nullptr);
	hit_ba = b->intersection(a.get(), &intersect_ba);
	hit_ba_without_data = b->intersection(a.get(), nullptr);

#if defined(USE_IMGUI)
	const math::float4 normal_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	const math::float4 hit_color = { 1.0f, 0.2f, 0.2f, 1.0f };
	draw_shape(*a, hit_ab ? hit_color : math::float4{ 0.4f, 0.8f, 1.0f, 1.0f });
	draw_shape(*b, hit_ba ? hit_color : normal_color);

	//	A基準の接触(黄)、B基準の接触(水色)
	for (size_t index = 0; index < intersect_ab.size(); ++index)
		draw_point_normal(to_float_info(intersect_ab[index]).point, to_float_info(intersect_ab[index]).normal, std::max(to_float_info(intersect_ab[index]).length, 0.3f * unit), { 1.0f, 1.0f, 0.0f, 1.0f });
	for (size_t index = 0; index < intersect_ba.size(); ++index)
		draw_point_normal(to_float_info(intersect_ba[index]).point, to_float_info(intersect_ba[index]).normal, std::max(to_float_info(intersect_ba[index]).length, 0.3f * unit), { 0.0f, 1.0f, 1.0f, 1.0f });

	//	押し出し後のAを緑で表示
	if (draw_ghost && hit_ab && intersect_ab.size() > 0)
		draw_shape(*a, { 0.2f, 1.0f, 0.2f, 1.0f }, scale3(to_float_info(intersect_ab[0]).normal, to_float_info(intersect_ab[0]).length));
#endif	//	defined(USE_IMGUI)
}

void collision_test_scene::update_cast_test()
{
	auto target = create_shape(to_unit(shape_descs[1]));
	if (!target)
		return;

	cast_desc desc = to_unit(cast);
	cast_intersect.clear();
	cast_hit = execute_cast(desc, *target, &cast_intersect);

#if defined(USE_IMGUI)
	draw_shape(*target, cast_hit ? math::float4{ 1.0f, 0.2f, 0.2f, 1.0f } : math::float4{ 1.0f, 1.0f, 1.0f, 1.0f });
	QUEUE_DEBUG_PRIMITIVE_LINE(desc.start, desc.end, math::float4(1.0f, 1.0f, 0.0f, 1.0f), false);

	//	始点(灰)・終点(暗灰)・接触時(橙)
	auto draw_caster = [&](const math::float3& center, const math::float4& color)
	{
		switch (desc.type)
		{
			case cast_types::segment:
				QUEUE_DEBUG_PRIMITIVE_SPHERE(center, 0.03f * unit, color, false);
				break;
			case cast_types::sphere:
				QUEUE_DEBUG_PRIMITIVE_SPHERE(center, desc.radius, color, true);
				break;
			case cast_types::capsule:
				QUEUE_DEBUG_PRIMITIVE_CAPSULE(center, to_quaternion(desc.rotation), desc.radius, desc.length, color, true);
				break;
		}
	};
	draw_caster(desc.start, { 0.7f, 0.7f, 0.7f, 1.0f });
	draw_caster(desc.end, { 0.35f, 0.35f, 0.35f, 1.0f });
	if (cast_hit && cast_intersect.size() > 0)
	{
		const auto info = to_float_info(cast_intersect[0]);
		draw_caster(info.point, { 1.0f, 0.6f, 0.0f, 1.0f });
		math::float3 contact = desc.type == cast_types::segment ? info.point : info.point2;
		draw_point_normal(contact, info.normal, 0.5f * unit, { 0.0f, 1.0f, 1.0f, 1.0f });
	}
#endif	//	defined(USE_IMGUI)
}

void collision_test_scene::run_self_tests()
{
	test_results.clear();
	test_passed_count = 0;

	const float tolerance = 0.002f;	//	許容誤差(メートル)
	auto add_result = [&](const std::string& name, bool passed, const std::string& detail)
	{
		test_results.push_back({ name, passed, detail });
		if (passed)
			++test_passed_count;
	};

	//	回転・移動した床の法線
	const math::float3 tilt_position = { 3.0f, 2.0f, -1.0f };
	const math::float3 tilt_rotation = { 30.0f, 20.0f, 0.0f };
	math::float3 tilt_normal;
	const math::float4 tilt_quaternion = to_quaternion(tilt_rotation);
	tilt_normal = (math::quaternionf(tilt_quaternion).rotate(math::vector3f(0, 1, 0))).to_float3();
	auto on_tilt = [&](float height) { return add3(tilt_position, scale3(tilt_normal, height)); };

	//	交差テスト
	const overlap_case overlap_cases[] =
	{
		{ "sphere-sphere",					make_sphere({ 0, 0, 0 }, 1.0f), make_sphere({ 1.5f, 0, 0 }, 1.0f), true, true, { -1, 0, 0 }, 0.5f },
		{ "sphere-sphere separated",		make_sphere({ 0, 0, 0 }, 1.0f), make_sphere({ 2.5f, 0, 0 }, 1.0f), false, false, {}, -1.0f },
		{ "sphere-sphere same center",		make_sphere({ 0, 0, 0 }, 1.0f), make_sphere({ 0, 0, 0 }, 1.0f), true, false, {}, 2.0f, false },
		{ "capsule-sphere side",			make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_sphere({ 0.9f, 0.8f, 0 }, 0.5f), true, true, { -1, 0, 0 }, 0.1f },
		{ "capsule-sphere cap",				make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_sphere({ 0, 1.9f, 0 }, 0.5f), true, true, { 0, -1, 0 }, 0.1f },
		{ "capsule-capsule parallel",		make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_capsule({ 0.8f, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), true, true, { -1, 0, 0 }, 0.2f },
		{ "capsule-capsule crossing",		make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_capsule({ 0, 0, 0 }, { 0, 0, 90 }, 0.5f, 2.0f), true, false, {}, 1.0f },
		{ "capsule-capsule separated",		make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_capsule({ 1.2f, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), false, false, {}, -1.0f },
		{ "box-sphere outside",				make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), make_sphere({ 1.4f, 0, 0 }, 0.5f), true, true, { -1, 0, 0 }, 0.1f },
		{ "box-sphere center inside",		make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), make_sphere({ 0.8f, 0, 0 }, 0.5f), true, true, { -1, 0, 0 }, 0.7f },
		{ "box-sphere same center",			make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), make_sphere({ 0, 0, 0 }, 0.5f), true, false, {}, 1.5f },
		{ "rotated box-sphere",				make_box({ 0, 0, 0 }, { 0, 45, 0 }, { 1, 1, 1 }), make_sphere({ 1.6f, 0, 0 }, 0.5f), true, true, { -1, 0, 0 }, 0.5f - (1.6f - sqrtf(2.0f)) },
		{ "box-box face",					make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), make_box({ 1.8f, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, true, { -1, 0, 0 }, 0.2f },
		{ "box-box rotated",				make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), make_box({ 2.2f, 0, 0 }, { 0, 45, 0 }, { 1, 1, 1 }), true, true, { -1, 0, 0 }, 1.0f - (2.2f - sqrtf(2.0f)) },
		{ "box-box rotated A",				make_box({ 0, 0, 0 }, { 0, 30, 0 }, { 1, 1, 1 }), make_box({ 0, 1.9f, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, true, { 0, -1, 0 }, 0.1f },
		{ "box-box separated",				make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), make_box({ 2.5f, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), false, false, {}, -1.0f },
		{ "capsule-box side",				make_capsule({ 1.3f, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, true, { 1, 0, 0 }, 0.2f },
		{ "capsule-box lying on top",		make_capsule({ 0, 1.4f, 0 }, { 0, 0, 90 }, 0.5f, 2.0f), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, true, { 0, 1, 0 }, 0.1f },
		{ "capsule-rotated box",			make_capsule({ 0, 2.3f, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_box({ 0, 0, 0 }, { 0, 30, 0 }, { 1, 1, 1 }), true, true, { 0, 1, 0 }, 0.2f },
		{ "sphere-floor",					make_sphere({ 0, 0.5f, 0 }, 1.0f), make_floor(), true, true, { 0, 1, 0 }, 0.5f },
		{ "sphere-floor separated",			make_sphere({ 0, 1.5f, 0 }, 1.0f), make_floor(), false, false, {}, -1.0f },
		{ "capsule-floor standing",			make_capsule({ 0, 1.3f, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), make_floor(), true, true, { 0, 1, 0 }, 0.2f },
		{ "capsule-floor lying",			make_capsule({ 0, 0.3f, 0 }, { 0, 0, 90 }, 0.5f, 2.0f), make_floor(), true, true, { 0, 1, 0 }, 0.2f },
		{ "capsule-floor over edge",		make_capsule({ 5.2f, 0.3f, 0 }, { 0, 0, 90 }, 0.5f, 2.0f), make_floor(), true, true, { 0, 1, 0 }, 0.2f },
		{ "box-floor",						make_box({ 0, 0.3f, 0 }, { 0, 0, 0 }, { 0.5f, 0.5f, 0.5f }), make_floor(), true, true, { 0, 1, 0 }, 0.2f },
		{ "box-floor separated",			make_box({ 0, 0.6f, 0 }, { 0, 0, 0 }, { 0.5f, 0.5f, 0.5f }), make_floor(), false, false, {}, -1.0f },
		{ "sphere-tilted floor",			make_sphere(on_tilt(0.5f), 1.0f), make_floor(tilt_position, tilt_rotation), true, true, tilt_normal, 0.5f },
		{ "capsule-tilted floor",			make_capsule(on_tilt(1.3f), tilt_rotation, 0.5f, 2.0f), make_floor(tilt_position, tilt_rotation), true, true, tilt_normal, 0.2f },
		{ "capsule-tilted floor separated",	make_capsule(on_tilt(1.6f), tilt_rotation, 0.5f, 2.0f), make_floor(tilt_position, tilt_rotation), false, false, {}, -1.0f },
	};

	for (auto& test : overlap_cases)
	{
		auto a = create_shape(to_unit(test.a));
		auto b = create_shape(to_unit(test.b));

		collision::intersect_data ab, ba;
		bool hit_ab = a->intersection(b.get(), &ab);
		bool hit_ab_null = a->intersection(b.get(), nullptr);
		bool hit_ba = b->intersection(a.get(), &ba);
		bool hit_ba_null = b->intersection(a.get(), nullptr);

		std::string detail;
		bool passed = true;
		auto fail = [&](const std::string& text) { passed = false; detail += text + " "; };

		if (hit_ab != test.expect_hit)
			fail(text_format("hit=%d expect=%d.", hit_ab, test.expect_hit));
		if (hit_ab != hit_ab_null || hit_ba != hit_ba_null)
			fail("result differs with intersect=nullptr.");
		if (hit_ab != hit_ba)
			fail(text_format("asymmetric (A->B %d, B->A %d).", hit_ab, hit_ba));

		if (hit_ab && ab.size() == 0)
			fail("no contact data.");
		if (hit_ab && ab.size() > 0)
		{
			const auto info = to_float_info(ab[0]);
			float depth = info.length / unit;
			detail += text_format("n=%s d=%.3f. ", to_string(info.normal).c_str(), depth);

			if (!is_finite3(info.normal) || !is_finite3(info.point) || !std::isfinite(info.length))
				fail("non finite value.");
			else if (fabsf(length3(info.normal) - 1.0f) > 0.01f)
				fail("normal not unit.");
			if (info.length < 0.0f)
				fail("negative depth.");
			if (test.check_normal && dot3(info.normal, normalize3(test.normal)) < 0.999f)
				fail(text_format("normal expect %s.", to_string(test.normal).c_str()));
			if (test.depth >= 0.0f && fabsf(depth - test.depth) > tolerance)
				fail(text_format("depth expect %.3f.", test.depth));
			if (test.check_opposite && ba.size() > 0 && dot3(info.normal, to_float_info(ba[0]).normal) > -0.99f)
				fail(text_format("B->A normal not opposite %s.", to_string(to_float_info(ba[0]).normal).c_str()));

			//	法線方向へ押し出すと離れるか
			shape_desc moved = test.a;
			moved.position = add3(test.a.position, scale3(info.normal, depth + 0.01f));
			auto moved_a = create_shape(to_unit(moved));
			if (moved_a->intersection(b.get(), nullptr))
				fail("still hit after push out.");
		}
		add_result(std::string("[overlap] ") + test.name, passed, detail);
	}

	//	キャストテスト
	const float edge = 1.0f + 0.5f / sqrtf(2.0f);
	const float corner = 1.0f + 0.5f / sqrtf(3.0f);
	const math::float3 diagonal2 = normalize3({ 1, 1, 0 });
	const math::float3 diagonal3 = normalize3({ 1, 1, 1 });
	const cast_case cast_cases[] =
	{
		{ "segment-sphere",					make_cast(cast_types::segment, { 0, 5, 0 }, { 0, -5, 0 }), make_sphere({ 0, 0, 0 }, 1.0f), true, 4.0f, true, { 0, 1, 0 } },
		{ "segment-sphere start inside",	make_cast(cast_types::segment, { 0, 0.5f, 0 }, { 0, -5, 0 }), make_sphere({ 0, 0, 0 }, 1.0f), true, 0.0f, false, {} },
		{ "segment-sphere miss",			make_cast(cast_types::segment, { 0, 5, 0 }, { 0, 2, 0 }), make_sphere({ 0, 0, 0 }, 1.0f), false, -1.0f, false, {} },
		{ "segment-capsule top",			make_cast(cast_types::segment, { 0, 5, 0 }, { 0, -5, 0 }), make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), true, 3.5f, true, { 0, 1, 0 } },
		{ "segment-capsule side",			make_cast(cast_types::segment, { 5, 0, 0 }, { -5, 0, 0 }), make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), true, 4.5f, true, { 1, 0, 0 } },
		{ "segment-box",					make_cast(cast_types::segment, { 0, 5, 0 }, { 0, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, 4.0f, true, { 0, 1, 0 } },
		{ "segment-rotated box",			make_cast(cast_types::segment, { 0, 5, 0 }, { 0, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 30 }, { 1, 1, 1 }), true, 5.0f - 1.0f / cosf(math::angle::to_radian(30.0f)), true, { -0.5f, sqrtf(3.0f) * 0.5f, 0 } },
		{ "segment-box miss short",			make_cast(cast_types::segment, { 0, 5, 0 }, { 0, 2, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), false, -1.0f, false, {} },
		{ "segment-floor",					make_cast(cast_types::segment, { 1, 5, 1 }, { 1, -5, 1 }), make_floor(), true, 5.0f, true, { 0, 1, 0 } },
		{ "segment-tilted floor",			make_cast(cast_types::segment, on_tilt(5.0f), on_tilt(-5.0f)), make_floor(tilt_position, tilt_rotation), true, 5.0f, true, tilt_normal },
		{ "sphere cast-sphere",				make_cast(cast_types::sphere, { 0, 5, 0 }, { 0, -5, 0 }), make_sphere({ 0, 0, 0 }, 1.0f), true, 3.5f, true, { 0, 1, 0 } },
		{ "sphere cast-capsule side",		make_cast(cast_types::sphere, { 5, 0, 0 }, { -5, 0, 0 }), make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), true, 4.0f, true, { 1, 0, 0 } },
		{ "sphere cast-capsule top",		make_cast(cast_types::sphere, { 0, 5, 0 }, { 0, -5, 0 }), make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), true, 3.0f, true, { 0, 1, 0 } },
		{ "sphere cast-box face",			make_cast(cast_types::sphere, { 0, 5, 0 }, { 0, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, 3.5f, true, { 0, 1, 0 } },
		{ "sphere cast-box edge",			make_cast(cast_types::sphere, { 5, 5, 0 }, { -5, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, sqrtf(2.0f) * (5.0f - edge), true, diagonal2 },
		{ "sphere cast-box corner",			make_cast(cast_types::sphere, { 5, 5, 5 }, { -5, -5, -5 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, sqrtf(3.0f) * (5.0f - corner), true, diagonal3 },
		{ "sphere cast-box start overlap in",	make_cast(cast_types::sphere, { 0, 1.2f, 0 }, { 0, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, 0.0f, true, { 0, 1, 0 } },
		{ "sphere cast-box start overlap out",	make_cast(cast_types::sphere, { 0, 1.2f, 0 }, { 0, 5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), false, -1.0f, false, {} },
		{ "sphere cast-rotated box",		make_cast(cast_types::sphere, { 0, 5, 0 }, { 0, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 30 }, { 1, 1, 1 }), true, -1.0f, false, {} },
		{ "sphere cast-floor",				make_cast(cast_types::sphere, { 0, 5, 0 }, { 0, -5, 0 }), make_floor(), true, 4.5f, true, { 0, 1, 0 } },
		{ "sphere cast-floor start overlap in",	make_cast(cast_types::sphere, { 0, 0.3f, 0 }, { 0, -5, 0 }), make_floor(), true, 0.0f, true, { 0, 1, 0 } },
		{ "sphere cast-floor slide",		make_cast(cast_types::sphere, { 0, 0.5f, 0 }, { 3, 0.5f, 0 }), make_floor(), false, -1.0f, false, {} },
		{ "sphere cast-tilted floor",		make_cast(cast_types::sphere, on_tilt(5.0f), on_tilt(-5.0f)), make_floor(tilt_position, tilt_rotation), true, 4.5f, true, tilt_normal },
		{ "capsule cast-sphere",			make_cast(cast_types::capsule, { 0, 5, 0 }, { 0, -5, 0 }), make_sphere({ 0, 0, 0 }, 1.0f), true, 2.5f, true, { 0, 1, 0 } },
		{ "capsule cast-box",				make_cast(cast_types::capsule, { 0, 5, 0 }, { 0, -5, 0 }), make_box({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }), true, 2.5f, true, { 0, 1, 0 } },
		{ "capsule cast-capsule",			make_cast(cast_types::capsule, { 5, 0, 0 }, { -5, 0, 0 }), make_capsule({ 0, 0, 0 }, { 0, 0, 0 }, 0.5f, 2.0f), true, 4.0f, true, { 1, 0, 0 } },
		{ "capsule cast-floor",				make_cast(cast_types::capsule, { 0, 5, 0 }, { 0, -5, 0 }), make_floor(), true, 3.5f, true, { 0, 1, 0 } },
		{ "capsule cast-floor lying",		make_cast(cast_types::capsule, { 0, 5, 0 }, { 0, -5, 0 }, 0.5f, 2.0f, { 0, 0, 90 }), make_floor(), true, 4.5f, true, { 0, 1, 0 } },
	};

	for (auto& test : cast_cases)
	{
		cast_desc desc = to_unit(test.cast);
		auto target = create_shape(to_unit(test.target));

		collision::intersect_data data;
		bool hit = execute_cast(desc, *target, &data);
		bool hit_null = execute_cast(desc, *target, nullptr);

		std::string detail;
		bool passed = true;
		auto fail = [&](const std::string& text) { passed = false; detail += text + " "; };

		if (hit != test.expect_hit)
			fail(text_format("hit=%d expect=%d.", hit, test.expect_hit));
		if (hit != hit_null)
			fail("result differs with intersect=nullptr.");
		if (hit && data.size() == 0)
			fail("no hit data.");
		if (hit && data.size() > 0)
		{
			const auto info = to_float_info(data[0]);
			float length = info.length / unit;
			detail += text_format("len=%.3f n=%s. ", length, to_string(info.normal).c_str());

			if (!is_finite3(info.normal) || !is_finite3(info.point) || !std::isfinite(info.length))
				fail("non finite value.");
			if (test.length >= 0.0f && fabsf(length - test.length) > tolerance)
				fail(text_format("length expect %.3f.", test.length));
			if (test.check_normal && dot3(info.normal, normalize3(test.normal)) < 0.999f)
				fail(text_format("normal expect %s.", to_string(normalize3(test.normal)).c_str()));

			//	キャストの接触時中心が始点+移動距離と一致するか
			if (desc.type != cast_types::segment)
			{
				math::float3 direction = normalize3(sub3(desc.end, desc.start));
				math::float3 center = add3(desc.start, scale3(direction, info.length));
				if (length3(sub3(center, info.point)) / unit > tolerance)
					fail(text_format("point %s is not center at hit.", to_string(scale3(info.point, 1.0f / unit)).c_str()));
			}
		}
		add_result(std::string("[cast] ") + test.name, passed, detail);
	}
}

void collision_test_scene::imgui_render_core()
{
	if (!ImGui::CollapsingHeader("collision test", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::RadioButton("overlap", &mode, 0);
	ImGui::SameLine();
	ImGui::RadioButton("cast", &mode, 1);
	ImGui::Separator();

	if (mode == 0)
	{
		edit_shape("A", shape_descs[0]);
		ImGui::Separator();
		edit_shape("B", shape_descs[1]);
		ImGui::Checkbox("draw pushed A (green)", &draw_ghost);
		ImGui::Separator();
		show_intersect("A->B (yellow)", hit_ab, hit_ab_without_data, intersect_ab);
		show_intersect("B->A (cyan)", hit_ba, hit_ba_without_data, intersect_ba);
		if (hit_ab != hit_ba)
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "asymmetric result");
	}
	else
	{
		ImGui::PushID("cast");
		static const char* type_names[] = { "segment", "sphere", "capsule" };
		int type = static_cast<int>(cast.type);
		if (ImGui::Combo("cast type", &type, type_names, IM_ARRAYSIZE(type_names)))
			cast.type = static_cast<cast_types>(type);
		ImGui::DragFloat3("start(m)", &cast.start.x, 0.01f);
		ImGui::DragFloat3("end(m)", &cast.end.x, 0.01f);
		if (cast.type != cast_types::segment)
			ImGui::DragFloat("radius(m)", &cast.radius, 0.01f, 0.01f, 100.0f);
		if (cast.type == cast_types::capsule)
		{
			ImGui::DragFloat("length(m)", &cast.length, 0.01f, 0.0f, 100.0f);
			ImGui::DragFloat3("rotation(deg)", &cast.rotation.x, 0.5f);
		}
		ImGui::PopID();
		ImGui::Separator();
		edit_shape("target", shape_descs[1]);
		ImGui::Separator();
		ImGui::Text("result : %s", cast_hit ? "HIT" : "---");
		for (size_t index = 0; index < cast_intersect.size(); ++index)
		{
			const auto info = to_float_info(cast_intersect[index]);
			ImGui::Text("  center %s", to_string(scale3(info.point, 1.0f / unit)).c_str());
			ImGui::Text("  contact %s", to_string(scale3(info.point2, 1.0f / unit)).c_str());
			ImGui::Text("  normal %s  length %.4f m", to_string(info.normal).c_str(), info.length / unit);
		}
	}

	ImGui::Separator();
	if (ImGui::Button("run self tests"))
		run_self_tests();
	if (!test_results.empty())
	{
		ImGui::SameLine();
		bool all = test_passed_count == test_results.size();
		ImGui::TextColored(all ? ImVec4(0.3f, 1, 0.3f, 1) : ImVec4(1, 0.3f, 0.3f, 1), "%zu / %zu passed", test_passed_count, test_results.size());
		if (ImGui::BeginChild("test results", ImVec2(0, 300), true))
		{
			for (auto& result : test_results)
			{
				ImGui::TextColored(result.passed ? ImVec4(0.3f, 1, 0.3f, 1) : ImVec4(1, 0.3f, 0.3f, 1), "%s %s", result.passed ? "PASS" : "FAIL", result.name.c_str());
				ImGui::TextWrapped("    %s", result.detail.c_str());
			}
		}
		ImGui::EndChild();
	}
}
#endif	//	defined(USE_IMGUI)
