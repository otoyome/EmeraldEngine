#include "Graphics/pipeline_layout.h"

#include "common_pipeline_states.h"

void pipeline_layout::clear()
{
	constant_buffers.clear();
	shader_resources.clear();
	samplers.clear();
}

pipeline_binding& pipeline_layout::acquire(std::vector<pipeline_binding>& bindings, const std::string& name, system_utility::hash_key key, binding_source source)
{
	for (auto& binding : bindings)
	{
		if (binding.key == key && binding.source == source && binding.name == name)
			return binding;
	}
	auto& binding = bindings.emplace_back();
	binding.name = name;
	binding.key = key;
	binding.source = source;
	return binding;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void pipeline_layout::add_constant_buffer(shader_stage stage, UINT slot, system_utility::hash_key key, binding_source source)
{
	acquire(constant_buffers, std::string(), key, source).slots[static_cast<UINT>(stage)] = slot;
}

void pipeline_layout::add_shader_resource(shader_stage stage, UINT slot, system_utility::hash_key key, binding_source source)
{
	acquire(shader_resources, std::string(), key, source).slots[static_cast<UINT>(stage)] = slot;
}

void pipeline_layout::add_shader_resource(shader_stage stage, UINT slot, const texture_ptr& texture)
{
	//	@tool 固定のリソースは纏めずに 1 つずつ持つ
	auto& binding = shader_resources.emplace_back();
	binding.source = binding_source::fixed;
	binding.fixed_texture = texture;
	binding.slots[static_cast<UINT>(stage)] = slot;
}

void pipeline_layout::add_sampler(shader_stage stage, UINT slot, const Microsoft::WRL::ComPtr<ID3D11SamplerState>& sampler)
{
	auto& binding = samplers.emplace_back();
	binding.source = binding_source::fixed;
	binding.fixed_sampler = sampler;
	binding.slots[static_cast<UINT>(stage)] = slot;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void pipeline_layout::add_reflected_constant_buffer(shader_stage stage, UINT slot, const std::string& name)
{
	acquire(constant_buffers, name, system_utility::generate_hash(name), binding_source::material).slots[static_cast<UINT>(stage)] = slot;
}

void pipeline_layout::add_reflected_shader_resource(shader_stage stage, UINT slot, const std::string& name)
{
	acquire(shader_resources, name, system_utility::generate_hash(name), binding_source::material).slots[static_cast<UINT>(stage)] = slot;
}

void pipeline_layout::add_reflected_sampler(shader_stage stage, UINT slot, const std::string& name)
{
	acquire(samplers, name, system_utility::generate_hash(name), binding_source::material).slots[static_cast<UINT>(stage)] = slot;
}

void pipeline_layout::finalize_reflection()
{
	//	@tool 名前から取得元を決める（描画中に名前で判定しないため）
	for (auto& binding : constant_buffers)
	{
		if (binding.source == binding_source::fixed)
			continue;

		const system_utility::hash_key key = binding.key;
		if (key == common_pipeline_states::scene_constant_buffer_hash_key)
			binding.source = binding_source::table;
		else if (key == common_pipeline_states::parameter_constant_buffer_hash_key)
			binding.source = binding_source::parameter;
		else if (key == common_pipeline_states::primitive_constant_buffer_hash_key || key == common_pipeline_states::decal_primitive_constant_buffer_hash_key)
			binding.source = binding_source::primitive;
	}
	for (auto& binding : shader_resources)
	{
		if (binding.source == binding_source::fixed)
			continue;

		const system_utility::hash_key key = binding.key;
		if (key == common_pipeline_states::primitive_headers_hash_key || key == common_pipeline_states::bone_matrices_hash_key)
			binding.source = binding_source::table;
	}

	//	@tool SRV 毎に対になるサンプラー（<SRV名>State）を探す
	constexpr std::string_view sampler_suffix = "State";
	for (auto& binding : shader_resources)
	{
		binding.paired_sampler = -1;
		if (binding.name.empty())
			continue;

		const std::string sampler_name = binding.name + std::string(sampler_suffix);
		for (size_t sampler_index = 0; sampler_index < samplers.size(); ++sampler_index)
		{
			if (samplers[sampler_index].name == sampler_name)
			{
				binding.paired_sampler = static_cast<int>(sampler_index);
				break;
			}
		}
	}
}
