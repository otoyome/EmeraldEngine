#pragma once

#include <map>

#include "material_resource.h"

//	マテリアルインスタンス
class material_instance
{
public:
	using domains = material_resource::domains;
	using surface_paths = material_resource::surface_paths;
	using particle_paths = material_resource::particle_paths;
	using decal_paths = material_resource::decal_paths;
	using system_paths = material_resource::system_paths;
	using shading_models = material_resource::shading_models;
	using blend_modes = material_resource::blend_modes;
	using blendable_locations = material_resource::blendable_locations;
	using translucency_paths = material_resource::translucency_paths;
	using flags = material_resource::flags;

public:
	//	リソースの取得/設定
	const material_resource_ptr& get_resource() const { return resource; }
	void set_resource(material_resource_ptr resource);
	bool is_valid() const { return resource != nullptr; }

	std::string_view get_name() const { return resource ? resource->get_name() : std::string_view(); }
	domains get_domain() const { return resource ? resource->get_domain() : domains::unknown; }
	blend_modes get_blend_mode() const { return resource ? resource->get_blend_mode() : blend_modes::unknown; }
	shading_models get_shading_model() const { return resource ? resource->get_shading_model() : shading_models::unlit; }
	blendable_locations get_blendable_location() const { return resource ? resource->get_blendable_location() : blendable_locations::unknown; }
	translucency_paths get_translucency_path() const { return resource ? resource->get_translucency_path() : translucency_paths::unknown; }
	bool is_flag(flags flag) const { return resource ? resource->is_flag(flag) : false; }
	float get_alpha_clip_threshold() const { return resource ? resource->get_alpha_clip_threshold() : 0.0f; }
	const material_resource::physical_datas& get_physical_data() const;

	graphics_pipeline_state_ptr get_pipeline_state(surface_paths value) const { return resource ? resource->get_pipeline_state(value) : nullptr; }
	graphics_pipeline_state_ptr get_pipeline_state(particle_paths value) const { return resource ? resource->get_pipeline_state(value) : nullptr; }
	graphics_pipeline_state_ptr get_pipeline_state(decal_paths value) const { return resource ? resource->get_pipeline_state(value) : nullptr; }
	graphics_pipeline_state_ptr get_pipeline_state(system_paths value) const { return resource ? resource->get_pipeline_state(value) : nullptr; }
	graphics_pipeline_state_ptr get_pipeline_state() const { return resource ? resource->get_pipeline_state() : nullptr; }


public:
	material_instance();
	explicit material_instance(material_resource_ptr parent);
	material_instance(const material_instance& other);
	material_instance(material_instance&& other);
	~material_instance();

	void sync_from_resource();
	void flush_constant_buffers(gpu_upload_list& upload_list);

	//	インスタンス側で情報を上書きしたか
	bool has_overrides() const { return !data_overrides.empty() || !texture_overrides.empty(); }
	bool is_data_overridden(std::string_view key_name) const;
	bool is_texture_overridden(std::string_view key_name) const;
	void reset_data(std::string_view key_name);
	void reset_texture(std::string_view key_name);
	void clear_overrides();

	const material_resource::constant_buffer* find_constant_buffer(const std::string& name) const;

	std::size_t get_texture_count(const std::string& name) const;
	texture_ptr find_texture(const std::string& name, std::size_t index) const;

public:
	//	パラメーター設定/取得
	//	配列の場合は名前の後に[n]でアクセス可能
	//	例)names[0] という感じで
	bool set_data(std::string_view key_name, const void* data, std::size_t data_size);
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
	material_instance& operator =(const material_instance& other);
	material_instance& operator =(material_instance&& other);

private:
	void rebuild();

	const std::string* find_resource_constant_buffer_name(const std::string& identifier) const;
	material_resource::constant_buffer* prepare_constant_buffer(const std::string& constant_buffer_name);

	bool write_data(const std::string& key_name, const void* data, std::size_t data_size);

private:
	material_resource_ptr resource;
	std::uint32_t resource_version = 0;
	bool built = false;

	//	上書きした値
	std::map<std::string, std::vector<unsigned char>> data_overrides;
	std::map<std::string, texture_ptr> texture_overrides;
	material_resource::constant_buffer_map constant_buffers;
	material_resource::constant_buffer_map previous_constant_buffers;
};
