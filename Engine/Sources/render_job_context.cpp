#include "render_job_context.h"

#include "render_data.h"
#include "System/misc.h"
#include "graphics/clustered_lighting_system.h"

namespace
{
	//	StructuredBuffer が必要数を満たさなければ 2 のべき乗で確保し直す
	void regenerate_structured_buffer(
		Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shader_resource_view,
		UINT& capacity,
		UINT stride,
		std::size_t required_count)
	{
		UINT need = static_cast<UINT>(required_count);
		if (buffer && capacity >= need)
			return;

		UINT new_capacity = capacity ? capacity : 256;
		while (new_capacity < need)
			new_capacity *= 2;

		auto device = render_data::instance()->get_device();

		D3D11_BUFFER_DESC buffer_desc = {};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_desc.ByteWidth = stride * new_capacity;
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.StructureByteStride = stride;

		Microsoft::WRL::ComPtr<ID3D11Buffer> new_buffer;
		HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, new_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = new_capacity;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> new_srv;
		hr = device->CreateShaderResourceView(new_buffer.Get(), &srv_desc, new_srv.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer = new_buffer;
		shader_resource_view = new_srv;
		capacity = new_capacity;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void gpu_upload_list::add_copy(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	std::vector<uint8_t> copied(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size_in_bytes);
	add_owned(buffer, use_box, offset_in_bytes, std::move(copied));
}

void gpu_upload_list::add_owned(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, std::vector<uint8_t>&& data)
{
	auto& target = commands.emplace_back();
	target.buffer = buffer;
	target.use_box = use_box;
	target.offset_in_bytes = offset_in_bytes;
	target.size_in_bytes = static_cast<UINT>(data.size());
	target.owned_data = std::move(data);
}

void gpu_upload_list::add_reference(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	auto& target = commands.emplace_back();
	target.buffer = buffer;
	target.use_box = use_box;
	target.offset_in_bytes = offset_in_bytes;
	target.data = data;
	target.size_in_bytes = size_in_bytes;
}

void gpu_upload_list::add_buffer_copy(const Microsoft::WRL::ComPtr<ID3D11Buffer>& destination, const Microsoft::WRL::ComPtr<ID3D11Buffer>& source, UINT size_in_bytes)
{
	auto& target = commands.emplace_back();
	target.buffer = destination;
	target.copy_source = source;
	target.size_in_bytes = size_in_bytes;
}

void gpu_upload_list::upload(const render_command_recorder& recorder) const
{
	for (const auto& target : commands)
	{
		if (!target.buffer || target.size_in_bytes == 0)
			continue;

		//	@tool バッファ間のコピー（先頭から size_in_bytes 分）
		if (target.copy_source)
		{
			D3D11_BOX box{};
			box.right = target.size_in_bytes;
			box.bottom = 1;
			box.back = 1;
			recorder.copy_subresource_region(target.buffer.Get(), 0, 0, 0, 0, target.copy_source.Get(), 0, &box);
			continue;
		}

		const void* source = target.owned_data.empty() ? target.data : target.owned_data.data();
		if (target.use_box)
		{
			D3D11_BOX box{};
			box.left = target.offset_in_bytes;
			box.right = target.offset_in_bytes + target.size_in_bytes;
			box.bottom = 1;
			box.back = 1;
			recorder.update_subresource(target.buffer.Get(), 0, &box, source, 0, 0);
		}
		else
		{
			recorder.update_subresource(target.buffer.Get(), 0, nullptr, source, 0, 0);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void mesh_container::build_transform_buffers()
{
	if (primitive_header_scratch.empty())
		primitive_header_scratch.emplace_back();
	if (bone_matrix_scratch.empty())
		bone_matrix_scratch.emplace_back();

	//	バッファ生成判定
	regenerate_structured_buffer(primitive_header_buffer, primitive_header_srv, primitive_header_capacity,
		sizeof(common_pipeline_states::primitive_header), primitive_header_scratch.size());
	regenerate_structured_buffer(bone_matrix_buffer, bone_matrix_srv, bone_matrix_capacity,
		sizeof(math::float4x4), bone_matrix_scratch.size());

	//	転送は描画の最初のパスで行う
	transform_uploads.commands.clear();
	transform_uploads.add_reference(primitive_header_buffer, true, 0, primitive_header_scratch.data(),
		static_cast<UINT>(primitive_header_scratch.size() * sizeof(common_pipeline_states::primitive_header)));
	transform_uploads.add_reference(bone_matrix_buffer, true, 0, bone_matrix_scratch.data(),
		static_cast<UINT>(bone_matrix_scratch.size() * sizeof(math::float4x4)));
}

void mesh_container::clear()
{
	draw_items.clear();
	shadow_items.clear();
	color_items.clear();
	primitive_header_scratch.clear();
	bone_matrix_scratch.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	//	フォワード描画用のライト情報
	struct forward_light_data
	{
		common_pipeline_states::forward_light_constants light;

		//	IBL情報
		texture_ptr diffuse_ibl_texture;
		texture_ptr specluar_ibl_texture;
		texture_ptr lut_ggx_texture;
	};

	//	近距離のライトを集める
	forward_light_data search_neighborhood_light(const render_job_context& context)
	{
		forward_light_data light_data = {};
		//	テキトーに近くのライトを詰める
		for (auto& light : context.render_data->light_container.lights)
		{
			//	クラスタードライティング対象はクラスターから参照するので詰めない
			if (context.clustered_lighting && clustered_lighting_system::is_target(light, context.render_data->settings))
				continue;

			switch(light.type)
			{
				case light_type::directional:
				{
					if (light_data.light.light_counts.x >= common_pipeline_states::directional_light_count)
						continue;

					light_data.light.directional_light_data[light_data.light.light_counts.x] = light.data;
					++light_data.light.light_counts.x;
					break;
				}
				case light_type::point:
				{
					if (light_data.light.light_counts.y >= common_pipeline_states::point_light_count)
						continue;

					light_data.light.point_light_data[light_data.light.light_counts.y] = light.data;
					++light_data.light.light_counts.y;
					break;
				}
				case light_type::spot:
				{
					if (light_data.light.light_counts.z >= common_pipeline_states::spot_light_count)
						continue;

					light_data.light.spot_light_data[light_data.light.light_counts.z] = light.data;
					++light_data.light.light_counts.z;
					break;
				}
			}
		}

		//	IBL
		for (const auto& light_probe : context.render_data->light_container.light_probes)
		{
			if (!light_probe.is_flag(light_probe::flags::active) || !light_probe.is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			light_data.diffuse_ibl_texture = light_probe.diffuse_ibl_texture;
			light_data.specluar_ibl_texture = light_probe.specular_ibl_texture;
			light_data.lut_ggx_texture = light_probe.lut_ggx_texture;

			//	間接光の情報を設定
			light_data.light.light_probe_data = light_probe.data;
			break;
		}
		return light_data;
	}
}

void render_job_context::register_frame_bind_table()
{
	constexpr resource_view_bind_layer layer = resource_view_bind_layer::frame_view;
	resource_view_bind_table& bind_table = recorder.get_bind_table();
	bind_table.clear(layer);

	//	シーン定数
	if (scene_constant_buffer)
		bind_table.set_constant_buffer(layer, common_pipeline_states::scene_constant_buffer_hash_key, scene_constant_buffer);

	//	変換行列のバッファ
	if (render_data)
	{
		bind_table.set_shader_resource_view(layer, common_pipeline_states::primitive_headers_hash_key, render_data->mesh_container.primitive_header_srv.Get());
		bind_table.set_shader_resource_view(layer, common_pipeline_states::bone_matrices_hash_key, render_data->mesh_container.bone_matrix_srv.Get());
	}

	//	クラスタードライティング設定
	if (clustered_lighting)
	{
		bind_table.set_constant_buffer(layer, common_pipeline_states::clustered_lighting_constant_buffer_hash_key, clustered_lighting->get_constant_buffer());
		bind_table.set_shader_resource_view(layer, common_pipeline_states::clustered_lights_hash_key, clustered_lighting->get_light_srv());
		bind_table.set_shader_resource_view(layer, common_pipeline_states::cluster_counts_hash_key, clustered_lighting->get_count_srv());
		bind_table.set_shader_resource_view(layer, common_pipeline_states::cluster_offsets_hash_key, clustered_lighting->get_offset_srv());
		bind_table.set_shader_resource_view(layer, common_pipeline_states::cluster_light_indices_hash_key, clustered_lighting->get_index_srv());
	}

	//	フォワード描画用のライト
	if (render_data)
	{
		const forward_light_data light_data = search_neighborhood_light(*this);
		bind_table.set_constants(layer, common_pipeline_states::forward_light_constant_buffer_hash_key, light_data.light, true);
		bind_table.set_texture(layer, common_pipeline_states::forward_only_diffuse_iem_hash_key, light_data.diffuse_ibl_texture);
		bind_table.set_texture(layer, common_pipeline_states::forward_only_specular_pmrem_hash_key, light_data.specluar_ibl_texture);
		bind_table.set_texture(layer, common_pipeline_states::forward_only_lut_ggx_hash_key, light_data.lut_ggx_texture);
	}
}
