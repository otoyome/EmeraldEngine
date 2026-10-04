#pragma once

#include	"collision.h"
#include	"line.h"

namespace collision
{
	class plane
	{
	public:
		math::float3 get_plane_position() const;
		math::float3 get_normal() const;
		float get_distance() const;
		const math::float4& get_parameter() const;
		void set_parameter(const math::float4& parameter);

	public:
		plane(const math::float4& parameter = math::float4(0.0f, 0.0f, 0.0f, 0.0f));
		plane(const math::float3& normal, float distance);

		void initialize(const math::float4& parameter);
		void initialize(const math::float3& normal, float distance);

		//	³‹K‰»
		void normlize();

		//	“_‚Æ•½–Ê‚Ì‹——£
		float calculate_distance(const math::float3& point) const;

		//	ü‚Æ•½–Ê‚ÌŒğ·”»’è
		bool to_line(const line& line, float* distance = nullptr, math::float3* insert_point = nullptr) const;

	private:
		math::float4 parameter; //	xyz : –@ü, w : ‹——£
	};
}	//	namespace collision

