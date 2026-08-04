#include "ik_solver.h"

namespace
{
	DirectX::XMVECTOR clamp_twist(DirectX::XMVECTOR Twist,
								  DirectX::XMVECTOR TwistAxis,
								  float min_angle,
								  float max_angle)
	{
		//	軸回転抽出
		DirectX::XMFLOAT4 twist;
		DirectX::XMStoreFloat4(&twist, Twist);

		//	対数クォータニオンに変換
		DirectX::XMFLOAT3 log_quaternion = math::convert_log_quaternion(twist);
		DirectX::XMVECTOR LogQuaternion = DirectX::XMLoadFloat3(&log_quaternion);

		//	twist 成分だけを抽出
		DirectX::XMVECTOR Axis = DirectX::XMVector3Normalize(TwistAxis);
		float projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(LogQuaternion, Axis));
		float angle = math::normalize_angle(projection);
		angle = std::min(std::max(angle, min_angle), max_angle);

		//	クォータニオンに変換
		DirectX::XMFLOAT3 new_log_quaternion;
		DirectX::XMStoreFloat3(&new_log_quaternion, DirectX::XMVectorScale(Axis, angle));
		DirectX::XMFLOAT4 new_quaternion = math::convert_quaternion(new_log_quaternion);
		return DirectX::XMLoadFloat4(&new_quaternion);
	}

	DirectX::XMVECTOR clamp_swing(DirectX::XMVECTOR Swing, float max_angle)
	{
		DirectX::XMVECTOR Axis;
		float angle;
		DirectX::XMQuaternionToAxisAngle(&Axis, &angle, Swing);
		if (angle <= max_angle)
			return Swing;

		return DirectX::XMQuaternionNormalize(DirectX::XMQuaternionRotationAxis(Axis, max_angle));
	}

	bool restrict_rotation(DirectX::XMFLOAT4& rotation, float* min, float* max)
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

	float normalize_angle(float angle)
	{
		float ret = angle;
		while (ret >= DirectX::XM_2PI)
		{
			ret -= DirectX::XM_2PI;
		}
		while (ret < 0)
		{
			ret += DirectX::XM_2PI;
		}

		return ret;
	}
	float difference_angle(float a, float b)
	{
		float diff = normalize_angle(a) - normalize_angle(b);
		if (diff > DirectX::XM_PI)
		{
			return diff - DirectX::XM_2PI;
		}
		else if (diff < -DirectX::XM_PI)
		{
			return diff + DirectX::XM_2PI;
		}
		return diff;
	}
	DirectX::XMFLOAT3 decompose(const DirectX::XMMATRIX& m, const DirectX::XMFLOAT3& before)
	{
		DirectX::XMFLOAT3 r;
		float sy = -m.r[0].m128_f32[2];
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
					r.z = std::asin(-m.r[1].m128_f32[0]);
				}
				else
				{
					r.x = DirectX::XM_PI;
					r.z = std::asin(m.r[1].m128_f32[0]);
				}
			}
			else
			{
				float cz = std::cos(before.z);
				if (cz > 0)
				{
					r.z = 0;
					r.x = std::asin(-m.r[2].m128_f32[1]);
				}
				else
				{
					r.z = DirectX::XM_PI;
					r.x = std::asin(m.r[2].m128_f32[1]);
				}
			}
		}
		else
		{
			r.x = std::atan2(m.r[1].m128_f32[2], m.r[2].m128_f32[2]);
			r.y = std::asin(-m.r[0].m128_f32[2]);
			r.z = std::atan2(m.r[0].m128_f32[1], m.r[0].m128_f32[0]);
		}

		const float pi = DirectX::XM_PI;
		DirectX::XMFLOAT3 tests[] =
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
	DirectX::XMFLOAT3 decompose(const DirectX::XMVECTOR& q, const DirectX::XMFLOAT3& before)
	{
		return decompose(DirectX::XMMatrixRotationQuaternion(q), before);
	}

	DirectX::XMFLOAT3 subtract_angle(const DirectX::XMFLOAT3& current_angle, const DirectX::XMFLOAT3& prev_angle)
	{
		DirectX::XMFLOAT3 angle;
		angle.x = normalize_angle(current_angle.x - prev_angle.x);
		angle.y = normalize_angle(current_angle.y - prev_angle.y);
		angle.z = normalize_angle(current_angle.z - prev_angle.z);
		return	angle;
	}
	DirectX::XMFLOAT3 additional_angle(const DirectX::XMFLOAT3& current_angle, const DirectX::XMFLOAT3& prev_angle)
	{
		DirectX::XMFLOAT3 angle;
		angle.x = normalize_angle(current_angle.x + prev_angle.x);
		angle.y = normalize_angle(current_angle.y + prev_angle.y);
		angle.z = normalize_angle(current_angle.z + prev_angle.z);
		return	angle;
	}
	DirectX::XMFLOAT3 clamp_angle(const DirectX::XMFLOAT3& current_angle, const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max)
	{
		DirectX::XMFLOAT3 angle;
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

void ik_solver::solve(size_t index, DirectX::XMFLOAT3 ik_target_position)
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
		auto WorldTramsform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());

		for (size_t link_node_index : solve_data.node_list)
		{
			auto& link_data = nodes[link_node_index];
			if (link_data.smoothness >= 0.99999f)
				continue;

			auto& link_node = instance_node_list[link_data.source_node_index];

			//	適応
			DirectX::XMVECTOR S, R, T;
			DirectX::XMMATRIX LocalTransform = DirectX::XMLoadFloat4x4(&link_node.local_transform);
			DirectX::XMMatrixDecompose(&S, &R, &T, LocalTransform);

			//	安定化させるために過去の計算結果を利用
			if (!link_data.reset_previous_rotation)
			{
				DirectX::XMVECTOR PreviousRotation = DirectX::XMLoadFloat4(&link_data.previous_rotation);
				R = DirectX::XMQuaternionSlerp(PreviousRotation, R, link_data.smoothness);
			}
			link_data.reset_previous_rotation = false;
			DirectX::XMStoreFloat4(&link_data.previous_rotation, R);
			LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), R, T);
			DirectX::XMStoreFloat4x4(&link_node.local_transform, LocalTransform);
		}

		//	グローバル行列再計算
		instance->update_transform(WorldTramsform);
	}
}

void ik_solver::solves(std::vector<DirectX::XMFLOAT3> ik_target_positions)
{
	for (size_t index = 0; index < solve_datas.size(); ++index)
	{
		solve(index, ik_target_positions[index]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_aim(solve_data& solve_data, DirectX::XMFLOAT3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();
	auto& model_resource_node_list = instance->get_resource()->get_nodes();

	if (solve_data.node_list.empty() || solve_data.node_list[0] == ik_resource::invalid_id)
		return;

	auto& link_data = nodes[solve_data.node_list[0]];
	auto& aim_node = instance_node_list[link_data.source_node_index];

	auto AimAxis = DirectX::XMLoadFloat3(&solve_data.aim_axis);
	auto AimOffset = DirectX::XMLoadFloat3(&solve_data.aim_offset);

	auto IKTargetPosition = DirectX::XMLoadFloat3(&ik_target_position);
	auto WorldTramsform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
	auto InverseWorldTramsform = DirectX::XMMatrixInverse(nullptr, WorldTramsform);
	IKTargetPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseWorldTramsform);
	instance->update_transform(WorldTramsform);

	auto InverseAimGlobalTransform = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&aim_node.global_transform));
	IKTargetPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseAimGlobalTransform);

	IKTargetPosition = DirectX::XMVectorAdd(IKTargetPosition, AimOffset);

	if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(IKTargetPosition, IKTargetPosition)) <= 1e-6f)
		return;

	auto IKTargetDirection = DirectX::XMVector3Normalize(IKTargetPosition);
	auto RotationAngle = acosf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(AimAxis, IKTargetDirection)));
	auto RotationAxis = DirectX::XMVector3Cross(AimAxis, IKTargetDirection);
	auto AdjustQuaternion = DirectX::XMQuaternionRotationAxis(RotationAxis, RotationAngle);

	DirectX::XMVECTOR S, R, T;
	auto LocalTransform = DirectX::XMLoadFloat4x4(&aim_node.local_transform);
	DirectX::XMMatrixDecompose(&S, &R, &T, LocalTransform);
	auto NewLocalRotation = DirectX::XMQuaternionMultiply(AdjustQuaternion, R);

	//	角度制限
	if (link_data.mode != constraint_mode::free)
	{
		bool reverse_flag = (link_data.use_axis == axis::reverse_x || link_data.use_axis == axis::reverse_y || link_data.use_axis == axis::reverse_z);
		//	軸算出
		auto get_rotation_axis = [&]()
		{
			auto& model_resource_node_list = instance->get_resource()->get_nodes();
			auto& resource_node = model_resource_node_list[link_data.source_node_index];
			DirectX::XMFLOAT3 pole_axis = {0, 0, 0};
			switch (link_data.use_axis)
			{
				case axis::x:			pole_axis = math::calculate_right(resource_node.rotation);	break;
				case axis::reverse_x:	pole_axis = math::calculate_right(resource_node.rotation);	break;
				case axis::y:			pole_axis = math::calculate_up(resource_node.rotation);		break;
				case axis::reverse_y:	pole_axis = math::calculate_up(resource_node.rotation);		break;
				case axis::z:			pole_axis = math::calculate_front(resource_node.rotation);	break;
				case axis::reverse_z:	pole_axis = math::calculate_front(resource_node.rotation);	break;
			}
			DirectX::XMVECTOR PoleAxis = DirectX::XMLoadFloat3(&pole_axis);
			return DirectX::XMVector3Normalize(DirectX::XMVectorScale(PoleAxis, reverse_flag ? -1.0f : +1.0f));
		};

		switch (link_data.mode)
		{
			case constraint_mode::hinge:
			{
				float min_hinge = link_data.limit_angles[0];
				float max_hinge = link_data.limit_angles[1];

				DirectX::XMVECTOR HingeAxis = get_rotation_axis();
				DirectX::XMVECTOR Twist = math::decompose_twist(NewLocalRotation, HingeAxis);
				NewLocalRotation = clamp_twist(Twist, HingeAxis, min_hinge, max_hinge);
				break;
			}
			case constraint_mode::ball_joint:
			{
				float min_twist = link_data.limit_angles[0];
				float max_twist = link_data.limit_angles[1];
				float max_swing = link_data.limit_angles[2];

				DirectX::XMVECTOR TwistAxis = get_rotation_axis();
				DirectX::XMVECTOR Twist, Swing;
				math::decompose_twist_and_swing(NewLocalRotation, TwistAxis, Twist, Swing);
				Twist = clamp_twist(Twist, TwistAxis, min_twist, max_twist);
				Swing = clamp_swing(Swing, max_swing);
				NewLocalRotation = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(Swing, Twist));
				break;
			}
			case constraint_mode::euler:
			{
				//	オイラー角による拘束
				DirectX::XMFLOAT4 limit_rotation;
				DirectX::XMStoreFloat4(&limit_rotation, NewLocalRotation);
				auto euler = math::convert_quaternion_to_euler(limit_rotation);
				euler.x = math::clamp(euler.x, link_data.limit_angles[0], link_data.limit_angles[3]);
				euler.y = math::clamp(euler.y, link_data.limit_angles[1], link_data.limit_angles[4]);
				euler.z = math::clamp(euler.z, link_data.limit_angles[2], link_data.limit_angles[5]);
				NewLocalRotation = DirectX::XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z);
				NewLocalRotation = DirectX::XMQuaternionNormalize(NewLocalRotation);
				break;
			}
		}
	}

	LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), NewLocalRotation, T);
	DirectX::XMStoreFloat4x4(&aim_node.local_transform, LocalTransform);
	instance->update_transform(WorldTramsform);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_limb(solve_data& solve_data, DirectX::XMFLOAT3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();

	if (!(solve_data.node_list.size() == 2 || solve_data.node_list.size() == 3) ||
		 solve_data.target_node_index == -1)
		return;

	auto IKTargetPosition = DirectX::XMLoadFloat3(&ik_target_position);
	auto WorldTramsform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
	auto InverseWorldTramsform = DirectX::XMMatrixInverse(nullptr, WorldTramsform);
	IKTargetPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseWorldTramsform);
	instance->update_transform(WorldTramsform);

	auto& root_node = instance_node_list[nodes[solve_data.node_list[0]].source_node_index];
	auto& middle_node = instance_node_list[nodes[solve_data.node_list[1]].source_node_index];
	auto& end_node = instance_node_list[nodes[solve_data.target_node_index].source_node_index];

	{
		DirectX::XMMATRIX RootGlobalTransform = DirectX::XMLoadFloat4x4(&root_node.global_transform);
		DirectX::XMMATRIX MiddleGlobalTransform = DirectX::XMLoadFloat4x4(&middle_node.global_transform);
		DirectX::XMMATRIX TipGlobalTransform = DirectX::XMLoadFloat4x4(&end_node.global_transform);

		//	計算用座標/ベクトル算出
		DirectX::XMVECTOR RootGlobalPosition = RootGlobalTransform.r[3];
		DirectX::XMVECTOR MiddleGlobalPosition = MiddleGlobalTransform.r[3];
		DirectX::XMVECTOR TipGlobalPosition = TipGlobalTransform.r[3];

		DirectX::XMVECTOR RootMiddleVec = DirectX::XMVectorSubtract(MiddleGlobalPosition, RootGlobalPosition);
		DirectX::XMVECTOR RootTargetVec = DirectX::XMVectorSubtract(IKTargetPosition, RootGlobalPosition);
		DirectX::XMVECTOR MiddleTipVec = DirectX::XMVectorSubtract(TipGlobalPosition, MiddleGlobalPosition);

		float root_middle_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(RootMiddleVec));
		float root_target_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(RootTargetVec));
		float middle_tip_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(MiddleTipVec));

		auto calculate_rotate_bone = [&](model_instance::node& node, const DirectX::XMVECTOR& Direction1, const DirectX::XMVECTOR& Direction2)
		{
			DirectX::XMVECTOR GlobalAxis = DirectX::XMVector3Cross(Direction1, Direction2);
			if (DirectX::XMVector3Equal(GlobalAxis, DirectX::XMVectorZero()))
				return;
			GlobalAxis = DirectX::XMVector3Normalize(GlobalAxis);
			
			DirectX::XMMATRIX ParentGlobalTransform = DirectX::XMMatrixIdentity();
			if (node.parent_index != model_instance::invalid_id)
			{
				auto& parent_node = instance_node_list[node.parent_index];
				ParentGlobalTransform = DirectX::XMLoadFloat4x4(&parent_node.global_transform);
			}
			DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
			DirectX::XMVECTOR LocalAxis = DirectX::XMVector3TransformNormal(GlobalAxis, InverseParentGlobalTransform);
			LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

			DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Direction1, Direction2);
			float dot = DirectX::XMVectorGetX(Dot);
			float angle = acosf(std::clamp(dot, -1.0f, 1.0f));

			DirectX::XMVECTOR S, R, T;
			DirectX::XMMATRIX LocalTransform = DirectX::XMLoadFloat4x4(&node.local_transform);
			DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(LocalAxis, angle);
			DirectX::XMMatrixDecompose(&S, &R, &T, LocalTransform);
			R = DirectX::XMQuaternionMultiply(R, LocalRotationAxis);
			LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), R, T);
			DirectX::XMStoreFloat4x4(&node.local_transform, LocalTransform);
		};

		DirectX::XMVECTOR RootTargetDirection = DirectX::XMVector3Normalize(RootTargetVec);
		DirectX::XMVECTOR RootMiddleDirection = DirectX::XMVector3Normalize(RootMiddleVec);

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
				DirectX::XMVECTOR PoleGlobalPosition = MiddleGlobalPosition;
				if (solve_data.node_list.size() == 3)
				{
					if (solve_data.node_list[2] != model_resource::invalid_id &&
						!solve_data.use_pole_node_offset)
					{
						auto& pole_node = instance_node_list[nodes[solve_data.node_list[2]].source_node_index];
						DirectX::XMMATRIX PoleGlobalTransform = DirectX::XMLoadFloat4x4(&pole_node.global_transform);
						PoleGlobalPosition = PoleGlobalTransform.r[3];
					}
					else
					{
						DirectX::XMVECTOR PoleOffset = DirectX::XMLoadFloat3(&solve_data.pole_offset);
						PoleGlobalPosition = DirectX::XMVector3TransformCoord(PoleOffset, MiddleGlobalTransform);
					}
				}
				DirectX::XMVECTOR RootPoleVec = DirectX::XMVectorSubtract(PoleGlobalPosition, RootGlobalPosition);
				DirectX::XMVECTOR RootPoleDirection = DirectX::XMVector3Normalize(RootPoleVec);
				DirectX::XMMATRIX RootParentGlobalTransform = DirectX::XMMatrixIdentity();
				if (root_node.parent_index != model_instance::invalid_id)
				{
					auto& parent_node = instance_node_list[root_node.parent_index];
					RootParentGlobalTransform = DirectX::XMLoadFloat4x4(&parent_node.global_transform);
				}
				DirectX::XMMATRIX InverseRootParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, RootParentGlobalTransform);
				DirectX::XMVECTOR PoleGlobalAxis = DirectX::XMVector3Cross(RootMiddleDirection, RootPoleDirection);
				DirectX::XMVECTOR PoleLocalAxis = DirectX::XMVector3TransformNormal(PoleGlobalAxis, InverseRootParentGlobalTransform);
				PoleLocalAxis = DirectX::XMVector3Normalize(PoleLocalAxis);
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(PoleLocalAxis, PoleLocalAxis)) < FLT_EPSILON)
				{
					PoleLocalAxis = DirectX::XMVectorSet(0, 0, 1, 0);
				}

				DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(PoleLocalAxis, angle);
				DirectX::XMVECTOR S, R, T;
				DirectX::XMMATRIX LocalTransform = DirectX::XMLoadFloat4x4(&root_node.local_transform);
				DirectX::XMMatrixDecompose(&S, &R, &T, LocalTransform);
				R = DirectX::XMQuaternionMultiply(R, LocalRotationAxis);
				LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), R, T);
				DirectX::XMStoreFloat4x4(&root_node.local_transform, LocalTransform);
			}
		}

		instance->update_transform(WorldTramsform);

		MiddleGlobalTransform = DirectX::XMLoadFloat4x4(&middle_node.global_transform);
		MiddleGlobalPosition = MiddleGlobalTransform.r[3];
		TipGlobalTransform = DirectX::XMLoadFloat4x4(&end_node.global_transform);
		TipGlobalPosition = TipGlobalTransform.r[3];

		MiddleTipVec = DirectX::XMVectorSubtract(TipGlobalPosition, MiddleGlobalPosition);
		DirectX::XMVECTOR MiddleTipDirection = DirectX::XMVector3Normalize(MiddleTipVec);
		DirectX::XMVECTOR MiddleTargetVec = DirectX::XMVectorSubtract(IKTargetPosition, MiddleGlobalPosition);
		DirectX::XMVECTOR MiddleTargetDirection = DirectX::XMVector3Normalize(MiddleTargetVec);

		calculate_rotate_bone(middle_node, MiddleTipDirection, MiddleTargetDirection);
		instance->update_transform(WorldTramsform);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_ccd(solve_data& solve_data, DirectX::XMFLOAT3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();
	auto& model_resource_node_list = instance->get_resource()->get_nodes();

	auto IKTargetPosition = DirectX::XMLoadFloat3(&ik_target_position);
	auto WorldTramsform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
	auto InverseWorldTramsform = DirectX::XMMatrixInverse(nullptr, WorldTramsform);
	IKTargetPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseWorldTramsform);

	//	CCD-IK
	for (UINT iteration = 0; iteration < solve_data.iteration; ++iteration)
	{
		if (solve_data.target_node_index == model_resource::invalid_id)
			continue;

		auto& target_node_data = nodes[solve_data.target_node_index];

		//	対象ノードを取得
		auto& target_node = instance_node_list[target_node_data.source_node_index];
		auto TargetPosition = DirectX::XMVectorSet(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43, 1);

		//	対象ノードがIK対象座標にたどり着けば終了
		DirectX::XMVECTOR vector = DirectX::XMVectorSubtract(IKTargetPosition, TargetPosition);
		float distance = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vector));
		if (distance <= 0.001f)
			break;

		bool remain_modify = false;
		DirectX::XMVECTOR RemainRotation = DirectX::XMVectorSet(0, 0, 0, 1);
		for (size_t link_node_index : solve_data.node_list)
		{
			auto& link_data = nodes[link_node_index];
			auto& link_node = instance_node_list[link_data.source_node_index];
			auto& resource_node = model_resource_node_list[link_data.source_node_index];

			DirectX::XMMATRIX InverseLinkGlobalMatrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&link_node.global_transform));

			DirectX::XMVECTOR EffectorGlobalPosition = DirectX::XMVectorSet(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43, 1);
			DirectX::XMVECTOR EffectorLocalPosition = DirectX::XMVector3TransformCoord(EffectorGlobalPosition, InverseLinkGlobalMatrix);
			DirectX::XMVECTOR TargetLocalPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseLinkGlobalMatrix);

			DirectX::XMVECTOR EffectorLocalDirection = DirectX::XMVector3Normalize(EffectorLocalPosition);
			DirectX::XMVECTOR TargetLocalDirection = DirectX::XMVector3Normalize(TargetLocalPosition);

			float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EffectorLocalDirection, TargetLocalDirection));
			float angle = acosf(dot);
			if (isnan(angle))
				continue;

			DirectX::XMVECTOR axis = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(EffectorLocalDirection, TargetLocalDirection));
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(axis, axis)) <= 0.001f)
				continue;

			angle = std::min(angle, solve_data.constraint_angle);

			DirectX::XMVECTOR AdditionalRotation = DirectX::XMQuaternionRotationAxis(axis, angle);
			//	余剰回転適応
			if (remain_modify)
			{
				remain_modify = false;
				AdditionalRotation = DirectX::XMQuaternionMultiply(RemainRotation, AdditionalRotation);
				AdditionalRotation = DirectX::XMQuaternionNormalize(AdditionalRotation);
			}

			//	適応
			DirectX::XMVECTOR S, OriginalLocalRotation, T;
			DirectX::XMMATRIX LocalTransform = DirectX::XMLoadFloat4x4(&link_node.local_transform);
			DirectX::XMMatrixDecompose(&S, &OriginalLocalRotation, &T, LocalTransform);
			DirectX::XMVECTOR NewLocalRotation = OriginalLocalRotation;
			NewLocalRotation = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(AdditionalRotation, NewLocalRotation));

			//	角度制限
			if (link_data.mode != constraint_mode::free)
			{
				bool reverse_flag = (link_data.use_axis == axis::reverse_x || link_data.use_axis == axis::reverse_y || link_data.use_axis == axis::reverse_z);
				//	軸算出
				auto get_rotation_axis = [&]()
				{
					DirectX::XMFLOAT3 pole_axis = {0, 0, 0};
					switch (link_data.use_axis)
					{
						case axis::x:			pole_axis = math::calculate_right(resource_node.rotation);	break;
						case axis::reverse_x:	pole_axis = math::calculate_right(resource_node.rotation);	break;
						case axis::y:			pole_axis = math::calculate_up(resource_node.rotation);		break;
						case axis::reverse_y:	pole_axis = math::calculate_up(resource_node.rotation);		break;
						case axis::z:			pole_axis = math::calculate_front(resource_node.rotation);	break;
						case axis::reverse_z:	pole_axis = math::calculate_front(resource_node.rotation);	break;
					}
					DirectX::XMVECTOR PoleAxis = DirectX::XMLoadFloat3(&pole_axis);
					return DirectX::XMVector3Normalize(DirectX::XMVectorScale(PoleAxis, reverse_flag ? -1.0f : +1.0f));
				};

				switch (link_data.mode)
				{
					case constraint_mode::hinge:
					{
						float min_hinge = link_data.limit_angles[0];
						float max_hinge = link_data.limit_angles[1];

						DirectX::XMVECTOR HingeAxis = get_rotation_axis();
						DirectX::XMVECTOR Twist = math::decompose_twist(NewLocalRotation, HingeAxis);
						NewLocalRotation = clamp_twist(Twist, HingeAxis, min_hinge, max_hinge);
						break;
					}
					case constraint_mode::ball_joint:
					{
						float min_twist = link_data.limit_angles[0];
						float max_twist = link_data.limit_angles[1];
						float max_swing = link_data.limit_angles[2];

						DirectX::XMVECTOR TwistAxis = get_rotation_axis();
						DirectX::XMVECTOR Twist, Swing;
						math::decompose_twist_and_swing(NewLocalRotation, TwistAxis, Twist, Swing);
						Twist = clamp_twist(Twist, TwistAxis, min_twist, max_twist);
						Swing = clamp_swing(Swing, max_swing);
						NewLocalRotation = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(Swing, Twist));
						break;
					}
					case constraint_mode::euler:
					{
						//	オイラー角による拘束
						DirectX::XMFLOAT4 limit_rotation;
						DirectX::XMStoreFloat4(&limit_rotation, NewLocalRotation);
						remain_modify = restrict_rotation(limit_rotation,
															link_data.limit_angles + 0,
															link_data.limit_angles + 3);
						if (remain_modify)
						{
							DirectX::XMVECTOR NormalizedLimitRotation = DirectX::XMLoadFloat4(&limit_rotation);
							NormalizedLimitRotation = DirectX::XMQuaternionNormalize(NormalizedLimitRotation);
							//	余剰回転を算出
							DirectX::XMVECTOR InverseNormalizedLimitRotation = DirectX::XMQuaternionInverse(NormalizedLimitRotation);
							RemainRotation = DirectX::XMQuaternionMultiply(NewLocalRotation, InverseNormalizedLimitRotation);
							DirectX::XMFLOAT4 work_rot;
							DirectX::XMStoreFloat4(&work_rot, RemainRotation);
							if (link_data.limit_angles[0] == link_data.limit_angles[3])
								work_rot.x = link_data.limit_angles[0];
							if (link_data.limit_angles[1] == link_data.limit_angles[4])
								work_rot.y = link_data.limit_angles[1];
							if (link_data.limit_angles[2] == link_data.limit_angles[5])
								work_rot.z = link_data.limit_angles[2];
							RemainRotation = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&work_rot));
							//	制限された回転を適応
							NewLocalRotation = NormalizedLimitRotation;
						}
						break;
					}
				}
			}

			LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), NewLocalRotation, T);
			DirectX::XMStoreFloat4x4(&link_node.local_transform, LocalTransform);

			//	グローバル行列再計算
			instance->update_transform(WorldTramsform);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ik_solver::solve_ccd_pmx(solve_data& solve_data, DirectX::XMFLOAT3 ik_target_position)
{
	auto& instance_node_list = instance->get_nodes();

	auto IKTargetPosition = DirectX::XMLoadFloat3(&ik_target_position);
	auto WorldTramsform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
	auto InverseWorldTramsform = DirectX::XMMatrixInverse(nullptr, WorldTramsform);
	IKTargetPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseWorldTramsform);

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
		auto TargetPosition = DirectX::XMVectorSet(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43, 1);

		//	対象ノードがIK対象座標にたどり着けば終了
		DirectX::XMVECTOR vector = DirectX::XMVectorSubtract(IKTargetPosition, TargetPosition);
		float distance = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vector));
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

				DirectX::XMMATRIX InverseLinkGlobalMatrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&link_node.global_transform));

				DirectX::XMVECTOR EffectorGlobalPosition = DirectX::XMVectorSet(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43, 1);
				DirectX::XMVECTOR EffectorLocalPosition = DirectX::XMVector3TransformCoord(EffectorGlobalPosition, InverseLinkGlobalMatrix);
				DirectX::XMVECTOR TargetLocalPosition = DirectX::XMVector3TransformCoord(IKTargetPosition, InverseLinkGlobalMatrix);

				DirectX::XMVECTOR EffectorLocalDirection = DirectX::XMVector3Normalize(EffectorLocalPosition);
				DirectX::XMVECTOR TargetLocalDirection = DirectX::XMVector3Normalize(TargetLocalPosition);

				float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EffectorLocalDirection, TargetLocalDirection));
				float angle = acosf(dot);
				if (isnan(angle))
					continue;

				DirectX::XMVECTOR axis = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(EffectorLocalDirection, TargetLocalDirection));
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(axis, axis)) <= 0.001f)
					continue;

				angle = std::min(angle, solve_data.constraint_angle);

				DirectX::XMVECTOR AdditionalRotation = DirectX::XMQuaternionRotationAxis(axis, angle);

				//	適応
				DirectX::XMVECTOR S, OriginalLocalRotation, T;
				DirectX::XMMATRIX LocalTransform = DirectX::XMLoadFloat4x4(&link_node.local_transform);
				DirectX::XMMatrixDecompose(&S, &OriginalLocalRotation, &T, LocalTransform);
				DirectX::XMVECTOR NewLocalRotation = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(AdditionalRotation, OriginalLocalRotation));

				//	角度制限
				if (link_data.plane_lock != constraint_plane_lock::free)
				{
					DirectX::XMFLOAT3 angle = decompose(NewLocalRotation, link_data.prev_angle);

					DirectX::XMFLOAT3 limit_angle = clamp_angle(angle, *reinterpret_cast<DirectX::XMFLOAT3*>(&link_data.limit_angles[0]), *reinterpret_cast<DirectX::XMFLOAT3*>(&link_data.limit_angles[3]));
					limit_angle = clamp_angle(subtract_angle(limit_angle, link_data.prev_angle),
											{-solve_data.constraint_angle, -solve_data.constraint_angle, -solve_data.constraint_angle},
											{+solve_data.constraint_angle, +solve_data.constraint_angle, +solve_data.constraint_angle});
					limit_angle = additional_angle(limit_angle, link_data.prev_angle);

					NewLocalRotation = DirectX::XMQuaternionRotationRollPitchYaw(limit_angle.x, limit_angle.y, limit_angle.z);
					link_data.prev_angle = limit_angle;
				}

				//	影響度を適応
				LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), NewLocalRotation, T);
				DirectX::XMStoreFloat4x4(&link_node.local_transform, LocalTransform);
			}
			//	グローバル行列再計算
			instance->update_transform(WorldTramsform);
		}
	}
}

void ik_solver::solve_ccd_pmx_plane(int iteration, solve_data& solve_data, node_data& link_data, DirectX::XMVECTOR ik_target_position, constraint_plane_lock axis)
{
	if (axis == constraint_plane_lock::free)
		return;

	DirectX::XMVECTOR rotate_axis;
	float limit_min_angle, limit_max_angle;

	switch (axis)
	{
		case constraint_plane_lock::x:
		{
			limit_min_angle = link_data.limit_angles[0];
			limit_max_angle = link_data.limit_angles[3];
			rotate_axis = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			break;
		}
		case constraint_plane_lock::y:
		{
			limit_min_angle = link_data.limit_angles[1];
			limit_max_angle = link_data.limit_angles[4];
			rotate_axis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			break;
		}
		case constraint_plane_lock::z:
		{
			limit_min_angle = link_data.limit_angles[2];
			limit_max_angle = link_data.limit_angles[5];
			rotate_axis = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
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

	DirectX::XMVECTOR target_position = DirectX::XMVectorSet(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43, 1);

	DirectX::XMMATRIX inverse_link_global_matrix = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&link_node.global_transform));

	DirectX::XMVECTOR effector_global_position = DirectX::XMVectorSet(target_node.global_transform._41, target_node.global_transform._42, target_node.global_transform._43, 1);
	DirectX::XMVECTOR effector_local_position = DirectX::XMVector3TransformCoord(effector_global_position, inverse_link_global_matrix);
	DirectX::XMVECTOR target_local_position = DirectX::XMVector3TransformCoord(ik_target_position, inverse_link_global_matrix);

	DirectX::XMVECTOR effector_local_direction = DirectX::XMVector3Normalize(effector_local_position);
	DirectX::XMVECTOR target_local_direction = DirectX::XMVector3Normalize(target_local_position);

	float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(effector_local_direction, target_local_direction));
	float angle = std::acos(dot);
	if(isnan(angle))
		return;

	angle = std::clamp(angle, -solve_data.constraint_angle, solve_data.constraint_angle);

	//	回転方向を判断するため、両方の方向に回転させてみて適応角度を変える
	DirectX::XMVECTOR rotation1 = DirectX::XMQuaternionRotationAxis(rotate_axis, +angle);
	DirectX::XMVECTOR target_vector1 = DirectX::XMVector3Rotate(effector_local_direction, rotation1);
	float dot1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(target_vector1, target_local_direction));

	DirectX::XMVECTOR rotation2 = DirectX::XMQuaternionRotationAxis(rotate_axis, -angle);
	DirectX::XMVECTOR target_vector2 = DirectX::XMVector3Rotate(effector_local_direction, rotation2);
	float dot2 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(target_vector2, target_local_direction));

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
		DirectX::XMVECTOR S, R, T;
		DirectX::XMMATRIX LocalTransform = DirectX::XMLoadFloat4x4(&link_node.local_transform);
		DirectX::XMMatrixDecompose(&S, &R, &T, LocalTransform);
		R = DirectX::XMQuaternionRotationAxis(rotate_axis, new_angle);
		//	影響度を適応
		LocalTransform = DirectX::XMMatrixAffineTransformation(S, DirectX::XMVectorZero(), R, T);
		DirectX::XMStoreFloat4x4(&link_node.local_transform, LocalTransform);
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
