#pragma once

#include "texture.h"
#include "material.h"

#include "model_resource.h"
#include "animation_resource.h"
#include "graphics/particle_resource.h"

#include "collision/capsule.h"
#include "collision/box.h"

//	エフェクトリソース
class effect_resource
{
public:
	static constexpr std::string_view default_exteision = "eme";

public:
	static constexpr size_t invalid_id = -1;

	struct model_data
	{
		std::string name;
		std::string file_path;
		DirectX::XMFLOAT3 scale = {1, 1, 1};
		DirectX::XMFLOAT4 rotation = {0, 0, 0, 1};
		DirectX::XMFLOAT3 position = {0, 0, 0};

		model_resource_ptr resource;

		bool is_valid() const;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct decal_data
	{
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	inline void set_file_path(std::string_view path) { file_path = path; }
	inline std::string_view get_file_path() const {return file_path;}

	inline const std::vector<model_data>& get_models() const {return models;}
	inline const std::vector<particle::data>& get_particles() const {return particles;}
	inline const std::vector<decal_data>& get_decals() const {return decals;}
	inline const std::vector<animation::data>& get_animations() const {return animations;}

#if defined(USE_IMGUI)
	inline std::vector<model_data>& get_models_for_editor() {return models;}
	inline std::vector<particle::data>& get_particles_for_editor() { return particles; }
	inline std::vector<decal_data>& get_decals_for_editor() {return decals;}
	inline std::vector<animation::data>& get_animations_for_editor() { return animations; }
#endif	//	defined(USE_IMGUI)

public:
	effect_resource();
	effect_resource(std::string_view file_path);
	~effect_resource();

	bool compute_animation_nodes(std::vector<animation::node>& nodes, size_t animation_index, float time) const;
	bool compute_animation_node(animation::node& node, size_t animation_index, size_t node_index, float time) const;

	size_t get_animation_index_from_name(std::string_view name) const;

public:
	bool serialize();
	bool deserialize();
	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);

private:
	void build();

private:
	std::string file_path;

	std::vector<model_data> models;	//	モデル情報
	std::vector<particle::data> particles;	//	パーティクル情報
	std::vector<decal_data> decals;	//	デカール情報
	std::vector<animation::data> animations;	//	アニメーション情報
};

using effect_resource_ptr	= std::shared_ptr<effect_resource>;

class effect_resource_manager
{
	SINGLETON(effect_resource_manager)

private:
	effect_resource_manager();
	~effect_resource_manager();

public:
	bool load(effect_resource_ptr& resource, std::string_view file_path);
};
