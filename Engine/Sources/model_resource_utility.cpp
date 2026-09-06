#include "model_resource_utility.h"

#include <cmath>

#include "graphics_utility.h"
#include "render_data.h"

#include "meshoptimizer/meshoptimizer.h"

#if defined(USE_IMGUI)
namespace
{
	void rebuild_gpu_buffer(buffer_view& view, UINT bind_flag)
	{
		if (view.size_in_bytes == 0)
			return;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(view.size_in_bytes);
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = bind_flag;
		D3D11_SUBRESOURCE_DATA subresource_data{};
		subresource_data.pSysMem = view.row_data.data();
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, view.buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void model_resource_utility::optimized_meshs(model_resource& resource)
{
	auto& meshes = resource.get_meshes_for_editor();
	int mesh_index = 0;
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0 ||
			mesh.vertex_buffer_views.empty() ||
			mesh.vertex_buffer_views.begin()->second.size_in_bytes == 0)
			continue;

		//	モーフがある場合はしない
		//	する場合はstreamsにモーフのやつを追加して対応する必要がある
		if (!mesh.morph_meshes.empty())
			continue;

		//	インデックスリストを生成
		std::vector<UINT> indices;
		indices.reserve(mesh.index_buffer_view.count());
		for (UINT index = 0; index < mesh.index_buffer_view.count(); ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			std::size_t vertex_count = mesh.vertex_buffer_views.begin()->second.count();

			//	stream生成
			std::vector<meshopt_Stream> streams;
			for (auto& vbv : mesh.vertex_buffer_views)
			{
				auto& stream = streams.emplace_back();
				stream.data = vbv.second.row_data.data();
				stream.size = vbv.second.stride_in_bytes;
				stream.stride = vbv.second.stride_in_bytes;
			}

			//	重複頂点最適化
			std::vector<UINT> remap(vertex_count);
			size_t new_vertex_count = meshopt_generateVertexRemapMulti(
				remap.data(), indices.data(), indices.size(),
				vertex_count, streams.data(), streams.size());

			//	頂点バッファ最適化
			std::vector<std::vector<UCHAR>> vertex_buffer;
			vertex_buffer.reserve(streams.size());
			for (auto& stream : streams)
			{
				auto& new_data = vertex_buffer.emplace_back(new_vertex_count * stream.stride);
				meshopt_remapVertexBuffer(new_data.data(),
						stream.data, vertex_count, stream.stride, remap.data());
			}

			//	インデックスバッファ最適化
			meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

			//	頂点キャッシュ最適化
			meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), new_vertex_count);

			//	オーバードロー最適化
			auto pos = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
			if (pos != mesh.vertex_buffer_views.end())
			{
				meshopt_optimizeOverdraw(
					indices.data(),
					indices.data(),
					indices.size(),
					reinterpret_cast<const float*>(vertex_buffer[0].data()),
					new_vertex_count,
					pos->second.stride_in_bytes,
					1.05f);
			}

			//	
			std::vector<UINT> fetch_remap(new_vertex_count);
			meshopt_optimizeVertexFetchRemap(fetch_remap.data(), indices.data(), indices.size(), new_vertex_count);

			//	ベース頂点バッファ
			for (UINT index = 0; index < streams.size(); ++index)
			{
				auto& stream = streams[index];
				auto& vertices = vertex_buffer[index];
				std::vector<UCHAR> temp(vertices.size());
				meshopt_remapVertexBuffer(temp.data(), vertices.data(), new_vertex_count,
										  stream.stride, fetch_remap.data());
				vertices.swap(temp);
			}

			//	インデックスバッファ最適化
			meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), fetch_remap.data());

			//	インデックスバッファ書き戻し
			{
				mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.index_buffer_view.row_data.data(), mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
			}

			//	頂点バッファ書き戻し
			{
				UINT index = 0;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams[index];
					auto& vertices = vertex_buffer[index];
					vbv.second.row_data.resize(vertices.size());
					memcpy_s(vbv.second.row_data.data(), vbv.second.row_data.size(),
							 vertices.data(), vertices.size());
					vbv.second.size_in_bytes = vertices.size();
					++index;
				}
			}
		}
	}
}

void model_resource_utility::generate_shadow_meshes(model_resource& resource, bool regenerate_buffer)
{
	auto& meshes = resource.get_meshes_for_editor();
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	インデックスリストを生成
		std::vector<UINT> indices;
		indices.reserve(mesh.index_buffer_view.count());
		for (UINT index = 0; index < mesh.index_buffer_view.count(); ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			meshopt_generateShadowIndexBufferMulti(indices.data(), indices.data(), indices.size(), vertex_count, streams.data(), streams.size());

			//	影のインデックスバッファ書き戻し
			{
				mesh.shadow_mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.shadow_mesh.index_buffer_view.row_data.data(),
						 mesh.shadow_mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.shadow_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.shadow_mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.shadow_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
				if (regenerate_buffer && mesh.shadow_mesh.index_buffer_view.size_in_bytes > 0)
				{
					D3D11_BUFFER_DESC buffer_desc{};
					buffer_desc.ByteWidth = static_cast<UINT>(mesh.shadow_mesh.index_buffer_view.size_in_bytes);
					buffer_desc.Usage = D3D11_USAGE_DEFAULT;
					buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
					D3D11_SUBRESOURCE_DATA subresource_data{};
					subresource_data.pSysMem = mesh.shadow_mesh.index_buffer_view.row_data.data();
					HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, mesh.shadow_mesh.index_buffer_view.buffer.ReleaseAndGetAddressOf());
					_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				}
			}
		}
	}
}

void model_resource_utility::generate_lod_meshs(model_resource& resource, int max_lod_level, bool regeneratge_buffer)
{
	auto& meshes = resource.get_meshes_for_editor();
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	インデックスリストを生成
		std::vector<UINT> indices;
		indices.reserve(mesh.index_buffer_view.count());
		for (UINT index = 0; index < mesh.index_buffer_view.count(); ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			mesh.lod_meshs.clear();

			for (INT lod = 0; lod < max_lod_level; ++lod)
			{
				std::vector<UINT> lod_indicesLod(indices.size());
				UINT target_indices_count = static_cast<UINT>(indices.size() / pow(2, lod + 1));
				float target_error = static_cast<float>(lod) / max_lod_level;
				lod_indicesLod.resize(meshopt_simplify(lod_indicesLod.data(),
									 indices.data(),
									 indices.size(),
									 reinterpret_cast<const float*>(streams[position_index].data),
									 vertex_count,
									 streams[position_index].stride,
									 target_indices_count,
									 target_error));

				model_resource::mesh::sub_mesh sub_mesh;
				sub_mesh.index_buffer_view.row_data.resize(lod_indicesLod.size() * sizeof(UINT));
				memcpy_s(sub_mesh.index_buffer_view.row_data.data(),
						 sub_mesh.index_buffer_view.row_data.size(),
						 lod_indicesLod.data(), lod_indicesLod.size() * sizeof(UINT));
				sub_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				sub_mesh.index_buffer_view.size_in_bytes = lod_indicesLod.size() * sizeof(UINT);
				sub_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
				if (regeneratge_buffer && sub_mesh.index_buffer_view.size_in_bytes > 0)
				{
					//	再生成の場合はGPUリソース生成
					D3D11_BUFFER_DESC buffer_desc{};
					buffer_desc.ByteWidth = static_cast<UINT>(sub_mesh.index_buffer_view.size_in_bytes);
					buffer_desc.Usage = D3D11_USAGE_DEFAULT;
					buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
					D3D11_SUBRESOURCE_DATA subresource_data{};
					subresource_data.pSysMem = sub_mesh.index_buffer_view.row_data.data();
					HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, sub_mesh.index_buffer_view.buffer.ReleaseAndGetAddressOf());
					_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				}
				mesh.lod_meshs.emplace_back(sub_mesh);
			}
		}
	}
}

void model_resource_utility::clear_lod_meshs(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
		mesh.lod_meshs.clear();
}

void model_resource_utility::clear_shadow_meshes(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
		mesh.shadow_mesh = {};
}

void model_resource_utility::optimize_meshs(model_resource& resource)
{
	optimized_meshs(resource);

	for (auto& mesh : resource.get_meshes_for_editor())
	{
		for (auto& vbv : mesh.vertex_buffer_views)
			rebuild_gpu_buffer(vbv.second, D3D11_BIND_VERTEX_BUFFER);
		rebuild_gpu_buffer(mesh.index_buffer_view, D3D11_BIND_INDEX_BUFFER);
	}

	clear_lod_meshs(resource);
	clear_shadow_meshes(resource);
}
#endif	//	defined(USE_IMGUI)
