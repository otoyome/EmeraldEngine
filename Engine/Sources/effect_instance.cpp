#include "effect_instance.h"
#include "texture.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	//	エミッター更新バイナリ構築
	compute_particle_system::update_emitter_data build_emitter_update_data(
		effect_instance& instance,
		::particle::data& particle_data,
		const effect_instance::node& node_data,
		float elapsed_time)
	{
		auto renderer = particle_data.get_item<::particle::item::renderer>();
		auto spawn    = particle_data.get_item<::particle::item::spawn>();
		auto velocity = particle_data.get_item<::particle::item::velocity>();

		particle_system_item::emitter::common common_data;
		{
			common_data.elapsed_time = elapsed_time;

			if (renderer)
			{
				system_utility::set_bitflag(common_data.emitter_state,
					particle_system_item::emitter::common::state::no_billboard,
					renderer->is_state(::particle::item::renderer::states::no_billboard));
			}

			auto WorldMatrix = DirectX::XMLoadFloat4x4(&node_data.world_transform);
			DirectX::XMVECTOR S, R, T;
			DirectX::XMMatrixDecompose(&S, &R, &T, WorldMatrix);
			DirectX::XMStoreFloat3(&common_data.scale, S);
			DirectX::XMStoreFloat4(&common_data.rotation, R);
			DirectX::XMStoreFloat3(&common_data.translation, T);

			if (spawn)
			{
				common_data.spawn_life_time_range = spawn->life_time_range;
				common_data.spawn_scale_range = spawn->scale_range;
				common_data.spawn_area_min = spawn->area_min;
				common_data.spawn_area_max = spawn->area_max;
				if (spawn->is_state(::particle::item::spawn::states::random))
					common_data.spawn_count = math::lerp(spawn->range.x, spawn->range.y, math::rand_range(0.0f, 1.0f));
				else
					common_data.spawn_count = spawn->range.x;

				if (instance.is_flag(effect_instance::flags::bursted) &&
					spawn->is_state(::particle::item::spawn::states::burst))
					common_data.spawn_count = 0;
			}
		}

		compute_particle_system::update_emitter_data data;
		auto append = [&](const void* p, size_t size)
		{
			size_t pos = data.item_binary.size();
			data.item_binary.resize(pos + size);
			memcpy_s(data.item_binary.data() + pos, data.item_binary.size() - pos, p, size);
		};

		if (renderer && renderer->type == ::particle::item::renderer::types::ribon)
		{
			particle_system_item::emitter::trail trail;
			trail.state = renderer->trail.state;
			trail.max = renderer->trail.max;
			trail.fade_rate = renderer->trail.time_fade_rate;
			append(&trail, sizeof(trail));
			common_data.active_items |= 1 << particle_system_item::emitter::trail::data_id;
		}
		if (renderer)
		{
			particle_system_item::emitter::uv uv;
			uv.rect = renderer->uv.rect;
			uv.state = renderer->uv.state;
			uv.split_count = renderer->uv.splits;
			uv.index = renderer->uv.index;
			uv.time = renderer->uv.time;
			append(&uv, sizeof(uv));
			common_data.active_items |= 1 << particle_system_item::emitter::uv::data_id;

			particle_system_item::emitter::color color_item;
			color_item.state = renderer->color.state;
			color_item.color_min = renderer->color.color_min;
			color_item.color_max = renderer->color.color_max;
			append(&color_item, sizeof(color_item));
			common_data.active_items |= 1 << particle_system_item::emitter::color::data_id;
		}
		if (velocity)
		{
			particle_system_item::emitter::velocity velocity_item;
			velocity_item.state = velocity->state;
			velocity_item.angle_range = velocity->angle_range;
			velocity_item.power_range = velocity->power_range;
			velocity_item.direction = velocity->direction;
			velocity_item.acceleration_min = velocity->acceleration_min;
			velocity_item.acceleration_max = velocity->acceleration_max;
			velocity_item.deceleration_max = velocity->deceleration_max;
			velocity_item.deceleration_min = velocity->deceleration_min;
			velocity_item.gravity = { 0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f };
			velocity_item.gravity.x *= velocity->gravity_scale.x;
			velocity_item.gravity.y *= velocity->gravity_scale.y;
			velocity_item.gravity.z *= velocity->gravity_scale.z;
			append(&velocity_item, sizeof(velocity_item));
			common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
		}

		data.common = common_data;
		return data;
	}
}

std::string_view effect_instance::get_name() const
{
	return name;
}

void effect_instance::set_name(std::string_view name)
{
	this->name = name;
}

effect_resource_ptr effect_instance::get_resource() const
{
	return resource;
}

const DirectX::XMFLOAT3& effect_instance::get_scale() const
{
	return scale;
}

void effect_instance::set_scale(const DirectX::XMFLOAT3& scale)
{
	this->scale = scale;
}

const math::rotator& effect_instance::get_rotation() const
{
	return rotation;
}

void effect_instance::set_rotation(const math::rotator& rotator)
{
	this->rotation = rotator;
}

const DirectX::XMFLOAT3& effect_instance::get_position() const
{
	return position;
}

void effect_instance::set_position(const DirectX::XMFLOAT3& position)
{
	this->position = position;
}

float effect_instance::get_animation_time() const
{
	return animation_time;
}

void effect_instance::set_animation_time(float animation_time)
{
	this->animation_time = animation_time;
}

float effect_instance::get_animation_speed() const
{
	return animation_speed;
}

void effect_instance::set_animation_speed(float animation_speed)
{
	this->animation_speed = animation_speed;
}

effect_instance::effect_instance()
: system_flags(0)
, resource()
{
	animation_time = 0.0f;
	set_flag(flags::active, true);
	set_flag(flags::color_render, true);
	set_flag(flags::shadow_caster, false);
	set_flag(effect_instance::flags::bursted, false);
}

effect_instance::effect_instance(effect_resource_ptr resource)
: effect_instance()
{
	this->resource = resource;
	build();
}

effect_instance::~effect_instance()
{
}

void effect_instance::update(float elapsed_time)
{
	if (!resource)
		return;

	//	アニメーション時間更新
	previous_animation_time = animation_time;
	animation_time += elapsed_time * animation_speed;

	//	初期姿勢に変換
	for (auto& node : nodes)
	{
		DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
		DirectX::XMFLOAT4 node_quaternion = math::rotator(node.rotation).convert_quaternion();
		DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node_quaternion));
		DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
		DirectX::XMStoreFloat4x4(&node.local_transform, Scale * Rotation * Translation);
	}

	//	アニメーション更新
	auto& animations = resource->get_animations();
	if (!animations.empty())
	{
		static constexpr int animation_index = 0;	//	0番を基本とする
		animation_time = std::clamp(animation_time, 0.0f, animations[animation_index].end_time);

		size_t animation_node_count = animations[animation_index].nodes.size();
		if (node_animations.size() < nodes.size())
			node_animations.resize(nodes.size());
		for (size_t index = 0; index < nodes.size() && index < animation_node_count; ++index)
		{
			animation::node an;
			if (!resource->compute_animation_node(an, animation_index, index, animation_time))
				continue;

			node_animations[index] = an;
			auto& in = nodes[index];
			DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(an.scale.x, an.scale.y, an.scale.z);
			DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&an.rotation));
			DirectX::XMMATRIX Matrix = Scale * Rotation;
			Matrix.r[3] = DirectX::XMVectorSet(an.translation.x, an.translation.y, an.translation.z, 1.0f);
			DirectX::XMStoreFloat4x4(&in.local_transform, Matrix);
		}
	}

	//	姿勢情報
	auto rot = rotation.convert_quaternion();
	auto S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	auto R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rot));
	auto T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	auto WorldTransform = S * R * T;

	//	ノード更新
	for (auto& node : nodes)
	{
		DirectX::XMMATRIX GlobalTransform = DirectX::XMLoadFloat4x4(&node.local_transform);
		if (node.parent_index != invalid_id)
		{
			auto& parent_node = nodes[node.parent_index];
			GlobalTransform *= DirectX::XMLoadFloat4x4(&parent_node.global_transform);
		}
		DirectX::XMStoreFloat4x4(&node.global_transform, GlobalTransform);
		DirectX::XMStoreFloat4x4(&node.world_transform, GlobalTransform * WorldTransform);
	}

	//	モデルの姿勢更新
	for (auto& model : models)
	{
		model.instance->update_transform(nodes[model.node_index].world_transform);
	}
}

void effect_instance::reset_to_initial_state()
{
	animation_time = 0.0f;
	set_flag(effect_instance::flags::bursted, false);
}

float effect_instance::get_particle_simulation_time() const
{
	return particle_simulation_time;
}

void effect_instance::set_particle_simulation_time(float time)
{
	particle_simulation_time = time;
}

void effect_instance::reset_particle_simulation_time()
{
	particle_simulation_time = 0.0f;
}

void effect_instance::build()
{
	if(!resource)
		return;

	//	ノード構築
	for (auto& node : resource->get_nodes())
	{
		auto& data = nodes.emplace_back();
		data.name = node.name;
		data.parent_index = node.parent_index;
		data.scale = node.scale;
		data.rotation = node.rotation;
		data.position = node.position;
	}

	//	モデル構築
	for (auto& model : resource->get_models())
	{
		if (!model.is_valid())
			continue;

		auto& data = models.emplace_back();
		data.node_index = model.node_index;
		data.instance = std::make_shared<model_instance>(model.resource);
	}

	//	パーティクル構築
	for (auto& particle : resource->get_particles())
	{
		if (!particle.is_valid())
			continue;

		auto& data = particles.emplace_back();
		data.node_index = particle.node_index;
		data.particle = particle.particle;
	}

	//	デカール構築
	for (auto& decal : resource->get_decals())
	{
		if (!decal.is_valid())
			continue;

		auto& data = decals.emplace_back();
		data.node_index = decal.node_index;
		decal.material->duplicate(*data.material);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
effect_manager::effect_manager()
: particle_system(particle_system)
{
	//	コンピュートパーティクルシステム生成
	{
		UINT emitter_max = 128;	//	エミッター最大数
		UINT emitter_data_size = sizeof(DirectX::XMFLOAT4X4) * 6;	//	エミッター一つの最大データサイズ(中で制限は特にかけてないですが)
		UINT particle_max = 100000;	//	パーティクル最大数(中で2の累乗に自動でアラインされます)
		UINT particle_data_size = sizeof(DirectX::XMFLOAT4) * 32;	//	パーティクル一つに保存できる最大データサイズ(中で制限は特にかけてないですが)
		particle_system = std::make_unique<compute_particle_system>(render_data::instance()->get_device().Get(),
			emitter_max,
			emitter_data_size,
			particle_max,
			particle_data_size);
	}
}

effect_manager::~effect_manager()
{
}

void effect_manager::update(float elapsed_time)
{
	remove_instances();
	add_instances();

	//	更新
	for (auto& item : items)
	{
		if (!item.effect_instance)
			continue;

		//	エフェクト本体更新
		item.effect_instance->update(elapsed_time);
	}

	remove_instances();
}

void effect_manager::update_gpu(graphics_context& context, float elapsed_time)
{
	if (!particle_system)
		return;

	//	時刻が巻き戻った場合リセット
	bool need_reset = false;
	for (auto& item : items)
	{
		if (!item.effect_instance)
			continue;
		if (item.effect_instance->get_animation_time() <
			item.effect_instance->get_particle_simulation_time() - 1.0e-4f)
		{
			need_reset = true;
			break;
		}
	}
	if (need_reset)
	{
		particle_system->reset();
		for (auto& item : items)
		{
			if (!item.effect_instance)
				continue;
			item.effect_instance->reset_particle_simulation_time();
			item.effect_instance->set_flag(effect_instance::flags::bursted, false);
		}
	}

	//	固定タイムステップでの更新
	constexpr float fixed_step = 1.0f / 60.0f;
	constexpr int   max_catchup_steps = 600;
	bool update = true;
	for (int step = 0; step < max_catchup_steps && update; ++step)
	{
		update = false;
		for (auto& item : items)
		{
			if (!item.effect_instance)
				continue;
			if (item.effect_instance->get_animation_time()
				- item.effect_instance->get_particle_simulation_time() > 1.0e-6f)
			{
				update = true;
				break;
			}
		}

		if (step > 0 && !update)
			break;

		for (auto& item : items)
		{
			if (!item.effect_instance)
				continue;

			//	ステップ時間
			float remaining = item.effect_instance->get_animation_time()
							- item.effect_instance->get_particle_simulation_time();
			float step_elapsed = 0.0f;
			if (remaining > 1.0e-6f)
				step_elapsed = (remaining < fixed_step) ? remaining : fixed_step;

			//	エミッターアイテム更新
			for (UINT index = 0; index < item.effect_instance->get_particles().size(); ++index)
			{
				auto& emitter_item = item.emitter_items[index];
				auto& particle_data = item.effect_instance->get_particles()[index];
				auto& node_data = item.effect_instance->get_nodes()[particle_data.node_index];

				auto data = build_emitter_update_data(*item.effect_instance,
													 particle_data.particle,
													 node_data,
													 step_elapsed);
				particle_system->update_emitter(emitter_item.system_index, data);
			}

			if (step_elapsed > 0.0f)
				item.effect_instance->set_particle_simulation_time(item.effect_instance->get_particle_simulation_time() + step_elapsed);

			if (step == 0)
				item.effect_instance->set_flag(effect_instance::flags::bursted, true);
		}

		particle_system->update(context, fixed_step);
	}
}

void effect_manager::get_container(effect_container& effects, decal_container& decals)
{
	effects.clear();

	//	バッファ/マテリアルの配列生成
	effects.sprite_trail_draw_datas.reserve(256);

	//	コンテナ生成
	for (auto& item : items)
	{
		auto instance = item.effect_instance;
		if (!instance->is_flag(effect_instance::flags::active))
			continue;

		if (!instance->is_flag(effect_instance::flags::color_render) &&
			!instance->is_flag(effect_instance::flags::shadow_caster))
			continue;

		//	デカール情報構築
		for (UINT index = 0; index < item.effect_instance->get_resource()->get_decals().size(); ++index)
		{
			auto& decal_data = item.effect_instance->get_resource()->get_decals()[index];
			if (!decal_data.is_valid())
				continue;

			auto& node_data = item.effect_instance->get_nodes()[decal_data.node_index];

			auto& decal = decals.decal_datas.emplace_back();
			auto WorldMatrix = DirectX::XMLoadFloat4x4(&node_data.world_transform);
			DirectX::XMVECTOR S, R, T;
			DirectX::XMMatrixDecompose(&S, &R, &T, WorldMatrix);
			DirectX::XMStoreFloat3(&decal.scale, S);
			DirectX::XMStoreFloat3(&decal.position, T);
			DirectX::XMFLOAT4 rotation;
			DirectX::XMStoreFloat4(&rotation, R);
			decal.rotation = rotation;
			decal.box = collision::bounding_box(decal.scale, decal.rotation, decal.position);
			decal.material.material = decal_data.material.get();
		}

		//	パーティクル描画情報構築
		for (UINT index = 0; index < item.effect_instance->get_particles().size(); ++index)
		{
			auto& emitter_item = item.emitter_items[index];
			if (emitter_item.system_index == invalid_emitter_id)
				continue;

			auto& particle_data = item.effect_instance->get_particles()[index];

			auto renderer = particle_data.particle.get_item<particle::item::renderer>();
			if (renderer && renderer->material)
			{
				UINT data_index = static_cast<UINT>(effects.sprite_trail_draw_datas.size());

				//	情報構築
				auto& data = effects.sprite_trail_draw_datas.emplace_back();
				particle_system->generate_render_data(emitter_item.system_index, data);
				data.material_data.material = renderer->material.get();

				//	色描画登録
				if (instance->is_flag(effect_instance::flags::color_render))
				{
					effects.sprite_trail_color_datas.emplace_back(data_index);
				}

				//	影描画登録
				if (instance->is_flag(effect_instance::flags::shadow_caster) &&
					data.material_data.material->is_flag(material::flags::shadow_caster))
				{
					effects.sprite_trail_shadow_datas.emplace_back(data_index);
				}
			}
		}
	}
}

bool effect_manager::register_instance(effect_instance_ptr instance)
{
	if (!instance)
		return false;
	if (std::find(instances_add.begin(), instances_add.end(), instance) != instances_add.end())
		return false;
	if (std::find_if(items.begin(),
					 items.end(),
					 [&](const item& i){return i.effect_instance == instance;}) != items.end())
		return false;

	auto it = std::find(instances_removes.begin(), instances_removes.end(), instance);
	if (it != instances_removes.end())
	{
		instances_removes.erase(it);
	}
	instances_add.emplace_back(instance);
	return true;
}

bool effect_manager::unregister_instance(effect_instance_ptr instance)
{
	if (!instance)
		return false;
	if (std::find(instances_removes.begin(), instances_removes.end(), instance) != instances_removes.end())
		return false;
	if (std::find_if(items.begin(),
					 items.end(),
					 [&](const item& i){return i.effect_instance == instance;}) == items.end())
		return false;

	auto it = std::find(instances_add.begin(), instances_add.end(), instance);
	if (it != instances_add.end())
	{
		instances_add.erase(it);
	}
	instances_removes.emplace_back(instance);
	return true;
}

void effect_manager::add_instances()
{
	for (auto& instance : instances_add)
	{
		auto& item = items.emplace_back();
		item.effect_instance = instance;

		//	モデル登録
		for (auto& model_data : item.effect_instance->get_models())
		{
			model_instance_manger::instance()->registered(model_data.instance);
		}

		//	パーティクル情報生成
		item.emitter_items.reserve(item.effect_instance->get_particles().size());
		for (auto& particle_data : item.effect_instance->get_particles())
		{
			if (!particle_data.particle.is_valid())
				continue;

			auto renderer = particle_data.particle.get_item<particle::item::renderer>();
			if (!renderer)
				continue;

			if (renderer->total_particle_count == 0)
				continue;

			auto emitter_resource = particle_data.particle.constract_system_resource();
			auto& emitter_item = item.emitter_items.emplace_back();
			emitter_item.system_index = particle_system->register_emitter(render_data::instance()->get_device().Get(),
																		  emitter_resource);
		}
	}
	system_utility::std_vector_swap_clear(instances_add);
}

void effect_manager::remove_instances()
{
	for (auto& instance : instances_removes)
	{
		auto it = std::find_if(items.begin(),
						       items.end(),
						       [&](const item& i){return i.effect_instance == instance;});
		if (it != items.end())
		{
			auto& item = *it;

			//	モデル登録解除
			//	@remainer	自動破棄される

			//	パーティクル情報解除
			for (auto& particle_data : item.effect_instance->get_particles())
			{
				if (!particle_data.particle.is_valid())
					continue;

				for (auto& emitter_item : item.emitter_items)
				{
					particle_system->unregister_emitter(emitter_item.system_index);
				}
			}

			items.erase(it);
		}
	}
	system_utility::std_vector_swap_clear(instances_removes);
}