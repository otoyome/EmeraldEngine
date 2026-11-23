#pragma once

#include <mutex>

#include "Collision/collision.h"
#include "collision_object.h"
#include "collision_octree.h"
#include "Core/parallel_for_executor.h"

using collision_object_ptr = std::shared_ptr<collision_object>;

//	è’ìÀîªíËÉèÅ[ÉãÉh
class engine_context;
class collision_world
{
public:
	struct ray_cast_descs
	{
		math::double3 start;
		math::double3 end;
		collision_filter_ptr filter;
		std::vector<collision_object_ptr> exclude_objects;
	};

	struct sphere_cast_descs : public ray_cast_descs
	{
		float radius;
	};

	struct capsule_cast_descs : public ray_cast_descs
	{
		float radius = 0.0f;
		float length = 0.0f;
		math::float4 rotation = { 0, 0, 0, 1 };
	};

public:
	collision_world();
	~collision_world();

public:
	void step(float elapsed_time, int max_substep = 10, float fixed_time_stamp = system_units::convert_fps_to_sec(0.1f));

	void finalize();

#if defined(USE_IMGUI)
	void debug_primitive_draw(engine_context& engine);
	void imgui_render();
#endif // defined(USE_IMGUI)

public:
	bool register_object(collision_object_ptr object);
	bool unregister_object(collision_object_ptr object);

	void apply_pending_objects();

private:
	void inner_step_simulation(float elapsed_time);
	void add_objects();
	void remove_objects();

public:
	bool ray_cast(const ray_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision);
	bool sphere_cast(const sphere_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision);
	bool capsule_cast(const capsule_cast_descs& desc, collision::intersect_data& intersect_data, collision_object_ptr& out_collision);

private:
	std::mutex mutex;
	std::vector<collision_object_ptr> collision_objects;
	std::vector<collision_object_ptr> collision_objects_add;
	std::vector<collision_object_ptr> collision_objects_remove;
	collision_octree broadphase;
	core::parallel_for_executor narrow_phase_executor;

private:
	std::vector<std::pair<collision_object*, collision_octree::aabb>> broadphase_entries;
	std::vector<collision_octree::object_pair> candidate_pairs;
	std::vector<collision::intersect_data> narrow_contacts;
	std::vector<char> narrow_hits;
	collision::intersect_data push_scratch1;
	collision::intersect_data push_scratch2;
};
