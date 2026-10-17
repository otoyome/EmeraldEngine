#pragma once

#include "model_resource.h"
#include "collision/frustum_culling.h"

#include "collision/sphere.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "Math/transform.h"

//	@tool モデルインスタンスの描画用プロキシ（構成が変わった時だけメインスレッドで作り直し、変更しない）
//	@tool 毎フレーム変わる行列・モーフ重み・ボーンは抽出時に求め、変わらない部分（LOD・頂点属性・マテリアル・影メッシュ）だけを持つ
//	@tool 描画側はインスタンスではなくこれ（と resource）を参照する
struct model_render_proxy
{
	struct mesh_entry
	{
		const model_resource::mesh* mesh = nullptr;
		UINT unique_id = 0;
		UINT system_flags = 0;									//	buffer_datas::flags
		std::vector<buffer_datas::sub_mesh> color_meshs;		//	LOD 込み（モーフは抽出時に先頭へ足す）
		buffer_datas::sub_mesh shadow_mesh;						//	影を生成しない場合は vertex_buffer_views が null
		bool shadow_caster = false;
		common_pipeline_states::primitive_header header{};		//	頂点属性の情報（モーフ・ボーンは抽出時に埋める）
		std::shared_ptr<const material_render_proxy> material;
	};

	//	@tool 作り直しの判定に使う構成情報
	model_resource_ptr resource;
	UINT resource_mesh_revision = 0;	//	@tool リソースのメッシュを書き換えた回数（エディターでの LOD 生成など）
	int lod_index = -1;
	UINT system_flags = 0;
	std::vector<bool> mesh_visibles;
	std::vector<std::shared_ptr<const material_render_proxy>> materials;

	std::vector<mesh_entry> meshes;
};

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
	inline const std::vector<mesh>& get_meshes() const { return meshes; }
	inline std::vector<mesh>& get_meshes() { return meshes; }
	inline const std::vector<morph>& get_morphes() const { return morphes; }
	inline std::vector<morph>& get_morphes() { return morphes; }

	//	@tool マテリアル（既定ではリソースのものを全インスタンスで共有する）
	//	@tool インスタンス毎に変えたい場合だけ上書き用の複製を作る（描画側のプロキシも共有分は1つで済む）
	size_t get_material_count() const;
	const material& get_material(size_t index) const;

	//	@tool インスタンス専用のマテリアルを取得する（無ければリソースのマテリアルを複製して作る）
	material& get_override_material(size_t index);
	void set_override_material(size_t index, const material& value);
	//	@tool 上書きをやめてリソースのマテリアルに戻す
	void reset_override_material(size_t index);
	bool is_material_overridden(size_t index) const;

	//	@tool 描画用プロキシを取得する（メインスレッド専用。構成が変わっていなければ前回のものを返す）
	std::shared_ptr<const model_render_proxy> get_render_proxy() const;

	const math::transformd& get_world() const { return world; }
	const math::transformd& get_previous_world() const { return previous_world; }
	void set_previous_world(const math::transformd& world) { previous_world = world; }

	math::float4x4 get_world_transform() const { return world.to_relative_matrix(math::vector3d::zero()).to_float4x4(); }

#if defined(USE_IMGUI)
	inline model_resource_ptr get_resource_for_editor() { return resource; }
#endif	//	defined(USE_IMGUI)

	size_t get_node_index_by_name(std::string_view node_name) const;

public:
	model_instance();
	model_instance(model_resource_ptr resource);
	~model_instance();

	//	@tool 上書き用マテリアルは個別に持つのでコピーはできない（ムーブのみ）
	model_instance(model_instance&&) = default;
	model_instance& operator=(model_instance&&) = default;

	bool compute_animation_nodes(std::vector<node_animation>& nodes, size_t animation_index, float time) const;
	bool compute_animation_node(node_animation& node, size_t animation_index, size_t node_index, float time) const;
	void set_node_poses(const std::vector<node_animation>& nodes);
	void set_node_pose(size_t node_index, const node_animation& node);
	bool set_morph(std::string_view morph_name, const morph_animation& morph);
	void update_transform(const math::transformd& world);
	void update_node_transforms();

private:
	void build_model();
	bool is_render_proxy_valid(const model_render_proxy& proxy) const;

private:
	UINT system_flags;
	int lod_index;
	UINT unique_id = 0;
	math::float2 lod_distance_range = {0.0f, 10000.0f};
	model_resource_ptr resource;
	math::transformd world;
	math::transformd previous_world;

	//	@tool 上書き用のマテリアル（null はリソースのものを使う）。生成後に場所が変わらないよう個別に確保する
	std::vector<std::unique_ptr<material>> override_materials;

	//	@tool 描画用プロキシ（メインスレッドでのみ作り直す）
	mutable std::shared_ptr<const model_render_proxy> render_proxy;
	std::vector<node> nodes;
	std::vector<mesh> meshes;
	std::vector<morph> morphes;

	inline static std::atomic<UINT> id_counter;
};

using model_instance_ptr = std::shared_ptr<model_instance>;

//	モデルインスタンスマネージャー
class model_instance_manger
{
	WORLD_SINGLETON(model_instance_manger)

	using	model_instances	= std::vector<std::weak_ptr<model_instance>>;

public:
	inline const model_instances& get_instances() const { return instances; }

private:
	model_instance_manger();
	~model_instance_manger();

public:
	void registered(std::weak_ptr<model_instance> instance);

	void get_container(mesh_container& container, const math::vector3d& render_origin);

private:
	model_instances instances;

	//	@tool 変換行列のバッファ（同時処理フレーム数より多く持ち、順番に使う）
	struct transform_buffer_set
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_header_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> primitive_header_srv;
		UINT primitive_header_capacity = 0;
		Microsoft::WRL::ComPtr<ID3D11Buffer> bone_matrix_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bone_matrix_srv;
		UINT bone_matrix_capacity = 0;
	};
	static constexpr UINT transform_buffer_set_count = 3;
	transform_buffer_set transform_buffer_sets[transform_buffer_set_count];
	UINT transform_buffer_set_index = 0;
};
