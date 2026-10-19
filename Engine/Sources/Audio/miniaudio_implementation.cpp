//	@tool miniaudio の実装を展開するファイル（プリコンパイルヘッダー・強制インクルードは使わない）
//	@tool OGG(Vorbis) の読込に stb_vorbis を使う（miniaudio の推奨手順の通り、ヘッダー部→miniaudio→実装部の順に読む）
#define	STB_VORBIS_HEADER_ONLY
#include "miniaudio/stb_vorbis.c"

#define	MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#undef	STB_VORBIS_HEADER_ONLY
#include "miniaudio/stb_vorbis.c"
