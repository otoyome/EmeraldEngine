#include "Object/Collision/collision_world.h"

namespace
{
	static constexpr float addjust_pushing_factor = 0.05f;
}

collision_world::collision_world()
: collision_objects()
, collision_objects_add()
{
}

collision_world::~collision_world()
{
	system_utility::std_vector_swap_clear(collision_objects);
	system_utility::std_vector_swap_clear(collision_objects_add);
	system_utility::std_vector_swap_clear(collision_objects_remove);
}

void collision_world::inner_step_simulation(float elapsed_time)
{
	//	ヒット処理
	float pushing_factor = 1.0f + addjust_pushing_factor;
	do
	{
		bool is_pushing = false;
		auto* collision_array = collision_objects.data();
		for (size_t index1 = 0; index1 < collision_objects.size(); ++index1)
		{
			auto& object1 = collision_objects[index1];
			if (!object1->is_flag(collision_object::flags::active))
				continue;

			for (size_t index2 = index1 + 1; index2 < collision_objects.size(); ++index2)
			{
				auto& object2 = collision_objects[index2];
				if (!object2->is_flag(collision_object::flags::active))
					continue;

				//	両方staticの場合は除外
				bool is_static_object = object1->is_flag(collision_object::flags::static_object) &&
										object2->is_flag(collision_object::flags::static_object);
				if (is_static_object)
					continue;

				//	カテゴリーチェック
				if (!object1->get_filter()->is_flag(object2->get_filter()->get_category()))
					continue;

				collision::intersect_data intersect_data;
				if (object1->intersect(object2, intersect_data))
				{
					collision::intersect_data data1 = intersect_data;
					collision::intersect_data data2 = intersect_data;
					//	押し合い処理
					bool is_push = !object1->is_flag(collision_object::flags::detect_only) &&
									!object2->is_flag(collision_object::flags::detect_only);
					if (is_push && !is_static_object)
					{
						data1.clear();
						data2.clear();
						//	押し合い係数生成
						float rate1 = 0, rate2 = 0;
						if (object1->is_flag(collision_object::flags::static_object))
						{
							rate2 = 1;
						}
						else if (object2->is_flag(collision_object::flags::static_object))
						{
							rate1 = 1;
						}
						else
						{
							float total_mass = object1->get_mass() + object2->get_mass();
							rate2 = object1->get_mass() / total_mass;
							rate1 = object2->get_mass() / total_mass;
						}

						bool is_inner_pushing = false;
						for (size_t index = 0; index < intersect_data.size(); ++index)
						{
							auto data = intersect_data[index];
							if (fabsf(data.length) <= FLT_EPSILON)
								continue;

							auto temp = data;
							temp.length = temp.length * rate1 * pushing_factor;
							data1.add(temp);

							temp.normal.x = -data.normal.x;
							temp.normal.y = -data.normal.y;
							temp.normal.z = -data.normal.z;
							temp.length = data.length * rate2 * pushing_factor;
							data2.add(temp);
							is_inner_pushing = true;
						}
						object1->solve_pushing_objects(data1);
						object2->solve_pushing_objects(data2);
						if (is_inner_pushing)
							is_pushing = true;
					}
					else
					{
						//	法線を反転しておく
						for (size_t index = 0; index < data2.size(); ++index)
						{
							auto& data = data2[index];
							if (fabsf(data.length) <= FLT_EPSILON)
								continue;

							data.normal.x = -data.normal.x;
							data.normal.y = -data.normal.y;
							data.normal.z = -data.normal.z;
						}

					}

					//	ヒットオブジェクトを記録
					object1->update_hit_objects(data1, object2);
					object2->update_hit_objects(data2, object1);
				}
			}
		}
		//	係数更新
		pushing_factor = is_pushing ? pushing_factor + addjust_pushing_factor : 0.0f;
	} while (1.0f <= pushing_factor && 1.0f <= addjust_pushing_factor * 10);
}

void collision_world::step(float elapsed_time, int max_substep, float fixed_time_stamp)
{
	remove_objects();
	add_objects();

	//	事前処理
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active))
			continue;

		object->pre_intersect();
	}

	//	衝突検知
	{
		//	細分化回数
		int loop_count = static_cast<int>(elapsed_time / fixed_time_stamp);
		int limit_loop_count = std::min(loop_count, max_substep);

		for (int count = 0; count < limit_loop_count; ++count)
		{
			inner_step_simulation(fixed_time_stamp);

			elapsed_time -= fixed_time_stamp;
		}

		//	一気に行う
		if (elapsed_time > 0.0f)
			inner_step_simulation(elapsed_time);
	}

	//	事後処理
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active))
			continue;

		object->post_intersect();
	}

	remove_objects();
	add_objects();
}

#if defined(USE_IMGUI)
void collision_world::debug_primitive_draw()
{
	//	デバッグライン描画
	for (auto& object : collision_objects)
	{
		object->debug_draw();
	}
}

void collision_world::imgui_render()
{
	if (ImGui::Begin("collisions"))
	{
		for (auto& object : collision_objects)
		{
			object->imgui_render();
		}
	}
	ImGui::End();
}
#endif // defined(USE_IMGUI)

bool collision_world::register_object(collision_object_ptr object)
{
	if (!object)
		return false;
	if (std::find(collision_objects_add.begin(), collision_objects_add.end(), object) != collision_objects_add.end())
		return false;
	if (std::find(collision_objects.begin(), collision_objects.end(), object) != collision_objects.end())
		return false;

	auto it = std::find(collision_objects_remove.begin(), collision_objects_remove.end(), object);
	if (it != collision_objects_remove.end())
	{
		collision_objects_remove.erase(it);
	}
	collision_objects_add.emplace_back(object);
	return true;
}

bool collision_world::unregister_object(collision_object_ptr object)
{
	if (!object)
		return false;
	if (std::find(collision_objects_remove.begin(), collision_objects_remove.end(), object) != collision_objects_remove.end())
		return false;
	if (std::find(collision_objects.begin(), collision_objects.end(), object) == collision_objects.end())
		return false;

	auto it = std::find(collision_objects_add.begin(), collision_objects_add.end(), object);
	if (it != collision_objects_add.end())
	{
		collision_objects_add.erase(it);
	}
	collision_objects_remove.emplace_back(object);
	return true;
}

bool collision_world::ray_cast(const ray_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision)
{
	collision::intersect_data data;
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active) ||
			object->is_flag(collision_object::flags::detect_only))
			continue;

		//	カテゴリーチェック
		if (!desc.filter->is_flag(object->get_filter()->get_category()))
			continue;

		//	除外オブジェクトチェック
		bool find = false;
		for (auto& exclude_object : desc.exclude_objects)
		{
			if (exclude_object == object)
			{
				find = true;
				break;
			}
		}

		if (!find && object->get_bounding_object()->intersect_segment(desc.start, desc.end, &data))
		{
			if (intersect_data.size() == 0)
			{
				intersect_data = data;
				out_collision = object;
			}
			else if (data[0].length < intersect_data[0].length)
			{
				intersect_data = data;
				out_collision = object;
			}
			data.clear();
		}
	}
	return intersect_data.size() > 0;
}

bool collision_world::sphere_cast(const sphere_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision)
{
	collision::intersect_data data;
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active) ||
			object->is_flag(collision_object::flags::detect_only))
			continue;

		//	カテゴリーチェック
		if (!desc.filter->is_flag(object->get_filter()->get_category()))
			continue;

		//	除外オブジェクトチェック
		bool find = false;
		for (auto& exclude_object : desc.exclude_objects)
		{
			if (exclude_object == object)
			{
				find = true;
				break;
			}
		}

		if (!find && object->get_bounding_object()->intersect_sphere_cast(desc.start, desc.end, desc.radius, &data))
		{
			if (intersect_data.size() == 0)
			{
				intersect_data = data;
				out_collision = object;
			}
			else if (data[0].length < intersect_data[0].length)
			{
				intersect_data = data;
				out_collision = object;
			}
			data.clear();
		}
	}
	return intersect_data.size() > 0;
}

void collision_world::add_objects()
{
	for (auto& object : collision_objects_add)
	{
		collision_objects.emplace_back(object);
	}
	system_utility::std_vector_swap_clear(collision_objects_add);
}

void collision_world::remove_objects()
{
	for (auto& object : collision_objects_remove)
	{
		collision_objects.erase(std::find(collision_objects.begin(), collision_objects.end(), object));
	}
	system_utility::std_vector_swap_clear(collision_objects_remove);
}