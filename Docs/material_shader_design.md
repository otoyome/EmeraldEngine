# マテリアルとシェーダーの再設計

## 1. 背景

### 現状の構成

Toon を例にすると、1 つのシェーディングを成立させるために次のファイルと登録が必要になっている。

| 種類 | ファイル | 中身 |
|---|---|---|
| 宣言 | `toon_rendering.hlsli` | 頂点要件(`HAS_TANGENT` / `TEXCOORD_MAX` / `BONE_MAX` / `HAS_MORPH`)、テクスチャ、定数バッファ |
| 面情報 | `toon_surface.hlsli` | `SurfaceData surface(VS_OUT, bool)` |
| 面情報(α) | `toon_surface_alpha.hlsli` | `float surface(VS_OUT, bool)`(深度/影用) |
| パスのラッパー | `toon_{depth,csm,gbuffer,forward}_{vs,gs,ps}.hlsl` | 上記と `model_vs.hlsli` / `surface_*_ps.hlsli` を include するだけ |
| 登録 | `load_shader_scene.cpp` | パスごとに `generate_pipeline_state` + シェーダー読込(1 件約 50 行) |

Gltf / Pmx / Ggst / EditModel も同じ構成で、`load_shader_scene` の `generate_pipeline_state` は 38 件ある。
マテリアルは `pipeline_states[index]` にパスごとの PSO 名を個別に持つ。

### 問題

- マテリアル固有の中身は「宣言」と「面情報」だけなのに、パスの数だけラッパーと登録を手で書いている。
- パスの知識(どのパスがあり、どの VS_OUT を使うか)がマテリアル側に散らばっている。パスを足すと全シェーディングを直すことになる。
- マテリアルが PSO 名をパスごとに個別設定するため、組み合わせの整合性(gbuffer は toon、shadow は gltf のような誤設定)を誰も保証していない。

構造はすでに「マテリアルの関数 × エンジンのパステンプレート」になっているので、これを**人が手で展開している部分を自動化する**のが本設計の主旨。

## 2. 全体像

```
[マテリアルシェーダー]  toon.hlsl            … 宣言 + surface() 等(人が書く。1 シェーディング 1 ファイル)
        ×
[パステンプレート]      surface_pass.hlsli   … パスごとの main(エンジンが持つ。マテリアルの末尾で include)
        ×
[静的パラメーター]      ブレンドモード / シェーディングモデル / 静的スイッチ
        ↓ 自動生成・キャッシュ
[シェーダーマップ]      material_shader_map  … パーミュテーションキー → PSO + パラメーター構成
        ↑ 参照
[マテリアルリソース]    *.ema                … シェーダーパス + 静的パラメーター + 値(アセット)
        ↑ 必要な分だけ複製
[マテリアルインスタンス]                      … 値の上書き(実行時)
```

- 前回案の「パイプラインステートセット」は人が作るアセットにせず、シェーダーマップとしてエンジン内部で生成する。
- マテリアルリソースが UE の Material、マテリアルインスタンスが UE の Material Instance Dynamic に相当する。

## 3. マテリアルシェーダー

1 シェーディング 1 ファイル(共通処理の include は別)。今の `*_rendering.hlsli` + `*_surface.hlsli` + `*_surface_alpha.hlsli` を 1 つにまとめた形。

```hlsl
//  Shaders/Materials/toon.hlsl

//  頂点要件
#define HAS_TANGENT
#define VERTEX_COLOR_MAX    1
#define TEXCOORD_MAX        2
#define BONE_MAX            8
#define HAS_MORPH
#include "model.hlsli"
#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ・定数バッファ(今の REGISTER_SHADER_RESOURCE_VIEW / cbuffer のまま)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)
...

//  面情報(gbuffer / forward)
SurfaceData surface(VS_OUT pin, bool is_front_face) { ... }

//  不透明度(depth / shadow。mask 時のみ使用)
float surface_opacity(VS_OUT pin, bool is_front_face) { ... }

//  任意のフック(後述)
#define MATERIAL_GEOMETRY_MAX_VERTEX_COUNT 6
void material_geometry(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output) { ... }

//  パスごとの main(末尾に必ず書く)
#include "surface_pass.hlsli"
```

- マテリアルシェーダー自体がコンパイルの入口になる(昔の fx ファイルに近い形)。`material_shader_map` がパス・ステージ・`VS_OUT` などを define で渡してコンパイルする。
- `VS_OUT` は define で渡されるので、マテリアル側は今と同じく `VS_OUT` に対して書く。
- `VS_OUT` の中身はパスごとに違う(深度パスには `current_clip_position` などが無い)。FXC は呼ばれない関数も型検査するので、`surface()` / `surface_opacity()` / `material_geometry()` は `MATERIAL_SURFACE` / `MATERIAL_SURFACE_OPACITY` / `MATERIAL_GEOMETRY` で囲む必要がある。
- `surface_alpha.hlsli` の `surface()` は名前が衝突していたので `surface_opacity()` に改名して同居させる。
- 置き場所は `Shaders/Materials/`。既存の `Shaders/Toon/` などは移行完了後に削除する。

## 4. パステンプレート

エンジンが持つ main。1 ドメイン 1 ファイルで、マテリアルシェーダーの末尾で include される。

```hlsl
//  Shaders/Passes/surface_pass.hlsli
//  SURFACE_STAGE_xxx × SURFACE_PASS_xxx で model_*_vs.hlsli / GS / surface_*_ps.hlsli を取り込む
```

`material_shader_map` が渡す define(`surface_pass_templates` の表で管理):

| ステージ | 渡す define |
|---|---|
| 全ステージ | `SURFACE_PASS_xxx`、`SURFACE_STAGE_xxx`、forward のみ `FORWARD_RENDERING` |
| VS | なし(`VS_OUT` は `model_*_vs.hlsli` 側で決まる) |
| GS(フック時) | `MATERIAL_GEOMETRY`、`VS_OUT` |
| PS | `MATERIAL_SURFACE` または `MATERIAL_SURFACE_OPACITY`、`VS_OUT` |

- define はハッシュキーに含まれるので、.cso はパス・ステージ・マテリアルごとに別になる。マテリアルシェーダーが入口なので、.meta とホットリロードも通常のシェーダーと同じ仕組みで動く。
- 標準のプリプロセッサだけで成り立つので、FXC / DXC のどちらでも独自の include 処理は要らない。
- パスを足すときは、`surface_pass.hlsli` の分岐と `surface_pass_templates` に 1 行足す。

#### 経緯

- 当初はパステンプレートを入口にし、`material_shader.hlsli` という仮想インクルードを include 処理(`fxc_include_handler`)でマテリアルシェーダーに差し替えていた。独自の仕組みで DXC 側にも同じ対応が要るため廃止した。
- `#include MATERIAL_SHADER_FILE` のようにマクロで include 先を渡す方式は、DXC では動くが FXC が対応していない(`error X1500`)ため使えない。
- 入口の変更前後で、fxc のアセンブリは 12 種類(4 パス × VS/GS/PS)すべて最適化あり・なしとも完全一致。

### surface ドメインのパス

| パス | VS | GS | PS | 使う関数 |
|---|---|---|---|---|
| depth_write | `model_depth_vs` | フック時のみ | `surface_depth_write_ps`(mask 時のみ) | `surface_opacity` |
| shadow_write | `model_cascade_shadow_map_vs`(インスタンシングでカスケード展開) | フック時のみ | `surface_depth_write_ps`(mask 時のみ) | `surface_opacity` |
| g_buffer | `model_vs` | フック時のみ | `surface_gbuffer_ps` | `surface` |
| forward | `model_vs`(`FORWARD_RENDERING`) | フック時のみ | `surface_forward_ps` | `surface` |

どのブレンドモードでどのパスを生成するかは、現状のレンダラーのパス選択に合わせる(opaque / mask は depth・shadow・gbuffer、translucent / addative は forward が基本)。

### ジオメトリシェーダーのフック

- Toon のアウトラインは depth / gbuffer / csm でほぼ同じ処理を別ファイルで持っている。
- `material_geometry()` が定義されているパスだけ、パステンプレートの GS が `material_geometry()` を呼ぶ(パスごとのプリプロセスで自動判定。10章の段階3を参照)。マテリアル側は `#if defined(MATERIAL_GEOMETRY)` の中に `MATERIAL_GEOMETRY_MAX_VERTEX_COUNT` と関数を書く。
- csm は元々エンジンの GS(カスケード展開)がある。Toon は影に輪郭線を出していないので段階1では合成せず、shadow_write へのフック指定はエラーにしている。必要になったらテンプレート側で「カスケード展開 + フック」を合成する。
- 段階1(Toon)で、最適化ありのコンパイル結果が旧ラッパーと完全一致することを確認済み。最適化なし(Debug)では GS だけ関数呼び出しの分だけ命令列が異なる(動作は同じ)。
- 段階2(Gltf / Pmx / Ggst / EditModel / 3d_failed)も、最適化ありで VS / PS / GS が旧ラッパーと一致。例外は Pmx の GS で、早期 `return` が関数内に入ったため分岐の向きだけ変わる(命令は同じ)。最適化なしは段階1と同じく GS だけ異なる。
- EditModel の GS は輪郭線の追加ではなく面の置き換え(`MATERIAL_GEOMETRY_MAX_VERTEX_COUNT` = 3)。
- カスケード展開は GS をやめ、インスタンシングに置き換えた(上の csm の記述は経緯)。これで shadow_write でもフックを使える。
  - 影の描画ではインスタンス数を「メッシュレット数 × `shadow_map_count`」にし、VS が `meshlet_decode_instance()` でレコード番号とカスケード番号に分ける。`shadow_map_count` は定数バッファにあるので描画ごとの定数は増えない。
  - VS が光源の行列で変換・深度補正し、`SV_RenderTargetArrayIndex` を出力する。D3D11.3 の任意機能 `VPAndRTArrayIndexFromAnyShaderFeedingRasterizer` が必要で、起動時に調べ、非対応ならエラーで終了する。
  - VS の先頭で、カスケードの視錐台(左右上下の4平面)とメッシュレットの境界球を比べ、外れていれば頂点を読む前に縮退させる。境界球はワールド座標で、CPU のカリングと同じ計算でレコードに入れる(カリングしないメッシュは `no_cull`)。
  - GS が行っていた微小ポリゴンのカリングは無くなった。
  - 影のパスでフックを使うときは、`VS_OUT` の `render_target_array_index` を書き換えずに出力し、クリップ座標は `view_projection_transform` ではなく `light_view_projection_transform[render_target_array_index]` で求める必要がある。Toon / Ggst の輪郭線は `w_tangent` を使うので、今のままでは影のパスに使えない。
  - `SV_InstanceID` には `StartInstanceLocation` が足されないので、ずらして描く場合はオフセットを定数で渡す必要がある。

## 5. 静的パラメーターとパーミュテーション

パーミュテーションキー:

```
{ マテリアルシェーダーのパス, ドメイン, パス, ブレンドモード, シェーディングモデル, 静的スイッチ群 }
```

- テンプレートには `PASS_xxx` / `BLEND_MODE_xxx` / `SHADING_MODEL_xxx` / 静的スイッチを define として渡す。
- 頂点要件は、接線・カラー・UV を常に読み、ボーン数とモーフはメッシュから決める(10章の段階3を参照)。ボーン数とモーフはキーに入る。
- 静的スイッチは「法線マップあり/なし」のようにシェーダー分岐を消したいものに限る。値で済むものは定数バッファに置く。
- PSO の状態(ブレンド・カリング・深度)は静的パラメーターから決まるので、インスタンスでは変更不可。

## 6. シェーダーマップ(`material_shader_map`)

- キー → 「パスごとの PSO 群 + パラメーター構成」を持つキャッシュ。必要になったキーだけ生成する。
- パラメーター構成は、今の `material::build_parameters()` がマテリアルごとに行っている「全パスのリフレクションを名前でマージする処理」を、キーごとに 1 回だけ行ったもの。
- コンパイルは既存の `shader_manager` + .cso キャッシュ(ハッシュ + インクルード時刻)をそのまま使う。
- 非同期読込では、コンパイルを `resource_manager` のワーカーで行う(`D3DCompile` / `Create*Shader` はスレッドセーフ)。PSO の登録はメインスレッドの `build()`。
- 移行期間は、生成した PSO を今と同じ名前(`toon_gbuffer` など)で `pipeline_state_manager` にも登録し、既存マテリアルを壊さない。

## 7. マテリアルリソース(`.ema`)

| 項目 | 内容 |
|---|---|
| シェーダー | マテリアルシェーダーのパス |
| 静的パラメーター | ドメイン、ブレンドモード、シェーディングモデル、静的スイッチ、両面などの PSO 状態 |
| 値 | 定数バッファの値(名前で保持)、テクスチャのパス |
| その他 | アルファクリップ閾値、フラグ、物理マテリアル |

- `build()` でシェーダーマップからエントリを引き、パラメーター構成に値を名前で当てはめる(シェーダー変更・ホットリロード時も名前で引き継ぐ。今の `temp_constant_buffers` の復元と同じ考え方)。
- 当面は `.emo` 埋め込みのまま型だけ分け、外部ファイル化は後段で行う(cereal のバージョンを上げ、埋め込みはフォールバックとして残す)。
- `pipeline_states[index]` に PSO 名を個別に持つ形は廃止する。

## 8. マテリアルインスタンス

- `model_instance` はスロットごとに `material_resource_ptr` を持つだけにし、インスタンスはゲーム側が書き換えを要求したときに初めて作る。
- インスタンスが持つのは上書きした定数バッファ(バッファ単位でコピー)と上書きしたテクスチャだけ。上書きの無いインスタンスはリソース側の定数バッファを共有する。
- 静的パラメーター(シェーダー・ブレンドモード等)は変更不可。変えたい場合は別のマテリアルリソースを使う。
- 描画側は snapshot / proxy にコピーした値を読む。ゲームスレッドが書き換えるインスタンス本体を描画中に直接触らない。
- 変更の伝播(ホットリロード → シェーダーマップ → リソース → インスタンス)は、`this` をキャプチャするコールバックではなくバージョン番号の比較で行う。

## 9. 他ドメイン

| ドメイン | 扱い |
|---|---|
| post_process | 同じ仕組み。パスは 1 つ、VS は `fullscreen_quad_vs` 固定。マテリアルは `float4 post_process(VS_OUT pin)` を定義する |
| user_interface | 同じ仕組み。パスは 1 つ、VS は `sprite_vs` 固定。マテリアルは `float4 user_interface(VS_OUT pin)` を定義する |
| deferred_decal | 同じ仕組み。パスは primitive / fullscreen。マテリアルは `DecalData decal_ps(DecalIn din)` を定義する |
| particle | 同じ仕組み。パスは `particle_passes`(depth_write / g_buffer / forward の 3 つ。マテリアル側も同じ並びの `material::particle_paths` を持ち、描画側は `particle_render(..., material::particle_paths::xxx)` で取る)、VS は `compute_particle_renderer_vs`。マテリアルは `surface()` / `surface_opacity()` を定義する(`VS_OUT` = `PS_IN`)。影は描かない(スプライト・トレイルの影は不要と判断し、カスケード展開の GS と `compute_particle_*csm*.hlsli` を削除した) |
| system / compute | 対象外。今どおり手動登録(遅延ライティングのマテリアルは PSO 名を持つ) |

- 実施済み。パステンプレートは `Shaders/Passes/<ドメイン>_pass.hlsli`、マテリアルシェーダーは surface が `Shaders/Materials` 直下、それ以外は `Shaders/Materials/<Particle|DeferredDecal|PostProcess|UserInterface>/`。編集窓のシェーダーの候補はドメインのフォルダから出す。
- `material_shader_map` はドメインごとの設定(パステンプレート・先に include するヘッダー・ステージの define・パスの一覧)を持つ。1 つのマテリアルシェーダーは 1 つのドメインでしか使えない(最初に要求されたドメインで固定)。
- 既定のマテリアルシェーダーは `material::default_particle_shader_path` / `default_decal_shader_path` / `default_user_interface_shader_path`。エラー表示用はドメインごとに用意した(particle は既定と同じ)。
- 旧ラッパー(`compute_particle_default_*.hlsl`、`default_decal_*_ps.hlsl`、`failed_decal_*_ps.hlsl`、`sprite_ps.hlsl`)と `load_shader_scene` の PSO 登録は削除した。ポストプロセスのカラーフィルター(`Shaders/Postprocess/color_filter_*_ps.hlsl`)と `postprocess_failed_ps.hlsl` は `Shaders/Materials/PostProcess/` に移し、`main` を `post_process` に改名した。最適化ありのコンパイル結果は旧ラッパーと命令数が一致することを確認済み。
- 旧コードでは複数のシーンで同じ PSO 名(`before_tonemapping_color_filter` など)を 2 回登録していて、後のマテリアルは先のシェーダーで描かれていた。マテリアルシェーダーにしたので、それぞれ自分のシェーダーで描かれる。

## 10. 移行計画

1. **パステンプレート + シェーダーマップ**:Toon だけを新方式に移す。生成 PSO を既存と同じ名前で登録し、見た目が変わらないことを autotest で確認する。GS フックもここで検証する。
2. **他シェーディングの移行**:Gltf / Pmx / Ggst / EditModel / Editor の failed 系を移し、ラッパーと `load_shader_scene` の surface 登録を削除する。
   - 実施済み。`surface_depth_write_ps.hlsli` は `surface_opacity()` を呼ぶ形にし、テンプレートの橋渡し(`#define surface surface_opacity`)を削除した。パーティクルの α 関数も `surface_opacity()` に改名。
   - PSO 名は `<名前>_depth` / `_shadowmap` / `_gbuffer` / `_forward` にそろうため、頂点カラー編集用は `edit_model_color_depth_write` / `edit_model_color_csm` から `edit_model_color_depth` / `edit_model_color_shadowmap` に変わった(参照はコードのみ)。
   - 3d_failed の shadow_write は、旧登録ではカスケード展開の GS を読み込んでいなかった。テンプレートでは常に GS が付く。
3. **マテリアルが PSO 名ではなくシェーダーパス + 静的パラメーターを持つ**:シリアライズのバージョンを上げ、変換処理(`data_converter_gltf` / `data_converter_mmd`)と編集窓を更新する。
   - 全ドメイン(system を除く)で実施済み(9章を参照)。
   - 入口はエンジンがメモリ上で生成する(`load_shader_file::source_text`)。マテリアルシェーダーはテンプレートを include しない。
   - 機能宣言の行(`//@material`)は廃止した。必要な情報はマテリアルとメッシュ、マテリアルシェーダーの中身から決める。
     - ドメイン:マテリアルの `domain`(system 以外はすべてマテリアルシェーダーで描く)。
     - 頂点要件:接線・頂点カラー1つ・UV 2つは常に読む(メッシュに無い要素はシェーダーの `meshlet_has_element()` で 0 になる)。ボーン数(`JOINTS` の組数 × 4)とモーフの有無はメッシュから決める。`model_resource::build_model()` が、マテリアルを使うメッシュを合わせた構成を `material::set_vertex_format()` で渡す。どのメッシュにも使われていないマテリアルは全部入り(ボーン16・モーフあり)。
     - GS フック:パスごとにマテリアルシェーダーを GS のステージとして `D3DPreprocess` し(インクルード先は空にする)、`material_geometry(` が残るパスで GS を付ける。使うパスは `#if defined(MATERIAL_GEOMETRY) && (defined(SURFACE_PASS_DEPTH_WRITE) || ...)` のように条件で書く。スイッチの値はこの判定に使わないので、GS の有無をスイッチで変えることはできない。
     - 静的スイッチ:マテリアルシェーダーに書かれた `MATERIAL_SWITCH(名前)`(マテリアルの設定で切り替える、既定 OFF)を書かれている順に集める。入口で `MATERIAL_SWITCH_名前` を 0/1 で define する。`#if` でも式でも使える。
     - テクスチャの有無は静的スイッチにしない(2026-11-02 に `HAS_TEXTURE()` を廃止)。シェーダー内で `HAS_RESOURCE(テクスチャ名)` を使い、テクスチャがあれば使い、なければ使わない(定数バッファのフラグを見る動的分岐。ドローコール単位でそろうので、通らない側のサンプリングは実行されない)。組み合わせごとの PSO が無くなり、テクスチャを差し替えても PSO を作り直さない。代わりに PS は全部ありの静的版より命令数が増える(G-Buffer の PS:glTF 120→139、Toon 105→118、PMX 90→95)。
   - パーミュテーションキーは「シェーダーパス × パス × PS の有無 × スイッチのビット × ボーン数 × モーフの有無」。ブレンドモードとシェーディングモデルは静的パラメーターとして持つが、シェーダーには今までどおり定数で渡す(キーに入れない)。
   - 最適化:深度・影の PS はマスクのときだけ付ける。VS はマテリアルを含めずに生成し、頂点の構成(ボーン数・モーフ)が同じマテリアル間で共有する。描画で使うパス(不透明・マスク=深度/G-Buffer、半透明・加算=フォワード、影=shadow_caster)だけ生成する。パラメーター構成は全スイッチ ON の G-Buffer / 深度から取る。
   - `asset.resave_models` / `asset.resave_effects` コマンドで Assets 以下の .emo / .eme を最新の形式で保存し直せる。
   - バージョン3:PSO 名は保存しない(system ドメインのマテリアルはコードで作り、ファイルには保存しない)。旧形式を読まなくなったので、シリアライズはバージョンによる分岐の無い 1 つのテンプレートにし、Binary の入出力で明示的にインスタンス化している。PSO 名で設定する API(`set_pipeline_state(名前)` など)と `get_pipeline_state_name()`、`uses_material_shader()` は削除した。名前で設定できるのは `set_pipeline_state(system_paths, 名前)` だけ。
   - 2026-11-02 に Assets 以下の .emo 11 個と .eme 14 個をバージョン3で保存し直し、旧形式(PSO 名の接頭辞からシェーダーパスへ)の変換処理を削除した。旧形式のファイルは、読めるがシェーダーパスが空になる(エラー表示のシェーダーで描かれる)。読み込めなかった `BossDestroy.eme`(エフェクト本体がバージョン2)は削除した。
   - 深度テスト無効(`flags::disable_depth_test`)を追加。半透明・加算のときだけ効く。
4. **マテリアルインスタンスと非同期ローダー**:書き換え時複製、バージョン番号での伝播、`resource::loader<material_resource>`、コンパイルのワーカー化。

## 11. 未決事項

- 2026-11-02 時点で、この章の事項は保留としている。
- D3D11 の間は FXC のみ使う(DXC の出力 DXIL は D3D11 で使えない)。D3D12 / Vulkan 移行時に DXC でのコンパイルを確認する。既存の `common.hlsli` / `gbuffer.hlsli` には、HLSL 2021 で禁止されたベクトル条件の三項演算子がある。
- マテリアルの関数を囲むガード(`MATERIAL_SURFACE` など)をなくすには、パス間で `VS_OUT` の型をそろえる必要がある。
- ホットリロードでマテリアルシェーダーのスイッチや GS の有無が変わると、シェーダーマップのキャッシュは捨てるが、マテリアルが持っているパイプラインステートは差し替わらない(機能宣言の行があったころから同じ)。
- ブレンドモードごとに生成するパスの一覧(現状のレンダラーの選択と突き合わせる)。
- パーミュテーション数の上限と、リリースビルドで事前コンパイルする一覧の作り方。
