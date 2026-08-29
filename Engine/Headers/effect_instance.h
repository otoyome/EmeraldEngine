#pragma once

#include "effect_resource.h"
#include "model_instance.h"

//	エフェクトインスタンス
class effect_instance
{
public:
	static constexpr size_t invalid_id = -1;

	enum class flags
	{
		active,			//	表示フラグ
		color_render,	//	色描画
		shadow_caster,	//	影生成

		bursted,		//	バースト生成処理
	};

	struct node
	{
		std::string name;
		size_t parent_index = invalid_id;

		DirectX::XMFLOAT3 scale = {1, 1, 1};
		DirectX::XMFLOAT3 rotation = {0, 0, 0};
		DirectX::XMFLOAT3 position = {0, 0, 0};

		DirectX::XMFLOAT4X4 local_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
		DirectX::XMFLOAT4X4 global_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
		DirectX::XMFLOAT4X4 world_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

		DirectX::XMFLOAT4X4 previous_world_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
	};

	struct model
	{
		size_t node_index;
		model_instance_ptr instance;
	};

	struct decal
	{
		size_t node_index;
		std::shared_ptr<material> material;
	};

	struct particle
	{
		size_t node_index;
		::particle::data particle;
	};

public:
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	std::string_view get_name() const;
	void set_name(std::string_view name);

	effect_resource_ptr get_resource() const;

	const DirectX::XMFLOAT3& get_scale() const;
	void set_scale(const DirectX::XMFLOAT3& scale);

	const math::rotator& get_rotation() const;
	void set_rotation(const math::rotator& rotator);

	const DirectX::XMFLOAT3& get_position() const;
	void set_position(const DirectX::XMFLOAT3& position);

	float get_animation_time() const;
	void set_animation_time(float animation_time);

	float get_animation_speed() const;
	float get_curved_animation_time() const;
	void set_animation_speed(float animation_speed);

	float get_particle_simulation_time() const;
	void set_particle_simulation_time(float time);

	inline std::vector<node>& get_nodes() { return nodes; }
	inline std::vector<model>& get_models() { return models; }
	inline std::vector<particle>& get_particles() { return particles; }
	inline std::vector<decal>& get_decals() { return decals; }

	inline const DirectX::XMFLOAT4X4& get_previous_world_transform() const { return previous_world_transform; }
	inline void set_previous_world_transform(const DirectX::XMFLOAT4X4& transform){ previous_world_transform = transform; }

public:
	effect_instance();
	effect_instance(effect_resource_ptr resource);
	~effect_instance();

	void update(float elapsed_time);

	void reset_to_initial_state();
	void reset_particle_simulation_time();

private:
	void build();

private:
	std::string name;
	UINT system_flags;
	effect_resource_ptr resource;

	std::vector<node> nodes;
	std::vector<model> models;
	std::vector<particle> particles;
	std::vector<decal> decals;

	//	姿勢情報
	DirectX::XMFLOAT3 scale = {1, 1, 1};
	math::rotator rotation = {0, 0, 0};
	DirectX::XMFLOAT3 position = {0, 0, 0};
	DirectX::XMFLOAT4X4 previous_world_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

	//	アニメーション関連
	float animation_time = 0.0f;
	float animation_speed = 1.0f;
	std::vector<animation::node> node_animations;
	float previous_animation_time = 0.0f;

	float particle_simulation_time = 0.0f;
};

using effect_instance_ptr = std::shared_ptr<effect_instance>;

//	エフェクトマネージャー
class effect_manager
{
	SINGLETON(effect_manager)

	//	パーティクルのエミッターアイテム
	struct item
	{
		//	パーティクルエミッター情報
		struct emitter_item
		{
			particle_system_item::emitter_resource particle_emitter;
			UINT system_index = invalid_emitter_id;
		};

		effect_instance_ptr effect_instance;
		std::vector<emitter_item> emitter_items;
	};

public:
	inline const std::vector<item>& get_items() const { return items; }
	inline std::shared_ptr<compute_particle_system> get_particle_system() const { return particle_system; }

private:
	effect_manager();
	~effect_manager();

public:
	void update(float elapsed_time);
	void update_gpu(graphics_context& context, float elapsed_time);

	bool register_instance(effect_instance_ptr instance);
	bool unregister_instance(effect_instance_ptr instance);

	void get_container(effect_container& effects, decal_container& decals);

private:
	void add_instances();
	void remove_instances();

private:
	std::shared_ptr<compute_particle_system> particle_system;

	std::vector<item> items;
	std::vector<effect_instance_ptr> instances_add;
	std::vector<effect_instance_ptr> instances_removes;
};
