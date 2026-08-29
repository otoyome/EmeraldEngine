#include "compute_particle_default_rendering.hlsli"

#define VS_OUT PS_IN
#include "compute_particle_default_surface_alpha.hlsli"

#include "surface_depth_write_ps.hlsli"
