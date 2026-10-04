#include "effect_instance.h"
#include "texture.h"

#include <unordered_map>

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	float random01(UINT seed)
	{
		seed ^= seed >> 16; seed *= 0x7feb352dU;
		seed ^= seed >> 15; seed *= 0x846ca68bU;
		seed ^= seed >> 16;
		return static_cast<float>(seed) / 4294967296.0f;
	}

	//	エミッター更新バイナリ構築
	compute_particle_system::update_emitter_data build_emitter_update_data(
		effect_instance& instance,
		::particle::data& particle_data,
		const effect_instance::node& node_data,
		float elapsed_time,
		UINT deterministic_seed,
		ID3D11DeviceContext* device_context,
		UINT emit_target_system_index = invalid_emitter_id)
	{
		auto renderer = particle_data.get_item<::particle::item::renderer>();
		auto spawn    = particle_data.get_item<::particle::item::spawn>();

		particle_system_item::emitter::common common_data;
		{
			common_data.elapsed_time = elapsed_time;
			common_data.random_seed = deterministic_seed;

			if (renderer)
			{
				system_utility::set_bitflag(common_data.emitter_state,
					particle_system_item::emitter::common::state::billboard_off,
					renderer->is_state(::particle::item::renderer::states::billboard_off));
				system_utility::set_bitflag(common_data.emitter_state,
					particle_system_item::emitter::common::state::reverse_depth_sort,
					renderer->is_state(::particle::item::renderer::states::reverse_depth_sort));
			}

			math::vector3f scale, translation;
			math::quaternionf rotation;
			math::matrixf(node_data.world_transform).decompose(scale, rotation, translation);
			common_data.scale = scale.to_float3();
			common_data.rotation = rotation.to_float4();
			common_data.translation = translation.to_float3();

			if (spawn)
			{
				common_data.spawn_life_time_range = spawn->life_time_range;
				common_data.spawn_scale_range = spawn->scale_range;
				common_data.spawn_area_min = spawn->area_min;
				common_data.spawn_area_max = spawn->area_max;
				if (spawn->is_state(::particle::item::spawn::states::random))
					common_data.spawn_count = math::lerp(spawn->range.x, spawn->range.y, random01(deterministic_seed));
				else
					common_data.spawn_count = spawn->range.x;

				if (instance.is_flag(effect_instance::flags::bursted) &&
					spawn->is_state(::particle::item::spawn::states::burst))
					common_data.spawn_count = 0;

				//	生成停止中であれば
				if (instance.is_flag(effect_instance::flags::spawn_stop))
					common_data.spawn_count = 0;

				//	止まってるフレームでは生成しない
				if (elapsed_time <= 0.0f)
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

		const math::matrixf IW = math::matrixf(node_data.world_transform).inverted();
		auto calculate_local_position = [&](math::float3 position)
		{
			//	@remainder	全部ワールド空間で計算してたわ・・・orz
			//auto AT = DirectX::XMLoadFloat3(&position);
			//DirectX::XMStoreFloat3(&position, DirectX::XMVector3TransformCoord(AT, IW));
			return position;
		};

		if (renderer && renderer->type == ::particle::item::renderer::types::ribon)
		{
			particle_system_item::emitter::trail trail;
			trail.state = renderer->trail.state;
			trail.max = renderer->trail.max;
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
			color_item.fade_range = renderer->color.fade_range;
			append(&color_item, sizeof(color_item));
			common_data.active_items |= 1 << particle_system_item::emitter::color::data_id;
		}
		if (auto velocity = particle_data.get_item<::particle::item::velocity>())
		{
			particle_system_item::emitter::velocity velocity_item;
			velocity_item.state = velocity->state;
			velocity_item.angle_range = velocity->angle_range;
			velocity_item.power_range = velocity->power_range;
			velocity_item.direction = velocity->direction;
			velocity_item.acceleration_min = velocity->acceleration_min;
			velocity_item.acceleration_max = velocity->acceleration_max;
			velocity_item.damping_max = velocity->damping_max;
			velocity_item.damping_min = velocity->damping_min;
			velocity_item.gravity = { 0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f };
			velocity_item.gravity.x *= velocity->gravity_scale.x;
			velocity_item.gravity.y *= velocity->gravity_scale.y;
			velocity_item.gravity.z *= velocity->gravity_scale.z;

			append(&velocity_item, sizeof(velocity_item));
			common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
		}
		if (auto attract = particle_data.get_item<::particle::item::attract>())
		{
			particle_system_item::emitter::attract attract_item;
			attract_item.strength = attract->strength;
			attract_item.radius = attract->radius;
			attract_item.falloff = attract->falloff;
			attract_item.kill_radius = attract->kill_radius;
			attract_item.damping = attract->damping;
			attract_item.position = calculate_local_position(instance.get_attract_position());

			append(&attract_item, sizeof(attract_item));
			common_data.active_items |= 1 << particle_system_item::emitter::attract::data_id;
		}
		if (auto event = particle_data.get_item<::particle::item::event>())
		{
			particle_system_item::emitter::event event_item;

			UINT event_state = 0;
			if (event->is_state(::particle::item::event::states::contact_collision))
				event_state |= static_cast<UINT>(particle_system_item::emitter::event::state::contact_collision);
			if (event->is_state(::particle::item::event::states::kill_on_hit))
				event_state |= static_cast<UINT>(particle_system_item::emitter::event::state::kill_on_hit);
			if (event->is_state(::particle::item::event::states::sweep_collision))
				event_state |= static_cast<UINT>(particle_system_item::emitter::event::state::sweep_collision);
			event_item.state = event_state;
			event_item.center = calculate_local_position(instance.get_event_position());
			event_item.radius = event->radius;
			event_item.target_emitter_id = emit_target_system_index;

			append(&event_item, sizeof(event_item));
			common_data.active_items |= 1 << particle_system_item::emitter::event::data_id;
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

const effect_resource_ptr effect_instance::get_resource() const
{
	return resource;
}

const math::float3& effect_instance::get_scale() const
{
	return scale;
}

void effect_instance::set_scale(const math::float3& scale)
{
	this->scale = scale;
}

const math::rotatorf& effect_instance::get_rotation() const
{
	return rotation;
}

void effect_instance::set_rotation(const math::rotatorf& rotator)
{
	this->rotation = rotator;
}

const math::float3& effect_instance::get_position() const
{
	return position;
}

void effect_instance::set_position(const math::float3& position)
{
	this->position = position;
}

const math::float3& effect_instance::get_attract_position() const
{
	return attract_position;
}

void effect_instance::set_attract_position(const math::float3& position)
{
	this->attract_position = position;
}

const math::float3& effect_instance::get_event_position() const
{
	return event_position;
}

void effect_instance::set_event_position(const math::float3& position)
{
	this->event_position = position;
}

float effect_instance::get_animation_time() const
{
	return animation_time;
}

float effect_instance::get_animation_end_time() const
{
	if (!resource)
		return 0.0f;

	auto& animations = resource->get_animations();
	return animations.empty() ? 0.0f : animations[0].end_time;
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
	set_flag(flags::spawn_stop, false);
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

	//	初期姿勢に変換
	for (auto& node : nodes)
	{
		node.local_transform = math::matrixf::transform(node.scale, math::rotatorf(math::vector3f(node.rotation)).to_quaternion(), node.position).to_float4x4();
	}

	//	アニメーション更新
	previous_animation_time = animation_time;
	animation_time += elapsed_time * animation_speed;
	auto& animations = resource->get_animations();
	if (!animations.empty())
	{
		static constexpr int animation_index = 0;	//	0番のみ

		if (is_flag(effect_instance::flags::animation_loop))
		{
			animation_time = fmodf(animation_time, animations[animation_index].end_time);
		}
		else
		{
			animation_time = std::clamp(animation_time, 0.0f, animations[animation_index].end_time);
		}

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
			in.local_transform = math::matrixf::transform(an.scale, math::quaternionf(an.rotation), an.translation).to_float4x4();
		}
	}

	//	姿勢情報
	const math::matrixf world_transform = math::matrixf::transform(scale, rotation.to_quaternion(), position);

	//	ノード更新
	for (auto& node : nodes)
	{
		math::matrixf global_transform(node.local_transform);
		if (node.parent_index != invalid_id)
		{
			auto& parent_node = nodes[node.parent_index];
			global_transform *= math::matrixf(parent_node.global_transform);
		}
		node.global_transform = global_transform.to_float4x4();
		node.world_transform = (global_transform * world_transform).to_float4x4();
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

UINT effect_instance::get_particle_simulation_step() const
{
	return particle_simulation_step;
}

void effect_instance::set_particle_simulation_step(UINT count)
{
	particle_simulation_step = count;
}

void effect_instance::reset_particle_simulation_step()
{
	particle_simulation_step = 0;
}

std::size_t effect_instance::search_node_index_by_name(std::string_view data_name)
{
	auto hash_key = system_utility::generate_hash(data_name);
	std::size_t node_index = 0;
	for (auto& node : nodes)
	{
		if (hash_key != system_utility::generate_hash(node.name))
		{
			++node_index;
			continue;
		}

		return node_index;
	}
	return invalid_id;
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
		data.material = std::make_shared<material>();
		decal.material->duplicate(*data.material);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
effect_manager::effect_manager()
: particle_system(particle_system)
{
	//	コンピュートパーティクルシステム生成
	{
		UINT emitter_max = 128;	//	512;
		UINT particle_max = 200000;	//	1000000;
		UINT emitter_data_size = sizeof(particle_system_item::emitter::common)
								+ sizeof(particle_system_item::emitter::trail)
								+ sizeof(particle_system_item::emitter::uv)
								+ sizeof(particle_system_item::emitter::color)
								+ sizeof(particle_system_item::emitter::velocity)
								+ sizeof(particle_system_item::emitter::attract)
								+ sizeof(particle_system_item::emitter::event);

		UINT particle_data_size = sizeof(particle_system_item::particle::common)
								+ sizeof(particle_system_item::particle::trail_header)
								+ sizeof(particle_system_item::particle::trail_body) * trail_body_max
								+ sizeof(particle_system_item::particle::uv)
								+ sizeof(particle_system_item::particle::velocity)
								+ sizeof(particle_system_item::particle::attract)
								+ sizeof(particle_system_item::particle::event);
		particle_system = std::make_unique<compute_particle_system>(render_data::instance()->get_device().Get(),
			emitter_max,
			emitter_data_size,
			particle_max,
			particle_data_size);
	}

	//	衝突判定用GPUリソース初期化
	{
		constexpr UINT voxel_pool_size = 16 * 1024 * 1024;
		constexpr UINT header_capacity = 64;
		cubic_sdf_resource = std::make_shared<cubic_sdf_buffer_pool>(render_data::instance()->get_device().Get(), voxel_pool_size, header_capacity);
		particle_system->set_cubic_sdf_gpu_resource(cubic_sdf_resource);
	}
}

effect_manager::~effect_manager()
{
	cubic_sdf_resource.reset();
}

uint32_t effect_manager::add_cubic_sdf(ID3D11DeviceContext* device_context, collision::cubic_sdf& sdf)
{
	return cubic_sdf_resource->register_sdf(device_context, sdf);
}

void effect_manager::remove_cubic_sdf(ID3D11DeviceContext* device_context, uint32_t sdf_id)
{
	if (cubic_sdf_resource)
		cubic_sdf_resource->unregister_sdf(device_context, sdf_id);
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

		item.effect_instance->update(elapsed_time);
	}

	remove_instances();
}

//	パーティクルのGPU更新
void effect_manager::update_gpu(graphics_context& context, float elapsed_time)
{
	(void) elapsed_time;
	if (!particle_system)
		return;

	//	パーティクルイベントクリア
	particle_events.clear();

	//	固定ステップ処理
	constexpr float fixed_step = system_units::convert_fps_to_sec(1.0f);
	constexpr int max_substep = 4;	//	最大ステップ数

	//	event による生成用に system_id を検索
	auto search_event_target = [](item& target_item, ::particle::data& particle) -> UINT
	{
		auto event = particle.get_item<::particle::item::event>();
		if (!event)
			return invalid_emitter_id;

		auto& instance = target_item.effect_instance;
		auto& particles = instance->get_particles();
		auto& nodes = instance->get_nodes();
		for (UINT k = 0; k < particles.size() && k < target_item.emitter_items.size(); ++k)
		{
			size_t node_index = particles[k].node_index;
			if (node_index < nodes.size() &&
				nodes[node_index].name == event->node_name)
			{
				return target_item.emitter_items[k].system_index;
			}
		}
		return invalid_emitter_id;
	};

	//	巻き戻し判定
	bool rewinding = false;
	for (auto& target_item : items)
	{
		auto& instance = target_item.effect_instance;
		if (instance &&
			instance->get_animation_time() < instance->get_particle_simulation_time() - 1.0e-4f)
		{
			rewinding = true;
			break;
		}
	}

	if (rewinding)
	{
		//	システム全体をクリアし、全エフェクトのシミュレーションカウンターを初期化
		//	再シミュレーションして元に戻す。。。
		particle_system->reset();
		for (auto& target_item : items)
		{
			auto& instance = target_item.effect_instance;
			if (!instance)
				continue;

			instance->set_particle_simulation_time(0.0f);
			instance->set_particle_simulation_step(0);
			instance->set_flag(effect_instance::flags::bursted, false);
		}
	}

	//	固定ステップで更新
	for (int step = 0; ; ++step)
	{
		//	何か一つでも更新するか判定
		bool any_update = false;

		for (auto& target_item : items)
		{
			auto& instance = target_item.effect_instance;
			if (!instance)
				continue;

			//	時間停止判定
			bool update = (instance->get_animation_time() - instance->get_particle_simulation_time() >= fixed_step);
			float step_elapsed = update ? fixed_step : 0.0f;
			any_update = any_update || update;

			//	今ステップ分のパラメーターを設定
			for (UINT index = 0; index < instance->get_particles().size(); ++index)
			{
				auto& emitter_item = target_item.emitter_items[index];
				auto& particle_data = instance->get_particles()[index];
				auto& node_data = instance->get_nodes()[particle_data.node_index];

				//	イベントによる生成ターゲットを取得
				UINT emit_target = search_event_target(target_item, particle_data.particle);

				//	乱数値を生成
				UINT seed = instance->get_particle_simulation_step() * 2654435761u + emitter_item.system_index;

				auto data = build_emitter_update_data(*instance, particle_data.particle, node_data, step_elapsed, seed, context.device_context, emit_target);
				particle_system->update_emitter(emitter_item.system_index, data);
			}

			//	時間更新
			if (update)
			{
				instance->set_particle_simulation_time(instance->get_particle_simulation_time() + fixed_step);
				instance->set_particle_simulation_step(instance->get_particle_simulation_step() + 1);
				instance->set_flag(effect_instance::flags::bursted, true);	//	前進した最初のフレームでバーストを許可
			}
		}

		bool hit_forward_limit = !rewinding && (step + 1 >= max_substep);
		bool last_step = !any_update || hit_forward_limit;	//	最終ステップだけ頂点バッファを作る

		particle_system->update(context, any_update ? fixed_step : 0.0f);

		//	イベントを取得しておく
		particle_system->fetch_events(context, work_particle_events);
		if (!work_particle_events.empty())
		{
			particle_events.reserve(particle_events.size() + work_particle_events.size());
			particle_events.insert(particle_events.end(), work_particle_events.begin(), work_particle_events.end());
			work_particle_events.clear();
		}

		if (last_step)
		{
			particle_system->build_primitives(context);
			break;
		}
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
			math::vector3f scale, position;
			math::quaternionf rotation;
			math::matrixf(node_data.world_transform).decompose(scale, rotation, position);
			decal.scale = scale.to_float3();
			decal.position = position.to_float3();
			decal.rotation = math::rotatorf(rotation);
			decal.box = collision::bounding_box(math::double3(decal.scale), rotation.to_double4(), math::double3(decal.position));
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