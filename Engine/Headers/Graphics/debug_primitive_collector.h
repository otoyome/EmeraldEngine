#pragma once

#if defined(USE_IMGUI)
#include "graphics_context.h"
#include "render_data.h"

//	デバッグ図形描画管理クラス
//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
class debug_primitive_manager
{
	SINGLETON(debug_primitive_manager);

	static constexpr size_t VertexCount = 0x000FFFFF;

	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

private:
	debug_primitive_manager();
	~debug_primitive_manager();

public:
	void get_container(debug_draw_primitive_container& container);

	void queue_line(const DirectX::XMFLOAT3 start, const DirectX::XMFLOAT3 end, const DirectX::XMFLOAT4 color, bool depth_write = true);
	void queue_sphere(const DirectX::XMFLOAT3 center, float radius, const DirectX::XMFLOAT4 color, bool depth_write = true);
	void queue_capsule(const DirectX::XMFLOAT3 center, const DirectX::XMFLOAT4 rotation, float radius, float length, const DirectX::XMFLOAT4& color, bool depth_write = true);
	void queue_box(const DirectX::XMFLOAT3 position, const DirectX::XMFLOAT4 rotation, const DirectX::XMFLOAT3 scale, const DirectX::XMFLOAT4& color, bool depth_write = true);

private:
	std::vector<vertex> vertices[static_cast<int>(debug_draw_primitive_container::line_buffer_id::max)];
	std::unordered_map<semantic, buffer_view> vertex_buffer_views[static_cast<int>(debug_draw_primitive_container::line_buffer_id::max)];
};

#define QUEUE_DEBUG_PRIMITIVE_LINE(START, END, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->queue_line(START, END, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_SPHERE(CENTER, RADIUS, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->queue_sphere(CENTER, RADIUS, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_CAPSULE(CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->queue_capsule(CENTER, ROTATION, RADIUS, LENGTH, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_BOX(POSITION, ROTATION, SCALE, COLOR, DEPTH_TEST) debug_primitive_manager::instance()->queue_box(SCALE, ROTATION, POSITION, COLOR, DEPTH_TEST)

#else // defined(USE_IMGUI)

#define QUEUE_DEBUG_PRIMITIVE_LINE(START, END, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_SPHERE(CENTER, RADIUS, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_CAPSULE(CENTER, RADIUS, LENGTH, COLOR, DEPTH_TEST)
#define QUEUE_DEBUG_PRIMITIVE_BOX(POSITION, ROTATION, SCALE, COLOR, DEPTH_TEST)

#endif // defined(USE_IMGUI)
