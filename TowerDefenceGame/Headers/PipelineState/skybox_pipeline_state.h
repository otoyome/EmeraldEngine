#pragma once

#include "common_pipeline_states.h"

//	スカイボックスパス
class skybox_pipeline_state : public graphics_pipeline_state
{
public:
	skybox_pipeline_state();
	~skybox_pipeline_state() override;
};
