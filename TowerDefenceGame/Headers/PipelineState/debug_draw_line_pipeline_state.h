#pragma once

#include "common_pipeline_states.h"

//	デバッグプリミティブ描画用(深度テスト有り)
class debug_draw_line_pipeline_state : public graphics_pipeline_state
{
protected:
	debug_draw_line_pipeline_state(bool depth_test);

public:
	debug_draw_line_pipeline_state();
	~debug_draw_line_pipeline_state() override;
};

//	デバッグプリミティブ描画用(深度テスト無し)
class debug_draw_line_disable_depth_test_pipeline_state : public debug_draw_line_pipeline_state
{
public:
	debug_draw_line_disable_depth_test_pipeline_state();
	~debug_draw_line_disable_depth_test_pipeline_state() override;
};
