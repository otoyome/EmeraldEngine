#pragma once

#include "common_pipeline_states.h"

namespace decal_material_parameter_name
{
	static constexpr std::string_view base_color = "base_color";
	static constexpr std::string_view base_texture = "base_texture";
	static constexpr std::string_view decal_direction = "decal_direction";
	static constexpr std::string_view decal_inverse_transform = "decal_inverse_transform";
}	//	namespace decal_material_parameter_name

//	遅延デカールプリミティブステンシル書き込みパス
class deferred_decal_stencil_write_pipeline_state : public graphics_pipeline_state
{
public:
	deferred_decal_stencil_write_pipeline_state();
	~deferred_decal_stencil_write_pipeline_state() override;
};
