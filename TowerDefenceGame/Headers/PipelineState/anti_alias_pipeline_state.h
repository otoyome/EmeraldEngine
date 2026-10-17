#pragma once

#include "common_pipeline_states.h"

class fxaa_pipeline_state : public graphics_pipeline_state
{
public:
	fxaa_pipeline_state();
	~fxaa_pipeline_state() override;
};

class smaa_edge_detection_pipeline_state : public graphics_pipeline_state
{
public:
	smaa_edge_detection_pipeline_state();
	~smaa_edge_detection_pipeline_state() override;
};

class smaa_blending_weight_calculation_pipeline_state : public graphics_pipeline_state
{
public:
	smaa_blending_weight_calculation_pipeline_state();
	~smaa_blending_weight_calculation_pipeline_state() override;

private:
	texture_ptr area_texture;
	texture_ptr search_texture;
};

class smaa_neighborhood_blending_pipeline_state : public graphics_pipeline_state
{
public:
	smaa_neighborhood_blending_pipeline_state();
	~smaa_neighborhood_blending_pipeline_state() override;
};
