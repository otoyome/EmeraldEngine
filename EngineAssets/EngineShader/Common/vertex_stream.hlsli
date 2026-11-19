//	モデル以外の頂点を SV_VertexID で読み込むための頂点ストリーム
//	C++ 側は graphics_context::set_vertex_streams で VS の t96 から設定する
//	中身の並びは使う側の hlsli で決める
#ifndef __VERTEX_STREAM_HLSLI__
#define __VERTEX_STREAM_HLSLI__

ByteAddressBuffer vertex_stream0 : register(t96);
ByteAddressBuffer vertex_stream1 : register(t97);
ByteAddressBuffer vertex_stream2 : register(t98);

#endif  //  __VERTEX_STREAM_HLSLI__
