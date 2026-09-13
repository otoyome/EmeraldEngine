#include "model_resource_utility.h"
#include "model_buffer_pool.h"

#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <functional>
#include <algorithm>

#include "graphics_utility.h"
#include "render_data.h"
#include "common_pipeline_states.h"

#include "meshoptimizer/meshoptimizer.h"

#if defined(USE_IMGUI)

void model_resource_utility::combine_unskinned_meshes(model_resource& resource, DirectX::XMINT3 grid_divisions)
{
	using namespace DirectX;

	auto& meshes = resource.get_meshes_for_editor();
	auto& nodes = resource.get_nodes_for_editor();

	const int div_x = std::max(1, grid_divisions.x);
	const int div_y = std::max(1, grid_divisions.y);
	const int div_z = std::max(1, grid_divisions.z);

	const semantic position_semantic("POSITION", 0);
	const semantic joints_semantic("JOINTS", 0);
	const semantic weights_semantic("WEIGHTS", 0);

	//	ノードのローカル/グローバル変換
	auto local_matrix = [](const model_resource::node& n)
	{
		XMMATRIX S = XMMatrixScalingFromVector(XMLoadFloat3(&n.scale));
		XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&n.rotation));
		XMMATRIX T = XMMatrixTranslationFromVector(XMLoadFloat3(&n.translation));
		return S * R * T;
	};
	std::function<XMMATRIX(size_t)> global_matrix = [&](size_t index) -> XMMATRIX
	{
		if (index == model_resource::invalid_id || index >= nodes.size())
			return XMMatrixIdentity();
		return local_matrix(nodes[index]) * global_matrix(nodes[index].parent_index);
	};

	//	結合対象判定
	auto is_combinable = [&](const model_resource::mesh& m)
	{
		if (!m.bones.empty()) return false;
		if (m.index_buffer_view.count() == 0) return false;
		if (!m.morph_meshes.empty()) return false;
		if (m.vertex_buffer_views.count(joints_semantic) != 0) return false;
		if (m.vertex_buffer_views.count(weights_semantic) != 0) return false;
		if (m.vertex_buffer_views.count(position_semantic) == 0) return false;
		return true;
	};

	//	対象メッシュのワールド中心と全体 AABB を求める
	struct combinable_info { size_t mesh_index; XMFLOAT3 center; };
	std::vector<combinable_info> combinables;
	std::vector<size_t> passthrough;
	XMFLOAT3 world_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
	XMFLOAT3 world_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index)
	{
		if (!is_combinable(meshes[mesh_index]))
		{
			passthrough.push_back(mesh_index);
			continue;
		}

		XMMATRIX gm = global_matrix(meshes[mesh_index].node_index);
		const XMFLOAT3 lmin = meshes[mesh_index].bounds_min;
		const XMFLOAT3 lmax = meshes[mesh_index].bounds_max;
		XMFLOAT3 wmin = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
		XMFLOAT3 wmax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int corner = 0; corner < 8; ++corner)
		{
			XMVECTOR p = XMVectorSet((corner & 1) ? lmax.x : lmin.x,
									 (corner & 2) ? lmax.y : lmin.y,
									 (corner & 4) ? lmax.z : lmin.z, 1.0f);
			XMFLOAT3 wp;
			XMStoreFloat3(&wp, XMVector3TransformCoord(p, gm));
			wmin.x = std::min(wmin.x, wp.x); wmin.y = std::min(wmin.y, wp.y); wmin.z = std::min(wmin.z, wp.z);
			wmax.x = std::max(wmax.x, wp.x); wmax.y = std::max(wmax.y, wp.y); wmax.z = std::max(wmax.z, wp.z);
		}

		combinable_info info;
		info.mesh_index = mesh_index;
		info.center = { (wmin.x + wmax.x) * 0.5f, (wmin.y + wmax.y) * 0.5f, (wmin.z + wmax.z) * 0.5f };
		combinables.push_back(info);

		world_min.x = std::min(world_min.x, wmin.x); world_min.y = std::min(world_min.y, wmin.y); world_min.z = std::min(world_min.z, wmin.z);
		world_max.x = std::max(world_max.x, wmax.x); world_max.y = std::max(world_max.y, wmax.y); world_max.z = std::max(world_max.z, wmax.z);
	}

	//	結合の余地なし
	if (combinables.size() < 2)
		return;

	const XMFLOAT3 world_size =
	{
		std::max(world_max.x - world_min.x, 1e-4f),
		std::max(world_max.y - world_min.y, 1e-4f),
		std::max(world_max.z - world_min.z, 1e-4f),
	};

	//	頂点レイアウトの集合をハッシュ化
	auto layout_signature = [](const model_resource::mesh& m)
	{
		std::vector<std::pair<system_utility::hash_key, int>> parts;
		for (auto& vbv : m.vertex_buffer_views)
			parts.emplace_back(vbv.first.get_hash_key(), static_cast<int>(vbv.second.format));
		std::sort(parts.begin(), parts.end());
		std::size_t hash = 1469598103934665603ull;
		for (auto& part : parts)
		{
			hash = (hash ^ static_cast<std::size_t>(part.first)) * 1099511628211ull;
			hash = (hash ^ static_cast<std::size_t>(part.second)) * 1099511628211ull;
		}
		return hash;
	};

	//	グループ化
	std::map<std::tuple<size_t, std::size_t, int>, std::vector<size_t>> groups;
	for (auto& info : combinables)
	{
		const auto& m = meshes[info.mesh_index];
		int gx = std::clamp(static_cast<int>((info.center.x - world_min.x) / world_size.x * div_x), 0, div_x - 1);
		int gy = std::clamp(static_cast<int>((info.center.y - world_min.y) / world_size.y * div_y), 0, div_y - 1);
		int gz = std::clamp(static_cast<int>((info.center.z - world_min.z) / world_size.z * div_z), 0, div_z - 1);
		int cell = gx + gy * div_x + gz * div_x * div_y;
		groups[std::make_tuple(m.material_index, layout_signature(m), cell)].push_back(info.mesh_index);
	}

	std::vector<model_resource::mesh> out_meshes;
	const size_t bone_cap = common_pipeline_states::mesh_bone_max;
	UINT combine_counter = 0;

	for (auto& group_pair : groups)
	{
		auto& group = group_pair.second;
		if (group.size() == 1)
		{
			out_meshes.push_back(meshes[group[0]]);
			continue;
		}

		//	mesh_bone_max ごとに分割して結合メッシュを作る
		for (size_t chunk_begin = 0; chunk_begin < group.size(); chunk_begin += bone_cap)
		{
			const size_t chunk_end = std::min(group.size(), chunk_begin + bone_cap);
			const size_t chunk_count = chunk_end - chunk_begin;
			if (chunk_count == 1)
			{
				out_meshes.push_back(meshes[group[chunk_begin]]);
				continue;
			}

			//	合成ノードを追加
			nodes.emplace_back();
			nodes.back().name = "combined_mesh_" + std::to_string(combine_counter++);
			nodes.back().parent_index = model_resource::invalid_id;
			const size_t new_node_index = nodes.size() - 1;

			const model_resource::mesh& base_src = meshes[group[chunk_begin]];

			out_meshes.emplace_back();
			model_resource::mesh& dst = out_meshes.back();
			dst.material_index = base_src.material_index;
			dst.node_index = new_node_index;

			//	ボーン1本
			dst.bones.reserve(chunk_count);
			for (size_t group_index = chunk_begin; group_index < chunk_end; ++group_index)
			{
				model_resource::bone bone;
				bone.node_index = meshes[group[group_index]].node_index;
				dst.bones.push_back(bone);
			}

			//	インデックスバッファ（R32_UINT）
			dst.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
			dst.index_buffer_view.stride_in_bytes = sizeof(UINT);
			dst.index_buffer_view.size_in_bytes = 0;
			dst.index_buffer_view.offset_in_bytes = 0;

			//	JOINTS / WEIGHTS + ソース側の全 semantic を空で用意
			{
				buffer_view joints_view;
				joints_view.format = DXGI_FORMAT_R32G32B32A32_UINT;
				joints_view.stride_in_bytes = sizeof(XMUINT4);
				dst.vertex_buffer_views.insert(std::make_pair(joints_semantic, joints_view));

				buffer_view weights_view;
				weights_view.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				weights_view.stride_in_bytes = sizeof(XMFLOAT4);
				dst.vertex_buffer_views.insert(std::make_pair(weights_semantic, weights_view));
			}
			for (auto& src_vbv : base_src.vertex_buffer_views)
			{
				buffer_view view;
				view.format = src_vbv.second.format;
				view.stride_in_bytes = src_vbv.second.stride_in_bytes;
				dst.vertex_buffer_views.insert(std::make_pair(src_vbv.first, view));
			}

			//	各メッシュを連結
			UINT base_vertex = 0;
			for (size_t bone_index = 0; bone_index < chunk_count; ++bone_index)
			{
				const model_resource::mesh& src = meshes[group[chunk_begin + bone_index]];
				const UINT src_index_count = static_cast<UINT>(src.index_buffer_view.count());
				const UINT src_vertex_count = static_cast<UINT>(src.vertex_buffer_views.begin()->second.count());

				//	インデックス
				{
					const size_t offset = dst.index_buffer_view.row_data.size();
					dst.index_buffer_view.row_data.resize(offset + static_cast<size_t>(src_index_count) * sizeof(UINT));
					UINT* out_indices = reinterpret_cast<UINT*>(dst.index_buffer_view.row_data.data() + offset);
					if (src.index_buffer_view.stride_in_bytes == 2)
					{
						const USHORT* s = reinterpret_cast<const USHORT*>(src.index_buffer_view.row_data.data());
						for (UINT i = 0; i < src_index_count; ++i) out_indices[i] = base_vertex + s[i];
					}
					else if (src.index_buffer_view.stride_in_bytes == 1)
					{
						const UCHAR* s = reinterpret_cast<const UCHAR*>(src.index_buffer_view.row_data.data());
						for (UINT i = 0; i < src_index_count; ++i) out_indices[i] = base_vertex + s[i];
					}
					else
					{
						const UINT* s = reinterpret_cast<const UINT*>(src.index_buffer_view.row_data.data());
						for (UINT i = 0; i < src_index_count; ++i) out_indices[i] = base_vertex + s[i];
					}
					dst.index_buffer_view.size_in_bytes = dst.index_buffer_view.row_data.size();
				}

				//	JOINTS = [boneIndex, 0, 0, 0]
				{
					buffer_view& joints_view = dst.vertex_buffer_views.at(joints_semantic);
					const size_t offset = joints_view.row_data.size();
					joints_view.row_data.resize(offset + static_cast<size_t>(src_vertex_count) * sizeof(XMUINT4));
					XMUINT4* out_joints = reinterpret_cast<XMUINT4*>(joints_view.row_data.data() + offset);
					for (UINT i = 0; i < src_vertex_count; ++i)
						out_joints[i] = XMUINT4(static_cast<UINT>(bone_index), 0, 0, 0);
					joints_view.size_in_bytes = joints_view.row_data.size();
				}
				//	WEIGHTS = [1, 0, 0, 0]
				{
					buffer_view& weights_view = dst.vertex_buffer_views.at(weights_semantic);
					const size_t offset = weights_view.row_data.size();
					weights_view.row_data.resize(offset + static_cast<size_t>(src_vertex_count) * sizeof(XMFLOAT4));
					XMFLOAT4* out_weights = reinterpret_cast<XMFLOAT4*>(weights_view.row_data.data() + offset);
					for (UINT i = 0; i < src_vertex_count; ++i)
						out_weights[i] = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
					weights_view.size_in_bytes = weights_view.row_data.size();
				}
				//	その他の頂点バッファをそのまま連結
				for (auto& src_vbv : src.vertex_buffer_views)
				{
					auto it = dst.vertex_buffer_views.find(src_vbv.first);
					if (it == dst.vertex_buffer_views.end())
						continue;
					const size_t offset = it->second.row_data.size();
					it->second.row_data.resize(offset + src_vbv.second.row_data.size());
					std::memcpy(it->second.row_data.data() + offset, src_vbv.second.row_data.data(), src_vbv.second.row_data.size());
					it->second.size_in_bytes = it->second.row_data.size();
				}

				base_vertex += src_vertex_count;
			}

			//	AABB 再計算
			{
				std::vector<XMMATRIX> bone_transforms(dst.bones.size());
				for (size_t bone_index = 0; bone_index < dst.bones.size(); ++bone_index)
				{
					bone_transforms[bone_index] =
						XMLoadFloat4x4(&dst.bones[bone_index].offset_matrix) * global_matrix(dst.bones[bone_index].node_index);
				}

				auto pos_it = dst.vertex_buffer_views.find(position_semantic);
				auto joints_it = dst.vertex_buffer_views.find(joints_semantic);
				XMFLOAT3 bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
				XMFLOAT3 bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				if (pos_it != dst.vertex_buffer_views.end() && joints_it != dst.vertex_buffer_views.end())
				{
					const XMFLOAT3* positions = reinterpret_cast<const XMFLOAT3*>(pos_it->second.row_data.data());
					const XMUINT4* joints = reinterpret_cast<const XMUINT4*>(joints_it->second.row_data.data());
					const size_t vertex_count = pos_it->second.count();
					for (size_t i = 0; i < vertex_count; ++i)
					{
						const UINT bone = (joints[i].x < bone_transforms.size()) ? joints[i].x : 0;
						XMFLOAT3 wp;
						XMStoreFloat3(&wp, XMVector3TransformCoord(XMLoadFloat3(&positions[i]), bone_transforms[bone]));
						bounds_min.x = std::min(bounds_min.x, wp.x); bounds_min.y = std::min(bounds_min.y, wp.y); bounds_min.z = std::min(bounds_min.z, wp.z);
						bounds_max.x = std::max(bounds_max.x, wp.x); bounds_max.y = std::max(bounds_max.y, wp.y); bounds_max.z = std::max(bounds_max.z, wp.z);
					}
				}
				dst.bounds_min = bounds_min;
				dst.bounds_max = bounds_max;
			}

			//	GPU バッファ生成(共有プール経由)
			model_buffer_pool_manager::instance()->build_vertex_buffers(dst.vertex_buffer_views);
			model_buffer_pool_manager::instance()->build_index_buffer(dst.index_buffer_view);
		}
	}

	//	非結合メッシュを後ろに積む
	for (size_t mesh_index : passthrough)
		out_meshes.push_back(meshes[mesh_index]);

	meshes = std::move(out_meshes);
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
				model_buffer_pool_manager::instance()->free_index_buffer(mesh.shadow_mesh.index_buffer_view);

				mesh.shadow_mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.shadow_mesh.index_buffer_view.row_data.data(),
						 mesh.shadow_mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.shadow_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.shadow_mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.shadow_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
				if (regenerate_buffer)
				{
					model_buffer_pool_manager::instance()->build_index_buffer(mesh.shadow_mesh.index_buffer_view);
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
			for (auto& old_lod_mesh : mesh.lod_meshs)
				model_buffer_pool_manager::instance()->free_index_buffer(old_lod_mesh.index_buffer_view);
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
				if (regeneratge_buffer)
				{
					model_buffer_pool_manager::instance()->build_index_buffer(sub_mesh.index_buffer_view);
				}
				mesh.lod_meshs.emplace_back(sub_mesh);
			}
		}
	}
}

void model_resource_utility::clear_lod_meshs(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		for (auto& lod_mesh : mesh.lod_meshs)
			model_buffer_pool_manager::instance()->free_index_buffer(lod_mesh.index_buffer_view);
		mesh.lod_meshs.clear();
	}
}

void model_resource_utility::clear_shadow_meshes(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		model_buffer_pool_manager::instance()->free_index_buffer(mesh.shadow_mesh.index_buffer_view);
		mesh.shadow_mesh = {};
	}
}

void model_resource_utility::optimize_meshs(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		model_buffer_pool_manager::instance()->free_vertex_buffers(mesh.vertex_buffer_views);
		model_buffer_pool_manager::instance()->free_index_buffer(mesh.index_buffer_view);
	}

	optimized_meshs(resource);

	for (auto& mesh : resource.get_meshes_for_editor())
	{
		model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.vertex_buffer_views);
		model_buffer_pool_manager::instance()->build_index_buffer(mesh.index_buffer_view);
	}

	clear_lod_meshs(resource);
	clear_shadow_meshes(resource);
}
#endif	//	defined(USE_IMGUI)
