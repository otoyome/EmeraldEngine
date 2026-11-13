#pragma once

#if defined(USE_IMGUI)
#include "Graphics/Core/graphics_context.h"
#include "Graphics/Core/render_data.h"

//	デバッグ図形描画管理クラス
//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
class debug_primitive_collector
{
	static constexpr size_t VertexCount = 0x000FFFFF;

	struct vertex
	{
		math::float3 position;
		math::float4 color;
	};

	struct queued_vertex
	{
		math::double3 position;
		math::float4 color;
	};

public:
	debug_primitive_collector();
	~debug_primitive_collector();

	debug_primitive_collector(const debug_primitive_collector&) = delete;
	debug_primitive_collector& operator=(const debug_primitive_collector&) = delete;

	template<class Owner>
	static debug_primitive_collector& from(Owner& owner) { return owner.get_debug_primitives(); }
	static debug_primitive_collector& from(debug_primitive_collector& collector) { return collector; }

	void get_container(debug_draw_primitive_container& container, const math::vector3d& render_origin);

	void queue_line(const math::double3 start, const math::double3 end, const math::float4 color, bool depth_write = true);
	void queue_sphere(const math::double3 center, float radius, const math::float4 color, bool depth_write = true);
	void queue_capsule(const math::double3 center, const math::float4 rotation, float radius, float length, const math::float4& color, bool depth_write = true);
	void queue_box(const math::double3 position, const math::float4 rotation, const math::float3 scale, const math::float4& color, bool depth_write = true);

	void queue_line(const math::float3 start, const math::float3 end, const math::float4 color, bool depth_write = true);
	void queue_sphere(const math::float3 center, float radius, const math::float4 color, bool depth_write = true);
	void queue_capsule(const math::float3 center, const math::float4 rotation, float radius, float length, const math::float4& color, bool depth_write = true);
	void queue_box(const math::float3 position, const math::float4 rotation, const math::float3 scale, const math::float4& color, bool depth_write = true);

private:
	void create_buffers();

private:
	std::vector<queued_vertex> vertices[magic_enum::enum_count<debug_draw_primitive_container::line_buffer_id>()];
	std::unordered_map<semantic, buffer_view> vertex_buffer_views[magic_enum::enum_count<debug_draw_primitive_container::line_buffer_id>()];
	std::vector<vertex> upload_vertices;
	bool buffers_created = false;
};

#define QUEUE_DEBUG_PRIMITIVE_LINE(CONTEXT, START, END, COLOR, DEPTH_TEST) debug_primitive_collector::from(CONTEXT).queue_line(START, END, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_SPHERE(CONTEXT, CENTER, RADIUS, COLOR, DEPTH_TEST) debug_primitive_collector::from(CONTEXT).queue_sphere(CENTER, RADIUS, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_CAPSULE(CONTEXT, CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST) debug_primitive_collector::from(CONTEXT).queue_capsule(CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_BOX(CONTEXT, POSITION, ROTATION, SCALE, COLOR, DEPTH_TEST) debug_primitive_collector::from(CONTEXT).queue_box(POSITION, ROTATION, SCALE, COLOR, DEPTH_TEST)

#else // defined(USE_IMGUI)

#define QUEUE_DEBUG_PRIMITIVE_LINE(CONTEXT, START, END, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_SPHERE(CONTEXT, CENTER, RADIUS, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_CAPSULE(CONTEXT, CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_BOX(CONTEXT, POSITION, ROTATION, SCALE, COLOR, DEPTH_TEST)

#endif // defined(USE_IMGUI)
