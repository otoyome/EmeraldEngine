#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"

#include "collision/box.h"


debug_primitive_manager::debug_primitive_manager()
{
	HRESULT hr;
	auto device = render_data::instance()->get_device();

	//	頂点バッファ
	for (auto& vertex_buffer_view : vertex_buffer_views)
	{
		buffer_view vertex_buffer;

		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(VertexCount * sizeof(vertex));
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		hr = device->CreateBuffer(&buffer_desc, nullptr, vertex_buffer.buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		vertex_buffer.format = DXGI_FORMAT_UNKNOWN;
		vertex_buffer.stride_in_bytes = sizeof(vertex);
		vertex_buffer.size_in_bytes = 0;
		vertex_buffer_view.insert_or_assign(semantic("VERTEX", 0), vertex_buffer);
	}
}

debug_primitive_manager::~debug_primitive_manager()
{
}

void debug_primitive_manager::get_container(debug_draw_primitive_container& container, const math::vector3d& render_origin, gpu_upload_list& out_uploads)
{
	//	描画登録
	HRESULT hr{ S_OK };
	for (UINT index = 0; index < static_cast<int>(debug_draw_primitive_container::line_buffer_id::max); ++index)
	{
		container.line_meshes[index].clear();
		container.line_draw_items[index].color_meshs = {};
		if (vertices[index].empty())
			continue;

		auto& main_mesh = container.line_meshes[index].emplace_back();
		container.line_draw_items[index].color_meshs = container.line_meshes[index];
		auto& view = vertex_buffer_views[index].begin()->second;

		//	描画原点からの相対座標に変換して転送
		upload_vertices.clear();
		upload_vertices.reserve(vertices[index].size());
		for (const auto& queued : vertices[index])
		{
			auto& upload = upload_vertices.emplace_back();
			upload.position = (math::vector3d(queued.position) - render_origin).to_float3();
			upload.color = queued.color;
		}

		const UINT copy_count = static_cast<UINT>(std::min<size_t>(VertexCount, vertices[index].size()));
		const UINT copy_bytes = copy_count * sizeof(vertex);
		out_uploads.add_copy(view.buffer, true, 0, upload_vertices.data(), copy_bytes);

		//	頂点数の設定
		view.size_in_bytes = view.stride_in_bytes * vertices[index].size();

		//	コンテナに登録
		//	@tool 頂点数はフレームで変わるので、このフレームの情報を複製して参照させる
		container.line_vertex_buffer_views[index] = vertex_buffer_views[index];
		main_mesh.vertex_buffer_views = &container.line_vertex_buffer_views[index];
		vertices[index].clear();
	}
}

void debug_primitive_manager::queue_line(const math::double3 start, const math::double3 end, const math::float4 color, bool depth_write)
{
	int index = depth_write ? 0 : 1;
	if (vertices[index].size() + 2 >= VertexCount)
		return;

	queued_vertex vertex;
	vertex.position = start;
	vertex.color = color;
	vertices[index].emplace_back(vertex);
	vertex.position = end;
	vertex.color = color;
	vertices[index].emplace_back(vertex);
}

void debug_primitive_manager::queue_sphere(const math::double3 center, float radius, const math::float4 color, bool depth_write)
{
	//	球の形状を登録
	std::vector<math::double3> vertices;
	vertices.emplace_back(center.x, center.y + radius, center.z);

	static constexpr uint32_t stacks = 8;
	static constexpr uint32_t slices = 8;
	float phi_step{ math::constants<float>::pi / stacks };
	float theta_step{ 2.0f * math::constants<float>::pi / slices };
	for (uint32_t i = 1; i <= stacks - 1; ++i)
	{
		float phi{ i * phi_step };
		for (uint32_t j = 0; j <= slices; ++j)
		{
			float theta{ j * theta_step };
			math::double3 position;
			position.x = center.x + radius * sinf(phi) * cosf(theta);
			position.y = center.y + radius * cosf(phi);
			position.z = center.z + radius * sinf(phi) * sinf(theta);
			vertices.emplace_back(position);
		}
	}
	vertices.emplace_back(center.x, center.y - radius, center.z);

	for (uint32_t i = 1; i <= slices; ++i)
	{
		queue_line(vertices[0], vertices[i], color, depth_write);
		queue_line(vertices[i], vertices[i + 1], color, depth_write);
	}

	uint32_t base_index{ 1 };
	uint32_t ring_vertex_count{ slices + 1 };
	for (uint32_t i = 0; i < stacks - 2; ++i)
	{
		for (uint32_t j = 0; j < slices; ++j)
		{
			uint32_t index[6] =
			{
				{base_index + i * ring_vertex_count + j},
				{base_index + (i + 1) * ring_vertex_count + j},
				{base_index + i * ring_vertex_count + j + 1},

				{base_index + (i + 1) * ring_vertex_count + j},
				{base_index + (i + 1) * ring_vertex_count + j + 1},
				{base_index + i * ring_vertex_count + j + 1},
			};
			queue_line(vertices[index[0]], vertices[index[1]], color, depth_write);
			queue_line(vertices[index[2]], vertices[index[0]], color, depth_write);
			queue_line(vertices[index[3]], vertices[index[4]], color, depth_write);
			queue_line(vertices[index[4]], vertices[index[5]], color, depth_write);
		}
	}
	uint32_t south_pole_index{ static_cast<uint32_t>(vertices.size() - 1) };
	base_index = south_pole_index - ring_vertex_count;
	for (uint32_t i = 0; i < slices; ++i)
	{
		queue_line(vertices[base_index + i + 1], vertices[base_index + i], color, depth_write);
		queue_line(vertices[south_pole_index], vertices[base_index + i], color, depth_write);
	}
}

void debug_primitive_manager::queue_capsule(const math::double3 center, const math::float4 rotation, float radius, float length, const math::float4& color, bool depth_write)
{
	//	カプセル形状を登録
	const math::quaternionf quaternion = math::quaternionf(rotation).normalized();
	const math::vector3d Center(center);
	const math::vector3d Up(quaternion.up());
	const math::vector3d Right(quaternion.right());
	const math::vector3d Front(quaternion.front());

	const math::vector3d Botom = Center + Up * (-length / 2);
	const math::vector3d Top = Center + Up * (+length / 2);

	math::double3 position, position2;
	position = Botom.to_double3();
	queue_sphere(position, radius, color, depth_write);
	position = Top.to_double3();
	queue_sphere(position, radius, color, depth_write);

	position = (Botom + Right * radius).to_double3();
	position2 = (Top + Right * radius).to_double3();
	queue_line(position, position2, color, depth_write);

	position = (Botom + Right * -radius).to_double3();
	position2 = (Top + Right * -radius).to_double3();
	queue_line(position, position2, color, depth_write);

	position = (Botom + Front * radius).to_double3();
	position2 = (Top + Front * radius).to_double3();
	queue_line(position, position2, color, depth_write);

	position = (Botom + Front * -radius).to_double3();
	position2 = (Top + Front * -radius).to_double3();
	queue_line(position, position2, color, depth_write);

	const math::vector3d Direction1 = (Right + Front).normalized();
	const math::vector3d Direction2 = (Right - Front).normalized();

	position = (Botom + Direction1 * radius).to_double3();
	position2 = (Top + Direction1 * radius).to_double3();
	queue_line(position, position2, color, depth_write);

	position = (Botom + Direction1 * -radius).to_double3();
	position2 = (Top + Direction1 * -radius).to_double3();
	queue_line(position, position2, color, depth_write);

	position = (Botom + Direction2 * radius).to_double3();
	position2 = (Top + Direction2 * radius).to_double3();
	queue_line(position, position2, color, depth_write);

	position = (Botom + Direction2 * -radius).to_double3();
	position2 = (Top + Direction2 * -radius).to_double3();
	queue_line(position, position2, color, depth_write);
}

void debug_primitive_manager::queue_box(const math::double3 position, const math::float4 rotation, const math::float3 scale, const math::float4& color, bool depth_write)
{
	collision::bounding_box box{math::double3(scale), math::double4(rotation), position};
	math::double3 vertices[8];
	box.calculate_vertex(vertices, 8);

	queue_line(vertices[0], vertices[1], color, depth_write);
	queue_line(vertices[1], vertices[2], color, depth_write);
	queue_line(vertices[2], vertices[3], color, depth_write);
	queue_line(vertices[3], vertices[0], color, depth_write);

	queue_line(vertices[4], vertices[5], color, depth_write);
	queue_line(vertices[5], vertices[6], color, depth_write);
	queue_line(vertices[6], vertices[7], color, depth_write);
	queue_line(vertices[7], vertices[4], color, depth_write);

	queue_line(vertices[0], vertices[4], color, depth_write);
	queue_line(vertices[1], vertices[5], color, depth_write);
	queue_line(vertices[2], vertices[6], color, depth_write);
	queue_line(vertices[3], vertices[7], color, depth_write);
}


void debug_primitive_manager::queue_line(const math::float3 start, const math::float3 end, const math::float4 color, bool depth_write)
{
	queue_line(math::double3(start), math::double3(end), color, depth_write);
}

void debug_primitive_manager::queue_sphere(const math::float3 center, float radius, const math::float4 color, bool depth_write)
{
	queue_sphere(math::double3(center), radius, color, depth_write);
}

void debug_primitive_manager::queue_capsule(const math::float3 center, const math::float4 rotation, float radius, float length, const math::float4& color, bool depth_write)
{
	queue_capsule(math::double3(center), rotation, radius, length, color, depth_write);
}

void debug_primitive_manager::queue_box(const math::float3 position, const math::float4 rotation, const math::float3 scale, const math::float4& color, bool depth_write)
{
	queue_box(math::double3(position), rotation, scale, color, depth_write);
}

#endif	//	defined(USE_IMGUI)