#pragma once

#include <unordered_map>

#include "texture.h"
#include "material_resource.h"
#include "animation_resource.h"
#include "collision/capsule.h"
#include "collision/box.h"
#include "buffer_view.h"

#include "retarget_profile.h"

#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"
#include "Resource/asset_reference.h"

class model_resource;

namespace resource
{
	template<>
	class loader<model_resource>;
}	//	namespace resource

//	モデルリソース
class model_resource : public resource::resource_base
{
	friend class resource::loader<model_resource>;

public:
	static constexpr std::string_view extension = "emo";

public:
	static constexpr size_t invalid_id = -1;

	struct node
	{
		std::string name;
		size_t parent_index = invalid_id;

		math::float3 scale = { 1, 1, 1 };
		math::float4 rotation = { 0, 0, 0, 1 };
		math::float3 translation = { 0, 0, 0 };
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct bone
	{
		size_t node_index;
		math::float4x4 offset_matrix = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct mesh
	{
		//	メッシュレット
		struct meshlet
		{
			UINT vertex_offset = 0;
			UINT triangle_offset = 0;
			UINT vertex_count = 0;
			UINT triangle_count = 0;

			//	バウンディングスフィア
			math::float3 bounds_center = { 0, 0, 0 };
			float bounds_radius = 0.0f;

			//	法線コーン
			math::float3 cone_apex = { 0, 0, 0 };
			math::float3 cone_axis = { 0, 0, 0 };
			float cone_cutoff = 0.0f;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		struct sub_mesh
		{
			std::unordered_map<semantic, buffer_view> vertex_buffer_views;
			buffer_view index_buffer_view;

			std::vector<meshlet> meshlets;
			std::vector<UINT> meshlet_vertices;
			std::vector<uint8_t> meshlet_triangles;

			//	以下、シリアライズ対象外データ
			UINT meshlet_vertex_base = 0;
			UINT meshlet_triangle_base = 0;
			std::vector<meshlet_draw_data> meshlet_draw_infos;

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

		math::float3 bounds_min;
		math::float3 bounds_max;

		std::vector<meshlet> meshlets;
		std::vector<UINT> meshlet_vertices;
		std::vector<uint8_t> meshlet_triangles;

		//	以下、シリアライズ対象外データ
		UINT meshlet_vertex_base = 0;
		UINT meshlet_triangle_base = 0;
		std::vector<meshlet_draw_data> meshlet_draw_infos;

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
		math::float4 rotation;
		math::float3 center;
		float radius;
		float length;
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	inline const std::vector<material_resource_ptr>& get_materials() const {return materials;}
	inline const std::vector<node>& get_nodes() const {return nodes;}
	inline const std::vector<mesh>& get_meshes() const {return  meshes;}
	inline const std::vector<morph>& get_morphes() { return morphes; }
	inline const std::vector<animation::data>& get_animations() const {return animations;}
	inline const std::vector<capsule_shadow>& get_capsule_shadows() const {return capsule_shadows;}

#if defined(USE_IMGUI)
	inline std::vector<material_resource_ptr>& get_materials_for_editor() { return materials; }
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
	//	マテリアルは .emr として保存し、パスで参照する
	bool serialize(std::string_view file_path);

	void paste_materials(const std::vector<material_resource_ptr>& materials);

	void rebuild_meshlet_data();

private:
	//	ファイル読込(ワーカースレッドから呼ばれる)。参照するマテリアルのパスを返す
	//	旧形式(マテリアルを中に持つ)の場合はパスが空になり、materials に読み込まれる
	bool load_file(std::string_view file_path, std::vector<resource::asset_reference<material_resource>>& out_material_references);

	void build_model();

private:
	static constexpr std::uint64_t file_signature = 0x454D4F5F4D415452ull;
	static constexpr std::uint32_t file_format_version = 3;	//	@tool 3:UUID はファイル先頭のヘッダーへ移動し、マテリアルを UUID で参照

	std::vector<material_resource_ptr> materials;
	std::vector<node> nodes;
	std::vector<mesh> meshes;
	std::vector<morph> morphes;
	std::vector<animation::data> animations;
	std::vector<capsule_shadow> capsule_shadows;
};

using model_resource_ptr	= std::shared_ptr<model_resource>;

namespace resource
{
	template<>
	class loader<model_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		operation_state build() override;
		std::shared_ptr<void> get_resource() const override { return resource; }

	private:
		model_resource_ptr resource;
		std::vector<asset_reference<material_resource>> material_references;
		std::vector<load_operation_ptr<material_resource>> material_operations;
	};
}	//	namespace resource

class model_resource_manager
{
	SINGLETON(model_resource_manager)

private:
	model_resource_manager();
	~model_resource_manager();

public:
	bool load(model_resource_ptr& resource, std::string_view file_path, bool force_load = false);
};
