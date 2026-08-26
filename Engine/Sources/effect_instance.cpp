#include "effect_instance.h"
#include "texture.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

namespace
{
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

effect_instance::effect_instance()
: system_flags(0)
, resource()
{
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

	//	アニメーション関係での更新かな？

	//	姿勢情報
	auto rot = rotation.convert_quaternion();
	auto S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	auto R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rot));
	auto T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	auto W = S * R * T;

	//	モデルの姿勢更新
	for (auto& model : model_datas)
	{
		//	ローカル姿勢を設定
		S = DirectX::XMMatrixScaling(model.scale.x, model.scale.y, model.scale.z);
		R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&model.rotation));
		T = DirectX::XMMatrixTranslation(model.position.x, model.position.y, model.position.z);
		auto WorldMatrix = S * R * T * W;

		model.instance->update_transform(WorldMatrix);
	}
}

void effect_instance::build()
{
	if(!resource)
		return;

	//	モデル構築
	for (auto& model : resource->get_models())
	{
		if (!model.is_valid())
			continue;

		auto& model_data = model_datas.emplace_back();
		model_data.scale = model.scale;
		model_data.rotation = model.rotation;
		model_data.position = model.position;
		model_data.instance = std::make_shared<model_instance>(model.resource);
	}

	//	パーティクル構築
	for (auto& particle_data : resource->get_particles())
	{
		if (!particle_data.is_valid())
			continue;

		particle_datas.emplace_back(particle_data);
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
	if (particle_system)
	{
		//	パーティクル更新情報ビルド
		for (auto& item : items)
		{
			if (!item.effect_instance)
				continue;

			for (UINT index = 0; index < item.effect_instance->get_particle_datas().size(); ++index)
			{
				auto& emitter_item = item.emitter_items[index];
				auto& particle_data = item.effect_instance->get_particle_datas()[index];

				auto renderer = particle_data.get_item<particle::item::renderer>();
				auto spawn = particle_data.get_item<particle::item::spawn>();
				auto velocity = particle_data.get_item<particle::item::velocity>();

				//	共通データ
				particle_system_item::emitter::common common_data;
				{
					common_data.emitter_state;

					common_data.scale = item.effect_instance->get_scale();
					common_data.rotation = item.effect_instance->get_rotation();
					common_data.translation = item.effect_instance->get_position();

					//	発生情報設定
					if (spawn)
					{
						common_data.spawn_life_time_range = spawn->life_time_range;
						common_data.spawn_scale_range = spawn->scale_range;
						common_data.spawn_area_min = spawn->area_min;
						common_data.spawn_area_max = spawn->area_max;
						if (spawn->is_state(particle::item::spawn::states::random))
						{
							common_data.spawn_count = math::lerp(spawn->range.x,
																 spawn->range.y,
																 math::rand_range(0.0f, 1.0f));
						}
						else if (spawn->is_state(particle::item::spawn::states::burst))
						{
							//	最初にのみ
							common_data.spawn_count = spawn->range.x;
						}
						else
						{
							common_data.spawn_count = spawn->range.x;
						}

						if (item.effect_instance->is_flag(effect_instance::flags::bursted) &&
							spawn->is_state(particle::item::spawn::states::burst))
						{
							common_data.spawn_count = 0;
						}
					}
				}

				//	更新アイテム
				compute_particle_system::update_emitter_data data;

				//	軌跡アイテム登録
				if (renderer &&
					renderer->type == particle::item::renderer::types::ribon)
				{
					particle_system_item::emitter::trail trail;
					trail.state = renderer->trail.state;
					trail.max = renderer->trail.max;
					trail.fade_rate = renderer->trail.time_fade_rate;

					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::trail);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&trail, sizeof(trail));

					common_data.active_items |= 1 << particle_system_item::emitter::trail::data_id;
				}

				//	UV設定
				if (renderer)
				{
					particle_system_item::emitter::uv uv;
					uv.rect = renderer->uv.rect;
					uv.state = renderer->uv.state;
					uv.split_count = renderer->uv.splits;
					uv.index = renderer->uv.index;
					uv.time = renderer->uv.time;
					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::uv);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&uv, sizeof(uv));

					common_data.active_items |= 1 << particle_system_item::emitter::uv::data_id;
				}

				//	色情報設定
				if (renderer)
				{
					particle_system_item::emitter::color color_item;
					color_item.state = renderer->color.state;
					color_item.color_min = renderer->color.color_min;
					color_item.color_max = renderer->color.color_max;

					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::color);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&color_item, sizeof(color_item));

					common_data.active_items |= 1 << particle_system_item::emitter::color::data_id;
				}

				//	速度情報設定
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

					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::velocity);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&velocity_item, sizeof(velocity_item));

					common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
				}

				//	エミッター更新情報登録
				data.common = common_data;
				particle_system->update_emitter(emitter_item.system_index, data);

				//	フラグ修復
				item.effect_instance->set_flag(effect_instance::flags::bursted, true);
			}
		}
		particle_system->update(context, elapsed_time);
	}
}

void effect_manager::get_container(effect_container& container)
{
	container.clear();

	//	バッファ/マテリアルの配列生成
	container.sprite_trail_draw_datas.reserve(256);

	//	コンテナ生成
	for (auto& item : items)
	{
		auto instance = item.effect_instance;
		if (!instance->is_flag(effect_instance::flags::active))
			continue;

		if (!instance->is_flag(effect_instance::flags::color_render) &&
			!instance->is_flag(effect_instance::flags::shadow_caster))
			continue;

		//	パーティクル描画情報構築
		for (UINT index = 0; index < item.effect_instance->get_particle_datas().size(); ++index)
		{
			auto& emitter_item = item.emitter_items[index];
			if (emitter_item.system_index == invalid_emitter_id)
				continue;

			auto& particle_data = item.effect_instance->get_particle_datas()[index];

			auto renderer = particle_data.get_item<particle::item::renderer>();
			if (renderer && renderer->material)
			{
				UINT data_index = static_cast<UINT>(container.sprite_trail_draw_datas.size());

				//	情報構築
				auto& data = container.sprite_trail_draw_datas.emplace_back();
				particle_system->generate_render_data(emitter_item.system_index, data);
				data.material_data.material = renderer->material.get();

				//	色描画登録
				if (instance->is_flag(effect_instance::flags::color_render))
				{
					container.sprite_trail_color_datas.emplace_back(data_index);
				}

				//	影描画登録
				if (instance->is_flag(effect_instance::flags::shadow_caster) &&
					data.material_data.material->is_flag(material::flags::shadow_caster))
				{
					container.sprite_trail_shadow_datas.emplace_back(data_index);
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
		for (auto& model_data : item.effect_instance->get_model_datas())
		{
			model_instance_manger::instance()->registered(model_data.instance);
		}

		//	パーティクル情報生成
		item.emitter_items.reserve(item.effect_instance->get_particle_datas().size());
		for (auto& particle_data : item.effect_instance->get_particle_datas())
		{
			if (!particle_data.is_valid())
				continue;

			auto& emitter_item = item.emitter_items.emplace_back();
			//	パーティクルエミッター設定
			auto emitter_resource = particle_data.constract_system_resource();
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
			for (auto& particle_data : item.effect_instance->get_particle_datas())
			{
				if (!particle_data.is_valid())
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