#pragma once

#include "common_pipeline_states.h"

//	リニア色空間>sRGB色空間に変換
class convert_linear_to_srgb_pipeline_state : public graphics_pipeline_state
{
public:
	convert_linear_to_srgb_pipeline_state();
	~convert_linear_to_srgb_pipeline_state() override;
};

//	リニア色空間>sRGB色空間に変換
class convert_srgb_to_linear_pipeline_state : public graphics_pipeline_state
{
public:
	convert_srgb_to_linear_pipeline_state();
	~convert_srgb_to_linear_pipeline_state() override;
};






