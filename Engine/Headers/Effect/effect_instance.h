#pragma once

#include "Effect/effect_resource.h"
#include "Model/model_instance.h"

#include "graphics/cubic_sdf_buffer_pool.h"
#include <unordered_map>

//	エフェクトインスタンス
class effect_instance
{
public:
	static constexpr size_t invalid_id = -1;

	enum class flags
	{
		active,			//	表示
		color_render,	//	色描画
		shadow_caster,	//	影生成
		animation_loop,	//	アニメーションループ

		spawn_stop,		//	パーティクル生成ストップ

		bursted,		//	バースト生成処理
	};

	struct node
	{
		std::string name;
		size_t parent_index = invalid_id;

		math::float3 scale = {1, 1, 1};
		math::float3 rotation = {0, 0, 0};
		math::float3 position = {0, 0, 0};

		math::float4x4 local_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
		math::float4x4 global_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
		math::float4x4 world_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

		math::float4x4 previous_world_transform = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
	};

	struct model
	{
		size_t node_index;
		model_instance_ptr instance;
	};

	struct decal
	{
		size_t node_index = invalid_id;
		material_instance material;
	};

	struct particle
	{
		size_t node_index;
		::particle::data particle;
		material_instance material;
	};

public:
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	std::string_view get_name() const;
	void set_name(std::string_view name);

	const effect_resource_ptr get_resource() const;

	const math::float3& get_scale() const;
	void set_scale(const math::float3& scale);

	const math::rotatorf& get_rotation() const;
	void set_rotation(const math::rotatorf& rotator);

	const math::vector3d& get_position() const;
	void set_position(const math::vector3d& position);

	const math::vector3d& get_attract_position() const;
	void set_attract_position(const math::vector3d& position);

	const math::vector3d& get_event_position() const;
	void set_event_position(const math::vector3d& position);

	float get_animation_time() const;
	void set_animation_time(float animation_time);

	float get_animation_speed() const;
	float get_animation_end_time() const;
	void set_animation_speed(float animation_speed);

	float get_particle_simulation_time() const;
	void set_particle_simulation_time(float time);
	UINT get_particle_simulation_step() const;
	void set_particle_simulation_step(UINT count);

	inline std::vector<node>& get_nodes() { return nodes; }
	inline std::vector<model>& get_models() { return models; }
	inline std::vector<particle>& get_particles() { return particles; }
	inline std::vector<decal>& get_decals() { return decals; }

	inline const math::float4x4& get_previous_world_transform() const { return previous_world_transform; }
	inline void set_previous_world_transform(const math::float4x4& transform){ previous_world_transform = transform; }

#if defined(USE_IMGUI)
	inline effect_resource_ptr get_resource_for_editor() {return resource;}
#endif	//	defined(USE_IMGUI)

public:
	effect_instance();
	effect_instance(effect_resource_ptr resource);
	~effect_instance();

	void update(float elapsed_time);

	void reset_to_initial_state();
	void reset_particle_simulation_time();
	void reset_particle_simulation_step();

	std::size_t search_node_index_by_name(std::string_view data_name);

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
	math::float3 scale = {1, 1, 1};
	math::rotatorf rotation = {0, 0, 0};
	math::vector3d position;
	math::float4x4 previous_world_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

	//	吸引座標
	math::vector3d attract_position;

	//	イベント座標
	math::vector3d event_position;

	//	アニメーション関連
	float animation_time = 0.0f;
	float animation_speed = 1.0f;
	std::vector<animation::node> node_animations;
	float previous_animation_time = 0.0f;

	float particle_simulation_time = 0.0f;
	UINT particle_simulation_step = 0;
};

using effect_instance_ptr = std::shared_ptr<effect_instance>;

//	エフェクトマネージャー
class effect_manager
{
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
	static constexpr UINT trail_body_max = 64;

public:
	inline const std::vector<item>& get_items() const { return items; }
	inline std::shared_ptr<compute_particle_system> get_particle_system() const { return particle_system; }
	inline const std::vector<compute_particle_system::particle_event>& get_particle_events() const { return particle_events; }

public:
	effect_manager();
	~effect_manager();

public:
	void update(float elapsed_time);
	void update_gpu(graphics_context& context, float elapsed_time);

	bool register_instance(effect_instance_ptr instance);
	bool unregister_instance(effect_instance_ptr instance);

	void get_container(effect_container& effects, decal_container& decals, const math::vector3d& render_origin);

	void collect_model_instances(std::vector<model_instance_ptr>& out_instances) const;

	uint32_t add_cubic_sdf(collision::cubic_sdf& sdf);
	void remove_cubic_sdf(uint32_t sdf_id);

private:
	void add_instances();
	void remove_instances();

private:
	std::shared_ptr<compute_particle_system> particle_system;
	std::vector<compute_particle_system::particle_event> particle_events, work_particle_events;

	std::vector<item> items;
	std::vector<effect_instance_ptr> instances_add;
	std::vector<effect_instance_ptr> instances_removes;

	std::shared_ptr<cubic_sdf_buffer_pool> cubic_sdf_resource;
};
