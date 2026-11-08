#pragma once

//	シーンビューパラメーター
struct view_parameters
{
	math::vector3d position = math::vector3d(0.0, 0.0, 0.0);				//	ワールド座標
	math::rotatorf rotation = math::rotatorf(0.0f, 0.0f, 0.0f);				//	回転情報
	float fov_y = math::angle::to_radian(45.0f);							//	視野角
	math::float2 clip_distance = math::float2(1.0f, 0.0f);					//	クリップ距離(x : 近クリップ, y : 遠クリップ)
	math::float4 screen_rect = math::float4(0.0f, 0.0f, 1920.0f, 1080.0f);	//	スクリーン矩形

	bool hybrid_projection = false;											//	透視投影/正射影のブレンドフラグ
	float hybrid_rate = 0.3f;												//	透視投影/正射影のブレンド率

	//	クリップ距離
	math::float2 get_effective_clip_distance() const
	{
		math::float2 ret = clip_distance;
		if (!(ret.y > ret.x))
		{
			//	無限遠として扱う
			ret.y = math::constants<float>::infinity;
		}
		return ret;
	}

	//	アスペクト比取得
	float get_aspect_ratio() const
	{
		return static_cast<float>(screen_rect.z - screen_rect.x) / static_cast<float>(screen_rect.w - screen_rect.y);
	}

	//	透視投影行列生成
	math::matrixf make_projection_matrix() const
	{
		const float aspect_ratio = get_aspect_ratio();
		const math::float2 effective_clip_distance = get_effective_clip_distance();
		math::matrixf projection = math::matrixf::perspective_fov(fov_y, aspect_ratio, effective_clip_distance.x, effective_clip_distance.y);
		if (hybrid_projection)
		{
			//	平行投影とのブレンド処理
			float p_r = math::saturate(hybrid_rate);
			float o_r = 1.0f - p_r;

			float ortho_height = tanf(0.5f * fov_y);
			float ortho_width = ortho_height * aspect_ratio;
			const math::matrixf ortho_projection = math::matrixf::orthographic(ortho_width, ortho_height, effective_clip_distance.x, effective_clip_distance.y);

			projection = projection * p_r + ortho_projection * o_r;
		}
		return projection;
	}
};
