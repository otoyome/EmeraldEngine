#pragma once

#include "model_resource.h"
#include "collision/frustum_culling.h"

#include "collision/sphere.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "Math/transform.h"

//	モデルインスタンス
class model_instance
{
public:
	static constexpr size_t invalid_id = -1;

	enum class flags
	{
		active,						//	表示フラグ
		color_render,				//	色描画
		shadow_caster,				//	影生成
		culling,					//	カリング
		self_shadow_off,			//	セルフシャドウオフ
		use_capsule_shadow,			//	カプセルシャドウ使用
	};

	struct node
	{
		std::string name;
		bool dirty = true;
		size_t parent_index = invalid_id;

		math::float4x4 local_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
		math::float4x4 global_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

		math::float4x4 previous_global_transform = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
	};

	struct morph
	{
		const model_resource::morph* morph;
		float weight = 0;
	};

	struct mesh
	{
		const model_resource::mesh* mesh;
		bool visible;
	};

	using node_animation = animation::node;
	using morph_animation = animation::morph;

public:
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	int get_lod_index() const {return lod_index;}
	UINT get_unique_id() const { return unique_id; }
	void set_lod_index(int lod_index) {this->lod_index = lod_index;}
	int get_lod_max() const{return meshes.empty() ? 0 : meshes.begin()->mesh ? static_cast<int>(meshes.begin()->mesh->lod_meshs.size()) : 0;}
	const math::float2& get_lod_distance_range() const {return lod_distance_range;}

	void set_lod_distance_range(math::float2 range) {lod_distance_range.x = math::max(range.x, 0.0f); lod_distance_range.y = math::max(range.y, lod_distance_range.x);}

	inline const model_resource_ptr get_resource() const { return resource; }
	inline const std::vector<node>& get_nodes() const { return nodes; }
	inline std::vector<node>& get_nodes() { return nodes; }
	inline const std::vector<material>& get_materials() const { return materials; }
	inline std::vector<material>& get_materials() { return materials; }
	inline const std::vector<mesh>& get_meshes() const { return meshes; }
	inline std::vector<mesh>& get_meshes() { return meshes; }
	inline const std::vector<morph>& get_morphes() const { return morphes; }
	inline std::vector<morph>& get_morphes() { return morphes; }
	//	@tool ワールド姿勢（double）。描画時に描画の原点からの相対行列へ変換する
	const math::transformd& get_world() const { return world; }
	const math::transformd& get_previous_world() const { return previous_world; }
	void set_previous_world(const math::transformd& world) { previous_world = world; }

	//	@tool ワールドの絶対座標での float 行列（原点から遠いと精度が落ちる。IK・エディタの移行が済むまでの互換用）
	math::float4x4 get_world_transform() const { return world.to_relative_matrix(math::vector3d::zero()).to_float4x4(); }

#if defined(USE_IMGUI)
	inline model_resource_ptr get_resource_for_editor() { return resource; }
	inline void set_materials_for_editor(const std::vector<material>& materials) { this->materials = materials; }
#endif	//	defined(USE_IMGUI)

	size_t get_node_index_by_name(std::string_view node_name) const;

public:
	model_instance();
	model_instance(model_resource_ptr resource);
	~model_instance();

	bool compute_animation_nodes(std::vector<node_animation>& nodes, size_t animation_index, float time) const;
	bool compute_animation_node(node_animation& node, size_t animation_index, size_t node_index, float time) const;
	void set_node_poses(const std::vector<node_animation>& nodes);
	void set_node_pose(size_t node_index, const node_animation& node);
	bool set_morph(std::string_view morph_name, const morph_animation& morph);
	//	@tool ワールド姿勢を設定し、ノードのグローバル行列を更新する
	void update_transform(const math::transformd& world);
	//	@tool ワールド姿勢は変えずに、ノードのグローバル行列（モデル空間）だけを更新する
	void update_node_transforms();

private:
	void build_model();

private:
	UINT system_flags;
	int lod_index;
	UINT unique_id = 0;
	math::float2 lod_distance_range = {0.0f, 10000.0f};
	model_resource_ptr resource;
	math::transformd world;				//	@tool ワールド姿勢
	math::transformd previous_world;	//	@tool 前フレームのワールド姿勢（モーションベクトル用）

	std::vector<material> materials;
	std::vector<node> nodes;
	std::vector<mesh> meshes;
	std::vector<morph> morphes;

	inline static std::atomic<UINT> id_counter;
};

using model_instance_ptr = std::shared_ptr<model_instance>;

//	モデルインスタンスマネージャー
class model_instance_manger
{
	SINGLETON(model_instance_manger)

	using	model_instances	= std::vector<std::weak_ptr<model_instance>>;

public:
	inline const model_instances& get_instances() const { return instances; }

private:
	model_instance_manger();
	~model_instance_manger();

public:
	void registered(std::weak_ptr<model_instance> instance);

	//	@tool ワールド行列は render_origin からの相対行列として作る
	void get_container(mesh_container& container, const math::vector3d& render_origin);

private:
	model_instances instances;
};
