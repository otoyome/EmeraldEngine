#include "Object/Collision/collision_world.h"

#include "Collision/capsule.h"

namespace
{
	static constexpr float addjust_pushing_factor = 0.05f;

	//	収束しない場合に当たり判定をやり直す回数の上限
	static constexpr int max_pushing_iteration = 10;

	//	この数以上の候補ペアがあるときだけフェーズAをスレッド分割する
	static constexpr size_t narrow_phase_parallel_min = 64;

	//	ブロードフェーズ用パラメータ
	static constexpr float broadphase_margin    = system_units::convert_m_to_unit(0.1f); //	AABBを膨らませる余白
	static constexpr int   broadphase_max_depth = 6;                                     //	八分木の最大分割深度

	void build_octree(const std::vector<collision_object_ptr>& objects, collision_octree& tree,
					  std::vector<std::pair<collision_object*, collision_octree::aabb>>& scratch)
	{
		scratch.clear();

		//	ルート領域を求めつつ、各オブジェクトのAABBを控えておく
		bool has_any = false;
		math::double3 world_min{};
		math::double3 world_max{};
		for (auto& object : objects)
		{
			if (!object->is_flag(collision_object::flags::active))
				continue;

			auto bounding = object->get_bounding_object();
			if (!bounding)
				continue;

			math::double3 world_min_corner;
			math::double3 world_max_corner;
			bounding->include_corner(world_min_corner, world_max_corner);
			const math::double3& min_corner = world_min_corner;
			const math::double3& max_corner = world_max_corner;

			collision_octree::aabb box{};
			box.min = { min_corner.x - broadphase_margin, min_corner.y - broadphase_margin, min_corner.z - broadphase_margin };
			box.max = { max_corner.x + broadphase_margin, max_corner.y + broadphase_margin, max_corner.z + broadphase_margin };
			scratch.push_back({ object.get(), box });

			if (!has_any)
			{
				world_min = box.min;
				world_max = box.max;
				has_any = true;
			}
			else
			{
				world_min.x = std::min(world_min.x, box.min.x);
				world_min.y = std::min(world_min.y, box.min.y);
				world_min.z = std::min(world_min.z, box.min.z);
				world_max.x = std::max(world_max.x, box.max.x);
				world_max.y = std::max(world_max.y, box.max.y);
				world_max.z = std::max(world_max.z, box.max.z);
			}
		}

		tree.clear();
		if (!has_any)
			return;

		collision_octree::aabb root{};
		root.min = world_min;
		root.max = world_max;
		tree.reset(root, broadphase_max_depth);

		//	控えたAABBでそのまま登録する（include_corner を二度呼ばない）
		for (auto& entry : scratch)
			tree.insert(entry.first, entry.second);
	}

	//	始点から終点までの移動範囲AABBを求める
	collision_octree::aabb calculate_cast_bounds(const math::double3& start, const math::double3& end, double extent)
	{
		extent += broadphase_margin;

		collision_octree::aabb bounds{};
		bounds.min = { std::min(start.x, end.x) - extent, std::min(start.y, end.y) - extent, std::min(start.z, end.z) - extent };
		bounds.max = { std::max(start.x, end.x) + extent, std::max(start.y, end.y) + extent, std::max(start.z, end.z) + extent };
		return bounds;
	}

	//	キャスト範囲のAABBと重なるか
	bool overlaps_cast_bounds(collision_object& object, const collision_octree::aabb& cast_bounds)
	{
		collision_octree::aabb object_bounds{};
		if (!object.get_world_bounds(object_bounds.min, object_bounds.max))
			return false;
		return collision_octree::aabb::intersects(object_bounds, cast_bounds);
	}
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
	(void)elapsed_time;

	//	ヒット処理
	float pushing_factor = 1.0f + addjust_pushing_factor;
	int   pushing_count  = 0;
	bool  is_pushing     = false;
	do
	{
		is_pushing = false;

		//	八分木で交差し得る組だけに絞り込む
		build_octree(collision_objects, broadphase, broadphase_entries);
		candidate_pairs.clear();
		broadphase.collect_pairs(candidate_pairs);

		//	判定処理
		const size_t pair_count = candidate_pairs.size();
		narrow_hits.assign(pair_count, 0);
		if (narrow_contacts.size() < pair_count)
			narrow_contacts.resize(pair_count);
		auto narrow_test = [&](size_t worker_index, size_t begin, size_t end)
		{
			(void)worker_index;

			for (size_t index = begin; index < end; ++index)
			{
				collision_object* raw1 = candidate_pairs[index].first;
				collision_object* raw2 = candidate_pairs[index].second;

				//	両方staticなら対象外
				if (raw1->get_body_type() == collision_object::body_types::static_body &&
					raw2->get_body_type() == collision_object::body_types::static_body)
					continue;

				//	応答チェック
				const collision_response response = collision_filter::get_response(*raw1->get_filter(), *raw2->get_filter());
				if (response == collision_response::ignore)
				{
					//	互いの応答の弱い方を使い ignore なら当たらない
					continue;
				}

				auto bounding1 = raw1->get_bounding_object();
				auto bounding2 = raw2->get_bounding_object();
				if (!bounding1 || !bounding2)
					continue;

				//	AABBを使って大雑把に判定処理
				collision_octree::aabb box1{};
				collision_octree::aabb box2{};
				{
					math::double3 min1, max1, min2, max2;
					bounding1->include_corner(min1, max1);
					bounding2->include_corner(min2, max2);
					box1.min = min1; box1.max = max1;
					box2.min = min2; box2.max = max2;
				}
				if (!collision_octree::aabb::intersects(box1, box2))
					continue;

				narrow_contacts[index].clear();
				//	形状に合わせて判定処理
				if (bounding1->intersection(bounding2.get(), &narrow_contacts[index]))
					narrow_hits[index] = (response == collision_response::block) ? 2 : 1;	//	1 は重なりだけ（overlap）、2 は押し返す（block）
			}
		};
		narrow_phase_executor.parallel_for(pair_count, narrow_phase_parallel_min, narrow_test);

		//	押し戻し処理
		for (size_t index = 0; index < pair_count; ++index)
		{
			if (!narrow_hits[index])
				continue;

			collision_object* raw1 = candidate_pairs[index].first;
			collision_object* raw2 = candidate_pairs[index].second;
			collision::intersect_data& hit = narrow_contacts[index];

			collision_object_ptr object1 = raw1->shared_from_this();
			collision_object_ptr object2 = raw2->shared_from_this();

			//	collision_object::intersect と同じく呼び出し側にヒットフラグを立てる
			raw1->set_flag(collision_object::flags::frame_hit, true);

			//	押し戻されるのは dynamic だけ。どちらかが dynamic の時だけ押し合う
			bool can_push = object1->get_body_type() == collision_object::body_types::dynamic_body ||
							object2->get_body_type() == collision_object::body_types::dynamic_body;

			collision::intersect_data& data1 = push_scratch1;
			collision::intersect_data& data2 = push_scratch2;
			data1.clear();
			data2.clear();

			//	押し合い
			bool is_push = narrow_hits[index] == 2;
			if (is_push && can_push)
			{
				//	押し戻し量の配分を決定
				float rate1 = 0, rate2 = 0;
				if (object1->get_body_type() != collision_object::body_types::dynamic_body)
				{
					rate2 = 1;
				}
				else if (object2->get_body_type() != collision_object::body_types::dynamic_body)
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
				for (size_t hit_index = 0; hit_index < hit.size(); ++hit_index)
				{
					auto data = hit[hit_index];
					if (std::fabs(data.length) <= FLT_EPSILON)
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
				//	検知のみ：data1 はそのまま、data2 は法線を反転しておく
				data1 = hit;
				data2 = hit;
				for (size_t hit_index = 0; hit_index < data2.size(); ++hit_index)
				{
					auto& data = data2[hit_index];
					if (std::fabs(data.length) <= FLT_EPSILON)
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

		pushing_factor += addjust_pushing_factor;
		++pushing_count;
	} while (is_pushing && pushing_count < max_pushing_iteration);
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
		//	細分化処理
		int loop_count = static_cast<int>(elapsed_time / fixed_time_stamp);
		int limit_loop_count = std::min(loop_count, max_substep);

		for (int count = 0; count < limit_loop_count; ++count)
		{
			inner_step_simulation(fixed_time_stamp);

			elapsed_time -= fixed_time_stamp;
		}

		//	残りを一気に処理
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
void collision_world::debug_primitive_draw(engine_context& engine)
{
	//	デバッグライン描画
	for (auto& object : collision_objects)
	{
		object->debug_draw(engine);
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

	std::lock_guard<std::mutex> lock(mutex);

	auto it = std::find(collision_objects_remove.begin(), collision_objects_remove.end(), object);
	if (it != collision_objects_remove.end())
	{
		collision_objects_remove.erase(it);
		return true;
	}

	if (std::find(collision_objects_add.begin(), collision_objects_add.end(), object) != collision_objects_add.end())
		return false;
	if (std::find(collision_objects.begin(), collision_objects.end(), object) != collision_objects.end())
		return false;

	collision_objects_add.emplace_back(object);
	return true;
}

bool collision_world::unregister_object(collision_object_ptr object)
{
	if (!object)
		return false;

	std::lock_guard<std::mutex> lock(mutex);

	auto it = std::find(collision_objects_add.begin(), collision_objects_add.end(), object);
	if (it != collision_objects_add.end())
	{
		collision_objects_add.erase(it);
		return true;
	}

	if (std::find(collision_objects_remove.begin(), collision_objects_remove.end(), object) != collision_objects_remove.end())
		return false;
	if (std::find(collision_objects.begin(), collision_objects.end(), object) == collision_objects.end())
		return false;

	collision_objects_remove.emplace_back(object);
	return true;
}

bool collision_world::ray_cast(const ray_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision)
{
	//	移動範囲のAABBで候補を絞り込む
	const collision_octree::aabb cast_bounds = calculate_cast_bounds(desc.start, desc.end, 0.0);

	collision::intersect_data data;
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active))
			continue;

		//	応答チェック
		if (collision_filter::get_response(*desc.filter, *object->get_filter()) != collision_response::block)
			continue;

		//	AABBが重ならなければ詳細判定しない
		if (!overlaps_cast_bounds(*object, cast_bounds))
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

		if (!find && object->get_bounding_object()->intersect_segment(math::double3(desc.start), math::double3(desc.end), &data))
		{
			if (intersect_data.size() == 0)
			{
				intersect_data.swap(data);
				out_collision = object;
			}
			else if (data[0].length < intersect_data[0].length)
			{
				intersect_data.swap(data);
				out_collision = object;
			}
			data.clear();
		}
	}
	return intersect_data.size() > 0;
}

bool collision_world::sphere_cast(const sphere_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision)
{
	//	移動範囲のAABBで候補を絞り込む
	const collision_octree::aabb cast_bounds = calculate_cast_bounds(desc.start, desc.end, desc.radius);

	collision::intersect_data data;
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active))
			continue;

		//	応答チェック
		if (collision_filter::get_response(*desc.filter, *object->get_filter()) != collision_response::block)
			continue;

		//	AABBが重ならなければ詳細判定しない
		if (!overlaps_cast_bounds(*object, cast_bounds))
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

		if (!find && object->get_bounding_object()->intersect_sphere_cast(math::double3(desc.start), math::double3(desc.end), desc.radius, &data))
		{
			if (intersect_data.size() == 0 || data[0].length < intersect_data[0].length)
			{
				intersect_data.swap(data);
				out_collision = object;
			}
			data.clear();
		}
	}
	return intersect_data.size() > 0;
}

bool collision_world::capsule_cast(const capsule_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision)
{
	//	移動範囲のAABBで候補を絞り込む
	const collision_octree::aabb cast_bounds = calculate_cast_bounds(desc.start, desc.end, desc.radius + desc.length * 0.5);

	collision::intersect_data data;
	for (auto& object : collision_objects)
	{
		if (!object->is_flag(collision_object::flags::active))
			continue;

		//	応答チェック
		if (collision_filter::get_response(*desc.filter, *object->get_filter()) != collision_response::block)
			continue;

		//	AABBが重ならなければ詳細判定しない
		if (!overlaps_cast_bounds(*object, cast_bounds))
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

		if (!find && object->get_bounding_object()->intersect_capsule_cast(
						math::double3(desc.start), math::double3(desc.end), desc.radius, desc.length, math::double4(desc.rotation), &data))
		{
			if (intersect_data.size() == 0 || data[0].length < intersect_data[0].length)
			{
				intersect_data.swap(data);
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
		auto it = std::find(collision_objects.begin(), collision_objects.end(), object);
		if (it != collision_objects.end())
		{
			collision_objects.erase(it);
		}
	}
	system_utility::std_vector_swap_clear(collision_objects_remove);
}
