#pragma once

#include <filesystem>
#include <unordered_map>

#include "texture.h"
#include "material.h"
#include "animation_resource.h"
#include "collision/capsule.h"
#include "collision/box.h"
#include "buffer_view.h"

#include "retarget_profile.h"

//	モデルリソース
class model_resource
{
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
			std::vector<pooled_meshlet> pooled_meshlets;

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
		std::vector<pooled_meshlet> pooled_meshlets;

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
	inline void set_file_path(std::string_view path) { file_path = path; }
	inline std::string_view get_file_path() const {return file_path;}

	inline const std::vector<material>& get_materials() const {return materials;}
	inline const std::vector<node>& get_nodes() const {return nodes;}
	inline const std::vector<mesh>& get_meshes() const {return  meshes;}
	inline const std::vector<morph>& get_morphes() { return morphes; }
	inline const std::vector<animation::data>& get_animations() const {return animations;}
	inline const std::vector<capsule_shadow>& get_capsule_shadows() const {return capsule_shadows;}
	//	@tool メッシュを書き換えた回数（描画用プロキシの作り直しの判定に使う）
	UINT get_mesh_revision() const { return mesh_revision; }
	//	@tool 中身（ノード・メッシュ・モーフ・マテリアル数など）を差し替えた回数（インスタンスの作り直しの判定に使う）
	UINT get_content_revision() const { return content_revision; }
	//	@tool ノード・メッシュ・モーフの配列を作り直した時に呼ぶ（各ワールドのインスタンスが次の抽出で作り直される）
	void notify_content_changed() { ++mesh_revision; ++content_revision; }
	//	@tool 抽出したフレームがメッシュを指している印を付ける（メインスレッドの抽出で呼ぶ。get_meshes_for_editor で複製するかの判定に使う）
	void mark_meshes_in_use() const { meshes_in_use = true; }

#if defined(USE_IMGUI)
	inline std::vector<material>& get_materials_for_editor() { return materials; }
	inline std::vector<node>& get_nodes_for_editor() { return nodes; }
	//	@tool メッシュを書き換える場合はここから取得する（描画中のフレームが指す配列は残して複製を返し、描画用プロキシとインスタンスを作り直させる）
	std::vector<mesh>& get_meshes_for_editor();
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

	//	@tool ファイルを読み直して中身を差し替える（同じ shared_ptr のまま。描画中のフレームの完了を待ってから差し替える）
	//	@tool 戻り値は差し替える前の中身を持つ物。インスタンスが作り直されるまで生かしておくこと（失敗・未変更なら null）
	std::shared_ptr<model_resource> reload();
	//	@tool 読み込んだ（または保存した）後にファイルが更新されたか
	bool is_file_changed() const;

	void paste_materials(const std::vector<material>& materials);

	//	@tool メッシュの頂点データ（ある属性の row_data 全体）を書き換えて、GPU 側（モデル用の共有プール）へ反映する
	//	@tool 頂点数・形式は変えない前提なので、描画の完了待ちや描画用プロキシの作り直しは要らない。大きさが違う場合は書き換えない
	bool update_vertex_attribute(size_t mesh_index, const semantic& target_semantic, const void* data, size_t size_in_bytes);

	void rebuild_meshlet_data();

private:
	void build_model();
	void update_file_write_time();

private:
	std::string file_path;
	std::filesystem::file_time_type file_write_time{};	//	@tool 読み込んだ（保存した）時のファイルの更新時刻

	std::vector<material> materials;
	UINT mesh_revision = 0;
	mutable bool meshes_in_use = false;	//	@tool 抽出したフレームがメッシュを指しているか
	UINT content_revision = 0;
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

	using model_resource_map = std::unordered_map<std::string, std::weak_ptr<model_resource>>;

private:
	model_resource_manager();
	~model_resource_manager();

public:
	bool load(model_resource_ptr& resource, std::string_view file_path, bool force_load = false);

	//	@tool 更新されたファイルを読み込み済みのリソースへ読み直す（メインスレッド専用。読み直したら true）
	//	@tool 差し替えた古い中身は render_data::retire_after_rendering で、描画中のフレームが使い終わってから破棄する
	bool reload_file(const std::filesystem::path& file_path);

private:
	model_resource_map resources;
};
