#include "Material/material_instance.h"

#include "Graphics/gpu_upload_list.h"

namespace
{
	void split_key(std::string_view key_name, std::string& out_name, UINT& out_element)
	{
		out_name = std::string(key_name);
		out_element = 0;

		auto head_pos = out_name.find("[");
		auto tail_pos = out_name.find("]");
		if (head_pos != std::string::npos &&
			tail_pos != std::string::npos &&
			head_pos < tail_pos)
		{
			out_element = atoi(out_name.substr(head_pos + 1, tail_pos - head_pos - 1).data());
			out_name = out_name.substr(0, head_pos);
		}
	}

	std::string canonical_key(std::string_view key_name)
	{
		std::string name;
		UINT array_element = 0;
		split_key(key_name, name, array_element);
		if (array_element == 0)
			return name;
		return name + "[" + std::to_string(array_element) + "]";
	}
}

material_instance::material_instance()
{
}

material_instance::material_instance(material_resource_ptr resource)
: resource(std::move(resource))
{
	rebuild();
}

material_instance::material_instance(const material_instance& other)
: resource(other.resource)
, data_overrides(other.data_overrides)
, texture_overrides(other.texture_overrides)
{
	rebuild();
}

material_instance::material_instance(material_instance&& other)
: resource(std::move(other.resource))
, resource_version(other.resource_version)
, built(other.built)
, data_overrides(std::move(other.data_overrides))
, texture_overrides(std::move(other.texture_overrides))
, constant_buffers(std::move(other.constant_buffers))
{
	other.built = false;
}

material_instance::~material_instance()
{
}

material_instance& material_instance::operator =(const material_instance& other)
{
	if (this == &other)
		return *this;

	resource = other.resource;
	data_overrides = other.data_overrides;
	texture_overrides = other.texture_overrides;
	constant_buffers.clear();
	rebuild();
	return *this;
}

material_instance& material_instance::operator =(material_instance&& other)
{
	if (this == &other)
		return *this;

	resource = std::move(other.resource);
	resource_version = other.resource_version;
	built = other.built;
	data_overrides = std::move(other.data_overrides);
	texture_overrides = std::move(other.texture_overrides);
	constant_buffers = std::move(other.constant_buffers);
	other.built = false;
	return *this;
}

void material_instance::set_resource(material_resource_ptr resource)
{
	if (this->resource == resource)
		return;

	this->resource = std::move(resource);
	rebuild();
}

void material_instance::sync_from_resource()
{
	if (!resource)
		return;

	resource->sync_shader_reloads();

	if (!built || resource_version != resource->get_version())
		rebuild();
}

void material_instance::flush_constant_buffers(gpu_upload_list& upload_list)
{
	if (!resource)
		return;

	resource->flush_constant_buffers(upload_list);

	for (auto& constant_buffer : constant_buffers)
	{
		if (!constant_buffer.second.dirty)
			continue;

		upload_list.update_whole(constant_buffer.second.buffer.Get(), constant_buffer.second.data_buffer.data(), static_cast<UINT>(constant_buffer.second.data_buffer.size()));
		constant_buffer.second.dirty = false;
	}
}

const material_resource::physical_datas& material_instance::get_physical_data() const
{
	static const material_resource::physical_datas default_physical_data{};

	return resource ? resource->get_physical_data() : default_physical_data;
}

const material_resource::constant_buffer* material_instance::find_constant_buffer(const std::string& name) const
{
	if (!constant_buffers.empty())
	{
		auto it = constant_buffers.find(name);
		if (it != constant_buffers.end())
			return &it->second;
	}

	if (!resource)
		return nullptr;

	auto it = resource->constant_buffers.find(name);
	return it != resource->constant_buffers.end() ? &it->second : nullptr;
}

std::size_t material_instance::get_texture_count(const std::string& name) const
{
	if (!resource)
		return 0;

	auto it = resource->shader_resources.find(name);
	return it != resource->shader_resources.end() ? it->second.resources.size() : 0;
}

texture_ptr material_instance::find_texture(const std::string& name, std::size_t index) const
{
	if (!texture_overrides.empty())
	{
		auto it = texture_overrides.find(index == 0 ? name : name + "[" + std::to_string(index) + "]");
		if (it != texture_overrides.end())
			return it->second;
	}

	if (!resource)
		return nullptr;

	auto it = resource->shader_resources.find(name);
	if (it == resource->shader_resources.end() || it->second.resources.size() <= index)
		return nullptr;
	return it->second.resources[index].get_texture();
}

bool material_instance::set_data(std::string_view key_name, const void* data, std::size_t data_size)
{
	if (!resource || !data)
		return false;

	if (!built || resource_version != resource->get_version())
		rebuild();

	std::string key = canonical_key(key_name);
	if (!write_data(key, data, data_size))
		return false;

	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	data_overrides[key].assign(bytes, bytes + data_size);
	return true;
}

bool material_instance::get_data(std::string_view key_name, void* data, std::size_t data_size) const
{
	std::string name;
	UINT array_element = 0;
	split_key(key_name, name, array_element);

	//	上書きがあればそこから読む
	for (auto& constant_buffer : constant_buffers)
	{
		auto& cb = constant_buffer.second;
		auto it_data = cb.identifieres.find(name);
		if (it_data == cb.identifieres.end())
			continue;

		if (data)
		{
			auto& subset = cb.subsets[it_data->second];
			std::size_t start_offset = subset.start_offset + subset.stride * array_element;
			if (start_offset >= cb.data_buffer.size())
				return false;

			std::size_t buffer_size = cb.data_buffer.size() - start_offset;
			memcpy_s(data, data_size, cb.data_buffer.data() + start_offset, std::min(data_size, buffer_size));
		}
		return true;
	}

	return resource ? resource->get_data(key_name, data, data_size) : false;
}

bool material_instance::set_texture(std::string_view key_name, texture_ptr value)
{
	if (!resource || !value)
		return false;

	std::string name;
	UINT array_element = 0;
	split_key(key_name, name, array_element);
	if (get_texture_count(name) <= array_element)
		return false;

	texture_overrides[canonical_key(key_name)] = std::move(value);
	return true;
}

bool material_instance::get_texture(std::string_view key_name, texture_ptr& value) const
{
	std::string name;
	UINT array_element = 0;
	split_key(key_name, name, array_element);
	if (get_texture_count(name) <= array_element)
		return false;

	value = find_texture(name, array_element);
	return true;
}

bool material_instance::is_data_overridden(std::string_view key_name) const
{
	return data_overrides.find(canonical_key(key_name)) != data_overrides.end();
}

bool material_instance::is_texture_overridden(std::string_view key_name) const
{
	return texture_overrides.find(canonical_key(key_name)) != texture_overrides.end();
}

void material_instance::reset_data(std::string_view key_name)
{
	if (data_overrides.erase(canonical_key(key_name)) > 0)
		rebuild();
}

void material_instance::reset_texture(std::string_view key_name)
{
	if (texture_overrides.erase(canonical_key(key_name)) > 0)
		rebuild();
}

void material_instance::clear_overrides()
{
	data_overrides.clear();
	texture_overrides.clear();
	rebuild();
}

void material_instance::rebuild()
{
	//	リソースから上書き用情報を生成
	built = true;

	previous_constant_buffers = std::move(constant_buffers);
	constant_buffers.clear();

	if (resource)
	{
		resource_version = resource->get_version();

		for (auto& data_override : data_overrides)
		{
			write_data(data_override.first, data_override.second.data(), data_override.second.size());
		}
	}

	previous_constant_buffers.clear();
}

const std::string* material_instance::find_resource_constant_buffer_name(const std::string& identifier) const
{
	for (auto& constant_buffer : resource->constant_buffers)
	{
		if (constant_buffer.second.identifieres.find(identifier) != constant_buffer.second.identifieres.end())
			return &constant_buffer.first;
	}
	return nullptr;
}

material_resource::constant_buffer* material_instance::prepare_constant_buffer(const std::string& constant_buffer_name)
{
	auto it = constant_buffers.find(constant_buffer_name);
	if (it != constant_buffers.end())
		return &it->second;

	auto it_resource = resource->constant_buffers.find(constant_buffer_name);
	if (it_resource == resource->constant_buffers.end())
		return nullptr;

	//	上書きの無い値は親の値を使うので、親の内容を丸ごと写してから上書きを書き込む
	material_resource::constant_buffer constant_buffer;
	constant_buffer.data_buffer = it_resource->second.data_buffer;
	constant_buffer.subsets = it_resource->second.subsets;
	constant_buffer.identifieres = it_resource->second.identifieres;
	constant_buffer.dirty = true;

	auto previous_it = previous_constant_buffers.find(constant_buffer_name);
	if (previous_it != previous_constant_buffers.end() &&
		previous_it->second.buffer &&
		previous_it->second.data_buffer.size() == constant_buffer.data_buffer.size())
	{
		constant_buffer.buffer = previous_it->second.buffer;
	}
	else
	{
		material_resource::create_constant_buffer_resource(constant_buffer);
	}

	return &constant_buffers.emplace(constant_buffer_name, std::move(constant_buffer)).first->second;
}

bool material_instance::write_data(const std::string& key_name, const void* data, std::size_t data_size)
{
	std::string name;
	UINT array_element = 0;
	split_key(key_name, name, array_element);

	const std::string* constant_buffer_name = find_resource_constant_buffer_name(name);
	if (!constant_buffer_name)
		return false;

	material_resource::constant_buffer* constant_buffer = prepare_constant_buffer(*constant_buffer_name);
	if (!constant_buffer)
		return false;

	auto& subset = constant_buffer->subsets[constant_buffer->identifieres.at(name)];
	std::size_t start_offset = subset.start_offset + subset.stride * array_element;
	if (start_offset >= constant_buffer->data_buffer.size())
		return false;

	std::size_t buffer_size = constant_buffer->data_buffer.size() - start_offset;
	memcpy_s(constant_buffer->data_buffer.data() + start_offset, buffer_size, data, std::min(buffer_size, data_size));
	constant_buffer->dirty = true;
	return true;
}
