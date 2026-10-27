//	miniaudio の実装を展開するファイル
//	OGG(Vorbis) の読込に stb_vorbis を使う
#define	STB_VORBIS_HEADER_ONLY
#include "miniaudio/stb_vorbis.c"

#define	MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#undef	STB_VORBIS_HEADER_ONLY
#include "miniaudio/stb_vorbis.c"
