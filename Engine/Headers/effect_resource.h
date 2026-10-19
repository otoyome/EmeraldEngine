#pragma once

#include <filesystem>
#include <unordered_map>

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
		std::shared_ptr<material> material;

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
	inline void set_file_path(std::string_view path) { file_path = path; }
	inline std::string_view get_file_path() const {return file_path;}

	inline const std::vector<node>& get_nodes() const {return nodes;}
	inline const std::vector<model>& get_models() const {return models;}
	inline const std::vector<particle>& get_particles() const {return particles;}
	inline const std::vector<decal>& get_decals() const {return decals;}
	inline const std::vector<animation::data>& get_animations() const {return animations;}

	//	@tool 中身を書き換えた回数（インスタンスの作り直しの判定に使う）
	UINT get_content_revision() const { return content_revision; }
	//	@tool エディターで中身を書き換えた時に呼ぶ（各ワールドのインスタンスが次の更新で作り直される）
	void notify_content_changed() { ++content_revision; }

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
	bool deserialize();
	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);

	//	@tool ファイルを読み直して中身を差し替える（同じ shared_ptr のまま。メインスレッド専用）
	//	@tool 戻り値は差し替える前の中身を持つ物（失敗・未変更なら null）
	std::shared_ptr<effect_resource> reload();
	//	@tool 読み込んだ（または保存した）後にファイルが更新されたか
	bool is_file_changed() const;

private:
	void build();
	void update_file_write_time();

private:
	std::string file_path;
	std::filesystem::file_time_type file_write_time{};	//	@tool 読み込んだ（保存した）時のファイルの更新時刻
	UINT content_revision = 0;

	std::vector<node> nodes;					//	ノード情報
	std::vector<model> models;					//	モデル情報
	std::vector<particle> particles;			//	パーティクル情報
	std::vector<decal> decals;					//	デカール情報
	std::vector<animation::data> animations;	//	アニメーション情報
};

using effect_resource_ptr	= std::shared_ptr<effect_resource>;

class effect_resource_manager
{
	SINGLETON(effect_resource_manager)

	using effect_resource_map = std::unordered_map<std::string, std::weak_ptr<effect_resource>>;

private:
	effect_resource_manager();
	~effect_resource_manager();

public:
	bool load(effect_resource_ptr& resource, std::string_view file_path, bool force_load = false);

	//	@tool 更新されたファイルを読み込み済みのリソースへ読み直す（メインスレッド専用。読み直したら true）
	bool reload_file(const std::filesystem::path& file_path);
	//	@tool 読み直しで差し替えた古い中身を破棄する
	void release_retired_resources() { retired_resources.clear(); }

private:
	effect_resource_map resources;
	std::vector<std::shared_ptr<effect_resource>> retired_resources;
};
