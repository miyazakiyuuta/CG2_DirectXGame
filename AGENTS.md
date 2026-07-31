# AGENTS.md

このファイルは Codex 用のプロジェクトメモ。`.gitignore` でリポジトリから除外している（コミット/プッシュ/GitHub参照の対象外）。

## 学習目的とルール（最優先）
このリポジトリは専門学校の学習・就職ポートフォリオを兼ねる。
目的: DirectX12の理解度を深め、コード・設計を自分の言葉で説明できる状態にし、
就職後の実践的な開発能力向上を目指す。

**解説を求められた場合:**
- 複数ファイル・クラスにまたがる設計の質問は、関連ファイルを横断的に参照し
  依存関係も含めて解説する（ファイル名・該当行を明示する）
- 一般的なDirectX12の設計パターンを先に説明してから、実際のコードがどう
  対応するか照合する形で解説する
- 専門用語は初出時に一言定義を添える

**実装について:**
「実装して」「修正して」「直して」等の直接的な指示がない限り、ファイルの
編集・作成は行わない。質問・相談だけの会話も多いため、解説依頼を実装依頼と
混同しない。編集する場合も、着手前に方針を1〜2行で提示し、承認を得てから行う。
判断に迷う場合（解説なのか実装依頼なのか曖昧な場合）は、実装せず先に確認する。

## プロジェクト概要
自作の DirectX 12 ゲームエンジン（専門学校CG課程ベース）。`project/` 配下が本体、`project/externals/` に DirectXTex / assimp / imgui、`generated/outputs/<Configuration>/` がビルド成果物。

## ビルド / 実行
- ソリューション: `project/CG2_DirectXGame.sln`
- MSBuild 例:
  ```
  & "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" \
    project\CG2_DirectXGame.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
  ```
- 構成は **Debug / Development / Release**（すべて x64）。
  - **Debug のみ `USE_IMGUI` 定義**。ImGui のUIを確認・操作したいときは Debug を使う。
  - Debug は `imgui.lib` / `DirectXTex.lib` を `generated/outputs/Debug/` から、assimp を `externals/assimp/lib/` からリンク。
- 実行時の作業ディレクトリはプロジェクト直下。シェーダ/テクスチャは `project/resources/...` から相対で読まれる。

## フレームループ / 時間
- deltaTime は実測値（上限0.1秒でクランプ）を `Framework::Run` → `Update(float)` → `SceneManager::Update(float)` → `BaseScene::Update(float)` / `ITransition::Update(float)` へ配布する。速度系の値は「毎フレーム量」ではなく「毎秒量 × dt」で書く。
- FPS制御は `Present(1,0)` の vsync のみ（固定60fpsのビジーウェイトは廃止済み。60Hz以外のモニタではフレームレートが変わるが、実dtなのでゲーム速度は不変）。
- スキニングは `Object3dCommon::DispatchSkinningAll()` がシーン描画前に全オブジェクト分を一括Dispatchし、UAV→頂点バッファの遷移バリアまで張る。`Object3d::Draw` ではDispatchしない。
- SkinCluster は共有部（`Model::SkinClusterShared`: 入力頂点SRV・インフルエンス・バインドポーズ）と個体部（`Model::SkinClusterInstance`: パレット・スキン済み頂点。`Object3d` が所有）に分離済み。同一スキンモデルの複数体表示に対応。
- マルチメッシュのモデルは**未対応**（先頭メッシュのみ使用し警告ログを出す）。
- 終了時は `Framework::Finalize` が全シングルトンを逆順で `Finalize`（delete）する。新しいシングルトンを足したらここに解放を追加すること。

## シェーダの扱い（重要）
- HLSL は **ビルド時にコンパイルしない**。`.vcxproj` では `<FxCompile>` だが全構成 `ExcludedFromBuild=true`。
- 実行時に DXC で動的コンパイルする（`DirectXCommon::CompileShader(L"resources/shaders/Xxx.PS.hlsl", L"ps_6_0")`）。
- そのため**ビルドが通ってもHLSLエラーは検出されない**。シェーダ変更時は DXC で事前検証する:
  ```
  & "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe" \
    -T ps_6_0 -E main -I project\resources\shaders project\resources\shaders\Xxx.PS.hlsl -Fo $env:TEMP\out.cso
  ```
  （パスはWindows SDKのバージョン固定。SDK更新でdxcが見つからなくなったらバージョン部分を直すこと）

## CPUパーティクル・アーキテクチャ
「汎用システム + パラメータ(`ParticleConfig`)駆動」方式。エフェクトの違いはコードではなくデータで表現する（1エフェクト=1クラスにはしない）。

- `engine/effect/ParticleManager.*`: シングルトン。グループ（テクスチャ+ブレンド単位のインスタンシングバッチ、上限1024/グループ）の資源・シミュレーション・描画を担当。**Initialize/Update/Draw/Finalize は `Framework` が呼ぶ**ので、シーンは `SetCamera(camera)` を渡すだけ。
- `engine/effect/ParticleEmitter`: 利用者向けファサード。`ParticleEmitter(groupName, texturePath, config, blendMode)` を生成するだけでグループ作成+Manager への自動登録が済み、`isActive` の間 `emitInterval`(秒)ごとに自動発生する（更新は Manager が実デルタタイムで行う。シーンから Update を呼ぶ必要なし）。単発は `EmitAt(pos, count)`。コピー不可（自動登録のため）。
- `engine/effect/ParticleConfig.h`: min/max 乱数範囲 + `acceleration`（重力）+ `startColor/endColor` + `endScaleRatio` による寿命補間。フェードアウトは `endColor.w=0`（既定）で表現。`BlendMode` enum もここ。
- プリセット方式: `ParticleManager::RegisterEffect(name, texture, config, blend)` 登録後は `ParticleManager::GetInstance()->Emit(name, pos, count)` の1行で発生可能。
- ImGui: `Game::DrawUI` の "Particle" ウィンドウでグループ統計・config・エミッタをライブ編集（Debug 構成のみ）。

## GPUパーティクル・アーキテクチャ
CPU側と同じ「Manager + Emitterファサード + config駆動」の外形で、内部はGPU完結（固定長バッファ+フリーリスト+CS駆動）。用途の棲み分けは **CPU=ゲームイベント単発（EmitAt）、GPU=大量常時放出（雨・塵・噴射など）**。

- `engine/effect/GPUParticleManager.*`: シングルトン。RootSignature/PSO（Init/Emit/Update CS + Add描画）は全グループ共有。**Initialize/Update/DispatchAll/Draw/Finalize は `Framework` が呼ぶ**。Update で射出タイミング判定とCBV書き込み、DispatchAll（シーン描画前・スキニングと同じ位置）で全グループの Emit/Update CS を実行し UAV→NON_PIXEL_SHADER_RESOURCE 遷移まで張る。バッファは ExecuteCommandLists 完了時に COMMON へ戻り Dispatch で UAV へ暗黙昇格するため、フレーム先頭の遷移バリアは不要。
- `engine/effect/GPUParticleEmitter`: ファサード。生成だけでグループ作成+自動登録。position/radius/emitCount/frequency/config を持ち、Manager が毎フレーム CBV に書き込む。**1グループ=1エミッタ**。単発Emit APIは未実装（継続放出のみ）。
- `engine/effect/GPUParticleConfig.h`: GPU専用config（CPU側 `ParticleConfig` とは別物。回転なし・ブレンドAdd固定）。CBVへは `GPUParticleConfigForGPU`（16バイト境界パディング済み）に詰め替えて渡す。
- `engine/effect/GPUParticle.h`（旧 Particle.h から改名）: GPU転送構造体の定義（`ParticleCS`/`PerView`/`EmitterSphere` 等）。CPUパーティクルとは無関係。
- 上限は **1グループ102,400粒**（`GPUParticleManager::kMaxParticles` と `GPUParticle.hlsli` の `kMaxParticles` を必ず一致させる）。生存管理はフリーリスト（u0:粒子 u1:カウンタ u2:空きスロット表。UAV3つは連続Allocate必須=1つのDescriptorTableで渡すため）。
- シェーダ: `GPUParticle.hlsli`（構造体定義・C++側と1:1）+ `InitializeParticle/EmitParticle/UpdateParticle.CS.hlsl` + `GPUParticle.VS.hlsl`（SV_VertexIDで四角形生成、頂点バッファ無し）。PSはCPU側と共用（`Particle.PS.hlsl`）。乱数はCS内ハッシュ（`RandomGenerator.hlsl`）。
- 描画は常に `DrawInstanced(6, kMaxParticles)`（死亡スロットはscale=0の縮退三角形でピクセルを出さない）。生存数だけ描くのは ExecuteIndirect が必要で将来課題。生存数はGPU側にしかないためImGuiに表示できない。

## ポストエフェクト・アーキテクチャ
「1エフェクト = 1 `IPostEffect` 派生クラス」方式。

- `engine/effect/IPostEffect.h`: `Initialize/Update/Draw/DrawImGui` と `enabled`・`name`。可分フィルタ用に `IsSeparable()`/`RenderFirstPass()`。
- `engine/effect/EffectManager.*`: 2枚の `RenderTarget` をピンポンし、`enabled` なエフェクトを登録順に適用。`FindEffect(name)` 取得 → 具象型へ `static_cast` してパラメータ設定。
- 各エフェクトは自前で RootSignature + PSO を生成。頂点バッファ無しのフルスクリーン三角形（`Fullscreen.VS.hlsl` + 各 `Xxx.PS.hlsl`、`DrawInstanced(3,1,0,0)`）。
- 定数バッファは `DirectXCommon::CreateBufferResource()` + `Map` しっぱなし。**HLSL構造体と16バイト境界を合わせる**（`float3`+`float`で1レジスタ）。
- 2枚目テクスチャを使う例 = `DepthBasedOutline`（t0=シーン色 / t1=2枚目 / b0=CBV / s0,s1）。`Dissolve` も同型（t1=maskノイズ）。
- 登録は `engine/base/Framework.cpp` の `Framework::Initialize` で `effectManager_->AddEffect(...)` を集中。RTV/DSVは `DirectXCommon::AllocateRtvIndex()/AllocateDsvIndex()` の自動採番（固定は 0/1=スワップチェインRTV と DSV0=メイン深度のみ）。
- シーンRTは専用深度バッファを持つ（`RenderTarget::CreateDepthBuffer`）。`DepthBasedOutline` はこのシーン深度を `SetDepthResource` で受け取ってサンプルする（ウィンドウサイズの深度ではない）。
- シーン入場時に `SceneManager` が `EffectManager::ResetAll()` で全effect OFF → 各シーンの `Initialize` が必要分だけ `enabled=true`。**新規effectは既定OFF**になる。
- ファイルテクスチャは `TextureManager::GetInstance()->LoadTexture(path)` → `GetSrvIndex(path)`（共有SRVヒープ）。
- `RenderTarget` のクリア色は黒 `{0,0,0,1}`、`BeginRenderNoDepth` が毎フレームクリア → `discard` した穴はクリア色（黒）になる。

## エディタ / デバッグ機能
- Debug（`USE_IMGUI`）のみエディタ風UI: `Game::DrawUI` が DockSpace + "Scene" ウィンドウを作り、シーンRTを `ImGui::Image` でアスペクト比維持表示（Development/Release はフルスクリーン直描き）。
- スクリーン座標が絡む機能（ギズモ・マウスピッキング等）は画面全体ではなく `Input::GetSceneImagePos()/GetSceneImageSize()` のSceneイメージ矩形を基準にする。Scene上のオーバーレイは同名 `ImGui::Begin("Scene")` で追記し、そのウィンドウのDrawListへ描く（ForegroundDrawList は ImGuizmo のホバー判定が効かず操作不能になるため不可）。
- `engine/scene/EditorObject.h`: Hierarchy/Inspector/ギズモ/シーン保存読込が共有する「シーン内オブジェクト」1件。name/transform はシリアライズが全構成で使うため USE_IMGUI 非依存、`drawInspector`（型別の追加UI）は Debug の Inspector からのみ呼ばれる。
- `engine/debug/EditorPanels.*`（Hierarchy+Inspector）/ `engine/debug/TransformGizmo.*`（ImGuizmoギズモ・クリック選択）: `Begin/End` は呼び出し側シーンが行う。利用例は `GamePlayScene::DrawImGui`。
- `engine/scene/SceneSerializer.*`: name をキーに Transform を JSON へ保存/復元（ファイルが無ければ何もしない=初回起動の正常系）。シーンが `BuildSerializeEntries()` で対象を列挙する。全構成で動く（Debug で編集した配置を Release でも読む）。

## コーディング規約
- 既存コードの作法・命名・アーキテクチャに必ず合わせる（独自流儀を持ち込まない）。
- 参考資料のコードはそのまま貼らず、このエンジンに最適化して書き直す。
- 機能追加は「既存の類似実装を読む → 計画提示 → 承認 → 実装 → ビルド確認」の順。
- 設計判断は「なぜそうするか」を説明する（学習目的）。
- C++20 / `/utf-8`。

## 新しいポストエフェクトを足す手順
1. `engine/effect/Xxx.h` / `Xxx.cpp` を `DepthBasedOutline` か `Vignette` を雛形に作成。
2. `resources/shaders/Xxx.PS.hlsl` を作成（`#include "Fullscreen.hlsli"`）。
3. `Framework.cpp` に include と `AddEffect(std::make_unique<Xxx>())` を追加。
4. `CG2_DirectXGame.vcxproj`（ClCompile/ClInclude/FxCompile）と `.vcxproj.filters` に登録。
5. ビルド（Debug|x64）＋ DXC でシェーダ検証。
