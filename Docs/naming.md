# 描画層の命名規則

1つの意味には1つの語だけを使い、1つの語には1つの意味だけを持たせる。
描画層（Engine の描画まわりと TowerDefenceGame/Renderer）で型名に付ける接尾辞は、下の表の物だけにする。
どの行にも当てはまらない型を作るときは、先にこの表に行を足すかどうかを決める。

## 接尾辞

| 接尾辞 | 意味（判定） | 例 |
|---|---|---|
| `_resource` | 共有されて長く残るアセット。中の要素は接尾辞なしの名詞にする | `model_resource`、`mesh`、`sub_mesh`、`meshlet`、`pooled_meshlet` |
| `_desc` | 何かを作るときの引数だけ。作り終えたら捨てる | `render_pass_desc`、`texture_sampler_desc` |
| `_render_proxy` | ゲーム側のオブジェクト1つを描画側に写した物。作成後は変更しない | `material_render_proxy`、`model_render_proxy`、`light_render_proxy` |
| `_draw_item` | 抽出のたびに作る、描画リストの1要素 | `mesh_draw_item`、`sprite_draw_item`、`particle_draw_item`、`decal_draw_item` |
| `_container` | 1視点分の、種類ごとの集まり | `mesh_container`、`effect_container` |
| `_packet` | スレッドの間で受け渡す単位 | `frame_packet`、`view_packet` |
| `_constants` | GPU の定数バッファの中身 | `scene_constants` |
| `_settings` | 利用者が変える設定 | `render_settings` |
| `_state` | 前のフレームから持ち越す状態 | `scene_view_state` |
| `_system` | スレッドや常駐する資源を持ち、毎フレームの処理の流れを担う仕組み | `render_system`、`clustered_lighting_system` |

## 描画層で使わない語

- `_data`：意味が広すぎて、何の物か分からない
- `_datas`：data はもともと複数形
- `_info`：`_data` と同じ理由
- `_snapshot`：オブジェクトの写しは `_render_proxy` に統一する

## 判定に迷うとき

- オブジェクトの写しで、そのまま1回の描画にもなる物は、描画パスが描画単位として扱うなら `_draw_item`、それ以外（ライトのように描画以外にも使う物）は `_render_proxy` にする。
- リソースの中身を描画側が直接参照する場合は、写しではないので `_render_proxy` にしない。
- 描画単位の番号の一覧は、`color_items` のように「用途 + `_items`」と呼ぶ。
- 同じ物の別の形（保存される形と、読み込み時に作り直した形など）は、接尾辞を足さずに違いを表す語を前に付ける（`meshlet` と `pooled_meshlet`）。

## 対象外

ゲーム側・エディター側の `_data`（`engine_config_data` など）は今のところ対象外。
定数バッファの中身に `_data`（`bloom_data` など）と `_constants` が混在しているのは、描画層が固まってから見直す。
