#pragma once

#include <map>

#include "texture.h"
#include "texture_resource.h"
#include "pipeline_state.h"

#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"
#include "Resource/resource_saver.h"
#include "Resource/asset_reference.h"
#include "Resource/asset_handle.h"
#include "Resource/resource_operation.h"

class gpu_upload_list;
class material_resource;

namespace resource
{
	template<>
	class loader<material_resource>;

	template<>
	class saver<material_resource>;
}	//	namespace resource

//	マテリアルリソース
class material_resource : public resource::resource_base
{
	friend class material_instance;
	friend class resource::loader<material_resource>;
	friend class resource::saver<material_resource>;

public:
	static constexpr std::string_view extension = "emr";

public:
	//	マテリアルの処理系統
	using domains = pipeline_state_base::domains;

	//	表面描画パス
	enum class surface_paths : std::uint8_t
	{
		depth_write_pass,	//	深度書き込みパス
		g_buffer_pass,		//	G-Buffer生成パス
		shadow_write_pass,	//	シャドウマップ生成パス
		forward_pass,		//	フォワードレンダリングパス
	};

	//	パーティクル描画パス
	enum class particle_paths : std::uint8_t
	{
		depth_write_pass,	//	深度書き込みパス
		g_buffer_pass,		//	G-Buffer生成パス
		forward_pass,		//	フォワードレンダリングパス
	};

	//	デカール描画パス
	enum class decal_paths : std::uint8_t
	{
		primitive,	//	プリミティブでの書き込み
		fullscreen,	//	フルスクリーンでの書き込み
	};

	//	システムのマテリアルをホットリロードさせたいため追加。
	//	本来は遅くなるのでリリース時は不要
	enum class system_paths : std::uint8_t
	{
		//	遅延ライティング
		deferred_indirect,		//	間接光
		deferred_directional,	//	平行光源
		deferred_point,			//	点光源
		deferred_spot,			//	スポットライト
	};

	//	surface 用計算モード
	enum class shading_models : std::uint8_t
	{
		unlit,			//	ライティング無し
		default_lit,	//	基本の計算

		//	[エンジン改造:シェーディング]
		ggst_lit,		//	GGSTシェーディング用計算
		toon_lit,		//	サンプルのレンダリング用計算
		//	[エンジン改造:シェーディング]
	};

	//	surface 用ブレンドモード
	enum class blend_modes : std::uint8_t
	{
		opaque,			//	不透明描画
		mask,			//	不透明描画+マスク
		translucent,	//	半透明描画

		addative,		//	加算合成

//		modulate,		//	変調(乗算)

		unknown,
	};

	//	post process 用描画タイミング
	using blendable_locations = blendable_locations;

	//	半透明 surface 用描画タイミング
	using translucency_paths = translucency_paths;

public:
	using constant_buffer_desc = pipeline_state_base::constant_buffer_view_desc;
	using variable_desc = constant_buffer_desc::variable_desc;
	using resource_view_desc = pipeline_state_base::resource_view_desc;
	using input_resource_desc = pipeline_state_base::input_resource_desc;
	using variable_kind = pipeline_state_base::variable_kinds;
	using variable_type = pipeline_state_base::variable_types;

	struct pipeline_state
	{
		std::string name{ "" };
		graphics_pipeline_state_ptr value{ nullptr };
		bool required = true;
		std::uint32_t reload_version = 0;

		bool is_valid() const { return value != nullptr && !name.empty(); }

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct constant_buffer
	{
		struct subset
		{
			UINT start_offset;
			UINT size;
			UINT stride;
			variable_kind kind;
			variable_type type;
			char padding1[2];

			UINT rows;
			UINT columns;
			struct
			{
				struct
				{
					float min = -FLT_MAX;
					float max = +FLT_MAX;
				} float_range;
				struct
				{
					int min = INT_MIN;
					int max = INT_MAX;
				} int_range;
				struct
				{
					int min = 0;
					int max = INT_MAX;
				} uint_range;
			} editor_info;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};
		bool dirty = true;
		std::vector<unsigned char> data_buffer;						//	バッファ情報
		std::vector<subset> subsets;								//	識別子オフセット
		std::unordered_map<std::string, std::size_t> identifieres;	//	識別子事の番号
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;				//	定数バッファリソース

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct shader_resource
	{
		struct resource
		{
			::resource::asset_handle<texture_resource> asset;
			texture_ptr get_texture() const { return asset ? asset->get_texture() : nullptr; }

			bool is_valid() const
			{
				return get_texture() != nullptr;
			}

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		std::vector<resource> resources;	//	リソース情報
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	enum class flags : std::uint8_t
	{
		shadow_caster,			//	影を生成するマテリアルかどうか
		shadow_receiver,		//	影を受けるマテリアルかどうか
		two_sided,				//	両面描画フラグ
		wire,					//	ワイヤーフレームで表示するかどうか
		indirect_lighting_off,	//	間接光の影響を強制的に受けなくさせる
		decal_off,				//	デカールが書き込みされるマテリアルかどうか

		//	[エンジン改造]
		//	シェーダー側との連携用フラグは最大32個まで
		//	追加する場合はシェーダーとの連携用コードがあるので修正必要
		ligting_xz_plane,		//	[エンジン改造:ライティング計算時にXZ平面で適応]
		//	[エンジン改造]

		disable_depth_test,		//	深度テストを行わない(半透明・加算のみ)
	};

	//	物理マテリアル
	struct physical_datas
	{
		float friction{ 0.5f };
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	using pipeline_state_array = std::vector<pipeline_state>;
	using constant_buffer_map = std::unordered_map<std::string, constant_buffer>;
	using constant_buffer_desc_map = std::unordered_map<std::string, constant_buffer_desc>;
	using shader_resource_map = std::unordered_map<std::string, shader_resource>;

public:
	material_resource();
	material_resource(std::string_view name);
	material_resource(const material_resource& material);
	material_resource(material_resource&& material);
	~material_resource();

	void flush_constant_buffers(gpu_upload_list& upload_list);

	template<class Archive>
	void serialize(Archive& archive, int version);

	void duplicate(material_resource& material) const;

	void build();

	void sync_shader_reloads();

	material_resource& operator =(const material_resource& material);

private:
	bool load_file(std::string_view file_path);
	bool save_file(std::string_view file_path);

public:
	//	名前取得・設定
	void set_name(std::string_view name);
	std::string_view get_name() const;

	//	ドメイン設定
	domains get_domain() const;
	void set_domain(domains domain);

	//	ブレンドモード設定
	blend_modes get_blend_mode() const;
	void set_blend_mode(blend_modes blend_mode);

	//	シェーディングモデル設定
	shading_models get_shading_model() const;
	void set_shading_model(shading_models shading_model);

	//	ブレンドロケーションの取得/設定
	blendable_locations get_blendable_location() const;
	void set_blendable_location(blendable_locations location);

	//	半透明描画タイミングの取得/設定
	translucency_paths get_translucency_path() const;
	void set_translucency_path(translucency_paths pass);

	//	システムフラグ類設定
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value);

	//	アルファクリップの閾値取得/設定
	float get_alpha_clip_threshold() const { return alpha_clip_threshold; }
	void set_alpha_clip_threshold(float value){ alpha_clip_threshold = value; }

	//	物理マテリアルの取得/設定
	inline const physical_datas& get_physical_data() const { return physical_data; }
	void set_physical_data(const physical_datas& value) { physical_data = value; }

	//	マテリアルシェーダー(Shaders/Materials 以下)の取得/設定
	//	設定するとパイプラインステートはシェーダーパス・静的パラメーターから自動で決まる
	void set_shader_path(std::string_view shader_path);
	std::string_view get_shader_path() const { return shader_path; }

	//	静的スイッチの取得/設定
	void set_static_switch(std::string_view name, bool value);
	bool get_static_switch(std::string_view name) const;
	const std::map<std::string, bool>& get_static_switches() const { return static_switches; }

	//	パイプラインステート取得・設定
	bool set_pipeline_state(system_paths type, std::string_view pipeline_state_name);

	graphics_pipeline_state_ptr get_pipeline_state(surface_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state(particle_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state(decal_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state(system_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state() const;

	//	バージョン取得
	std::uint32_t get_version() const { return version; }

public:
	//	パラメーター群取得
	const constant_buffer_map& get_constant_buffers() const;
	const shader_resource_map& get_shader_resources() const;

#if defined(USE_IMGUI)
	constant_buffer_map& get_constant_buffers_for_editor();
	shader_resource_map& get_shader_resources_for_editor();

	//	エラー判定
	bool is_error() const;
#endif	//	defined(USE_IMGUI)

public:
	//	パラメーター設定/取得
	//	配列の場合は名前の後に[n]でアクセス可能
	//	例)names[0] という感じで
	bool set_data(std::string_view key_name, void* data, std::size_t data_size);
	bool get_data(std::string_view key_name, void* data, std::size_t data_size) const;

	bool get_int(std::string_view key_name, int& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_int2(std::string_view key_name, math::int2& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_int3(std::string_view key_name, math::int3& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_int4(std::string_view key_name, math::int4& value) const {return get_data(key_name, &value, sizeof(value));}
	bool set_int(std::string_view key_name, int value) {return set_data(key_name, &value, sizeof(value));}
	bool set_int2(std::string_view key_name, math::int2 value) {return set_data(key_name, &value, sizeof(value));}
	bool set_int3(std::string_view key_name, math::int3 value) {return set_data(key_name, &value, sizeof(value));}
	bool set_int4(std::string_view key_name, math::int4 value) {return set_data(key_name, &value, sizeof(value));}

	bool set_uint(std::string_view key_name, UINT value) {return set_data(key_name, &value, sizeof(value));}
	bool set_uint2(std::string_view key_name, math::uint2 value) {return set_data(key_name, &value, sizeof(value));}
	bool set_uint3(std::string_view key_name, math::uint3 value) {return set_data(key_name, &value, sizeof(value));}
	bool set_uint4(std::string_view key_name, math::uint4 value) {return set_data(key_name, &value, sizeof(value));}
	bool get_uint(std::string_view key_name, UINT& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_uint2(std::string_view key_name, math::uint2& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_uint3(std::string_view key_name, math::uint3& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_uint4(std::string_view key_name, math::uint4& value) const {return get_data(key_name, &value, sizeof(value));}

	bool set_float(std::string_view key_name, float value) {return set_data(key_name, &value, sizeof(value));}
	bool set_float2(std::string_view key_name, math::float2 value) {return set_data(key_name, &value, sizeof(value));}
	bool set_float3(std::string_view key_name, math::float3 value) {return set_data(key_name, &value, sizeof(value));}
	bool set_float4(std::string_view key_name, math::float4 value) {return set_data(key_name, &value, sizeof(value));}
	bool get_float(std::string_view key_name, float& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_float2(std::string_view key_name, math::float2& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_float3(std::string_view key_name, math::float3& value) const {return get_data(key_name, &value, sizeof(value));}
	bool get_float4(std::string_view key_name, math::float4& value) const {return get_data(key_name, &value, sizeof(value));}

	bool set_float4x4(std::string_view key_name, math::float4x4 value) {return set_data(key_name, &value, sizeof(value));}
	bool get_float4x4(std::string_view key_name, math::float4x4& value) const {return get_data(key_name, &value, sizeof(value));}

	bool set_texture(std::string_view key_name, texture_resource_ptr value);
	bool get_texture(std::string_view key_name, texture_ptr& value) const;

private:
	bool set_pipeline_state(domains domain, UINT index, std::string_view pipeline_state_name);
	graphics_pipeline_state_ptr get_pipeline_state(domains domain, UINT index) const;

	void serialize_stream(std::stringstream& ostream) const;
	void deserialize_stream(std::stringstream& istream);

	void build_parameters();

	//	マテリアルシェーダーから各パスのパイプラインステートを取り直す
	void refresh_pipeline_states();

	//	パイプラインステートを差し替える
	void assign_pipeline_state(UINT index, std::string_view name, graphics_pipeline_state_ptr value, bool required);

	//	定数バッファのリソースを作る
	static void create_constant_buffer_resource(constant_buffer& target);

private:
	std::string name;
	domains domain;
	blend_modes blend_mode;
	shading_models shading_model;
	blendable_locations blendable_location;
	translucency_paths translucency_path;
	UINT system_flags;
	float alpha_clip_threshold;

	physical_datas physical_data;
	pipeline_state_array pipeline_states;
	std::string shader_path;
	std::map<std::string, bool> static_switches;
	shader_resource_map shader_resources;
	constant_buffer_map constant_buffers;

	std::uint32_t version = 0;
	std::uint32_t shader_map_version = 0;

#if defined(USE_IMGUI)
	mutable bool pipeline_states_verified = false;
#endif	//	defined(USE_IMGUI)
};

using material_resource_ptr = std::shared_ptr<material_resource>;

namespace resource
{
	template<>
	class loader<material_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		operation_state build() override;
		std::shared_ptr<void> get_resource() const override { return resource; }

		static material_resource_ptr create_fallback(const asset_reference<material_resource>& reference, material_resource::domains domain);

		bool revert(material_resource& target);

	private:
		void assign_textures();

	private:
		struct texture_operation
		{
			std::string name;
			std::size_t index = 0;
			asset_reference<texture_resource> reference;
			load_operation_ptr<texture_resource> operation;
		};

		material_resource_ptr resource;
		std::vector<texture_operation> texture_operations;
	};

	template<>
	class saver<material_resource>
	{
	public:
		bool save(material_resource& resource, std::string_view file_path);
	};
}	//	namespace resource
