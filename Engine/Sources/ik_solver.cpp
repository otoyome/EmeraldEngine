#include "ik_solver.h"

namespace
{
	math::quaternionf clamp_twist(const math::quaternionf& Twist,
								  const math::vector3f& TwistAxis,
								  float min_angle,
								  float max_angle)
	{
		//	軸回転抽出
		const math::float4 twist = Twist.to_float4();

		//	対数クォータニオンに変換
		const math::exponential_quaternionf log_twist{ math::quaternionf(twist) };
		math::float3 log_quaternion(log_twist.x, log_twist.y, log_twist.z);
		math::vector3f LogQuaternion(log_quaternion);

		//	twist 成分だけを抽出
		math::vector3f Axis = TwistAxis.normalized();
		float projection = math::vector3f::dot(LogQuaternion, Axis);
		float angle = math::angle::normalize(projection);
		angle = std::min(std::max(angle, min_angle), max_angle);

		//	クォータニオンに変換
		math::float3 new_log_quaternion;
		new_log_quaternion = (Axis * angle).to_float3();
		return math::exponential_quaternionf(new_log_quaternion.x, new_log_quaternion.y, new_log_quaternion.z).to_quaternion();
	}

	math::quaternionf clamp_swing(const math::quaternionf& Swing, float max_angle)
	{
		math::vector3f Axis;
		float angle;
		Swing.axis_angle(Axis, angle);
		if (angle <= max_angle)
			return Swing;

		return math::quaternionf::rotation_axis(Axis, max_angle).normalized();
	}

	bool restrict_rotation(math::float4& rotation, float* min, float* max)
	{
		bool lock = false;
		float* rot = &rotation.x;
		for (int kk = 0; kk < 3; ++kk)
		{
			if (rot[kk] < min[kk])
			{
				rot[kk] = min[kk];
				lock = true;
			}
			if (rot[kk] > max[kk])
			{
				rot[kk] = max[kk];
				lock = true;
			}
		}
		return	lock;
	}

	float difference_angle(float a, float b)
	{
		float diff = math::angle::normalize_positive(a) - math::angle::normalize_positive(b);
		if (diff > math::constants<float>::pi)
		{
			return diff - math::constants<float>::pi2;
		}
		else if (diff < -math::constants<float>::pi)
		{
			return diff + math::constants<float>::pi2;
		}
		return diff;
	}
	math::float3 decompose(const math::matrixf& m, const math::float3& before)
	{
		math::float3 r;
		float sy = -m.storage[0][2];
		const float e = 1.0e-6f;

		if ((1.0f - std::abs(sy)) < e)
		{
			r.y = std::asin(sy);
			float sx = std::sin(before.x);
			float sz = std::sin(before.z);
			if (std::abs(sx) < std::abs(sz))
			{
				float cx = std::cos(before.x);
				if (cx > 0)
				{
					r.x = 0;
					r.z = std::asin(-m.storage[1][0]);
				}
				else
				{
					r.x = math::constants<float>::pi;
					r.z = std::asin(m.storage[1][0]);
				}
			}
			else
			{
				float cz = std::cos(before.z);
				if (cz > 0)
				{
					r.z = 0;
					r.x = std::asin(-m.storage[2][1]);
				}
				else
				{
					r.z = math::constants<float>::pi;
					r.x = std::asin(m.storage[2][1]);
				}
			}
		}
		else
		{
			r.x = std::atan2(m.storage[1][2], m.storage[2][2]);
			r.y = std::asin(-m.storage[0][2]);
			r.z = std::atan2(m.storage[0][1], m.storage[0][0]);
		}

		const float pi = math::constants<float>::pi;
		math::float3 tests[] =
		{
			{ r.x + pi, pi - r.y, r.z + pi },
			{ r.x + pi, pi - r.y, r.z - pi },
			{ r.x + pi, -pi - r.y, r.z + pi },
			{ r.x + pi, -pi - r.y, r.z - pi },
			{ r.x - pi, pi - r.y, r.z + pi },
			{ r.x - pi, pi - r.y, r.z - pi },
			{ r.x - pi, -pi - r.y, r.z + pi },
			{ r.x - pi, -pi - r.y, r.z - pi },
		};

		float errX = std::abs(difference_angle(r.x, before.x));
		float errY = std::abs(difference_angle(r.y, before.y));
		float errZ = std::abs(difference_angle(r.z, before.z));
		float minErr = errX + errY + errZ;
		for (const auto test : tests)
		{
			float err = std::abs(difference_angle(test.x, before.x))
				+ std::abs(difference_angle(test.y, before.y))
				+ std::abs(difference_angle(test.z, before.z));
			if (err < minErr)
			{
				minErr = err;
				r = test;
			}
		}
		return r;
	}
	math::float3 decompose(const math::quaternionf& q, const math::float3& before)
	{
		return decompose(math::matrixf::rotation_quaternion(q), before);
	}

	math::float3 subtract_angle(const math::float3& current_angle, const math::float3& prev_angle)
	{
		math::float3 angle;
		angle.x = math::angle::normalize_positive(current_angle.x - prev_angle.x);
		angle.y = math::angle::normalize_positive(current_angle.y - prev_angle.y);
		angle.z = math::angle::normalize_positive(current_angle.z - prev_angle.z);
		return	angle;
	}
	math::float3 additional_angle(const math::float3& current_angle, const math::float3& prev_angle)
	{
		math::float3 angle;
		angle.x = math::angle::normalize_positive(current_angle.x + prev_angle.x);
		angle.y = math::angle::normalize_positive(current_angle.y + prev_angle.y);
		angle.z = math::angle::normalize_positive(current_angle.z + prev_angle.z);
		return	angle;
	}
	math::float3 clamp_angle(const math::float3& current_angle, const math::float3& min, const math::float3& max)
	{
		math::float3 angle;
		angle.x = std::clamp(current_angle.x, min.x, max.x);
		angle.y = std::clamp(current_angle.y, min.y, max.y);
		angle.z = std::clamp(current_angle.z, min.z, max.z);
		return	angle;
	}
}

ik_solver::ik_solver(model_instance_ptr instance, ik_resource_ptr resource)
: instance(instance), resource(resource)
{
	build();
}

ik_solver::~ik_solver()
{
}

const ik_resource_ptr ik_solver::get_resource() const
{
	return resource;
}

const std::vector<ik_solver::behavior_data>& ik_solver::get_behavior_data_list() const
{
	return resource->get_behavior_data_list();
}

const std::vector<ik_solver::solve_data>& ik_solver::get_solve_data() const
{
	return solve_datas;
}

size_t ik_solver::search_index_by_name(std::string_view data_name)
{
	return resource->search_index_by_name(data_name);
}

void ik_solver::solve(size_t index, math::float3 ik_target_position)
{
	if (index >= solve_datas.size())
		return;

	auto& solve_data = solve_datas[index];
	switch (solve_data.mode)
	{
		case solve_mode::limb:
		{
			solve_limb(solve_data, ik_target_position);
			break;
		}
		case solve_mode::ccd:
		{
			solve_ccd(solve_data, ik_target_position);
			break;
		}
		case solve_mode::ccd_pmx:
		{
			solve_ccd_pmx(solve_data, ik_target_position);
			break;
		}
		case solve_mode::aim:
		{
			solve_aim(solve_data, ik_target_position);
			break;
		}
	}

	//	過去の計算結果と補間
	{
		auto& instance_node_list = instance->get_nodes();
		auto WorldTramsform = math::matrixf(instance->get_world_transform());

		for (size_t link_node_index : solve_data.node_list)
		{
			auto& link_data = nodes[link_node_index];
			if (link_data.smoothness >= 0.99999f)
				continue;

			auto& link_node = instance_node_list[link_data.source_node_index];

			//	適応
			math::vector3f S, T;
			math::quaternionf R;
			math::matrixf LocalTransform(link_node.local_transform);
			LocalTransform.decompose(S, R, T);

			//	安定化させるために過去の計算結果を利用
			if (!link_data.reset_previous_rotation)
			{
				math::quaternionf PreviousRotation(link_data.previous_rotation);
				R = math::slerp(PreviousRotation, R, link_data.smoothness);
			}
			link_data.reset_previous_rotation = false;
			link_data.previous_rotation = R.to_float4();
			LocalTransform = math::matrixf::transform(S, R, T);
			link_node.local_transform = LocalTransform.to_float4x4();
		}

		//	グローバル行列再計算
		instance->update_transform(WorldTramsform);
	}
}

void ik_solver::solves(std::vector<math::float3> ik_target_positions)
{
	for (size_t index = 0; index < solve_datas.size(); ++index)
	{
		solve(index, ik_target_positions[index]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_aim(solve_data& solve_data, math::float3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();
	auto& model_resource_node_list = instance->get_resource()->get_nodes();

	if (solve_data.node_list.empty() || solve_data.node_list[0] == ik_resource::invalid_id)
		return;

	auto& link_data = nodes[solve_data.node_list[0]];
	auto& aim_node = instance_node_list[link_data.source_node_index];

	auto AimAxis = math::vector3f(solve_data.aim_axis);
	auto AimOffset = math::vector3f(solve_data.aim_offset);

	auto IKTargetPosition = math::vector3f(ik_target_position);
	auto WorldTramsform = math::matrixf(instance->get_world_transform());
	auto InverseWorldTramsform = WorldTramsform.inverted();
	IKTargetPosition = math::vector3f::transform_coord(IKTargetPosition, InverseWorldTramsform);
	instance->update_transform(WorldTramsform);

	auto InverseAimGlobalTransform = math::matrixf(aim_node.global_transform).inverted();
	IKTargetPosition = math::vector3f::transform_coord(IKTargetPosition, InverseAimGlobalTransform);

	IKTargetPosition = IKTargetPosition + AimOffset;

	if (math::vector3f::dot(IKTargetPosition, IKTargetPosition) <= 1e-6f)
		return;

	auto IKTargetDirection = IKTargetPosition.normalized();
	auto RotationAngle = acosf(math::vector3f::dot(AimAxis, IKTargetDirection));
	auto RotationAxis = math::vector3f::cross(AimAxis, IKTargetDirection);
	auto AdjustQuaternion = math::quaternionf::rotation_axis(RotationAxis, RotationAngle);

	math::vector3f S, T;
	math::quaternionf R;
	auto LocalTransform = math::matrixf(aim_node.local_transform);
	LocalTransform.decompose(S, R, T);
	auto NewLocalRotation = AdjustQuaternion * R;

	//	角度制限
	if (link_data.mode != constraint_mode::free)
	{
		bool reverse_flag = (link_data.use_axis == axis::reverse_x || link_data.use_axis == axis::reverse_y || link_data.use_axis == axis::reverse_z);
		//	軸算出
		auto get_rotation_axis = [&]()
		{
			auto& model_resource_node_list = instance->get_resource()->get_nodes();
			auto& resource_node = model_resource_node_list[link_data.source_node_index];
			math::float3 pole_axis = {0, 0, 0};
			switch (link_data.use_axis)
			{
				case axis::x:			pole_axis = math::quaternionf(resource_node.rotation).right().to_float3();	break;
				case axis::reverse_x:	pole_axis = math::quaternionf(resource_node.rotation).right().to_float3();	break;
				case axis::y:			pole_axis = math::quaternionf(resource_node.rotation).up().to_float3();		break;
				case axis::reverse_y:	pole_axis = math::quaternionf(resource_node.rotation).up().to_float3();		break;
				case axis::z:			pole_axis = math::quaternionf(resource_node.rotation).front().to_float3();	break;
				case axis::reverse_z:	pole_axis = math::quaternionf(resource_node.rotation).front().to_float3();	break;
			}
			math::vector3f PoleAxis(pole_axis);
			return (PoleAxis * (reverse_flag ? -1.0f : +1.0f)).normalized();
		};

		switch (link_data.mode)
		{
			case constraint_mode::hinge:
			{
				float min_hinge = link_data.limit_angles[0];
				float max_hinge = link_data.limit_angles[1];

				const math::vector3f HingeAxis = get_rotation_axis();
				const math::quaternionf Twist = NewLocalRotation.twist(HingeAxis);
				NewLocalRotation = clamp_twist(Twist, HingeAxis, min_hinge, max_hinge);
				break;
			}
			case constraint_mode::ball_joint:
			{
				float min_twist = link_data.limit_angles[0];
				float max_twist = link_data.limit_angles[1];
				float max_swing = link_data.limit_angles[2];

				const math::vector3f TwistAxis = get_rotation_axis();
				math::quaternionf Twist, Swing;
				NewLocalRotation.decompose_twist_swing(TwistAxis, Twist, Swing);
				Twist = clamp_twist(Twist, TwistAxis, min_twist, max_twist);
				Swing = clamp_swing(Swing, max_swing);
				NewLocalRotation = (Swing * Twist).normalized();
				break;
			}
			case constraint_mode::euler:
			{
				//	オイラー角による拘束
				math::float4 limit_rotation;
				limit_rotation = NewLocalRotation.to_float4();
				auto euler = math::rotatorf(math::quaternionf(limit_rotation));
				euler.x = math::clamp(euler.x, link_data.limit_angles[0], link_data.limit_angles[3]);
				euler.y = math::clamp(euler.y, link_data.limit_angles[1], link_data.limit_angles[4]);
				euler.z = math::clamp(euler.z, link_data.limit_angles[2], link_data.limit_angles[5]);
				NewLocalRotation = math::quaternionf::rotation_xyz(euler.x, euler.y, euler.z);
				NewLocalRotation = NewLocalRotation.normalized();
				break;
			}
		}
	}

	LocalTransform = math::matrixf::transform(S, NewLocalRotation, T);
	aim_node.local_transform = LocalTransform.to_float4x4();
	instance->update_transform(WorldTramsform);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_limb(solve_data& solve_data, math::float3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();

	if (!(solve_data.node_list.size() == 2 || solve_data.node_list.size() == 3) ||
		 solve_data.target_node_index == -1)
		return;

	auto IKTargetPosition = math::vector3f(ik_target_position);
	auto WorldTramsform = math::matrixf(instance->get_world_transform());
	auto InverseWorldTramsform = WorldTramsform.inverted();
	IKTargetPosition = math::vector3f::transform_coord(IKTargetPosition, InverseWorldTramsform);
	instance->update_transform(WorldTramsform);

	auto& root_node = instance_node_list[nodes[solve_data.node_list[0]].source_node_index];
	auto& middle_node = instance_node_list[nodes[solve_data.node_list[1]].source_node_index];
	auto& end_node = instance_node_list[nodes[solve_data.target_node_index].source_node_index];

	{
		math::matrixf RootGlobalTransform(root_node.global_transform);
		math::matrixf MiddleGlobalTransform(middle_node.global_transform);
		math::matrixf TipGlobalTransform(end_node.global_transform);

		//	計算用座標/ベクトル算出
		math::vector3f RootGlobalPosition = RootGlobalTransform.to_vector3(3);
		math::vector3f MiddleGlobalPosition = MiddleGlobalTransform.to_vector3(3);
		math::vector3f TipGlobalPosition = TipGlobalTransform.to_vector3(3);

		math::vector3f RootMiddleVec = MiddleGlobalPosition - RootGlobalPosition;
		math::vector3f RootTargetVec = IKTargetPosition - RootGlobalPosition;
		math::vector3f MiddleTipVec = TipGlobalPosition - MiddleGlobalPosition;

		float root_middle_length = RootMiddleVec.length();
		float root_target_length = RootTargetVec.length();
		float middle_tip_length = MiddleTipVec.length();

		auto calculate_rotate_bone = [&](model_instance::node& node, const math::vector3f& Direction1, const math::vector3f& Direction2)
		{
			math::vector3f GlobalAxis = math::vector3f::cross(Direction1, Direction2);
			if (GlobalAxis == math::vector3f::zero())
				return;
			GlobalAxis = GlobalAxis.normalized();
			
			math::matrixf ParentGlobalTransform;
			if (node.parent_index != model_instance::invalid_id)
			{
				auto& parent_node = instance_node_list[node.parent_index];
				ParentGlobalTransform = math::matrixf(parent_node.global_transform);
			}
			math::matrixf InverseParentGlobalTransform = ParentGlobalTransform.inverted();
			math::vector3f LocalAxis = math::vector3f::transform_normal(GlobalAxis, InverseParentGlobalTransform);
			LocalAxis = LocalAxis.normalized();

			float Dot = math::vector3f::dot(Direction1, Direction2);
			float dot = Dot;
			float angle = acosf(std::clamp(dot, -1.0f, 1.0f));

			math::vector3f S, T;
			math::quaternionf R;
			math::matrixf LocalTransform(node.local_transform);
			math::quaternionf LocalRotationAxis = math::quaternionf::rotation_axis(LocalAxis, angle);
			LocalTransform.decompose(S, R, T);
			R = R * LocalRotationAxis;
			LocalTransform = math::matrixf::transform(S, R, T);
			node.local_transform = LocalTransform.to_float4x4();
		};

		math::vector3f RootTargetDirection = RootTargetVec.normalized();
		math::vector3f RootMiddleDirection = RootMiddleVec.normalized();

		calculate_rotate_bone(root_node, RootMiddleDirection, RootTargetDirection);

		//	曲がる場合
		if (root_target_length < root_middle_length + middle_tip_length)
		{
			// ヘロンの公式で三角形の面積を求める
			float s = (root_middle_length + middle_tip_length + root_target_length) / 2;
			float square = sqrtf(s * (s - root_middle_length) * (s - middle_tip_length) * (s - root_target_length));

			float root_middle_height = (2 * square) / root_middle_length;
			float angle = asinf(root_middle_height / root_target_length);
			if (angle > FLT_EPSILON)
			{
				//	ポール座標
				math::vector3f PoleGlobalPosition = MiddleGlobalPosition;
				if (solve_data.node_list.size() == 3)
				{
					if (solve_data.node_list[2] != model_resource::invalid_id &&
						!solve_data.use_pole_node_offset)
					{
						auto& pole_node = instance_node_list[nodes[solve_data.node_list[2]].source_node_index];
						math::matrixf PoleGlobalTransform(pole_node.global_transform);
						PoleGlobalPosition = PoleGlobalTransform.to_vector3(3);
					}
					else
					{
						math::vector3f PoleOffset(solve_data.pole_offset);
						PoleGlobalPosition = math::vector3f::transform_coord(PoleOffset, MiddleGlobalTransform);
					}
				}
				math::vector3f RootPoleVec = PoleGlobalPosition - RootGlobalPosition;
				math::vector3f RootPoleDirection = RootPoleVec.normalized();
				math::matrixf RootParentGlobalTransform;
				if (root_node.parent_index != model_instance::invalid_id)
				{
					auto& parent_node = instance_node_list[root_node.parent_index];
					RootParentGlobalTransform = math::matrixf(parent_node.global_transform);
				}
				math::matrixf InverseRootParentGlobalTransform = RootParentGlobalTransform.inverted();
				math::vector3f PoleGlobalAxis = math::vector3f::cross(RootMiddleDirection, RootPoleDirection);
				math::vector3f PoleLocalAxis = math::vector3f::transform_normal(PoleGlobalAxis, InverseRootParentGlobalTransform);
				PoleLocalAxis = PoleLocalAxis.normalized();
				if (math::vector3f::dot(PoleLocalAxis, PoleLocalAxis) < FLT_EPSILON)
				{
					PoleLocalAxis = math::vector3f(0, 0, 1);
				}

				math::quaternionf LocalRotationAxis = math::quaternionf::rotation_axis(PoleLocalAxis, angle);
				math::vector3f S, T;
				math::quaternionf R;
				math::matrixf LocalTransform(root_node.local_transform);
				LocalTransform.decompose(S, R, T);
				R = R * LocalRotationAxis;
				LocalTransform = math::matrixf::transform(S, R, T);
				root_node.local_transform = LocalTransform.to_float4x4();
			}
		}

		instance->update_transform(WorldTramsform);

		MiddleGlobalTransform = math::matrixf(middle_node.global_transform);
		MiddleGlobalPosition = MiddleGlobalTransform.to_vector3(3);
		TipGlobalTransform = math::matrixf(end_node.global_transform);
		TipGlobalPosition = TipGlobalTransform.to_vector3(3);

		MiddleTipVec = TipGlobalPosition - MiddleGlobalPosition;
		math::vector3f MiddleTipDirection = MiddleTipVec.normalized();
		math::vector3f MiddleTargetVec = IKTargetPosition - MiddleGlobalPosition;
		math::vector3f MiddleTargetDirection = MiddleTargetVec.normalized();

		calculate_rotate_bone(middle_node, MiddleTipDirection, MiddleTargetDirection);
		instance->update_transform(WorldTramsform);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_ccd(solve_data& solve_data, math::float3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();
	auto& model_resource_node_list = instance->get_resource()->get_nodes();

	auto IKTargetPosition = math::vector3f(ik_target_position);
	auto WorldTramsform = math::matrixf(instance->get_world_transform());
	auto InverseWorldTramsform = WorldTramsform.inverted();
	IKTargetPosition = math::vector3f::transform_coord(IKTargetPosition, InverseWorldTramsform);

	//	CCD-IK
	for (UINT iteration = 0; iteration < solve_data.iteration; ++iteration)
	{
		if (solve_data.target_node_index == model_resource::invalid_id)
			continue;

		auto& target_node_data = nodes[solve_data.target_node_index];

		//	対象ノードを取得
		auto& target_node = instance_node_list[target_node_data.source_node_index];
		auto TargetPosition = math::vector3f(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43);

		//	対象ノードがIK対象座標にたどり着けば終了
		math::vector3f vector = IKTargetPosition - TargetPosition;
		float distance = vector.square_length();
		if (distance <= 0.001f)
			break;

		bool remain_modify = false;
		math::quaternionf RemainRotation(0, 0, 0, 1);
		for (size_t link_node_index : solve_data.node_list)
		{
			auto& link_data = nodes[link_node_index];
			auto& link_node = instance_node_list[link_data.source_node_index];
			auto& resource_node = model_resource_node_list[link_data.source_node_index];

			math::matrixf InverseLinkGlobalMatrix = math::matrixf(link_node.global_transform).inverted();

			math::vector3f EffectorGlobalPosition(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43);
			math::vector3f EffectorLocalPosition = math::vector3f::transform_coord(EffectorGlobalPosition, InverseLinkGlobalMatrix);
			math::vector3f TargetLocalPosition = math::vector3f::transform_coord(IKTargetPosition, InverseLinkGlobalMatrix);

			math::vector3f EffectorLocalDirection = EffectorLocalPosition.normalized();
			math::vector3f TargetLocalDirection = TargetLocalPosition.normalized();

			float dot = math::vector3f::dot(EffectorLocalDirection, TargetLocalDirection);
			float angle = acosf(dot);
			if (isnan(angle))
				continue;

			math::vector3f axis = (math::vector3f::cross(EffectorLocalDirection, TargetLocalDirection)).normalized();
			if (math::vector3f::dot(axis, axis) <= 0.001f)
				continue;

			angle = std::min(angle, solve_data.constraint_angle);

			math::quaternionf AdditionalRotation = math::quaternionf::rotation_axis(axis, angle);
			//	余剰回転適応
			if (remain_modify)
			{
				remain_modify = false;
				AdditionalRotation = RemainRotation * AdditionalRotation;
				AdditionalRotation = AdditionalRotation.normalized();
			}

			//	適応
			math::vector3f S, T;
			math::quaternionf OriginalLocalRotation;
			math::matrixf LocalTransform(link_node.local_transform);
			LocalTransform.decompose(S, OriginalLocalRotation, T);
			math::quaternionf NewLocalRotation = OriginalLocalRotation;
			NewLocalRotation = (AdditionalRotation * NewLocalRotation).normalized();

			//	角度制限
			if (link_data.mode != constraint_mode::free)
			{
				bool reverse_flag = (link_data.use_axis == axis::reverse_x || link_data.use_axis == axis::reverse_y || link_data.use_axis == axis::reverse_z);
				//	軸算出
				auto get_rotation_axis = [&]()
				{
					math::float3 pole_axis = {0, 0, 0};
					switch (link_data.use_axis)
					{
						case axis::x:			pole_axis = math::quaternionf(resource_node.rotation).right().to_float3();	break;
						case axis::reverse_x:	pole_axis = math::quaternionf(resource_node.rotation).right().to_float3();	break;
						case axis::y:			pole_axis = math::quaternionf(resource_node.rotation).up().to_float3();		break;
						case axis::reverse_y:	pole_axis = math::quaternionf(resource_node.rotation).up().to_float3();		break;
						case axis::z:			pole_axis = math::quaternionf(resource_node.rotation).front().to_float3();	break;
						case axis::reverse_z:	pole_axis = math::quaternionf(resource_node.rotation).front().to_float3();	break;
					}
					math::vector3f PoleAxis(pole_axis);
					return (PoleAxis * (reverse_flag ? -1.0f : +1.0f)).normalized();
				};

				switch (link_data.mode)
				{
					case constraint_mode::hinge:
					{
						float min_hinge = link_data.limit_angles[0];
						float max_hinge = link_data.limit_angles[1];

						const math::vector3f HingeAxis = get_rotation_axis();
						const math::quaternionf Twist = NewLocalRotation.twist(HingeAxis);
						NewLocalRotation = clamp_twist(Twist, HingeAxis, min_hinge, max_hinge);
						break;
					}
					case constraint_mode::ball_joint:
					{
						float min_twist = link_data.limit_angles[0];
						float max_twist = link_data.limit_angles[1];
						float max_swing = link_data.limit_angles[2];

						const math::vector3f TwistAxis = get_rotation_axis();
						math::quaternionf Twist, Swing;
						NewLocalRotation.decompose_twist_swing(TwistAxis, Twist, Swing);
						Twist = clamp_twist(Twist, TwistAxis, min_twist, max_twist);
						Swing = clamp_swing(Swing, max_swing);
						NewLocalRotation = (Swing * Twist).normalized();
						break;
					}
					case constraint_mode::euler:
					{
						//	オイラー角による拘束
						math::float4 limit_rotation;
						limit_rotation = NewLocalRotation.to_float4();
						remain_modify = restrict_rotation(limit_rotation,
															link_data.limit_angles + 0,
															link_data.limit_angles + 3);
						if (remain_modify)
						{
							math::quaternionf NormalizedLimitRotation(limit_rotation);
							NormalizedLimitRotation = NormalizedLimitRotation.normalized();
							//	余剰回転を算出
							math::quaternionf InverseNormalizedLimitRotation = NormalizedLimitRotation.inverted();
							RemainRotation = NewLocalRotation * InverseNormalizedLimitRotation;
							math::float4 work_rot;
							work_rot = RemainRotation.to_float4();
							if (link_data.limit_angles[0] == link_data.limit_angles[3])
								work_rot.x = link_data.limit_angles[0];
							if (link_data.limit_angles[1] == link_data.limit_angles[4])
								work_rot.y = link_data.limit_angles[1];
							if (link_data.limit_angles[2] == link_data.limit_angles[5])
								work_rot.z = link_data.limit_angles[2];
							RemainRotation = math::quaternionf(work_rot).normalized();
							//	制限された回転を適応
							NewLocalRotation = NormalizedLimitRotation;
						}
						break;
					}
				}
			}

			LocalTransform = math::matrixf::transform(S, NewLocalRotation, T);
			link_node.local_transform = LocalTransform.to_float4x4();

			//	グローバル行列再計算
			instance->update_transform(WorldTramsform);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_ccd_pmx(solve_data& solve_data, math::float3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();

	auto IKTargetPosition = math::vector3f(ik_target_position);
	auto WorldTramsform = math::matrixf(instance->get_world_transform());
	auto InverseWorldTramsform = WorldTramsform.inverted();
	IKTargetPosition = math::vector3f::transform_coord(IKTargetPosition, InverseWorldTramsform);

	//	初期化
	for (size_t link_node_index : solve_data.node_list)
	{
		auto& link_data = nodes[link_node_index];
		link_data.plane_mode_angle = 0;
		link_data.prev_angle = {0, 0, 0};
	}

	//	CCD-IK
	for (UINT iteration = 0; iteration < solve_data.iteration; ++iteration)
	{
		if (solve_data.target_node_index == model_resource::invalid_id)
			continue;

		auto& target_node_data = nodes[solve_data.target_node_index];

		//	対象ノードを取得
		auto& target_node = instance_node_list[target_node_data.source_node_index];
		auto TargetPosition = math::vector3f(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43);

		//	対象ノードがIK対象座標にたどり着けば終了
		math::vector3f vector = IKTargetPosition - TargetPosition;
		float distance = vector.square_length();
		if (distance <= 0.001f)
			break;

		for (size_t link_node_index : solve_data.node_list)
		{
			auto& link_data = nodes[link_node_index];
			//	平面の処理に変更
			if (link_data.plane_lock != constraint_plane_lock::free)
			{
				solve_ccd_pmx_plane(iteration, solve_data, link_data, IKTargetPosition, link_data.plane_lock);
			}
			//	平面処理以外
			else
			{
				auto& link_node = instance_node_list[link_data.source_node_index];

				math::matrixf InverseLinkGlobalMatrix = math::matrixf(link_node.global_transform).inverted();

				math::vector3f EffectorGlobalPosition(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43);
				math::vector3f EffectorLocalPosition = math::vector3f::transform_coord(EffectorGlobalPosition, InverseLinkGlobalMatrix);
				math::vector3f TargetLocalPosition = math::vector3f::transform_coord(IKTargetPosition, InverseLinkGlobalMatrix);

				math::vector3f EffectorLocalDirection = EffectorLocalPosition.normalized();
				math::vector3f TargetLocalDirection = TargetLocalPosition.normalized();

				float dot = math::vector3f::dot(EffectorLocalDirection, TargetLocalDirection);
				float angle = acosf(dot);
				if (isnan(angle))
					continue;

				math::vector3f axis = (math::vector3f::cross(EffectorLocalDirection, TargetLocalDirection)).normalized();
				if (math::vector3f::dot(axis, axis) <= 0.001f)
					continue;

				angle = std::min(angle, solve_data.constraint_angle);

				math::quaternionf AdditionalRotation = math::quaternionf::rotation_axis(axis, angle);

				//	適応
				math::vector3f S, T;
				math::quaternionf OriginalLocalRotation;
				math::matrixf LocalTransform(link_node.local_transform);
				LocalTransform.decompose(S, OriginalLocalRotation, T);
				math::quaternionf NewLocalRotation = (AdditionalRotation * OriginalLocalRotation).normalized();

				//	角度制限
				if (link_data.plane_lock != constraint_plane_lock::free)
				{
					math::float3 angle = decompose(NewLocalRotation, link_data.prev_angle);

					math::float3 limit_angle = clamp_angle(angle, *reinterpret_cast<math::float3*>(&link_data.limit_angles[0]), *reinterpret_cast<math::float3*>(&link_data.limit_angles[3]));
					limit_angle = clamp_angle(subtract_angle(limit_angle, link_data.prev_angle),
											{-solve_data.constraint_angle, -solve_data.constraint_angle, -solve_data.constraint_angle},
											{+solve_data.constraint_angle, +solve_data.constraint_angle, +solve_data.constraint_angle});
					limit_angle = additional_angle(limit_angle, link_data.prev_angle);

					NewLocalRotation = math::quaternionf::rotation_xyz(limit_angle.x, limit_angle.y, limit_angle.z);
					link_data.prev_angle = limit_angle;
				}

				//	影響度を適応
				LocalTransform = math::matrixf::transform(S, NewLocalRotation, T);
				link_node.local_transform = LocalTransform.to_float4x4();
			}
			//	グローバル行列再計算
			instance->update_transform(WorldTramsform);
		}
	}
}

void ik_solver::solve_ccd_pmx_plane(int iteration, solve_data& solve_data, node_data& link_data, const math::vector3f& ik_target_position, constraint_plane_lock axis)
{
	if (axis == constraint_plane_lock::free)
		return;

	math::vector3f rotate_axis;
	float limit_min_angle, limit_max_angle;

	switch (axis)
	{
		case constraint_plane_lock::x:
		{
			limit_min_angle = link_data.limit_angles[0];
			limit_max_angle = link_data.limit_angles[3];
			rotate_axis = math::vector3f(1.0f, 0.0f, 0.0f);
			break;
		}
		case constraint_plane_lock::y:
		{
			limit_min_angle = link_data.limit_angles[1];
			limit_max_angle = link_data.limit_angles[4];
			rotate_axis = math::vector3f(0.0f, 1.0f, 0.0f);
			break;
		}
		case constraint_plane_lock::z:
		{
			limit_min_angle = link_data.limit_angles[2];
			limit_max_angle = link_data.limit_angles[5];
			rotate_axis = math::vector3f(0.0f, 0.0f, 1.0f);
			break;
		}
	}
	//	入れ替え
	if (limit_max_angle < limit_min_angle)
	{
		auto temp = limit_min_angle;
		limit_min_angle = limit_max_angle;
		limit_max_angle = temp;
	}

	auto& instance_node_list = instance->get_nodes();

	auto& target_node_data = nodes[solve_data.target_node_index];

	auto& target_node = instance_node_list[target_node_data.source_node_index];
	auto& link_node = instance_node_list[link_data.source_node_index];

	math::vector3f target_position(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43);

	math::matrixf inverse_link_global_matrix = math::matrixf(link_node.global_transform).inverted();

	math::vector3f effector_global_position(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43);
	math::vector3f effector_local_position = math::vector3f::transform_coord(effector_global_position, inverse_link_global_matrix);
	math::vector3f target_local_position = math::vector3f::transform_coord(ik_target_position, inverse_link_global_matrix);

	math::vector3f effector_local_direction = effector_local_position.normalized();
	math::vector3f target_local_direction = target_local_position.normalized();

	float dot = math::vector3f::dot(effector_local_direction, target_local_direction);
	float angle = std::acos(dot);
	if(isnan(angle))
		return;

	angle = std::clamp(angle, -solve_data.constraint_angle, solve_data.constraint_angle);

	//	回転方向を判断するため、両方の方向に回転させてみて適応角度を変える
	math::quaternionf rotation1 = math::quaternionf::rotation_axis(rotate_axis, +angle);
	math::vector3f target_vector1 = rotation1.rotate(effector_local_direction);
	float dot1 = math::vector3f::dot(target_vector1, target_local_direction);

	math::quaternionf rotation2 = math::quaternionf::rotation_axis(rotate_axis, -angle);
	math::vector3f target_vector2 = rotation2.rotate(effector_local_direction);
	float dot2 = math::vector3f::dot(target_vector2, target_local_direction);

	float new_angle = link_data.plane_mode_angle + ((dot1 > dot2) ? +angle : -angle);
	if (iteration == 0)
	{
		if (new_angle < limit_min_angle || new_angle > limit_max_angle)
		{
			if (-new_angle > limit_min_angle && -new_angle < limit_max_angle)
			{
				new_angle *= -1;
			}
			else
			{
				float halfRadian = (limit_min_angle + limit_max_angle) * 0.5f;
				if (abs(halfRadian - new_angle) > abs(halfRadian + new_angle))
				{
					new_angle *= -1;
				}
			}
		}
	}
	new_angle = std::clamp(new_angle, limit_min_angle, limit_max_angle);
	link_data.plane_mode_angle = new_angle;

	//	IKノード情報に変換
	{
		math::vector3f S, T;
		math::quaternionf R;
		math::matrixf LocalTransform(link_node.local_transform);
		LocalTransform.decompose(S, R, T);
		R = math::quaternionf::rotation_axis(rotate_axis, new_angle);
		//	影響度を適応
		LocalTransform = math::matrixf::transform(S, R, T);
		link_node.local_transform = LocalTransform.to_float4x4();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::build()
{
	nodes.clear();
	for (auto& resource_node : resource->get_node_list())
	{
		size_t source_node_index = instance->get_node_index_by_name(resource_node.source_node_name);
		if (source_node_index == model_resource::invalid_id)
			continue;

		node_data& nd = nodes.emplace_back();
		nd.mode = resource_node.mode;
		nd.source_node_index = source_node_index;
		memcpy_s(nd.limit_angles, sizeof(nd.limit_angles),
				 resource_node.limit_angles, sizeof(resource_node.limit_angles));
		nd.use_axis = resource_node.use_axis;

		if (nd.mode == constraint_mode::euler)
		{
			float* la = nd.limit_angles;
			     if (la[0] != la[3] && la[1] == la[4] && la[2] == la[5]) nd.plane_lock = constraint_plane_lock::x;
			else if (la[0] == la[3] && la[1] != la[4] && la[2] == la[5]) nd.plane_lock = constraint_plane_lock::y;
			else if (la[0] == la[3] && la[1] == la[4] && la[2] != la[5]) nd.plane_lock = constraint_plane_lock::z;
		}
	}

	solve_datas.clear();
	for (auto& resource_data : resource->get_solve_data_list())
	{
		solve_data& ik = solve_datas.emplace_back();
		ik.mode = resource_data.mode;
		ik.node_list = resource_data.node_list;
		ik.target_node_index = resource_data.target_node_index;

		ik.iteration = resource_data.iteration;
		ik.constraint_angle = resource_data.constraint_angle;

		ik.use_pole_node_offset = resource_data.use_pole_node_offset;
		ik.pole_offset = resource_data.pole_offset;

		ik.aim_axis = resource_data.aim_axis;
		ik.aim_offset = resource_data.aim_offset;
	}
}
