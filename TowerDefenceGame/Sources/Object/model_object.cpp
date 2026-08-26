#include "Object/model_object.h"

model_instance_ptr model_object::get_model_instance()
{
	update_transform();
	return model_instance;
}

ik_resource_ptr model_object::get_ik_resource()
{
	if (!ik_solver)
		return nullptr;

	return ik_solver->get_resource();
}

void model_object::set_ik_resource(ik_resource_ptr ik_resource)
{
	ik_solver = std::make_shared<::ik_solver>(model_instance, ik_resource);

	ik_datas.clear();
	ik_datas.resize(ik_resource->get_behavior_data_list().size());
}

const DirectX::XMFLOAT3& model_object::get_scale() const
{
	return scale;
}

void model_object::set_scale(const DirectX::XMFLOAT3& scale)
{
	dirty = true;
	this->scale = scale;
}

const math::rotator& model_object::get_rotation() const
{
	return rotator;
}

void model_object::set_rotation(const math::rotator& rotator)
{
	dirty = true;
	this->rotator = rotator;
}

const DirectX::XMFLOAT3& model_object::get_position() const
{
	return position;
}

void model_object::set_position(const DirectX::XMFLOAT3& position)
{
	dirty = true;
	this->position = position;
}

bool model_object::get_apply_root_animation_transform() const
{
	return apply_root_animation_transform;
}

void model_object::set_apply_root_animation_transform(bool flag)
{
	apply_root_animation_transform = flag;
}

model_object::model_object(std::string_view name, std::string_view file_path)
: game_object(name)
{
	//	モデルファイルが存在するかチェック
	std::filesystem::path path(file_path);
	if (std::filesystem::exists(path))
	{
		model_resource_ptr resource;
		if (model_resource_manager::instance()->load(resource, file_path))
		{
			model_instance = std::make_shared<::model_instance>(resource);
			model_instance->update_transform(DirectX::XMMatrixIdentity());
			model_instance_manger::instance()->registered(model_instance);
		}
	}

	//	IK情報チェック
	if (model_instance)
	{
		path.replace_extension(ik_resource::default_exteision);
		if (std::filesystem::exists(path))
		{
			ik_resource_ptr resource;
			if (ik_resource_manager::instance()->load(resource, path.string()))
			{
				set_ik_resource(resource);
			}
		}
	}

#if defined(USE_IMGUI)
	editor_context = editor_items::generate_context();
#endif	//	defined(USE_IMGUI)
}

model_object::model_object(std::string_view name, model_resource_ptr resource)
: game_object(name)
{
	previous_node_animations.clear();
	model_instance = std::make_shared<::model_instance>(resource);
	play_animation(animation_index, animation_loop, 0.0f);
	model_instance->update_transform(DirectX::XMMatrixIdentity());
	model_instance_manger::instance()->registered(model_instance);

	//	IK情報チェック
	if (model_instance)
	{
		std::filesystem::path path(model_instance->get_resource()->get_file_path());
		path.replace_extension(ik_resource::default_exteision);
		if (std::filesystem::exists(path))
		{
			ik_resource_ptr resource;
			if (ik_resource_manager::instance()->load(resource, path.string()))
			{
				set_ik_resource(resource);
			}
		}
	}

#if defined(USE_IMGUI)
	editor_context = editor_items::generate_context();
#endif	//	defined(USE_IMGUI)
}

model_object::~model_object()
{
}

void model_object::fixed_update(float elapsed_time)
{
	if (!model_instance)
		return;

	//	アニメーション更新
	auto& animations = model_instance->get_resource()->get_animations();
	if (!animations.empty())
	{
		auto& animation_data = animations[animation_index];
		float previous_time = animation_time;
		animation_time += elapsed_time * animation_speed;
		float current_time = animation_time;
		if (current_time > animation_data.end_time)
		{
			current_time = animation_loop ? fmodf(current_time, animation_data.end_time) : animation_data.end_time;
		}

		update_node_animation(animation_data, current_time, previous_time);
		update_morph_animation(animation_data, current_time, previous_time);
	}
}

void model_object::pre_render()
{
	update_transform();

	//	IK処理
	update_ik();
}

void model_object::set_node_pose(size_t node_index, const model_instance::node_animation& node)
{
	if (!model_instance)
		return;

	model_instance->set_node_pose(node_index, node);
	dirty = true;
}

bool model_object::register_root_animation_translation_callback(call_back_root_animation_translation callback)
{
	on_root_animation_translation = callback;
	return true;
}

bool model_object::unregister_root_animation_translation_callback()
{
	on_root_animation_translation = {};
	return true;
}

bool model_object::register_root_animation_rotation_callback(call_back_root_animation_rotation callback)
{
	on_root_animation_rotation = callback;
	return true;
}

bool model_object::unregister_root_animation_rotation_callback()
{
	on_root_animation_rotation = {};
	return true;
}

void model_object::update_transform()
{
	if (!dirty || !model_instance)
		return;

	dirty = false;
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotator.convert_quaternion()));
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	model_instance->update_transform(Scale * Rotation * Translation);
}

void model_object::play_animation(size_t animation_index, bool loop, float blend_time)
{
	this->animation_index = animation_index;
	this->animation_loop = loop;
	this->animation_time = 0;
	this->animation_blend_time = blend_time;
	previous_node_animations.clear();
	if (this->animation_blend_time > 0)
	{
		previous_node_animations = current_node_animations;
	}
}

size_t model_object::get_animation_index() const
{
	return animation_index;
}

std::string_view model_object::get_playing_animation_name() const
{
	if (animation_index == model_resource::invalid_id || !model_instance)
		return "";

	return model_instance->get_resource()->get_animations()[animation_index].name;
}

size_t model_object::get_animation_index_from_name(std::string_view animation_name) const
{
	if (!model_instance)
		return model_resource::invalid_id;

	return model_instance->get_resource()->get_animation_index_from_name(animation_name);
}

float model_object::get_animation_end_time() const
{
	if (animation_index == model_resource::invalid_id || !model_instance)
		return 0.0f;

	return model_instance->get_resource()->get_animations()[animation_index].end_time;
}

float model_object::get_animation_time() const
{
	if (!model_instance)
		return 0.0f;

	auto& animations = model_instance->get_resource()->get_animations();
	if (animations.empty())
		return animation_time;

	auto& animation_data = animations[animation_index];

	float current_time = animation_time;
	if (current_time > animation_data.end_time)
	{
		current_time = animation_loop ? fmodf(current_time, animation_data.end_time) : animation_data.end_time;
	}
	return current_time;
}

void model_object::set_animation_time(float animation_time)
{
	this->animation_time = animation_time;
}

float model_object::get_animation_speed() const
{
	return animation_speed;
}

void model_object::set_animation_speed(float animation_speed)
{
	this->animation_speed = animation_speed;
}

std::vector<std::string_view> model_object::get_animation_names() const
{
	std::vector<std::string_view> names;
	if (model_instance)
	{
		for (auto& animation : model_instance->get_resource()->get_animations())
		{
			names.emplace_back(animation.name);
		}
	}
	return names;
}

size_t model_object::get_animation_max() const
{
	if (!model_instance)
		return -1;

	return model_instance->get_resource()->get_animations().size() - 1;
}

bool model_object::get_animation_loop() const
{
	return animation_loop;
}

void model_object::set_animation_loop(bool flag)
{
	animation_loop = flag;
}

bool model_object::set_ik_behavior_datas(std::string_view name, const ik_data& data)
{
	if (!ik_solver)
		return false;

	std::string data_name = std::string(name);
	auto& data_list = ik_solver->get_behavior_data_list();
	auto it = std::find_if(data_list.begin(),
						   data_list.end(),
						   [data_name](const ik_resource::behavior_data& r) {return data_name == r.name;});
	return it != data_list.end() ? set_ik_behavior_datas(std::distance(data_list.begin(), it), data) : false;
}

bool model_object::set_ik_behavior_datas(size_t index, const ik_data& data)
{
	if (index >= ik_datas.size())
		return false;

	ik_datas[index] = data;
	return true;
}

size_t model_object::search_ik_behavior_index(std::string_view name) const
{
	if (!ik_solver)
		return ik_resource::invalid_id;
	std::string data_name = std::string(name);
	auto& data_list = ik_solver->get_behavior_data_list();
	auto it = std::find_if(data_list.begin(),
						   data_list.end(),
						   [data_name](const ik_resource::behavior_data& r) {return data_name == r.name;});
	return it != data_list.end() ? std::distance(data_list.begin(), it) : ik_resource::invalid_id;
}

void model_object::on_active()
{
	if (model_instance)
		model_instance->set_flag(model_instance::flags::active, true);
}

void model_object::on_deactive()
{
	if (model_instance)
		model_instance->set_flag(model_instance::flags::active, false);
}

void model_object::on_finalize()
{
	ik_solver.reset();
}

void model_object::update_node_animation(const animation::data& animation_data, float current_time, float previous_time)
{
	//	アニメーション算出
	model_instance->compute_animation_nodes(current_node_animations, animation_index, current_time);

	//	ルートモーション
	if (animation_data.root_motion_xz || animation_data.root_motion_y || animation_data.root_rotation)
	{
		size_t node_index = model_instance->get_node_index_by_name(animation_data.root_motion_node_name);
		if (node_index != model_resource::invalid_id)
		{
			//	ワールド行列
			DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
			DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotator.convert_quaternion()));
			DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
			DirectX::XMMATRIX WorldTransform = Scale * Rotation * Translation;

			model_instance::node_animation& current_node_animation = current_node_animations[node_index];
			model_instance::node_animation begin_node_animation, previous_node_animation, end_node_animation;
			bool is_end_compite_animation = false;
			model_instance->compute_animation_node(begin_node_animation, animation_index, node_index, 0.0f);
			model_instance->compute_animation_node(previous_node_animation, animation_index, node_index, previous_time);

			if (animation_data.root_motion_xz || animation_data.root_motion_y)
			{
				//	ローカル移動値を算出
				DirectX::XMFLOAT3 local_translation;
				if (animation_loop && (previous_time > current_time))
				{
					if (!is_end_compite_animation)
					{
						is_end_compite_animation = true;
						model_instance->compute_animation_node(end_node_animation, animation_index, node_index, animation_data.end_time);
					}
					local_translation.x = (end_node_animation.translation.x - previous_node_animation.translation.x) + (current_node_animation.translation.x - begin_node_animation.translation.x);
					local_translation.y = (end_node_animation.translation.y - previous_node_animation.translation.y) + (current_node_animation.translation.y - begin_node_animation.translation.y);
					local_translation.z = (end_node_animation.translation.z - previous_node_animation.translation.z) + (current_node_animation.translation.z - begin_node_animation.translation.z);
				}
				else
				{
					local_translation.x = current_node_animation.translation.x - previous_node_animation.translation.x;
					local_translation.y = current_node_animation.translation.y - previous_node_animation.translation.y;
					local_translation.z = current_node_animation.translation.z - previous_node_animation.translation.z;
				}

				//	親のノード空間へ移動
				DirectX::XMMATRIX ParentGlobalTransform;
				if (model_instance->get_nodes()[node_index].parent_index != model_resource::invalid_id)
				{
					auto& nodes = model_instance->get_nodes();
					auto& node = nodes[node_index];
					auto& parent_node = nodes[node.parent_index];
					ParentGlobalTransform = DirectX::XMLoadFloat4x4(&parent_node.global_transform);
				}
				else
				{
					ParentGlobalTransform = DirectX::XMMatrixIdentity();
				}
				DirectX::XMVECTOR LocalTranslation = DirectX::XMLoadFloat3(&local_translation);
				DirectX::XMVECTOR GlobalTranslation = DirectX::XMVector3TransformNormal(LocalTranslation, ParentGlobalTransform);

				//	アニメーション側から情報を破棄
				{
					if (!animation_data.root_motion_xz)
					{
						GlobalTranslation = DirectX::XMVectorSetX(GlobalTranslation, 0);
						GlobalTranslation = DirectX::XMVectorSetZ(GlobalTranslation, 0);
					}
					if (!animation_data.root_motion_y)
					{
						GlobalTranslation = DirectX::XMVectorSetY(GlobalTranslation, 0);
					}

					DirectX::XMVECTOR LocalPosition = DirectX::XMLoadFloat3(&current_node_animation.translation);
					DirectX::XMVECTOR GlobalPosition = DirectX::XMVector3TransformNormal(LocalPosition, ParentGlobalTransform);

					if (animation_data.root_motion_xz)
					{
						GlobalPosition = DirectX::XMVectorSetX(GlobalPosition, 0);
						GlobalPosition = DirectX::XMVectorSetZ(GlobalPosition, 0);
					}
					if (animation_data.root_motion_y)
					{
						GlobalPosition = DirectX::XMVectorSetY(GlobalPosition, 0);
					}
					DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
					LocalPosition = DirectX::XMVector3Transform(GlobalPosition, InverseParentGlobalTransform);

					DirectX::XMStoreFloat3(&current_node_animation.translation, LocalPosition);
				}

				//	グローバル空間での移動ベクトルをワールド空間に移動させる
				DirectX::XMVECTOR WorldTranslation = DirectX::XMVector3TransformNormal(GlobalTranslation, WorldTransform);
				DirectX::XMFLOAT3 world_translation;
				DirectX::XMStoreFloat3(&world_translation, WorldTranslation);
				// 位置を更新
				if (apply_root_animation_transform)
				{
					position.x += world_translation.x;
					position.y += world_translation.y;
					position.z += world_translation.z;
					dirty = true;
				}

				//	コールバック呼び出し
				if (on_root_animation_translation)
					on_root_animation_translation(world_translation, current_time - previous_time);
			}

			if (animation_data.root_rotation)
			{
				//	ローカル回転を算出
				DirectX::XMVECTOR LocalRotation, WorkRotation[2];
				if (animation_loop && (previous_time > current_time))
				{
					if (!is_end_compite_animation)
					{
						is_end_compite_animation = true;
						model_instance->compute_animation_node(end_node_animation, animation_index, node_index, animation_data.end_time);
					}
					WorkRotation[0] = DirectX::XMLoadFloat4(&end_node_animation.rotation);
					WorkRotation[1] = DirectX::XMLoadFloat4(&previous_node_animation.rotation);
					WorkRotation[1] = DirectX::XMQuaternionInverse(WorkRotation[1]);
					LocalRotation = DirectX::XMQuaternionMultiply(WorkRotation[0], WorkRotation[1]);
					WorkRotation[0] = DirectX::XMLoadFloat4(&current_node_animation.rotation);
					WorkRotation[1] = DirectX::XMLoadFloat4(&begin_node_animation.rotation);
					WorkRotation[1] = DirectX::XMQuaternionInverse(WorkRotation[1]);
					WorkRotation[1] = DirectX::XMQuaternionMultiply(WorkRotation[0], WorkRotation[1]);
					LocalRotation = DirectX::XMQuaternionMultiply(LocalRotation, WorkRotation[1]);
				}
				else
				{
					WorkRotation[0] = DirectX::XMLoadFloat4(&current_node_animation.rotation);
					WorkRotation[1] = DirectX::XMLoadFloat4(&previous_node_animation.rotation);
					WorkRotation[1] = DirectX::XMQuaternionInverse(WorkRotation[1]);
					LocalRotation = DirectX::XMQuaternionMultiply(WorkRotation[0], WorkRotation[1]);
				}

				//	ローカル回転量を破棄
				current_node_animation.rotation = begin_node_animation.rotation;

				//	ワールド回転更新
				DirectX::XMFLOAT4 local_rotation;
				DirectX::XMStoreFloat4(&local_rotation, LocalRotation);
				if (apply_root_animation_transform)
				{
					math::rotator local_rotator = local_rotation;
					rotator.x = math::normalize_angle(rotator.x + local_rotator.x);
					rotator.y = math::normalize_angle(rotator.y + local_rotator.y);
					rotator.z = math::normalize_angle(rotator.z + local_rotator.z);
					dirty = true;
				}
				//	コールバック呼び出し
				if (on_root_animation_rotation)
					on_root_animation_rotation(local_rotation, current_time - previous_time);
			}
		}
	}

	//	ブレンド処理
	if (!previous_node_animations.empty())
	{
		if (animation_time <= animation_blend_time)
		{
			float rate = math::clamp(animation_time / animation_blend_time, 0.0f, 1.0f);
			rate *= rate;
			for (size_t index = 0; index < previous_node_animations.size(); ++index)
			{
				auto& node = current_node_animations[index];
				auto& current = current_node_animations[index];
				auto& previous = previous_node_animations[index];
				DirectX::XMStoreFloat3(&node.scale, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&previous.scale), DirectX::XMLoadFloat3(&current.scale), rate));
				DirectX::XMStoreFloat4(&node.rotation, DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&previous.rotation), DirectX::XMLoadFloat4(&current.rotation), rate));
				DirectX::XMStoreFloat3(&node.translation, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&previous.translation), DirectX::XMLoadFloat3(&current.translation), rate));
			}
		}
		else
		{
			previous_node_animations.clear();
		}
	}

	//	IK制御用の情報を保存
	animation_foot_ik = animation_data.foot_ik;
	animation_hand_ik = animation_data.hand_ik;
	animation_aim_ik = animation_data.aim_ik;

	//	ノード姿勢設定
	model_instance->set_node_poses(current_node_animations);
	dirty = true;
}

void model_object::update_morph_animation(const animation::data& animation_data, float current_time, float previous_time)
{
	//animation_data.compute_animation
	//auto& animation_data = animations[animation_index];

}

void model_object::update_ik()
{
	if (!ik_solver)
		return;

	update_transform();

	auto& model_instance_nodes = model_instance->get_nodes();
	auto& behavior_data_list = ik_solver->get_behavior_data_list();
	auto WorldMatrix = DirectX::XMLoadFloat4x4(&model_instance->get_world_transform());

	if (ik_datas.size() != behavior_data_list.size())
	{
		ik_datas.resize(behavior_data_list.size());
	}

	constexpr float k_pelvis_smooth_speed = 25.0f;
	const float dt = 0.01666f; // 既存のdt取得関数に置き換えてください

	auto ExpLerp = [](float current, float target, float speed, float dt) -> float
	{
		float t = 1.0f - std::expf(-speed * dt);
		return current + (target - current) * t;
	};

	for (UINT index = 0; index < behavior_data_list.size(); ++index)
	{
		auto& ik_data = ik_datas[index];
		if (!ik_data.visible)
		{
			ik_data.initialized = false;
			continue;
		}

		auto& behavior_data = behavior_data_list[index];
		switch (behavior_data.mode)
		{
			case ::ik_resource::behavior_mode::foot:
			{
				if (!animation_foot_ik)
				{
					ik_data.initialized = false;
					continue;
				}

				if (behavior_data.solve_ids.empty() ||
					behavior_data.solve_ids.size() < 2 ||
					behavior_data.solve_ids[0] == ik_resource::invalid_id ||
					behavior_data.solve_ids[1] == ik_resource::invalid_id)
					continue;

				auto& left_solve_data = ik_solver->get_solve_data()[behavior_data.solve_ids[0]];
				auto& right_solve_data = ik_solver->get_solve_data()[behavior_data.solve_ids[1]];
				auto& left_node_data = ik_solver->get_resource()->get_node_list()[left_solve_data.target_node_index];
				auto& right_node_data = ik_solver->get_resource()->get_node_list()[right_solve_data.target_node_index];
				auto& left_node = model_instance_nodes[model_instance->get_node_index_by_name(left_node_data.source_node_name)];
				auto& right_node = model_instance_nodes[model_instance->get_node_index_by_name(right_node_data.source_node_name)];
				auto& foot_data = std::get<ik_resource::foot_data>(behavior_data.data);

				//	各足の座標を算出
				DirectX::XMFLOAT4X4 left_world_matrix, right_world_matrix;
				DirectX::XMStoreFloat4x4(&left_world_matrix, DirectX::XMLoadFloat4x4(&left_node.global_transform) * WorldMatrix);
				DirectX::XMStoreFloat4x4(&right_world_matrix, DirectX::XMLoadFloat4x4(&right_node.global_transform) * WorldMatrix);
				const DirectX::XMFLOAT3 left_foot_position(left_world_matrix._41, left_world_matrix._42, left_world_matrix._43);
				const DirectX::XMFLOAT3 right_foot_position(right_world_matrix._41, right_world_matrix._42, right_world_matrix._43);

				//	重力方向
				static constexpr DirectX::XMFLOAT3 foot_ik_direction = {0, -1, 0};

				//	各足が埋まるかを確認
				bool left_hit, right_hit;
				collision::intersect_data left_hit_data, right_hit_data;
				{
					collision_world::ray_cast_descs left_ray_data, right_ray_data;
					left_ray_data.filter = 
					right_ray_data.filter = collision_filter_manager::instance()->search_filter("ik");

					left_ray_data.start =
					left_ray_data.end = DirectX::XMFLOAT3(left_world_matrix._41, left_world_matrix._42, left_world_matrix._43);
					left_ray_data.start.x -= foot_ik_direction.x * foot_data.ray_origin_offsets[0];
					left_ray_data.start.y -= foot_ik_direction.y * foot_data.ray_origin_offsets[0];
					left_ray_data.start.z -= foot_ik_direction.z * foot_data.ray_origin_offsets[0];
					left_ray_data.end.x += foot_ik_direction.x * (foot_data.ray_origin_offsets[0] + foot_data.ray_length);
					left_ray_data.end.y += foot_ik_direction.y * (foot_data.ray_origin_offsets[0] + foot_data.ray_length);
					left_ray_data.end.z += foot_ik_direction.z * (foot_data.ray_origin_offsets[0] + foot_data.ray_length);

					right_ray_data.start =
					right_ray_data.end = DirectX::XMFLOAT3(right_world_matrix._41, right_world_matrix._42, right_world_matrix._43);
					right_ray_data.start.x -= foot_ik_direction.x * foot_data.ray_origin_offsets[1];
					right_ray_data.start.y -= foot_ik_direction.y * foot_data.ray_origin_offsets[1];
					right_ray_data.start.z -= foot_ik_direction.z * foot_data.ray_origin_offsets[1];
					right_ray_data.end.x += foot_ik_direction.x * (foot_data.ray_origin_offsets[1] + foot_data.ray_length);
					right_ray_data.end.y += foot_ik_direction.y * (foot_data.ray_origin_offsets[1] + foot_data.ray_length);
					right_ray_data.end.z += foot_ik_direction.z * (foot_data.ray_origin_offsets[1] + foot_data.ray_length);

					collision_object_ptr obj;
					left_hit = object_manager::instance()->get_world().ray_cast(left_ray_data, left_hit_data, obj);
					right_hit = object_manager::instance()->get_world().ray_cast(right_ray_data, right_hit_data, obj);
				}

				//	接地距離
				const float left_baseline  = foot_data.ray_origin_offsets[0] - foot_data.foot_offset;
				const float right_baseline = foot_data.ray_origin_offsets[1] - foot_data.foot_offset;

				//	腰位置を下げる
				{
					float length = 0.0f;
					if (left_hit)
						length = left_hit_data[0].length - left_baseline;
					if (right_hit)
						length = std::max(length, right_hit_data[0].length - right_baseline);
					length = std::max(length, 0.0f);

					if (!ik_data.initialized)
						ik_data.smoothed_pelvis_length = length;
					else
						ik_data.smoothed_pelvis_length = ExpLerp(ik_data.smoothed_pelvis_length, length, k_pelvis_smooth_speed, dt);

					//	ワールド空間でのオフセット位置を算出
					auto PelvisOffset = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&foot_ik_direction), ik_data.smoothed_pelvis_length);

					//	該当ノード全体に適応
					for (auto& node_index : foot_data.adjust_node_indices)
					{
						auto& pelvis_node_data = ik_solver->get_resource()->get_node_list()[node_index];
						auto& pelvis_node = model_instance_nodes[model_instance->get_node_index_by_name(pelvis_node_data.source_node_name)];

						DirectX::XMMATRIX ParentPelvisWorldMatrix = WorldMatrix;
						if (pelvis_node.parent_index != ik_resource::invalid_id)
						{
							auto& parent_node_data = model_instance_nodes[pelvis_node.parent_index];
							ParentPelvisWorldMatrix = DirectX::XMLoadFloat4x4(&parent_node_data.global_transform) * WorldMatrix;
						}
						DirectX::XMMATRIX InversePelvisWorldMatrix = DirectX::XMMatrixInverse(nullptr, ParentPelvisWorldMatrix);

						DirectX::XMFLOAT3 local_direction;
						DirectX::XMStoreFloat3(&local_direction, DirectX::XMVector3TransformNormal(PelvisOffset, InversePelvisWorldMatrix));
						pelvis_node.local_transform._41 += local_direction.x;
						pelvis_node.local_transform._42 += local_direction.y;
						pelvis_node.local_transform._43 += local_direction.z;
					}
					model_instance->update_transform(model_instance->get_world_transform());
				}

				//	ヒット位置に足を補正
				if (left_hit)
				{
					DirectX::XMFLOAT3 ik_pos = left_hit_data[0].point;
					ik_pos.x += foot_ik_direction.x * foot_data.foot_offset;
					ik_pos.y += foot_ik_direction.y * foot_data.foot_offset;
					ik_pos.z += foot_ik_direction.z * foot_data.foot_offset;
					ik_solver->solve(behavior_data.solve_ids[0], ik_pos);
				}
				if (right_hit)
				{
					DirectX::XMFLOAT3 ik_pos = right_hit_data[0].point;
					ik_pos.x += foot_ik_direction.x * foot_data.foot_offset;
					ik_pos.y += foot_ik_direction.y * foot_data.foot_offset;
					ik_pos.z += foot_ik_direction.z * foot_data.foot_offset;
					ik_solver->solve(behavior_data.solve_ids[1], ik_pos);
				}

				ik_data.initialized = true;
				break;
			}
			case ::ik_resource::behavior_mode::hand:
			{
				if (!animation_hand_ik)
				{
					ik_data.initialized = false;
					continue;
				}
				auto& hand_data = std::get<ik_resource::hand_data>(behavior_data.data);

				// 左手のIKターゲットをワールド座標へ変換して適用
				//if (hand_data.enable_left)
				{
					DirectX::XMFLOAT3 left_target_pos;
					DirectX::XMStoreFloat3(
						&left_target_pos,
						DirectX::XMVector3Transform(
							DirectX::XMLoadFloat3(&ik_data.left_hand_target_position),
							WorldMatrix));

					ik_solver->solve(behavior_data.solve_ids[0], left_target_pos);
				}

				// 右手のIKターゲットをワールド座標へ変換して適用
				//if (hand_data.enable_right)
				{
					DirectX::XMFLOAT3 right_target_pos;
					DirectX::XMStoreFloat3(
						&right_target_pos,
						DirectX::XMVector3Transform(
							DirectX::XMLoadFloat3(&ik_data.right_hand_target_position),
							WorldMatrix));

					ik_solver->solve(behavior_data.solve_ids[1], right_target_pos);
				}
				break;
			}
			case ::ik_resource::behavior_mode::lookat:
			{
				if (!animation_aim_ik)
				{
					ik_data.initialized = false;
					continue;
				}

				for (auto& solve_id : behavior_data.solve_ids)
				{
					ik_solver->solve(solve_id, ik_data.aim_target_position);
				}
				break;
			}
		}
	}
}

#if defined(USE_IMGUI)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CHECK_BOX(FLAG) \
{\
	bool is_##FLAG = model_instance->is_flag(model_instance::flags::##FLAG);\
	ImGui::Checkbox(#FLAG, &is_##FLAG);\
	model_instance->set_flag(model_instance::flags::##FLAG, is_##FLAG);\
}

void model_object::imgui_render_core()
{
	if (!model_instance)
		return;

	CHECK_BOX(color_render);
	CHECK_BOX(shadow_caster);
	CHECK_BOX(culling);
	CHECK_BOX(self_shadow_off);
	CHECK_BOX(use_capsule_shadow);
	CHECK_BOX(debug_draw_culling_object);

	ImGui::DragFloat3("scale", &scale.x, 0.01f);
	ImGui::DragAngle3("rotation", &rotator.x, 0.01f);
	ImGui::DragFloat3("position", &position.x, 0.01f);
	{
		DirectX::XMFLOAT2 lod_range_rate = get_model_instance()->get_lod_range_rate();
		ImGui::SliderFloat2("lod_range_rate", &lod_range_rate.x, 0.0f, 1.0f);
		get_model_instance()->set_lod_range_rate(lod_range_rate);
	}
	{
		int lod_index = get_model_instance()->get_lod_index();
		int lod_max = get_model_instance()->get_lod_max();
		ImGui::SliderInt("force_lod_mesh_index", &lod_index, -1, lod_max);
		get_model_instance()->set_lod_index(lod_index);
	}

	int animation_id = static_cast<int>(animation_index);
	ImGui::DragInt("animation_index", &animation_id, 0.01f, 0, static_cast<int>(model_instance->get_resource()->get_animations().size()) - 1);
	if (animation_index != animation_id)
	{
		play_animation(animation_id, animation_loop);
	}
	ImGui::Checkbox("animation_loop", &animation_loop);
	ImGui::DragFloat("animation_time", &animation_time, 0.01f);

	//	マテリアル
	if (ImGui::TreeNode("material list"))
	{
		for (auto& material : model_instance->get_materials())
		{
			if (ImGui::TreeNode(system_utility::convert_utf8(material.get_name()).data()))
			{
				editor_items::edit_material(editor_context, material);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	//	ノードリスト
	if (ImGui::TreeNode("node list"))
	{
		ImGui::BeginDisabled();
		for (auto& node : model_instance->get_nodes())
		{
			ImGui::PushID(editor_context->get_item_id());
			ImGui::Text(system_utility::convert_utf8(node.name).data());
			if (node.parent_index == model_resource::invalid_id)
				ImGui::InputText("Parent", std::string(""));
			else
				ImGui::InputText("Parent", model_instance->get_nodes()[node.parent_index].name);
			DirectX::XMVECTOR LS, LR, LT;
			DirectX::XMVECTOR GS, GR, GT;
			DirectX::XMMatrixDecompose(&LS, &LR, &LT, DirectX::XMLoadFloat4x4(&node.local_transform));
			DirectX::XMMatrixDecompose(&GS, &GR, &GT, DirectX::XMLoadFloat4x4(&node.global_transform));
			DirectX::XMFLOAT3 ls = system_utility::convert_xmfloat3_from_vector(LS);
			math::rotator lr = math::rotator(system_utility::convert_xmfloat4_from_vector(LR));
			DirectX::XMFLOAT3 lt = system_utility::convert_xmfloat3_from_vector(LT);
			ImGui::DragFloat3("Local Scale", &ls.x);
			ImGui::DragAngle3("Local Rotation", &lr.x);
			ImGui::DragFloat3("Local Translation", &lt.x);
			ls = system_utility::convert_xmfloat3_from_vector(GS);
			lr = math::rotator(system_utility::convert_xmfloat4_from_vector(GR));
			lt = system_utility::convert_xmfloat3_from_vector(GT);
			ImGui::DragFloat3("Global Scale", &ls.x);
			ImGui::DragAngle3("Global Rotation", &lr.x);
			ImGui::DragFloat3("Global Translation", &lt.x);

			ImGui::Separator();
			ImGui::PopID();
		}
		ImGui::EndDisabled();
		ImGui::TreePop();
	}

	//	IK
	if (ik_solver && ImGui::TreeNode("ik list"))
	{
		UINT ik_node_index = 0;
		auto& datas = ik_solver->get_resource()->get_behavior_data_list();
		for (auto& ik_data : ik_datas)
		{
			if (!ImGui::TreeNode(system_utility::convert_utf8(ik_solver->get_resource()->get_behavior_data_list()[ik_node_index++].name).data()))
				continue;

			ImGui::Checkbox("visible", &ik_data.visible);
			ImGui::BeginDisabled();
			ImGui::DragFloat3("target_position", &ik_data.aim_target_position.x, 0.01f);
			ImGui::EndDisabled();
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}
#undef CHECK_BOX
#endif	//	defined(USE_IMGUI)

