#include "Graphics/binding_table.h"

#include "render_data.h"
#include "common_pipeline_states.h"

namespace
{
	//  テクスチャフラグの対応表
	constexpr std::pair<texture::flags, UINT> convert_texture_flags[] =
	{
		{texture::flags::srgb, common_pipeline_states::shader_resource_view_flags_srgb},
		{texture::flags::cube_map, common_pipeline_states::shader_resource_view_flags_cube_map},
	};
}

texture_bind texture_bind::from_texture(const texture& value)
{
	texture_bind bind;
	bind.shader_resource_view = value.get_shader_resource_view();
	bind.sampler_state = render_data::instance()->get_sampler_state(value.get_sampler_desc().to_d3d11_desc());
	for (auto& flag : convert_texture_flags)
	{
		system_utility::set_bitflag(bind.shader_resource_view_flags, flag.second, value.is_flag(flag.first));
	}
	bind.is_texture = true;
	return bind;
}

resource_view_bind_entry& resource_view_bind_table::acquire(resource_view_bind_layer layer, system_utility::hash_key key)
{
	++revision;
	auto& entries = layers[static_cast<int>(layer)];
	for (auto& entry : entries)
	{
		if (entry.key == key)
		{
			entry = resource_view_bind_entry();
			entry.key = key;
			return entry;
		}
	}
	auto& entry = entries.emplace_back();
	entry.key = key;
	return entry;
}

void resource_view_bind_table::set_texture(resource_view_bind_layer layer, system_utility::hash_key key, const texture_ptr& value)
{
	if (!value)
	{
		remove(layer, key);
		return;
	}
	auto& entry = acquire(layer, key);
	entry.kind = resource_view_bind_entry::kinds::texture;
	entry.texture = value;
	entry.view = texture_bind::from_texture(*value);
}

void resource_view_bind_table::set_shader_resource_view(resource_view_bind_layer layer, system_utility::hash_key key, ID3D11ShaderResourceView* value)
{
	if (!value)
	{
		remove(layer, key);
		return;
	}
	auto& entry = acquire(layer, key);
	entry.kind = resource_view_bind_entry::kinds::shader_resource_view;
	entry.view.shader_resource_view = value;
}

void resource_view_bind_table::set_constant_buffer(resource_view_bind_layer layer, system_utility::hash_key key, const Microsoft::WRL::ComPtr<ID3D11Buffer>& value)
{
	auto& entry = acquire(layer, key);
	entry.kind = resource_view_bind_entry::kinds::constant_buffer;
	entry.constant_buffer = value;
}

void resource_view_bind_table::set_constants(resource_view_bind_layer layer, system_utility::hash_key key, const void* data, UINT size, bool requires_draw_data)
{
	auto& entry = acquire(layer, key);
	entry.kind = resource_view_bind_entry::kinds::constant_data;
	entry.constant_data.resize(size);
	memcpy(entry.constant_data.data(), data, size);
	entry.requires_draw_data = requires_draw_data;
}

void resource_view_bind_table::remove(resource_view_bind_layer layer, system_utility::hash_key key)
{
	++revision;
	auto& entries = layers[static_cast<int>(layer)];
	auto it = std::find_if(entries.begin(), entries.end(), [&](const resource_view_bind_entry& entry) { return entry.key == key; });
	if (it != entries.end())
		entries.erase(it);
}

void resource_view_bind_table::clear(resource_view_bind_layer layer)
{
	++revision;
	layers[static_cast<int>(layer)].clear();
}

const resource_view_bind_entry* resource_view_bind_table::find(system_utility::hash_key key) const
{
	//	@tool パス層を優先する
	for (int layer = static_cast<int>(resource_view_bind_layer::max) - 1; layer >= 0; --layer)
	{
		for (const auto& entry : layers[layer])
		{
			if (entry.key == key)
				return &entry;
		}
	}
	return nullptr;
}

texture_ptr resource_view_bind_table::get_texture(system_utility::hash_key key) const
{
	const resource_view_bind_entry* entry = find(key);
	return entry && entry->kind == resource_view_bind_entry::kinds::texture ? entry->texture : nullptr;
}
