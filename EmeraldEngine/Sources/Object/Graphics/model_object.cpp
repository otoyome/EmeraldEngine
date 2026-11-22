#include "Object/Graphics/model_object.h"
#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"

#include "Resource/resource_manager.h"

#if defined(USE_IMGUI)
#include "Object/asset_reference_field.h"
#endif	//	defined(USE_IMGUI)

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

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

bool model_object::get_apply_root_animation_transform() const
{
	return apply_root_animation_transform;
}

void model_object::set_apply_root_animation_transform(bool flag)
{
	apply_root_animation_transform = flag;
}

model_object::model_object(std::string_view name)
: spatial_object(name)
{
#if defined(USE_IMGUI)
	editor_context = editor_items::generate_context();
#endif	//	defined(USE_IMGUI)
}

model_object::model_object(std::string_view name, std::string_view file_path)
: model_object(name)
{
	load_model(file_path);
}

model_object::model_object(std::string_view name, model_resource_ptr resource)
: spatial_object(name)
{
	previous_node_animations.clear();
	model_instance = std::make_shared<::model_instance>(resource);
	play_animation(animation_index, animation_loop, 0.0f);
	model_instance->update_transform(math::transformd::identity());

	//	IK情報チェック
	if (model_instance)
	{
		std::filesystem::path path(model_instance->get_resource()->get_file_path());
		path.replace_extension(ik_resource::extension);
		if (std::filesystem::exists(path))
		{
			auto operation = resource_manager::instance()->load<ik_resource>(path.string());
			if (operation->is_succeeded())
			{
				set_ik_resource(operation->get_resource());
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
		float previous_time = (std::min)(animation_time, animation_data.end_time);
		animation_time += elapsed_time * animation_speed;
		if (animation_loop && animation_data.end_time > 0.0f && animation_time > animation_data.end_time)
		{
			animation_time = fmodf(animation_time, animation_data.end_time);
		}
		float current_time = (std::min)(animation_time, animation_data.end_time);

		update_node_animation(animation_data, current_time, previous_time);
		update_morph_animation(animation_data, current_time, previous_time);
	}
	fixed_updated = true;
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
	if (!model_instance)
		return;

	const std::uint32_t spatial_version = get_spatial()->get_version();
	if (!dirty && applied_spatial_version == spatial_version)
		return;

	dirty = false;
	applied_spatial_version = spatial_version;
	model_instance->update_transform(get_world());
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

	float delta_time = current_time - previous_time;
	if (animation_loop && previous_time > current_time)
		delta_time += animation_data.end_time;

	//	ルートモーション
	if (animation_data.root_motion_xz || animation_data.root_motion_y || animation_data.root_rotation)
	{
		size_t node_index = model_instance->get_node_index_by_name(animation_data.root_motion_node_name);
		if (node_index != model_resource::invalid_id)
		{
			//	ワールド行列
			const math::matrixf world_transform = get_world().to_relative_matrix(get_world_translation());

			model_instance::node_animation& current_node_animation = current_node_animations[node_index];
			model_instance::node_animation begin_node_animation, previous_node_animation, end_node_animation;
			bool is_end_compite_animation = false;
			model_instance->compute_animation_node(begin_node_animation, animation_index, node_index, 0.0f);
			model_instance->compute_animation_node(previous_node_animation, animation_index, node_index, previous_time);

			if (animation_data.root_motion_xz || animation_data.root_motion_y)
			{
				//	ローカル移動値を算出
				math::float3 local_translation;
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
				math::matrixf parent_global_transform;
				if (model_instance->get_nodes()[node_index].parent_index != model_resource::invalid_id)
				{
					auto& nodes = model_instance->get_nodes();
					auto& node = nodes[node_index];
					auto& parent_node = nodes[node.parent_index];
					parent_global_transform = math::matrixf(parent_node.global_transform);
				}
				math::vector3f global_translation = math::vector3f::transform_normal(local_translation, parent_global_transform);

				//	アニメーション側から情報を破棄
				{
					if (!animation_data.root_motion_xz)
					{
						global_translation.x = 0;
						global_translation.z = 0;
					}
					if (!animation_data.root_motion_y)
					{
						global_translation.y = 0;
					}

					math::vector3f global_position = math::vector3f::transform_normal(current_node_animation.translation, parent_global_transform);

					if (animation_data.root_motion_xz)
					{
						global_position.x = 0;
						global_position.z = 0;
					}
					if (animation_data.root_motion_y)
					{
						global_position.y = 0;
					}
					const math::matrixf inverse_parent_global_transform = parent_global_transform.inverted();
					current_node_animation.translation = math::vector3f::transform(global_position, inverse_parent_global_transform).to_vector3().to_float3();
				}

				//	グローバル空間での移動ベクトルをワールド空間に移動させる
				const math::float3 world_translation = math::vector3f::transform_normal(global_translation, world_transform).to_float3();
				// 位置を更新
				if (apply_root_animation_transform)
				{
					set_world_translation(get_world_translation() + math::vector3d(world_translation));
				}

				//	コールバック呼び出し
				if (on_root_animation_translation)
					on_root_animation_translation(world_translation, delta_time);
			}

			if (animation_data.root_rotation)
			{
				//	ローカル回転を算出
				math::quaternionf local_rotation_quaternion;
				if (animation_loop && (previous_time > current_time))
				{
					if (!is_end_compite_animation)
					{
						is_end_compite_animation = true;
						model_instance->compute_animation_node(end_node_animation, animation_index, node_index, animation_data.end_time);
					}
					local_rotation_quaternion = math::quaternionf(end_node_animation.rotation) * math::quaternionf(previous_node_animation.rotation).inverted();
					local_rotation_quaternion *= math::quaternionf(current_node_animation.rotation) * math::quaternionf(begin_node_animation.rotation).inverted();
				}
				else
				{
					local_rotation_quaternion = math::quaternionf(current_node_animation.rotation) * math::quaternionf(previous_node_animation.rotation).inverted();
				}

				//	ローカル回転量を破棄
				current_node_animation.rotation = begin_node_animation.rotation;

				//	ワールド回転更新
				const math::float4 local_rotation = local_rotation_quaternion.to_float4();
				if (apply_root_animation_transform)
				{
					const math::rotatord local_rotator{ math::rotatorf(math::quaternionf(local_rotation)) };
					math::rotatord rotator = get_world_euler();
					rotator.x = math::angle::normalize(rotator.x + local_rotator.x);
					rotator.y = math::angle::normalize(rotator.y + local_rotator.y);
					rotator.z = math::angle::normalize(rotator.z + local_rotator.z);
					set_world_euler(rotator);
				}
				//	コールバック呼び出し
				if (on_root_animation_rotation)
					on_root_animation_rotation(local_rotation, delta_time);
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
				node.scale = math::lerp(math::vector3f(previous.scale), math::vector3f(current.scale), rate).to_float3();
				node.rotation = math::slerp(math::quaternionf(previous.rotation), math::quaternionf(current.rotation), rate).to_float4();
				node.translation = math::lerp(math::vector3f(previous.translation), math::vector3f(current.translation), rate).to_float3();
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
	if (animation_data.morphs.empty())
		return;

	//	アニメーション算出
	animation_data.compute_animation(current_morph_animations, current_time);

	//	モーフ設定
	const size_t morph_count = std::min(current_morph_animations.size(), animation_data.morphs.size());
	for (size_t index = 0; index < morph_count; ++index)
	{
		model_instance->set_morph(animation_data.morphs[index].morph_name, current_morph_animations[index]);
	}
}

void model_object::update_ik()
{
	if (!ik_solver)
		return;

	//	固定更新せずに来た場合、IKの計算分ガタつくので更新
	if (!fixed_updated && !current_node_animations.empty())
	{
		model_instance->set_node_poses(current_node_animations);
		dirty = true;
	}
	fixed_updated = false;

	update_transform();

	auto& model_instance_nodes = model_instance->get_nodes();
	auto& behavior_data_list = ik_solver->get_behavior_data_list();
	math::matrixf WorldMatrix(model_instance->get_world_transform());

	if (ik_datas.size() != behavior_data_list.size())
	{
		ik_datas.resize(behavior_data_list.size());
	}

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
				math::float4x4 left_world_matrix, right_world_matrix;
				left_world_matrix = (math::matrixf(left_node.global_transform) * WorldMatrix).to_float4x4();
				right_world_matrix = (math::matrixf(right_node.global_transform) * WorldMatrix).to_float4x4();
				const math::float3 left_foot_position(left_world_matrix._41, left_world_matrix._42, left_world_matrix._43);
				const math::float3 right_foot_position(right_world_matrix._41, right_world_matrix._42, right_world_matrix._43);

				//	各足が埋まるかを確認
				bool left_hit, right_hit;
				collision::intersect_data left_hit_data, right_hit_data;
				{
					collision_world::ray_cast_descs left_ray_data, right_ray_data;
					left_ray_data.filter = 
					right_ray_data.filter = collision_filter_manager::instance()->search_filter("ik");

					left_ray_data.start =
					left_ray_data.end = math::double3(left_world_matrix._41, left_world_matrix._42, left_world_matrix._43);
					left_ray_data.start.x -= foot_ik_direction.x * foot_data.ray_origin_offsets[0];
					left_ray_data.start.y -= foot_ik_direction.y * foot_data.ray_origin_offsets[0];
					left_ray_data.start.z -= foot_ik_direction.z * foot_data.ray_origin_offsets[0];
					left_ray_data.end.x += foot_ik_direction.x * (foot_data.ray_origin_offsets[0] + foot_data.ray_length);
					left_ray_data.end.y += foot_ik_direction.y * (foot_data.ray_origin_offsets[0] + foot_data.ray_length);
					left_ray_data.end.z += foot_ik_direction.z * (foot_data.ray_origin_offsets[0] + foot_data.ray_length);

					right_ray_data.start =
					right_ray_data.end = math::double3(right_world_matrix._41, right_world_matrix._42, right_world_matrix._43);
					right_ray_data.start.x -= foot_ik_direction.x * foot_data.ray_origin_offsets[1];
					right_ray_data.start.y -= foot_ik_direction.y * foot_data.ray_origin_offsets[1];
					right_ray_data.start.z -= foot_ik_direction.z * foot_data.ray_origin_offsets[1];
					right_ray_data.end.x += foot_ik_direction.x * (foot_data.ray_origin_offsets[1] + foot_data.ray_length);
					right_ray_data.end.y += foot_ik_direction.y * (foot_data.ray_origin_offsets[1] + foot_data.ray_length);
					right_ray_data.end.z += foot_ik_direction.z * (foot_data.ray_origin_offsets[1] + foot_data.ray_length);

					collision_object_ptr obj;
					left_hit = get_context().get_objects().get_world().ray_cast(left_ray_data, left_hit_data, obj);
					right_hit = get_context().get_objects().get_world().ray_cast(right_ray_data, right_hit_data, obj);
				}

				//	接地距離
				const float left_baseline  = foot_data.ray_origin_offsets[0] - foot_data.foot_offset;
				const float right_baseline = foot_data.ray_origin_offsets[1] - foot_data.foot_offset;

				//	腰位置を下げる
				{
					float length = 0.0f;
					if (left_hit)
						length = static_cast<float>(left_hit_data[0].length) - left_baseline;
					if (right_hit)
						length = std::max(length, static_cast<float>(right_hit_data[0].length) - right_baseline);
					length = std::max(length, 0.0f);

					//	ワールド空間でのオフセット位置を算出
					const math::vector3f pelvis_offset = math::vector3f(foot_ik_direction) * length;

					//	該当ノード全体に適応
					for (auto& node_index : foot_data.adjust_node_indices)
					{
						auto& pelvis_node_data = ik_solver->get_resource()->get_node_list()[node_index];
						auto& pelvis_node = model_instance_nodes[model_instance->get_node_index_by_name(pelvis_node_data.source_node_name)];

						math::matrixf parent_pelvis_world_matrix = WorldMatrix;
						if (pelvis_node.parent_index != ik_resource::invalid_id)
						{
							auto& parent_node_data = model_instance_nodes[pelvis_node.parent_index];
							parent_pelvis_world_matrix = math::matrixf(parent_node_data.global_transform) * WorldMatrix;
						}
						const math::vector3f local_direction = math::vector3f::transform_normal(pelvis_offset, parent_pelvis_world_matrix.inverted());
						pelvis_node.local_transform._41 += local_direction.x;
						pelvis_node.local_transform._42 += local_direction.y;
						pelvis_node.local_transform._43 += local_direction.z;
					}
					model_instance->update_node_transforms();
				}

				//	ヒット位置に足を補正
				if (left_hit)
				{
					math::float3 ik_pos(left_hit_data[0].point);
					ik_pos.x += foot_ik_direction.x * foot_data.foot_offset;
					ik_pos.y += foot_ik_direction.y * foot_data.foot_offset;
					ik_pos.z += foot_ik_direction.z * foot_data.foot_offset;
					ik_solver->solve(behavior_data.solve_ids[0], ik_pos);
				}
				if (right_hit)
				{
					math::float3 ik_pos(right_hit_data[0].point);
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
					const math::float3 left_target_pos = math::vector3f::transform(ik_data.left_hand_target_position, WorldMatrix).to_vector3().to_float3();

					ik_solver->solve(behavior_data.solve_ids[0], left_target_pos);
				}

				// 右手のIKターゲットをワールド座標へ変換して適用
				//if (hand_data.enable_right)
				{
					const math::float3 right_target_pos = math::vector3f::transform(ik_data.right_hand_target_position, WorldMatrix).to_vector3().to_float3();

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
	base::imgui_render_core();

	resource::asset_reference<model_resource> model_reference = resource::asset_reference<model_resource>::from(model_instance ? model_instance->get_resource() : nullptr);
	if (asset_reference_field::imgui_combo("model", model_reference, resource::asset_types::model))
		change_model(asset_registry::instance()->find_path(model_reference));

	if (!model_instance)
		return;

	CHECK_BOX(color_render);
	CHECK_BOX(shadow_caster);
	CHECK_BOX(culling);
	CHECK_BOX(self_shadow_off);
	CHECK_BOX(use_capsule_shadow);

	{
		math::float2 lod_distance_range = get_model_instance()->get_lod_distance_range();
		ImGui::DragFloat2("lod_distance_range", &lod_distance_range.x, 1.0f, 0.0f, 100000.0f, "%.1f");
		get_model_instance()->set_lod_distance_range(lod_distance_range);
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
				editor_items::edit_shared_material(editor_context, material.get_resource());
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
			if (node.parent_index != model_resource::invalid_id)
				ImGui::InputText("Parent", model_instance->get_nodes()[node.parent_index].name);
			math::vector3f local_scale, local_translation, global_scale, global_translation;
			math::quaternionf local_rotation, global_rotation;
			math::matrixf(node.local_transform).decompose(local_scale, local_rotation, local_translation);
			math::matrixf(node.global_transform).decompose(global_scale, global_rotation, global_translation);
			math::float3 ls = local_scale.to_float3();
			math::rotatorf lr = math::rotatorf(local_rotation);
			math::float3 lt = local_translation.to_float3();
			ImGui::DragFloat3("Local Scale", &ls.x);
			ImGui::DragAngle3("Local Rotation", &lr.x);
			ImGui::DragFloat3("Local Translation", &lt.x);
			ls = global_scale.to_float3();
			lr = math::rotatorf(global_rotation);
			lt = global_translation.to_float3();
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

void model_object::load_model(std::string_view file_path)
{
	//	モデルファイルが存在するかチェック
	std::filesystem::path path(file_path);
	if (std::filesystem::exists(path))
	{
		model_resource_ptr resource;
		if (model_resource_manager::instance()->load(resource, file_path))
		{
			model_instance = std::make_shared<::model_instance>(resource);
			model_instance->update_transform(math::transformd::identity());
		}
	}

	//	IK情報チェック
	if (model_instance)
	{
		path.replace_extension(ik_resource::extension);
		if (std::filesystem::exists(path))
		{
			auto operation = resource_manager::instance()->load<ik_resource>(path.string());
			if (operation->is_succeeded())
			{
				set_ik_resource(operation->get_resource());
			}
		}
	}
}

void model_object::change_model(std::string_view file_path)
{
	model_instance.reset();
	ik_solver.reset();
	ik_datas.clear();
	current_node_animations.clear();
	previous_node_animations.clear();
	current_morph_animations.clear();
	animation_index = 0;
	animation_time = 0.0f;
	load_model(file_path);
	if (model_instance)
		model_instance->set_flag(model_instance::flags::active, is_active());
	dirty = true;
	applied_spatial_version = UINT32_MAX;
}

void model_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);

	const model_resource_ptr resource = model_instance ? model_instance->get_resource() : nullptr;
	const resource::asset_reference<model_resource> reference = resource::asset_reference<model_resource>::from(resource);
	game_object_serializer::save_asset_reference(archive, "model", reference.uuid);

	bool color_render = model_instance && model_instance->is_flag(model_instance::flags::color_render);
	bool shadow_caster = model_instance && model_instance->is_flag(model_instance::flags::shadow_caster);
	bool culling = model_instance && model_instance->is_flag(model_instance::flags::culling);
	bool self_shadow_off = model_instance && model_instance->is_flag(model_instance::flags::self_shadow_off);
	bool use_capsule_shadow = model_instance && model_instance->is_flag(model_instance::flags::use_capsule_shadow);
	archive(CEREAL_NVP(color_render),
		CEREAL_NVP(shadow_caster),
		CEREAL_NVP(culling),
		CEREAL_NVP(self_shadow_off),
		CEREAL_NVP(use_capsule_shadow));
}

void model_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);

	const universally_unique_identifier uuid = game_object_serializer::load_asset_reference(archive, "model");
	if (!uuid.is_empty())
		load_model(game_object_serializer::resolve_asset_path(uuid));

	bool color_render = false;
	bool shadow_caster = false;
	bool culling = false;
	bool self_shadow_off = false;
	bool use_capsule_shadow = false;
	archive(CEREAL_NVP(color_render),
		CEREAL_NVP(shadow_caster),
		CEREAL_NVP(culling),
		CEREAL_NVP(self_shadow_off),
		CEREAL_NVP(use_capsule_shadow));

	if (model_instance)
	{
		model_instance->set_flag(model_instance::flags::color_render, color_render);
		model_instance->set_flag(model_instance::flags::shadow_caster, shadow_caster);
		model_instance->set_flag(model_instance::flags::culling, culling);
		model_instance->set_flag(model_instance::flags::self_shadow_off, self_shadow_off);
		model_instance->set_flag(model_instance::flags::use_capsule_shadow, use_capsule_shadow);
	}
}

