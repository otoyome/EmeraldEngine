#pragma once

#include "common_pipeline_states.h"

//	•`‚«–ß‚µ
class write_back_pipeline_state : public graphics_pipeline_state
{
public:
	write_back_pipeline_state();
	~write_back_pipeline_state() override;
};

//	Fî•ñ‚ğ‘Î”ˆ³k
class compress_rgb_to_log_pipeline_state : public graphics_pipeline_state
{
public:
	compress_rgb_to_log_pipeline_state();
	~compress_rgb_to_log_pipeline_state() override;
};

//	‘Î”ˆ³k‚³‚ê‚½Fî•ñ‚ğ‰ğ“€
class decompress_rgb_to_log_pipeline_state : public graphics_pipeline_state
{
public:
	decompress_rgb_to_log_pipeline_state();
	~decompress_rgb_to_log_pipeline_state() override;
};

//	SSR:UV¶¬
class screen_space_reflection_uv_pipeline_state : public graphics_pipeline_state
{
public:
	screen_space_reflection_uv_pipeline_state();
	~screen_space_reflection_uv_pipeline_state() override;
};

//	SSR:Color¶¬
class screen_space_reflection_color_pipeline_state : public graphics_pipeline_state
{
public:
	screen_space_reflection_color_pipeline_state();
	~screen_space_reflection_color_pipeline_state() override;
};

//	SSR:‡¬
class screen_space_reflection_composite_pipeline_state : public graphics_pipeline_state
{
public:
	screen_space_reflection_composite_pipeline_state();
	~screen_space_reflection_composite_pipeline_state() override;
};
