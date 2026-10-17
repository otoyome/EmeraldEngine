#pragma once

#include "texture.h"
#include "pipeline_state.h"

//	マテリアル
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class material_render_proxy;
struct gpu_upload_list;

class material
{
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

	//	デカール描画パス
	enum class decal_paths : std::uint8_t
	{
		primitive,	//	プリミティブでの書き込み
		fullscreen,	//	フルスクリーンでの書き込み
	};

	//	システムのマテリアルをホットリロードさせたいため追加。
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

#if defined(USE_IMGUI)
		UINT reload_callback_id = UINT_MAX;
#endif	//	defined(USE_IMGUI)

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
			bool is_file_path_or_key{ false };	//	ファイルパスかキー情報かどうか
			std::string path{ "" };				//	ファイルパスかキー文字列
			texture_ptr value{ nullptr };		//	テクスチャ本体

			bool is_valid() const
			{
				return !path.empty() && value != nullptr;
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
	};

	//	物理マテリアル
	struct physical_datas
	{
		float friction{ 0.5f };
		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct set_surface_pipeline_states_desc
	{
		surface_paths type;
		std::string_view pipeline_state_name;
	};

	using pipeline_state_array = std::vector<pipeline_state>;
	using constant_buffer_map = std::unordered_map<std::string, constant_buffer>;
	using constant_buffer_desc_map = std::unordered_map<std::string, constant_buffer_desc>;
	using shader_resource_map = std::unordered_map<std::string, shader_resource>;

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
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); mark_render_proxy_dirty(); }

	//	アルファクリップの閾値取得/設定
	float get_alpha_clip_threshold() const { return alpha_clip_threshold; }
	void set_alpha_clip_threshold(float value){ alpha_clip_threshold = value; mark_render_proxy_dirty(); }

	//	物理マテリアルの取得/設定
	inline const physical_datas& get_physical_data() const { return physical_data; }
	void set_physical_data(const physical_datas& value) { physical_data = value; }

	//	パイプラインステート取得・設定
	bool set_pipeline_state(const set_surface_pipeline_states_desc* descs, UINT size);
	bool set_pipeline_state(surface_paths type, std::string_view pipeline_state_name);
	bool set_pipeline_state(decal_paths type, std::string_view pipeline_state_name);
	bool set_pipeline_state(system_paths type, std::string_view pipeline_state_name);
	bool set_pipeline_state(std::string_view pipeline_state_name);

	std::string_view get_pipeline_state_name(surface_paths type) const;
	std::string_view get_pipeline_state_name(decal_paths type) const;
	std::string_view get_pipeline_state_name(system_paths type) const;
	std::string_view get_pipeline_state_name() const;
	graphics_pipeline_state_ptr get_pipeline_state(surface_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state(decal_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state(system_paths value) const;
	graphics_pipeline_state_ptr get_pipeline_state() const;

public:
	//	パラメーター群取得
	const constant_buffer_map& get_constant_buffers() const;
	const shader_resource_map& get_shader_resources() const;

#if defined(USE_IMGUI)
	constant_buffer_map& get_constant_buffers_for_editor();
	shader_resource_map& get_shader_resources_for_editor();
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

	bool set_texture(std::string_view key_name, texture_ptr value);
	bool get_texture(std::string_view key_name, texture_ptr& value) const;

public:
	material();
	material(std::string_view name);
	material(const material& material);
	material(material&& material);
	~material();

	//	マテリアル描画プロキシー取得
	std::shared_ptr<const material_render_proxy> get_render_proxy() const;

	//	シリアライズ
	template<class Archive>
	void serialize(Archive& archive, int version);

	//	複製処理
	void duplicate(material& material) const;

	//	ビルド処理
	void build();

public:
	//	変更のあったマテリアルの定数を、フレームの転送リストに登録
	static void flush_parameters(gpu_upload_list& out_uploads);

public:
	material& operator =(const material& material);

private:
	bool set_pipeline_state(domains domain, UINT index, std::string_view pipeline_state_name);
	std::string_view get_pipeline_state_name(domains domain, UINT index) const;
	graphics_pipeline_state_ptr get_pipeline_state(domains domain, UINT index) const;

	void serialize_stream(std::stringstream& ostream) const;
	void deserialize_stream(std::stringstream& istream);

	void build_parameters();

	void request_upload();

	void mark_render_proxy_dirty();

	bool is_texture_revision_changed(const material_render_proxy& proxy) const;

#if defined(USE_IMGUI)
	bool is_error() const;
	//	@tool エラー状態を求め直す（build_parameters から呼ぶ。メインスレッド専用）
	void update_error_state();
	void on_updates(const pipeline_state_base* ps);
	bool error_state = true;
#endif	//	defined(USE_IMGUI)

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
	shader_resource_map shader_resources;
	constant_buffer_map constant_buffers;

	bool upload_requested = false;

	//	描画側の複製情報
	UINT render_state_version = 0;
	mutable UINT render_proxy_version = UINT_MAX;
	mutable std::shared_ptr<const material_render_proxy> render_proxy;
};

//	マテリアル描画プロキシー
class material_render_proxy
{
	friend class material;

public:
	using domains = material::domains;
	using flags = material::flags;
	using blend_modes = material::blend_modes;
	using shading_models = material::shading_models;
	using surface_paths = material::surface_paths;
	using decal_paths = material::decal_paths;
	using system_paths = material::system_paths;

public:
	const void* get_source_id() const { return source_id; }
	std::string_view get_name() const { return name; }
	domains get_domain() const { return domain; }
	blend_modes get_blend_mode() const { return blend_mode; }
	shading_models get_shading_model() const { return shading_model; }
	blendable_locations get_blendable_location() const { return blendable_location; }
	translucency_paths get_translucency_path() const { return translucency_path; }
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	float get_alpha_clip_threshold() const { return alpha_clip_threshold; }
	graphics_pipeline_state_ptr get_pipeline_state(surface_paths value) const { return (domain == domains::surface || domain == domains::particle) ? get_pipeline_state(domain, static_cast<UINT>(value)) : nullptr; }
	graphics_pipeline_state_ptr get_pipeline_state(decal_paths value) const { return get_pipeline_state(domains::deferred_decal, static_cast<UINT>(value)); }
	graphics_pipeline_state_ptr get_pipeline_state(system_paths value) const { return get_pipeline_state(domains::system, static_cast<UINT>(value)); }
	graphics_pipeline_state_ptr get_pipeline_state() const { return pipeline_states.empty() ? nullptr : pipeline_states[0]; }
	ID3D11Buffer* find_constant_buffer(system_utility::hash_key key) const;
	const std::vector<texture_bind>* find_textures(system_utility::hash_key key) const;

private:
	graphics_pipeline_state_ptr get_pipeline_state(domains domain, UINT index) const
	{
		if (this->domain != domain || index >= pipeline_states.size())
			return nullptr;
		return pipeline_states[index];
	}

private:
	const void* source_id = nullptr;
	std::string name;
	domains domain = domains::unknown;
	blend_modes blend_mode = blend_modes::opaque;
	shading_models shading_model = shading_models::unlit;
	blendable_locations blendable_location = blendable_locations::after_dof_translucency;
	translucency_paths translucency_path = translucency_paths::after_dof;
	UINT system_flags = 0;
	float alpha_clip_threshold = 0.5f;

	std::vector<graphics_pipeline_state_ptr> pipeline_states;
	std::vector<std::pair<system_utility::hash_key, Microsoft::WRL::ComPtr<ID3D11Buffer>>> constant_buffers;
	std::vector<std::pair<system_utility::hash_key, std::vector<texture_bind>>> textures;

	std::vector<UINT> texture_revisions;
};
using material_render_proxy_ptr = std::shared_ptr<const material_render_proxy>;
