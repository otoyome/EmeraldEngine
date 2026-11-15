#include "Editor/editor_icons.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

#include "Editor/editor_camera.h"
#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Object/spatial_object.h"
#include "Object/model_object.h"
#include "Object/light_object.h"
#include "Object/camera_object.h"
#include "Object/GameObjects/System/start_point_object.h"
#include "Graphics/Debug/debug_primitive_collector.h"
#include "System/imgui_texture_keeper.h"
#include "Texture/texture_importer.h"
#include "Texture/texture_resource_utility.h"

namespace
{
	//	@tool アイコンの画像を置くフォルダ
	constexpr std::string_view icon_folder = "Assets/Editor/Icons/";

	//	@tool 画面上のアイコンの大きさ（ピクセル）
	constexpr float icon_size = 32.0f;

	//	@tool 型ごとのアイコン（上から順に調べ、派生した型にも当てる）
	struct icon_type
	{
		std::string_view type;
		std::string_view file;
	};
	const icon_type icon_types[] =
	{
		{ directional_light_object::type_name(), "directional_light" },
		{ point_light_object::type_name(), "point_light" },
		{ spot_light_object::type_name(), "spot_light" },
		{ sky_light_object::type_name(), "sky_light" },
		{ light_probe_object::type_name(), "light_probe" },
		{ camera_object::type_name(), "camera" },
		{ start_point_object::type_name(), "start_point" },
	};
	//	@tool 表に無い形の無いオブジェクトのアイコン
	constexpr std::string_view default_icon_file = "object";
	constexpr size_t default_icon_index = std::size(icon_types);

	//	@tool 選んでいる物の形の色
	const math::float4 shape_color(1.0f, 0.85f, 0.3f, 1.0f);
	const math::float4 inner_shape_color(1.0f, 0.85f, 0.3f, 0.5f);

	math::double3 offset_point(const math::vector3d& base, const math::vector3f& direction, double length)
	{
		return math::double3(base.x + direction.x * length, base.y + direction.y * length, base.z + direction.z * length);
	}

	//	@tool center のまわりの円（right・up が作る面の上）
	void queue_circle(debug_primitive_collector& lines, const math::vector3d& center, const math::vector3f& right, const math::vector3f& up, double radius, const math::float4& color)
	{
		constexpr int segment_count = 32;
		math::double3 previous{};
		for (int index = 0; index <= segment_count; ++index)
		{
			const double angle = 2.0 * 3.14159265358979323846 * index / segment_count;
			const math::vector3f direction = right * static_cast<float>(std::cos(angle)) + up * static_cast<float>(std::sin(angle));
			const math::double3 point = offset_point(center, direction, radius);
			if (index > 0)
				lines.queue_line(previous, point, color);
			previous = point;
		}
	}
}

void editor_icons::load_textures()
{
	if (textures_loaded)
		return;
	textures_loaded = true;

	auto load = [](std::string_view file) -> texture_resource_ptr
	{
		const std::string base = std::string(icon_folder) + std::string(file);
		const std::string etx_path = base + ".etx";
		const std::string png_path = base + ".png";

		//	@tool .etx が無ければ .png から取り込む（色は変換せず、そのままの値で ImGui に出す）
		std::error_code error;
		if (!std::filesystem::exists(etx_path, error) && std::filesystem::exists(png_path, error) && texture_importer::is_valid())
		{
			texture_import_settings settings = texture_import_settings::make_preset(texture_import_settings::presets::data);
			settings.format = texture_import_settings::gpu_formats::uncompressed;
			settings.generate_mipmaps = true;
			if (!texture_importer::instance()->import(png_path, settings))
				DEBUG_ERROR_CATEGORY(logger::categorys::system, "editor_icons import failed[%s]", png_path.c_str());
		}
		if (!std::filesystem::exists(etx_path, error))
			return nullptr;
		return texture_resource_utility::load_texture_resource(etx_path);
	};

	textures.clear();
	for (const icon_type& icon : icon_types)
		textures.emplace_back(load(icon.file));
	textures.emplace_back(load(default_icon_file));
}

size_t editor_icons::find_icon(const game_object& object)
{
	for (size_t index = 0; index < std::size(icon_types); ++index)
	{
		if (object.check_type(icon_types[index].type))
			return index;
	}
	return default_icon_index;
}

std::vector<editor_icons::placed_icon> editor_icons::collect(engine_context& context, const ImVec2& image_size)
{
	std::vector<placed_icon> result;
	if (image_size.x <= 0.0f || image_size.y <= 0.0f)
		return result;

	//	@tool ビューポートを映しているエディターカメラの視点（editor_picker のレイと同じ求め方）
	object_manager& objects = context.get_objects();
	const std::shared_ptr<editor_camera_object> camera = objects.search_object<editor_camera_object>();
	if (!camera || !camera->get_camera())
		return result;
	const view_parameters& parameters = camera->get_camera()->get_parameters();
	const float aspect = image_size.x / image_size.y;
	const float tan_half_fov = std::tan(parameters.fov_y * 0.5f);
	const math::vector3f right = parameters.rotation.right();
	const math::vector3f up = parameters.rotation.up();
	const math::vector3f front = parameters.rotation.front();

	for (const std::shared_ptr<game_object>& object : objects.search_game_objects<spatial_object>())
	{
		//	@tool 形のあるモデルと一時オブジェクト（エディターカメラなど）には出さない
		if (!object || object->is_transient() || object->check_type(model_object::type_name()))
			continue;

		const math::vector3d world_offset = static_cast<spatial_object*>(object.get())->get_world_translation() - parameters.position;
		const math::vector3f offset(world_offset.to_float3());
		const float depth = math::vector3f::dot(offset, front);
		if (depth <= parameters.clip_distance.x)
			continue;

		const float x = math::vector3f::dot(offset, right) / (depth * tan_half_fov * aspect);
		const float y = math::vector3f::dot(offset, up) / (depth * tan_half_fov);
		const ImVec2 center((x + 1.0f) * 0.5f * image_size.x, (1.0f - y) * 0.5f * image_size.y);
		const float half = icon_size * 0.5f;
		if (center.x < -half || center.y < -half || center.x > image_size.x + half || center.y > image_size.y + half)
			continue;

		placed_icon& icon = result.emplace_back();
		icon.object = object;
		icon.center = center;
		icon.depth = depth;
		icon.icon_index = find_icon(*object);
	}

	std::sort(result.begin(), result.end(), [](const placed_icon& a, const placed_icon& b) { return a.depth > b.depth; });
	return result;
}

void editor_icons::draw(engine_context& context, const ImVec2& image_position, const ImVec2& image_size)
{
	load_textures();

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->PushClipRect(image_position, ImVec2(image_position.x + image_size.x, image_position.y + image_size.y), true);

	//	@tool 選んでいる物は色を付け、非アクティブな物は薄く出す
	const std::shared_ptr<game_object> selected = context.get_selected_object();
	const float half = icon_size * 0.5f;
	for (const placed_icon& icon : collect(context, image_size))
	{
		ImU32 color = IM_COL32(255, 255, 255, 230);
		if (icon.object == selected)
			color = IM_COL32(255, 190, 60, 255);
		else if (!icon.object->is_active())
			color = IM_COL32(255, 255, 255, 100);

		const ImVec2 center(image_position.x + icon.center.x, image_position.y + icon.center.y);
		const texture_resource_ptr& texture = icon.icon_index < textures.size() ? textures[icon.icon_index] : nullptr;
		if (texture && texture->get_texture())
			draw_list->AddImage(imgui_texture_keeper::instance()->keep(texture->get_texture()), ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), color);
		else
			draw_list->AddCircleFilled(center, half * 0.4f, color);	//	@tool 画像が読めない時は丸を出す
	}

	draw_list->PopClipRect();
}

std::shared_ptr<game_object> editor_icons::pick(engine_context& context, const ImVec2& position, const ImVec2& image_size)
{
	//	@tool 上に描かれている（カメラに近い）物から調べる
	const std::vector<placed_icon> icons = collect(context, image_size);
	const float half = icon_size * 0.5f;
	for (auto it = icons.rbegin(); it != icons.rend(); ++it)
	{
		if (std::fabs(position.x - it->center.x) <= half && std::fabs(position.y - it->center.y) <= half)
			return it->object;
	}
	return nullptr;
}

void editor_icons::queue_selected_shape(engine_context& context)
{
	const std::shared_ptr<game_object> selected = context.get_selected_object();
	if (!selected || selected->is_transient() || !selected->check_type(spatial_object::type_name()))
		return;

	debug_primitive_collector& lines = context.get_debug_primitives();
	const spatial_object& spatial = *static_cast<spatial_object*>(selected.get());
	const math::vector3d position = spatial.get_world_translation();
	const math::rotatorf rotation(spatial.get_world_euler());
	const math::vector3f right = rotation.right();
	const math::vector3f up = rotation.up();
	const math::vector3f front = rotation.front();

	//	@tool 点光源：届く範囲の球
	if (selected->check_type(point_light_object::type_name()))
	{
		const light_ptr light = static_cast<light_object*>(selected.get())->get_light();
		if (light)
			lines.queue_sphere(position.to_double3(), light->get_range(), shape_color);
		return;
	}

	//	@tool スポットライト：外側の円錐（内側の円錐は薄く円だけ）
	if (selected->check_type(spot_light_object::type_name()))
	{
		const light_ptr light = static_cast<light_object*>(selected.get())->get_light();
		if (!light)
			return;
		const double range = light->get_range();
		auto queue_cone = [&](double half_angle, const math::float4& color, bool edges)
		{
			const math::vector3d center(offset_point(position, front, range * std::cos(half_angle)));
			const double radius = range * std::sin(half_angle);
			queue_circle(lines, center, right, up, radius, color);
			if (!edges)
				return;
			const math::vector3f directions[] = { right, right * -1.0f, up, up * -1.0f };
			for (const math::vector3f& direction : directions)
				lines.queue_line(position.to_double3(), offset_point(center, direction, radius), color);
		};
		queue_cone(light->get_outer_cone(), shape_color, true);
		queue_cone(light->get_inner_cone(), inner_shape_color, false);
		return;
	}

	//	@tool 平行光源：向きの矢印
	if (selected->check_type(directional_light_object::type_name()))
	{
		constexpr double length = 30.0;
		constexpr double head_length = 6.0;
		constexpr double head_width = 3.0;
		const math::double3 tip = offset_point(position, front, length);
		lines.queue_line(position.to_double3(), tip, shape_color);
		const math::vector3d head_base(offset_point(position, front, length - head_length));
		const math::vector3f directions[] = { right, right * -1.0f, up, up * -1.0f };
		for (const math::vector3f& direction : directions)
			lines.queue_line(tip, offset_point(head_base, direction, head_width), shape_color);
		return;
	}

	//	@tool 範囲付きのライトプローブ：内側と外側の箱
	if (selected->check_type(area_light_probe_object::type_name()))
	{
		const area_light_probe_object& probe = *static_cast<area_light_probe_object*>(selected.get());
		const math::float4 quaternion = rotation.to_quaternion().to_float4();
		lines.queue_box(position.to_double3(), quaternion, probe.get_outer_range(), shape_color);
		lines.queue_box(position.to_double3(), quaternion, probe.get_inner_range(), inner_shape_color);
		return;
	}

	//	@tool カメラ：視錐台（長さは固定、縦横比は 16:9）
	if (selected->check_type(camera_object::type_name()))
	{
		const camera_ptr camera = static_cast<camera_object*>(selected.get())->get_camera();
		if (!camera)
			return;
		constexpr double length = 20.0;
		constexpr float aspect = 16.0f / 9.0f;
		const float tan_half_fov = std::tan(camera->get_parameters().fov_y * 0.5f);
		const math::vector3d center(offset_point(position, front, length));
		const double half_height = length * tan_half_fov;
		const double half_width = half_height * aspect;
		math::double3 corners[4];
		const float signs[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		for (int index = 0; index < 4; ++index)
		{
			const math::vector3d edge(offset_point(center, right, half_width * signs[index][0]));
			corners[index] = offset_point(edge, up, half_height * signs[index][1]);
		}
		for (int index = 0; index < 4; ++index)
		{
			lines.queue_line(position.to_double3(), corners[index], shape_color);
			lines.queue_line(corners[index], corners[(index + 1) % 4], shape_color);
		}
		return;
	}
}
#endif	//	defined(USE_IMGUI)
