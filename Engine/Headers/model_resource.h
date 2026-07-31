#pragma once

#include "texture.h"
#include "material.h"
#include "animation_resource.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "retarget_profile.h"

//	モデルリソース
class model_resource
{
public:
	static constexpr std::string_view default_exteision = "emo";

public:
	static constexpr size_t invalid_id = -1;

	struct node
	{
		std::string name;
		size_t parent_index = invalid_id;

		DirectX::XMFLOAT3 scale = { 1, 1, 1 };
		DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 };
		DirectX::XMFLOAT3 translation = { 0, 0, 0 };
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct bone
	{
		size_t node_index;
		DirectX::XMFLOAT4X4 offset_matrix = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct mesh
	{
		struct sub_mesh
		{
			std::unordered_map<semantic, buffer_view> vertex_buffer_views;
			buffer_view index_buffer_view;
			bool is_valid() const
			{
				return index_buffer_view.size_in_bytes > 0 &&
						vertex_buffer_views.size() > 0;
			}
			template<class Archive>
			void serialize(Archive& archive, int version);
		};
		struct morph_mesh
		{
			//	@remainder	コンピュートシェーダーでSparse分だけやるようにした方が圧倒的に早いし制限を取っ払う事ができる。
			std::unordered_map<semantic, buffer_view> vertex_buffer_views;
			bool is_valid() const
			{
				return vertex_buffer_views.size() > 0;
			}
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		std::vector<bone> bones;
		size_t material_index = invalid_id;
		size_t node_index = invalid_id;

		std::unordered_map<semantic, buffer_view> vertex_buffer_views;
		buffer_view index_buffer_view;

		std::vector<morph_mesh> morph_meshes;
		std::unordered_map<std::string, size_t> morph_indices;

		sub_mesh shadow_mesh;
		std::vector<sub_mesh> lod_meshs;

		DirectX::XMFLOAT3 bounds_min;
		DirectX::XMFLOAT3 bounds_max;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct morph
	{
		struct morph_part_data
		{
			std::string mesh_name;
			template<class Archive>
			void serialize(Archive& archive, int version);
		};
		std::string name;
		std::vector<morph_part_data> parts;
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct capsule_shadow
	{
		size_t node_index;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 center;
		float radius;
		float length;
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	inline void set_file_path(std::string_view path) { file_path = path; }
	inline std::string_view get_file_path() const {return file_path;}

	inline const std::vector<material>& get_materials() const {return materials;}
	inline const std::vector<node>& get_nodes() const {return nodes;}
	inline const std::vector<mesh>& get_meshes() const {return  meshes;}
	inline const std::vector<morph>& get_morphes() { return morphes; }
	inline const std::vector<animation::data>& get_animations() const {return animations;}
	inline const std::vector<capsule_shadow>& get_capsule_shadows() const {return capsule_shadows;}

#if defined(USE_IMGUI)
	inline std::vector<material>& get_materials_for_editor() { return materials; }
	inline std::vector<node>& get_nodes_for_editor() { return nodes; }
	inline std::vector<mesh>& get_meshes_for_editor() { return meshes; }
	inline std::vector<morph>& get_morphes_for_editor() { return morphes; }
	inline std::vector<animation::data>& get_animations_for_editor() { return animations; }
	inline std::vector<capsule_shadow>& get_capsule_shadows_for_editor() {return capsule_shadows;}
#endif	//	defined(USE_IMGUI)

public:
	model_resource();
	model_resource(std::string_view file_path);
	~model_resource();

	bool compute_animation_nodes(std::vector<animation::node>& nodes, size_t animation_index, float time) const;
	bool compute_animation_node(animation::node& node, size_t animation_index, size_t node_index, float time) const;

	size_t get_animation_index_from_name(std::string_view name) const;
	size_t get_node_index_from_name(std::string_view name) const;
	size_t get_mesh_index_from_name(std::string_view name) const;

public:
	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);

	void paste_materials(const std::vector<material>& materials);

public:	//	メッシュ最適化
	void optimized_meshs();
	void generate_shadow_meshes();
	void generate_lod_meshs(int max_lod_level, bool regeneratge_buffer = true);

private:
	void build_model();

private:
	std::string file_path;

	std::vector<material> materials;
	std::vector<node> nodes;
	std::vector<mesh> meshes;
	std::vector<morph> morphes;
	std::vector<animation::data> animations;
	std::vector<capsule_shadow> capsule_shadows;
};

using model_resource_ptr	= std::shared_ptr<model_resource>;

class model_resource_manager
{
	SINGLETON(model_resource_manager)

private:
	model_resource_manager();
	~model_resource_manager();

public:
	bool load(model_resource_ptr& resource, std::string_view file_path);

};
