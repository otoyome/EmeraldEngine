//  インクルード前に下記の物を定義しておく必要がある
//  使わない場合は未定義でOK
//  #define UAV RWByteAddressBuffer
//  #define SRV ByteAddressBuffer
//  #define PREFIX 関数名の接頭子を付ける

#ifdef UAV
#include "compute_particle_uav_stream_item.hlsli"
#define UAV_GLOBAL_VARIABLE UAV
#else   //  UAV
#define UAV_GLOBAL_VARIABLE (0)
#endif  //  UAV

#ifdef SRV
#include "compute_particle_srv_stream_item.hlsli"
#define SRV_GLOBAL_VARIABLE SRV
#else   //  SRV
#define SRV_GLOBAL_VARIABLE (0)
#endif  //  SRV

#ifdef PREFIX
#define FUNCITION_NAME_MERGE(A, B) A##B
#else   //  PREFIX
#define FUNCITION_NAME_MERGE(A, B) B
#endif  //  PREFIX


//  生成マクロ
#define TYPE1_GENERATE_FUNCTIONS(TYPE) \
void FUNCITION_NAME_MERGE(PREFIX, stream_ptr)(uint rw, inout uint offset, inout TYPE value) { \
    if (rw == rw_read_only) stream_read_srv(SRV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_read) stream_read_uav(UAV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_write) stream_write(UAV_GLOBAL_VARIABLE, offset, value); \
    else stream_none(offset, value); \
}
#define TYPE2_GENERATE_FUNCTIONS(TYPE) \
void FUNCITION_NAME_MERGE(PREFIX, stream_ptr)(uint rw, inout uint offset, inout TYPE value) { \
    if (rw == rw_read_only) stream_read_srv(SRV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_read) stream_read_uav(UAV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_write) stream_write(UAV_GLOBAL_VARIABLE, offset, value); \
    else stream_none(offset, value); \
}
#define TYPE3_GENERATE_FUNCTIONS(TYPE) \
void FUNCITION_NAME_MERGE(PREFIX, stream_ptr)(uint rw, inout uint offset, inout TYPE value) { \
    if (rw == rw_read_only) stream_read_srv(SRV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_read) stream_read_uav(UAV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_write) stream_write(UAV_GLOBAL_VARIABLE, offset, value); \
    else stream_none(offset, value); \
}
#define TYPE4_GENERATE_FUNCTIONS(TYPE) \
void FUNCITION_NAME_MERGE(PREFIX, stream_ptr)(uint rw, inout uint offset, inout TYPE value) { \
    if (rw == rw_read_only) stream_read_srv(SRV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_read) stream_read_uav(UAV_GLOBAL_VARIABLE, offset, value); \
    else if (rw == rw_write) stream_write(UAV_GLOBAL_VARIABLE, offset, value); \
    else stream_none(offset, value); \
}

#include "compute_particle_type_generate_function_stream_item.hlsli"

#undef TYPE1_GENERATE_FUNCTIONS
#undef TYPE2_GENERATE_FUNCTIONS
#undef TYPE3_GENERATE_FUNCTIONS
#undef TYPE4_GENERATE_FUNCTIONS
#undef UAV_GLOBAL_VARIABLE
#undef SRV_GLOBAL_VARIABLE

#if defined(SRV)
#undef SRV
#endif // defined(SRV)

#if defined(UAV)
#undef UAV
#endif // defined(UAV)

#if defined(PREFIX)
#undef PREFIX
#endif // defined(PREFIX)
