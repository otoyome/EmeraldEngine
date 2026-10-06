#include "Object/Collision/collision_object.h"

#include "Collision/sphere.h"
#include "Collision/capsule.h"
#include "Collision/box.h"
#include "Collision/mesh.h"

#if defined(_DEBUG)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(_DEBUG)

collision_filter_ptr collision_object::get_filter() const
{
	return filter;
}

void collision_object::set_filter(std::string_view name)
{
	if (!filter || filter->get_name() != name)
	{
		filter = collision_filter_manager::instance()->search_filter(name);
	}
}

const collision_object::user_datas& collision_object::get_user_data() const
{
	return user_data;
}

void collision_object::set_user_data(const user_datas& user_data)
{
	this->user_data = user_data;
}

float collision_object::get_mass() const
{
	return mass;
}

void collision_object::set_mass(float mass)
{
	this->mass = mass;
}

const material::physical_datas& collision_object::get_physical_data() const
{
	return physical_data;
}

void collision_object::set_physical_data(const material::physical_datas& data)
{
	physical_data = data;
}

std::shared_ptr<collision::bounding_object> collision_object::get_bounding_object()
{
	update();
	return bounding_object;
}

void collision_object::set_bounding_object(std::shared_ptr<collision::bounding_object> object)
{
	bounding_object = object;
}

void collision_object::set_draw_normal_color(math::float4 color)
{
	this->draw_normal_color = color;
}

void collision_object::set_draw_hit_color(math::float4 color)
{
	this->draw_hit_color = color;
}

const math::rotatorf& collision_object::get_rotation() const
{
	return rotation;
}

void collision_object::set_rotation(const math::rotatorf& rotation)
{
	this->rotation = rotation;
	set_flag(flags::dirty, true);
}

const math::float3& collision_object::get_position() const
{
	return position;
}

void collision_object::set_position(const math::float3& position)
{
	this->position = position;
	set_flag(flags::dirty, true);
}

const math::float3& collision_object::get_local_scale() const
{
	return local_scale;
}

void collision_object::set_local_scale(const math::float3& scale)
{
	local_scale = scale;
	set_flag(flags::dirty, true);
}

const math::rotatorf& collision_object::get_local_rotation() const
{
	return local_rotation;
}

void collision_object::set_local_rotation(const math::rotatorf& rotation)
{
	local_rotation = rotation;
	set_flag(flags::dirty, true);
}

const math::float3& collision_object::get_local_offset() const
{
	return local_offset;
}

void collision_object::set_local_offset(const math::float3& offset)
{
	local_offset = offset;
	set_flag(flags::dirty, true);
}

collision_object::collision_object()
: filter()
, flag_data(0)
, mass(48.0f * 0.25f)
, local_scale(1.0f, 1.0f, 1.0f)
, local_rotation(0.0f, 0.0f, 0.0f)
, local_offset(0.0f, 0.0f, 0.0f)
, rotation(0.0f, 0.0f, 0.0f)
, position(0.0f, 0.0f, 0.0f)
, draw_normal_color(1.0f, 1.0f, 1.0f, 1.0f)
, draw_hit_color(1.0f, 0.0f, 0.0f, 1.0f)
, bounding_object()
, collision_enter()
, collision_stay()
, collision_end()
, hit_objects()
{
	//	Id設定だけは被らないようにしておく
	{
		static size_t id_counter = 0;
		static std::mutex id_mutex;
		std::lock_guard<std::mutex> lock_guard(id_mutex);
		id = ++id_counter;
		assert(id != invalid_id);
	}
	set_filter("default");
	set_flag(flags::active, true);
	set_flag(flags::detect_only, false);
	set_flag(flags::dirty, true);
	set_flag(flags::draw_line, true);
}

collision_object::~collision_object()
{
}

void collision_object::pre_intersect()
{
	set_flag(flags::frame_hit, false);
	for (auto& it : hit_objects)
	{
		it.set_flag(hit_object::flags::added, false);
		it.set_flag(hit_object::flags::aleady, true);
	}
}

void collision_object::solve_pushing_objects(collision::intersect_data& intersect_data)
{
	if (is_flag(flags::detect_only))
		return;

	for (int index = 0; index < intersect_data.size(); ++index)
	{
		auto& info = intersect_data[index];
		math::vector3f Normal(info.normal);
		math::vector3f P1(get_position());
		P1 = P1 + (Normal * static_cast<float>(info.length));
		math::float3 p1;
		p1 = P1.to_float3();
		set_position(p1);
	}
}

bool collision_object::intersect(std::shared_ptr<collision_object>& object, collision::intersect_data& intersect_data)
{
	if (!bounding_object || !object->bounding_object)
		return false;

	if (get_bounding_object()->intersection(object->get_bounding_object().get(), &intersect_data))
	{
		set_flag(flags::frame_hit, true);
		return true;
	}
	return false;
}

void collision_object::update_hit_objects(collision::intersect_data& intersect_data, std::weak_ptr<collision_object> to)
{
	//	ヒットオブジェクトを記録
	auto it = std::find_if(hit_objects.begin(), hit_objects.end(),
						   [to](hit_object& value) {return system_utility::equals(value.to, to);});
	if (it != hit_objects.end())
	{
		it->set_flag(hit_object::flags::added, true);
	}
	else
	{
		hit_objects.emplace_back(to);
	}
}

void collision_object::post_intersect()
{
	//	状況に応じて関数を呼び出す
	auto it = hit_objects.begin();
	while (it != hit_objects.end())
	{
		collision_object::data data;
		data.from = shared_from_this();
		if (!it->to.expired())
		{
			data.to = it->to.lock();
		}

		if (it->is_flag(hit_object::flags::aleady))
		{
			//	新規追加
			if (it->is_flag(hit_object::flags::added))
			{
				if (collision_stay)
					collision_stay(data);
				++it;
			}
			else
			{
				//	存在する場合だけ呼び出す
				if (collision_end)
					collision_end(data);
				it = hit_objects.erase(it);
			}
			continue;
		}
		else if (!it->is_flag(hit_object::flags::aleady) && it->is_flag(hit_object::flags::added))
		{
			//	新規追加
			if (collision_enter)
				collision_enter(data);
			++it;
		}
		else
		{
			++it;
		}
	}
}

void collision_object::set_collision_enter(on_collision_enter collision_enter)
{
	this->collision_enter = collision_enter;
}

void collision_object::set_collision_stay(on_collision_stay collision_stay)
{
	this->collision_stay = collision_stay;
}

void collision_object::set_collision_end(on_collision_end collision_end)
{
	this->collision_end = collision_end;
}

void collision_object::update()
{
	if (!bounding_object || !is_flag(flags::dirty))
		return;

	set_flag(flags::dirty, false);

	const math::matrixf local_matrix = math::matrixf::rotation_quaternion(local_rotation.to_quaternion()) * math::matrixf::translation(local_offset);
	const math::matrixf world_matrix = local_matrix * math::matrixf::rotation_quaternion(rotation.to_quaternion()) * math::matrixf::translation(position);
	math::vector3f dummy, translation;
	math::quaternionf quaternion;
	world_matrix.decompose(dummy, quaternion, translation);

	const math::float3 bounding_translation = translation.to_float3();
	const math::float4 bounding_rotation = quaternion.to_float4();
	bounding_object->set_local_scale(math::double3(local_scale));
	bounding_object->set_rotation(math::double4(bounding_rotation));
	bounding_object->set_position(math::double3(bounding_translation));
}

void collision_object::debug_draw()
{
#if defined(USE_IMGUI)
	if (!bounding_object || !is_flag(flags::draw_line))
		return;

	update();
	math::float4 draw_color = is_flag(flags::frame_hit) ? draw_hit_color : draw_normal_color;
	switch (bounding_object->get_type())
	{
		case collision::bounding_object::types::sphere:
		{
			auto object = std::static_pointer_cast<collision::bounding_sphere>(bounding_object);
			QUEUE_DEBUG_PRIMITIVE_SPHERE(math::float3(object->get_position()), static_cast<float>(object->get_radius()), draw_color, true);
			break;
		}
		case collision::bounding_object::types::capsule:
		{
			auto object = std::static_pointer_cast<collision::bounding_capsule>(bounding_object);
			QUEUE_DEBUG_PRIMITIVE_CAPSULE(math::float3(object->get_position()), math::float4(object->get_rotation()), static_cast<float>(object->get_radius()), static_cast<float>(object->get_length()), draw_color, true);
			break;
		}
		case collision::bounding_object::types::box:
		{
			auto object = std::static_pointer_cast<collision::bounding_box>(bounding_object);
			QUEUE_DEBUG_PRIMITIVE_BOX(math::float3(object->get_position()), math::float4(object->get_rotation()), math::float3(object->get_local_scale()), draw_color, true);
			break;
		}
		case collision::bounding_object::types::mesh:
		{
			if (!draw_mesh_color)
				return;

			auto object = std::static_pointer_cast<collision::bounding_mesh>(bounding_object);
			auto positions = object->get_positions().data();
			static constexpr math::float4 DrawColor[] =
			{
				{1.0f, 1.0f, 1.0f, 1.0f},
				{1.0f, 1.0f, 0.0f, 1.0f},
				{1.0f, 0.0f, 1.0f, 1.0f},
				{0.0f, 1.0f, 1.0f, 1.0f},
				{1.0f, 0.0f, 0.0f, 1.0f},
				{0.0f, 1.0f, 0.0f, 1.0f},
				{0.0f, 0.0f, 1.0f, 1.0f},

				{0.5f, 0.5f, 0.5f, 1.0f},
				{0.5f, 0.5f, 0.0f, 1.0f},
				{0.5f, 0.0f, 0.5f, 1.0f},
				{0.0f, 0.5f, 0.5f, 1.0f},
				{0.5f, 0.0f, 0.0f, 1.0f},
				{0.0f, 0.5f, 0.0f, 1.0f},
				{0.0f, 0.0f, 0.5f, 1.0f},
			};

			math::float4 rotation = {0, 0, 0, 1};
			INT draw_mesh_collision_index = draw_mesh_collision_x_index +
				draw_mesh_collision_y_index * object->get_separate_count().x +
				draw_mesh_collision_z_index * object->get_separate_count().x * object->get_separate_count().y;
			math::float3 extent = object->get_area_unit();
			extent.x *= 0.5f;
			extent.y *= 0.5f;
			extent.z *= 0.5f;
			for (UINT index = 0; index < object->get_block_datas().size(); ++index)
			{
				auto& block = object->get_block_datas()[index];

				math::float4 draw_color = index == draw_mesh_collision_index ? draw_hit_color : draw_normal_color;
				QUEUE_DEBUG_PRIMITIVE_BOX(block.center, rotation, extent, draw_color, true);
				auto ti = block.triangles.data();
				auto t = object->get_triangles().data();
				for (UINT index = 0;index < block.triangles.size(); ++index)
				{
					auto triangle = t + ti[index];
					auto a = (positions + triangle->a)->to_float3();
					auto b = (positions + triangle->b)->to_float3();
					auto c = (positions + triangle->c)->to_float3();
					QUEUE_DEBUG_PRIMITIVE_LINE(a, b, draw_color, true);
					QUEUE_DEBUG_PRIMITIVE_LINE(c, b, draw_color, true);
					QUEUE_DEBUG_PRIMITIVE_LINE(a, c, draw_color, true);
				}
			}
			break;
		}
	}
#endif // defined(USE_IMGUI)
}

#if defined(USE_IMGUI)
void collision_object::imgui_render()
{
	draw_mesh_color = false;
	if (!bounding_object)
		return;

	update();
	ImGui::PushID(0x41203040 + static_cast<int>(id));

	std::string name_node = name;
	name_node += " : ";
	name_node += NAMEOF_ENUM(bounding_object->get_type());
	if (ImGui::TreeNode(system_utility::convert_shift_jis_to_utf8(name_node.data()).data()))
	{
		ImGui::DragFloat("mass", &mass, 0.01f);
		ImGui::DragFloat("friction", &physical_data.friction, 0.01f);
		switch (bounding_object->get_type())
		{
			case collision::bounding_object::types::sphere:
			{
				ImGui::DragFloat("local_radius", &local_scale.x, 0.01f);
				ImGui::DragFloat3("local_rotation", &local_rotation.x, 0.01f);
				ImGui::DragFloat3("local_offset", &local_offset.x, 0.01f);

				ImGui::DragFloat3("rotation", &rotation.x, 0.01f);
				ImGui::DragFloat3("position", &position.x, 0.01f);
				break;
			}
			case collision::bounding_object::types::capsule:
			{
				ImGui::DragFloat("local_radius", &local_scale.x, 0.01f);
				ImGui::DragFloat("local_length", &local_scale.y, 0.01f);
				ImGui::DragFloat3("local_rotation", &local_rotation.x, 0.01f);
				ImGui::DragFloat3("local_offset", &local_offset.x, 0.01f);

				ImGui::DragFloat3("rotation", &rotation.x, 0.01f);
				ImGui::DragFloat3("position", &position.x, 0.01f);
				break;
			}
			case collision::bounding_object::types::box:
			{
				ImGui::DragFloat3("local_scale", &local_scale.x, 0.01f);
				ImGui::DragFloat3("local_rotation", &local_rotation.x, 0.01f);
				ImGui::DragFloat3("local_offset", &local_offset.x, 0.01f);

				ImGui::DragFloat3("rotation", &rotation.x, 0.01f);
				ImGui::DragFloat3("position", &position.x, 0.01f);
				break;
			}
			case collision::bounding_object::types::mesh:
			{
				auto bo = std::static_pointer_cast<collision::bounding_mesh>(bounding_object);
				if (bo->get_block_datas().size() > 0)
				{
					ImGui::DragInt("draw_mesh_collision_x_index", &draw_mesh_collision_x_index, 1, -1, bo->get_separate_count().x - 1);
					ImGui::DragInt("draw_mesh_collision_y_index", &draw_mesh_collision_y_index, 1, -1, bo->get_separate_count().y - 1);
					ImGui::DragInt("draw_mesh_collision_z_index", &draw_mesh_collision_z_index, 1, -1, bo->get_separate_count().z - 1);
				}
				draw_mesh_color = true;
				break;
			}
		}
		set_flag(flags::dirty, true);
		ImGui::TreePop();
	}
	ImGui::PopID();
}
#endif	//	defined(USE_IMGUI)
