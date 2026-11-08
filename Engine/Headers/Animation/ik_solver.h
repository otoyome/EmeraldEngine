#pragma once

#include "Animation/ik_resource.h"
#include "Model/model_instance.h"

class ik_solver
{
	using axis = ik_resource::axis;
	using constraint_mode = ik_resource::constraint_mode;
	using solve_mode = ik_resource::solve_mode;
	using behavior_data = ik_resource::behavior_data;

	enum class constraint_plane_lock
	{
		free,
		x,
		y,
		z,
	};

public:
	struct node_data
	{
		size_t source_node_index = -1;
		constraint_mode mode = constraint_mode::free;
		constraint_plane_lock plane_lock = constraint_plane_lock::free;
		float limit_angles[6] = {};
		axis use_axis = axis::z;

		bool reset_previous_rotation = true;
		float smoothness = 0.3f;
		math::float4 previous_rotation = { 0, 0, 0, 1 };	//	時間フィルタリング
		math::float3 prev_angle = {0, 0, 0};
		float plane_mode_angle = 0;	//	ccd_pmx用
	};
	struct solve_data
	{
		solve_mode mode;

		std::vector<size_t> node_list;
		size_t target_node_index;

		UINT iteration;
		float constraint_angle;

		bool use_pole_node_offset;
		math::float3 pole_offset;

		math::float3 aim_axis;
		math::float3 aim_offset;
	};

public:
	const ik_resource_ptr get_resource() const;
	const std::vector<behavior_data>& get_behavior_data_list() const;

	const std::vector<solve_data>& get_solve_data() const;
	size_t search_index_by_name(std::string_view data_name);

public:
	ik_solver(model_instance_ptr instance, ik_resource_ptr resource);
	~ik_solver();

	void solve(size_t index, math::float3 ik_target_position);
	void solves(std::vector<math::float3> ik_target_positions);

private:
	void solve_aim(solve_data& solve_data, math::float3 ik_target_position);
	void solve_limb(solve_data& solve_data, math::float3 ik_target_position);
	void solve_ccd(solve_data& solve_data, math::float3 ik_target_position);
	void solve_ccd_pmx(solve_data& solve_data, math::float3 ik_target_position);
	void solve_ccd_pmx_plane(int iteration, solve_data& solve_data, node_data& link_data, const math::vector3f& ik_target_position, constraint_plane_lock axis);

	void build();

private:
	model_instance_ptr instance;
	ik_resource_ptr resource;

	std::vector<node_data> nodes;
	std::vector<solve_data> solve_datas;
};

using ik_solver_ptr = std::shared_ptr<ik_solver>;
