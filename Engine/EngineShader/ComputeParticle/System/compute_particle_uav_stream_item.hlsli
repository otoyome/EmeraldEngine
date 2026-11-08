#ifndef __COMPUTE_PARTICLE_UAV_STREAM_ITEM_HLSLI__
#define __COMPUTE_PARTICLE_UAV_STREAM_ITEM_HLSLI__

#include "compute_particle_common_stream_item_define.hlsli"

#define UAV_ARGUMENT RWByteAddressBuffer buffer
#define UAV_LOAD1_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load(offset)); OFFSET1;
#define UAV_LOAD2_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load2(offset)); OFFSET2;
#define UAV_LOAD3_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load3(offset)); OFFSET3;
#define UAV_LOAD4_OFFSET(TYPE) value = CAST(TYPE)(buffer.Load4(offset)); OFFSET4;

#define UAV_WRITE1_OFFSET(TYPE) buffer.Store(offset, asuint(value)); OFFSET1;
#define UAV_WRITE2_OFFSET(TYPE) buffer.Store2(offset, asuint(value)); OFFSET2;
#define UAV_WRITE3_OFFSET(TYPE) buffer.Store3(offset, asuint(value)); OFFSET3;
#define UAV_WRITE4_OFFSET(TYPE) buffer.Store4(offset, asuint(value)); OFFSET4;

//  ê∂ê¨É}ÉNÉç
#define TYPE1_GENERATE_FUNCTIONS(TYPE) \
void stream_read_uav(UAV_ARGUMENT, inout uint offset, inout TYPE value) { UAV_LOAD1_OFFSET(TYPE); } \
void stream_write(UAV_ARGUMENT, inout uint offset, in TYPE value) { UAV_WRITE1_OFFSET(TYPE); }
#define TYPE2_GENERATE_FUNCTIONS(TYPE) \
void stream_read_uav(UAV_ARGUMENT, inout uint offset, inout TYPE value) { UAV_LOAD2_OFFSET(TYPE); } \
void stream_write(UAV_ARGUMENT, inout uint offset, in TYPE value) { UAV_WRITE2_OFFSET(TYPE); }
#define TYPE3_GENERATE_FUNCTIONS(TYPE) \
void stream_read_uav(UAV_ARGUMENT, inout uint offset, inout TYPE value) { UAV_LOAD3_OFFSET(TYPE); } \
void stream_write(UAV_ARGUMENT, inout uint offset, in TYPE value) { UAV_WRITE3_OFFSET(TYPE); }
#define TYPE4_GENERATE_FUNCTIONS(TYPE) \
void stream_read_uav(UAV_ARGUMENT, inout uint offset, inout TYPE value) { UAV_LOAD4_OFFSET(TYPE); } \
void stream_write(UAV_ARGUMENT, inout uint offset, in TYPE value) { UAV_WRITE4_OFFSET(TYPE); }


#include "compute_particle_type_generate_function_stream_item.hlsli"

#undef TYPE1_GENERATE_FUNCTIONS
#undef TYPE2_GENERATE_FUNCTIONS
#undef TYPE3_GENERATE_FUNCTIONS
#undef TYPE4_GENERATE_FUNCTIONS

#undef UAV_WRITE1_OFFSET
#undef UAV_WRITE2_OFFSET
#undef UAV_WRITE3_OFFSET
#undef UAV_WRITE4_OFFSET

#undef UAV_LOAD1_OFFSET
#undef UAV_LOAD2_OFFSET
#undef UAV_LOAD3_OFFSET
#undef UAV_LOAD4_OFFSET

#include "compute_particle_common_stream_item_undef.hlsli"

#endif  //  __COMPUTE_PARTICLE_UAV_STREAM_ITEM_HLSLI__
