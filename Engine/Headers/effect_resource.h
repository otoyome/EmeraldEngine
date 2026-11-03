#pragma once

#include <unordered_map>

#include "texture.h"
#include "material_resource.h"

#include "model_resource.h"
#include "animation_resource.h"
#include "graphics/particle_resource.h"

#include "collision/capsule.h"
#include "collision/box.h"

#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"

class effect_resource;

namespace resource
{
	template<>
	class loader<effect_resource>;
}	//	namespace resource

//	エフェクトリソース
class effect_resource : public resource::resource_base
{
	friend class resource::loader<effect_resource>;

public:
	static constexpr std::string_view extension = "eme";

public:
	static constexpr size_t invalid_id = -1;

	struct node
	{
		cache_string name;
		size_t parent_index = invalid_id;

		math::float3 scale = {1, 1, 1};
		math::float3 rotation = {0, 0, 0};
		math::float3 position = {0, 0, 0};

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct model
	{
		size_t node_index = -1;
		std::string file_path;
		model_resource_ptr resource;

		bool is_valid() const;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct decal
	{
		size_t node_index = -1;
		std::shared_ptr<material_resource> material;
		std::string material_path;

		bool is_valid() const;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct particle
	{
		size_t node_index = -1;
		::particle::data particle;

		bool is_valid() const;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	inline const std::vector<node>& get_nodes() const {return nodes;}
	inline const std::vector<model>& get_models() const {return models;}
	inline const std::vector<particle>& get_particles() const {return particles;}
	inline const std::vector<decal>& get_decals() const {return decals;}
	inline const std::vector<animation::data>& get_animations() const {return animations;}

#if defined(USE_IMGUI)
	inline std::vector<node>& get_nodes_for_editor() {return nodes;}
	inline std::vector<model>& get_models_for_editor() {return models;}
	inline std::vector<particle>& get_particles_for_editor() { return particles; }
	inline std::vector<decal>& get_decals_for_editor() {return decals;}
	inline std::vector<animation::data>& get_animations_for_editor() { return animations; }
#endif	//	defined(USE_IMGUI)

public:
	effect_resource();
	effect_resource(std::string_view file_path);
	~effect_resource();

	bool compute_animation_nodes(std::vector<animation::node>& nodes, size_t animation_index, float time) const;
	bool compute_animation_node(animation::node& node, size_t animation_index, size_t node_index, float time) const;

public:
	bool serialize();
	bool serialize(std::string_view file_path);

private:
	bool load_file(std::string_view file_path);

	void build();

private:
	static constexpr std::uint64_t file_signature = 0x454D455F48454144ull;
	static constexpr std::uint32_t file_format_version = 1;

	std::vector<node> nodes;					//	ノード情報
	std::vector<model> models;					//	モデル情報
	std::vector<particle> particles;			//	パーティクル情報
	std::vector<decal> decals;					//	デカール情報
	std::vector<animation::data> animations;	//	アニメーション情報
};

using effect_resource_ptr	= std::shared_ptr<effect_resource>;

namespace resource
{
	template<>
	class loader<effect_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		operation_state build() override;
		std::shared_ptr<void> get_resource() const override { return resource; }

	private:
		effect_resource_ptr resource;
		std::vector<load_operation_ptr<model_resource>> model_operations;
		std::vector<load_operation_ptr<material_resource>> decal_material_operations;
		std::vector<load_operation_ptr<material_resource>> particle_material_operations;
	};
}	//	namespace resource

class effect_resource_manager
{
	SINGLETON(effect_resource_manager)

private:
	effect_resource_manager();
	~effect_resource_manager();

public:
	bool load(effect_resource_ptr& resource, std::string_view file_path, bool force_load = false);
};