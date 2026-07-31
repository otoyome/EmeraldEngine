#ifndef __COMPUTE_PARTICLE_SRV_STREAM_ITEM_HLSLI__
#define __COMPUTE_PARTICLE_SRV_STREAM_ITEM_HLSLI__

#include "compute_particle_common_stream_item_define.hlsli"

#define SRV_ARGUMENT ByteAddressBuffer buffer
#define SRV_LOAD1_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load(offset)); OFFSET1;
#define SRV_LOAD2_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load2(offset)); OFFSET2;
#define SRV_LOAD3_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load3(offset)); OFFSET3;
#define SRV_LOAD4_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load4(offset)); OFFSET4;

//  ê∂ê¨É}ÉNÉç
#define TYPE1_GENERATE_FUNCTIONS(TYPE) \
void stream_read_srv(SRV_ARGUMENT, inout uint offset, inout TYPE value) { SRV_LOAD1_OFFSET(TYPE); }
#define TYPE2_GENERATE_FUNCTIONS(TYPE) \
void stream_read_srv(SRV_ARGUMENT, inout uint offset, inout TYPE value) { SRV_LOAD2_OFFSET(TYPE); }
#define TYPE3_GENERATE_FUNCTIONS(TYPE) \
void stream_read_srv(SRV_ARGUMENT, inout uint offset, inout TYPE value) { SRV_LOAD3_OFFSET(TYPE); }
#define TYPE4_GENERATE_FUNCTIONS(TYPE) \
void stream_read_srv(SRV_ARGUMENT, inout uint offset, inout TYPE value) { SRV_LOAD4_OFFSET(TYPE); }

#include "compute_particle_type_generate_function_stream_item.hlsli"

#undef TYPE1_GENERATE_FUNCTIONS
#undef TYPE2_GENERATE_FUNCTIONS
#undef TYPE3_GENERATE_FUNCTIONS
#undef TYPE4_GENERATE_FUNCTIONS

#undef SRV_LOAD1_OFFSET
#undef SRV_LOAD2_OFFSET
#undef SRV_LOAD3_OFFSET
#undef SRV_LOAD4_OFFSET

#include "compute_particle_common_stream_item_undef.hlsli"

#endif  //  __COMPUTE_PARTICLE_SRV_STREAM_ITEM_HLSLI__
