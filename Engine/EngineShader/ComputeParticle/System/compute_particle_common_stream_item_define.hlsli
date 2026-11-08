//  compute_particle_common_stream_item_undef とニコイチで使う事
#define asuint2 asuint
#define asuint3 asuint
#define asuint4 asuint

#define asfloat2 asfloat
#define asfloat3 asfloat
#define asfloat4 asfloat

#define asint2 asint
#define asint3 asint
#define asint4 asint

#define SET_DUMMY_VALUE(TYPE) value = (TYPE)0
#define OFFSET1 offset += 4 * 1
#define OFFSET2 offset += 4 * 2
#define OFFSET3 offset += 4 * 3
#define OFFSET4 offset += 4 * 4
#define CAST(TYPE) as##TYPE

//  これだけは一回のみインクルードさせたいので
#ifndef __RW_CONSTANT_VALUE__
#define __RW_CONSTANT_VALUE__
static const uint rw_read_only = 0;
static const uint rw_read = 1;
static const uint rw_write = 2;
static const uint rw_none = -1;

//  ダミー関数群生成マクロ
#define TYPE1_GENERATE_FUNCTIONS(TYPE) \
void stream_none(inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET1; } \
void stream_read_srv(int4 dummy_srv_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET1; } \
void stream_read_uav(int4 dummy_uav_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET1; } \
void stream_write(int4 dummy_uav_buffer, inout uint offset, in TYPE value) { OFFSET1; }

#define TYPE2_GENERATE_FUNCTIONS(TYPE) \
void stream_none(inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET2; } \
void stream_read_srv(int4 dummy_srv_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET2; } \
void stream_read_uav(int4 dummy_uav_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET2; } \
void stream_write(int4 dummy_uav_buffer, inout uint offset, in TYPE value) { OFFSET2; }

#define TYPE3_GENERATE_FUNCTIONS(TYPE) \
void stream_none(inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET3; } \
void stream_read_srv(int4 dummy_srv_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET3; } \
void stream_read_uav(int4 dummy_uav_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET3; } \
void stream_write(int4 dummy_uav_buffer, inout uint offset, in TYPE value) { OFFSET3; }

#define TYPE4_GENERATE_FUNCTIONS(TYPE) \
void stream_none(inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET4; } \
void stream_read_srv(int4 dummy_srv_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET4; } \
void stream_read_uav(int4 dummy_uav_buffer, inout uint offset, out TYPE value) { SET_DUMMY_VALUE(TYPE); OFFSET4; } \
void stream_write(int4 dummy_uav_buffer, inout uint offset, in TYPE value) { OFFSET4; }

#include "compute_particle_type_generate_function_stream_item.hlsli"

#undef TYPE1_GENERATE_FUNCTIONS
#undef TYPE2_GENERATE_FUNCTIONS
#undef TYPE3_GENERATE_FUNCTIONS
#undef TYPE4_GENERATE_FUNCTIONS

#endif  //  __RW_CONSTANT_VALUE__
