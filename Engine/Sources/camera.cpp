#include "camera.h"
#include "graphics/meshlet_culling_system.h"

camera::camera()
: system_flags(0)
, position(0.0f, 0.0f, 0.0f)
, rotation(0.0f, 0.0f, 0.0f)

, right(1.0f, 0.0f, 0.0f)
, up(0.0f, 1.0f, 0.0f)
, front(0.0f, 0.0f, 1.0f)

, view_matrix()
, projection_matrix()

, previous_position(0.0, 0.0, 0.0)
, previous_rotation(math::quaternionf::identity())
, previous_projection_matrix()

, fov_y(math::angle::to_radian(45.0f))
, screen_rect(0.0f, 0.0f, 1920.0f, 1080.0f)
, clip_distance(1.0f, 10000.0f)
, hybrid_rate(0.3f)
{
	set_flag(flags::active, true);
	set_flag(flags::dirty, true);
	set_flag(flags::update_previous_matrix, true);
}

camera::~camera()
{
}

bool camera::is_flag(flags flag) const
{
	return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag));
}

void camera::set_flag(flags flag, bool value)
{
	if (flag == flags::hybrid_projection && value != is_flag(flags::hybrid_projection))
	{
		set_flag(flags::dirty, true);
	}

	if (flag == flags::camera_relative && value != is_flag(flags::camera_relative))
	{
		//	描画の原点が変わるため作り直す
		set_flag(flags::dirty, true);
	}

	system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value);
}

const math::vector3d& camera::get_position() const
{
	return position;
}

void camera::set_position(const math::vector3d& position)
{
	set_flag(flags::dirty, true);
	this->position = position;
}

math::vector3d camera::get_render_origin() const
{
	return is_flag(flags::camera_relative) ? position : math::vector3d::zero();
}

math::float3 camera::get_relative_position() const
{
	return (position - get_render_origin()).to_float3();
}

const math::rotatorf& camera::get_rotation() const
{
	return rotation;
}

void camera::set_rotation(const math::rotatorf& rotation)
{
	set_flag(flags::dirty, true);
	this->rotation = rotation;
}

float camera::get_fov_y() const
{
	return fov_y;
}

void camera::set_fov_y(float fov_y)
{
	set_flag(flags::dirty, true);
	this->fov_y = fov_y;
}

float camera::get_hybrid_rate() const
{
	return hybrid_rate;
}

void camera::set_hybrid_rate(float hybrid_rate)
{
	set_flag(flags::dirty, true);
	this->hybrid_rate = hybrid_rate;
}

const math::float4& camera::get_screen_rect() const
{
	return screen_rect;
}

void camera::set_screen_rect(math::float4 screen_rect)
{
	set_flag(flags::dirty, true);
	this->screen_rect = screen_rect;
}

const math::float2& camera::get_clip_distance() const
{
	return clip_distance;
}

void camera::set_clip_distance(math::float2 clip_distance)
{
	set_flag(flags::dirty, true);
	this->clip_distance = clip_distance;
}

math::float2 camera::get_effective_clip_distance() const
{
	math::float2 ret = clip_distance;
	if (!(ret.y > ret.x))
	{
		//	無限遠として扱う
		ret.y = math::constants<float>::infinity;
	}
	return ret;
}

const math::float3& camera::get_right()
{
	update_parameters();
	return right;
}

const math::float3& camera::get_up()
{
	update_parameters();
	return up;
}

const math::float3& camera::get_front()
{
	update_parameters();
	return front;
}

const math::float4x4& camera::get_view_matrix()
{
	update_parameters();
	return view_matrix;
}

const math::float4x4& camera::get_projection_matrix()
{
	update_parameters();
	return projection_matrix;
}

const math::float4x4& camera::get_view_projection_matrix()
{
	update_parameters();
	return view_projection_matrix;
}

const math::float4x4& camera::get_previous_view_projection_matrix()
{
	update_parameters();

	//	前回の情報が無い場合は現在の行列を使う
	if (is_flag(flags::update_previous_matrix))
	{
		previous_view_projection_matrix = view_projection_matrix;
		return previous_view_projection_matrix;
	}

	//	前回の視線行列は、現在の描画の原点から見た前回のカメラ位置で作り直す
	const math::vector3f previous_relative_position((previous_position - get_render_origin()).to_float3());
	const math::matrixf previous_view = math::matrixf::inverse_rigid_transformation(previous_rotation, previous_relative_position);
	previous_view_projection_matrix = (previous_view * math::matrixf(previous_projection_matrix)).to_float4x4();
	return previous_view_projection_matrix;
}

void camera::cache_previous_view_projection_matrix()
{
	update_parameters();

	set_flag(flags::update_previous_matrix, false);
	previous_position = position;
	previous_rotation = rotation.to_quaternion();
	previous_projection_matrix = projection_matrix;
}

const collision::frustum_culling& camera::get_frustum_culling()
{
	update_parameters();
	return frustum_culling;
}

void camera::update_parameters()
{
	if (!is_flag(flags::dirty))
		return;

	set_flag(flags::dirty, false);

	const math::quaternionf rotation_quaternion = rotation.to_quaternion();
	const math::matrixf rotation_matrix = math::matrixf::rotation_quaternion(rotation_quaternion);
	right = rotation_matrix.to_vector3(0).to_float3();
	up = rotation_matrix.to_vector3(1).to_float3();
	front = rotation_matrix.to_vector3(2).to_float3();

	//	描画の原点からの相対位置で視線行列を作る
	const math::vector3f relative_position(get_relative_position());
	const math::matrixf view = math::matrixf::inverse_rigid_transformation(rotation_quaternion, relative_position);
	view_matrix = view.to_float4x4();

	const float aspect_ratio = static_cast<float>(screen_rect.z - screen_rect.x) / static_cast<float>(screen_rect.w - screen_rect.y);
	const math::float2 effective_clip_distance = get_effective_clip_distance();
	math::matrixf projection = math::matrixf::perspective_fov(fov_y, aspect_ratio, effective_clip_distance.x, effective_clip_distance.y);
	if (is_flag(flags::hybrid_projection))
	{
		//	平行投影とのブレンド処理
		float p_r = math::saturate(hybrid_rate);
		float o_r = 1.0f - p_r;

		float ortho_height = tanf(0.5f * fov_y);
		float ortho_width = ortho_height * aspect_ratio;
		const math::matrixf ortho_projection = math::matrixf::orthographic(ortho_width, ortho_height, effective_clip_distance.x, effective_clip_distance.y);

		projection = projection * p_r + ortho_projection * o_r;
	}
	projection_matrix = projection.to_float4x4();

	const math::matrixf view_projection = view * projection;
	view_projection_matrix = view_projection.to_float4x4();

	frustum_culling.create_planes(view_projection_matrix);
	const math::float4x4 inverse_view_projection = view_projection.inverted().to_float4x4();
	frustum_culling.create_frustum_corners(inverse_view_projection);
}

std::shared_ptr<occlusion_culling_system> camera::get_occlusion_culling()
{
	if (!occlusion_culling)
		occlusion_culling = std::make_shared<occlusion_culling_system>();
	return occlusion_culling;
}

std::shared_ptr<hiz_buffer> camera::get_hiz_buffer()
{
	if (!hiz)
		hiz = std::make_shared<hiz_buffer>();
	return hiz;
}

std::shared_ptr<meshlet_culling_system> camera::get_meshlet_culling()
{
	if (!meshlet_culling)
		meshlet_culling = std::make_shared<meshlet_culling_system>();
	return meshlet_culling;
}

#if defined(USE_IMGUI)
bool camera::resolve_debug_visible(UINT unique_id, bool current_visible)
{
	std::lock_guard<std::mutex> lock(frozen_visibilities_mutex);

	if (!freeze_culling)
	{
		frozen_visibilities[unique_id] = current_visible;
		return current_visible;
	}

	auto it = frozen_visibilities.find(unique_id);
	if (it == frozen_visibilities.end())
	{
		frozen_visibilities.emplace(unique_id, current_visible);
		return current_visible;
	}
	return it->second;
}
#endif	//	defined(USE_IMGUI)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
camera_manager::camera_manager()
{
}

camera_manager::~camera_manager()
{
}

bool camera_manager::generate_camera(camera_ptr& out_camera)
{
	out_camera = std::make_shared<camera>();
	cameras.emplace_back(out_camera);
	return true;
}

void camera_manager::get_container(camera_container& container)
{
	std::size_t constant_buffer_count = 0;

	container.cameras.clear();
	auto it = cameras.begin();
	for (; it != cameras.end();)
	{
		if (it->expired())
		{
			it = cameras.erase(it);
			continue;
		}

		auto camera = it->lock();
		if (!camera->is_flag(camera::flags::active))
			continue;

		//	プリミティブ用定数バッファ登録
		if (constant_buffer_count + 1 >= constant_bufferes.size())
		{
			generate_constant_bufferes();
		}

		camera_container::camera_data data;
		data.camera = camera;
		data.constant_buffer = constant_bufferes[constant_buffer_count++];
		container.cameras.emplace_back(data);
		++it;
	}
}

void camera_manager::generate_constant_bufferes()
{
	auto render_data = render_data::instance();
	auto device = render_data->get_device();

	HRESULT hr;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	constant_bufferes.emplace_back();
	auto& buffer = constant_bufferes.back();
	buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
	hr = device->CreateBuffer(&buffer_desc, nullptr, buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
