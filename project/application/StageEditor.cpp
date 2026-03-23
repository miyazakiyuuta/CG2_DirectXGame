#define NOMINMAX

#include "StageEditor.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/WinApp.h"
#include "debug/DebugSphere.h"
#include "io/Input.h"
#include "utility/CollisionUtility.h"

/// <summary>
/// コンストラクタ
/// </summary>
StageEditor::StageEditor(Object3dCommon* objCommon, Camera* camera)
    : loader_(objCommon, camera)
    , controller_(camera)
{
    // Object3dCommon と Camera の参照を保存
    object3dCommon_ = objCommon;
    camera_ = camera;

    // デフォルトモデル名を初期化
    strcpy_s(defaultModelBuf_, sizeof(defaultModelBuf_), defaultModel_.c_str());
    strcpy_s(batchModelBuf_, sizeof(batchModelBuf_), defaultModel_.c_str());

    previewMarker_ = std::make_unique<Object3d>();
    // Object3d の初期化は Object3dCommon が必要なので、コンストラクタ内で行う
    if (object3dCommon_) {
        previewMarker_->Initialize(object3dCommon_);
        previewMarker_->SetCamera(camera_);
        previewMarker_->SetScale({ 0.5f, 0.5f, 0.5f });
    }

    // プレイスメントプレビュー用の球体も初期化しておく
    previewSphere_ = std::make_unique<DebugSphere>();
    previewSphere_->Initialize(Object3dCommon::GetInstance()->GetDxCommon());
    selectionLastBlinkTime_ = std::chrono::steady_clock::now();
}

/// <summary>
/// 初期化
/// </summary>
void StageEditor::Initialize(const std::string& defaultModel)
{
    // デフォルトモデル名を設定
    defaultModel_ = defaultModel;

    // ImGui 用固定長バッファも更新
    strcpy_s(defaultModelBuf_, sizeof(defaultModelBuf_), defaultModel_.c_str());
    // ステージデータを初期化
    data_.name = "stage";
    // 描画インスタンス管理クラスも初期化
    SaveHistorySnapshot();

    // プレビューマーカーのモデルを設定
    if (previewMarker_) {
        previewMarker_->SetModel(defaultModel_);
    }
}

/// <summary>
/// 更新処理（入力チェックとオブジェクト生成・削除）
/// </summary>
void StageEditor::Update()
{
#ifdef USE_IMGUI

    // 編集モードであれば、生成キーと削除キーの入力をチェックしてオブジェクトの追加・削除を行う
    if (isEditMode_) {
        // 生成キーが押された場合は、生成基準点に新しいオブジェクトを追加
        if (controller_.IsCreateKeyPressed()) {
            StageObject o;
            // オブジェクトIDは一意である必要があるため、次のIDを割り当ててインクリメント
            o.id = nextId_++;
            o.modelName = defaultModel_;
            o.position = GetCreateOrigin();
            o.blockId = placingBlockId_;
            o.color = placingColor_;

            // データに追加
            data_.objects.push_back(o);
            // 描画インスタンスも追加・更新
            loader_.UpdateOrCreateInstance(o);
            // 生成されたオブジェクトを選択状態にする
            if (selectedObjectId_ == o.id) {
                loader_.SetInstanceColorById(o.id, selectionHighlightColor_);
            }
            // 履歴にスナップショットを保存
            SaveHistorySnapshot();
        }

        // 削除キーが押された場合は、最後に追加されたオブジェクトを削除
        if (controller_.IsDeleteKeyPressed()) {
            if (!data_.objects.empty()) {
                StageObject removed = data_.objects.back();
                data_.objects.pop_back();
                loader_.RemoveInstanceById(removed.id);
                SaveHistorySnapshot();
            }
        }
    }

    // ホットキーによる生成原点移動モードの切り替え
    if (isEditMode_ && useHotkey_) {
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            // ホットキーが設定されている場合は、キーの状態をチェックして移動原点モードを切り替える
            if (toggleKeyBuf_[0] != '\0') {
                int vk = toupper(static_cast<unsigned char>(toggleKeyBuf_[0]));
                bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
                // キーが押された瞬間にモードを切り替える（キーが離されるまで切り替えない）
                if (down && !hotkeyPrevDown_) {
                    moveOriginMode_ = !moveOriginMode_;
                    if (moveOriginMode_) {
                        createReferenceIndex_ = 3;
                    }
                }
                hotkeyPrevDown_ = down;
            }
        }
    }

    // 編集モードで移動原点モードが有効な場合は、マウス位置とホイール入力から生成原点を更新する
    if (isEditMode_ && moveOriginMode_ && camera_) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            POINT pt;
            long wheel = Input::GetInstance()->GetMouseWheel();
            bool skipXZUpdate = false;
            // ホイール入力がある場合はY座標を更新し、XZ平面の更新はスキップする
            if (wheel != 0) {
                createOrigin_.y += static_cast<float>(wheel) * createHeightSensitivity_ * 0.001f;
                skipXZUpdate = true;
            }

            // マウス位置からXZ平面上の交点を計算して生成原点を更新
            if (GetCursorPos(&pt) && !skipXZUpdate) {
                HWND hwnd = WinApp::GetInstance()->GetHwnd();
                POINT clientPt = pt;
                // 画面座標をクライアント座標に変換
                ScreenToClient(hwnd, &clientPt);
                const float w = static_cast<float>(WinApp::kClientWidth);
                const float h = static_cast<float>(WinApp::kClientHeight);
                float px = static_cast<float>(clientPt.x);
                float py = static_cast<float>(clientPt.y);
                float nx = (px / w) * 2.0f - 1.0f;
                float ny = -((py / h) * 2.0f - 1.0f);

                // ビュープロジェクション行列の逆行列を計算
                Matrix4x4 vp = camera_->GetViewProjectionMatrix();
                Matrix4x4 invVP = vp;
                invVP = invVP.Inverse();

                // 画面上の点をワールド空間に変換するために、近クリップ面と遠クリップ面の座標を計算
                Vector3 nearClip = { nx, ny, 0.0f };
                Vector3 farClip = { nx, ny, 1.0f };

                // 逆行列を使ってワールド空間の座標を計算
                Vector3 pNear = invVP.Transform(nearClip);
                Vector3 pFar = invVP.Transform(farClip);

                // 近クリップ面と遠クリップ面を結ぶ線分と、Y=生成原点の高さの平面との交点を計算して生成原点を更新
                Vector3 dir = pFar - pNear;

                // 交点を計算するための平面のY座標は、現在の生成原点のY座標を使用
                float planeY = createOrigin_.y;
                // 線分が平面と平行でない場合に交点を計算
                if (std::abs(dir.y) > 1e-6f) {
                    float t = (planeY - pNear.y) / dir.y;
                    Vector3 intersect = pNear + dir * t;
                    createOrigin_.x = intersect.x;
                    createOrigin_.z = intersect.z;
                    createOrigin_.y = planeY;
                }
            }
        }
    }

    // 選択中のオブジェクトがある場合は、点滅させるためにカラーを更新する
    if (selectedObjectId_ != -1) {
        // 点滅の周期を計算するために、現在の時間と最後の点滅時間の差を秒単位で計算
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - selectionLastBlinkTime_).count();

        // 点滅の周期が経過したら、点滅状態を切り替えて最後の点滅時間を更新
        Vector4 originalColor { 1.0f, 1.0f, 1.0f, 1.0f };
        bool found = false;
        // 点滅の周期が経過したら、点滅状態を切り替えて最後の点滅時間を更新
        for (const auto& obj : data_.objects) {
            if (obj.id == selectedObjectId_) {
                originalColor = obj.color;
                found = true;
                break;
            }
        }

        // 点滅の周期を計算して、選択ハイライトのアルファ値を変化させる
        if (found) {
            float phase = fmodf(elapsed, selectionBlinkInterval_) / selectionBlinkInterval_;
            float t = 0.5f * (1.0f + std::sinf(2.0f * 3.14159265f * phase));
            float lerpedAlpha = originalColor.w * (1.0f - t) + selectionBlinkAlpha_ * t;
            loader_.SetInstanceColorById(selectedObjectId_, { originalColor.x, originalColor.y, originalColor.z, lerpedAlpha });
        }
    }

    // ImGui を使用して編集モードの UI を描画
    DrawImGui();

#endif // !USE_IMGUI
}

/// <summary>
/// 描画処理（StageLoaderの描画呼び出し）
/// </summary>
void StageEditor::Draw()
{
    // StageLoader に描画を任せる
    loader_.DrawAndUpdate();

    // プレイスメントプレビューの描画
    if (isEditMode_ && previewSphere_) {
        std::vector<Vector3> centers;
        // 生成原点のプレビューを描画
        if (showCreatePreview_) {
            centers.push_back(GetCreateOrigin());
            previewSphere_->Draw(centers, previewRadius_, { 0.0f, 1.0f, 0.0f, 1.0f }, *camera_);
        }

        // バッチ生成のプレビューを描画
        if (showBatchPreview_) {
            centers.clear();
            Vector3 dirA, dirB;
            // バッチ生成の法線方向に応じて、配置の基準となる2方向のベクトルを決定する
            if (batchNormalIndex_ == 0) {
                dirA = Vector3 { 0.0f, 0.0f, 1.0f };
                dirB = Vector3 { 0.0f, 1.0f, 0.0f };
            } else if (batchNormalIndex_ == 1) {
                dirA = Vector3 { 1.0f, 0.0f, 0.0f };
                dirB = Vector3 { 0.0f, 0.0f, 1.0f };
            } else {
                dirA = Vector3 { 1.0f, 0.0f, 0.0f };
                dirB = Vector3 { 0.0f, 1.0f, 0.0f };
            }

            // バッチ配置の中心が生成原点になるように、半分のオフセットを計算する
            float halfA = (batchCountA_ - 1) * 0.5f * batchSpacing_;
            float halfB = (batchCountB_ - 1) * 0.5f * batchSpacing_;

            Vector3 origin = GetCreateOrigin();

            // 2重ループでバッチ配置の各位置を計算して、プレイスメントプレビューの中心リストに追加する
            for (int ia = 0; ia < batchCountA_; ++ia) {
                for (int ib = 0; ib < batchCountB_; ++ib) {
                    Vector3 pos = origin + dirA * ((ia * batchSpacing_) - halfA) + dirB * ((ib * batchSpacing_) - halfB);
                    centers.push_back(pos);
                }
            }

            // プレイスメントプレビューの中心リストが空でない場合は、球体を描画する
            if (!centers.empty()) {
                previewSphere_->Draw(centers, previewRadius_, { 0.0f, 0.5f, 1.0f, 1.0f }, *camera_);
            }
        }
    }
}

/// <summary>
/// 編集モードの切り替え
/// </summary>
void StageEditor::ToggleEditMode() { isEditMode_ = !isEditMode_; }

/// <summary>
/// 編集モードかどうかを返す
/// </summary>
bool StageEditor::IsEditMode() const { return isEditMode_; }

/// <summary>
/// JSONファイルにステージデータを保存
/// </summary>
bool StageEditor::Save(const std::string& path)
{
    return StageSerializer::SaveToFile(data_, path);
}

/// <summary>
/// JSONファイルからステージデータを読み込み、オブジェクトを生成
/// </summary>
bool StageEditor::Load(const std::string& path)
{
    // JSONファイルからステージデータを読み込む
    auto d = StageSerializer::LoadFromFile(path);
    // 読み込みに失敗した場合は false を返す
    if (!d) {
        return false;
    }

    // 読み込んだデータを保存して、描画インスタンスを生成する
    data_ = *d;

    // 読み込んだデータのオブジェクトIDを走査して、次に生成するオブジェクトのIDが重複しないように次のIDを設定する
    int maxId = 0;
    // オブジェクトIDの最大値を見つける
    for (auto& o : data_.objects) {
        if (o.id > maxId) {
            maxId = o.id;
        }
    }

    // 次のIDを最大値の次に設定する
    nextId_ = maxId + 1;

    // 描画インスタンスを生成する
    loader_.CreateFromData(data_);
    // 履歴にスナップショットを保存する
    SaveHistorySnapshot();

    return true;
}

/// <summary>
/// ステージデータとオブジェクトをすべてクリア
/// </summary>
void StageEditor::Clear()
{
    data_.objects.clear();
    loader_.Clear();
    nextId_ = 1;
    SaveHistorySnapshot();
}

/// <summary>
/// ステージデータへの参照を返す（外部から直接データを操作したい場合に使用）
/// </summary>
StageData& StageEditor::GetStageData() { return data_; }

/// <summary>
/// ブロック用の AABB 一覧を返す
/// </summary>
std::vector<CollisionUtility::AABB> StageEditor::GetBlockAABBs() const
{
    std::vector<CollisionUtility::AABB> result;
    // 結果のベクターは、ステージ内のオブジェクト数を上限として予約しておく
    result.reserve(data_.objects.size());

    // ステージ内のオブジェクトを走査して、条件を満たすものをブロックとして AABB を計算して結果に追加する
    for (const auto& o : data_.objects) {

        // ブロックとして扱う条件をチェックする
        if (o.modelName != "Cube.obj") {
            continue;
        }
        // ブロックIDが Water, BugSpawn, PlayerSpawn 以外をブロックとして扱う
        if (o.blockId == BlockID::Water || o.blockId == BlockID::BugSpawn || o.blockId == BlockID::PlayerSpawn) {
            continue;
        }

        // 条件を満たすオブジェクトはブロックとして扱うので、AABB を計算して結果に追加する
        CollisionUtility::AABB aabb;
        // AABB の最小点と最大点を計算するために、オブジェクトの位置とスケールを使用する
        Vector3 halfSize = { 1.0f * o.scale.x, 1.0f * o.scale.y, 1.0f * o.scale.z };
        aabb.min = { o.position.x - halfSize.x, o.position.y - halfSize.y, o.position.z - halfSize.z };
        aabb.max = { o.position.x + halfSize.x, o.position.y + halfSize.y, o.position.z + halfSize.z };
        // 計算した AABB を結果に追加する
        result.push_back(aabb);
    }

    return result;
}

/// <summary>
/// 水ブロック用の AABB 一覧を返す
/// </summary>
std::vector<CollisionUtility::AABB> StageEditor::GetWaterBlockAABBs() const
{
    std::vector<CollisionUtility::AABB> result;
    // 結果のベクターは、ステージ内のオブジェクト数を上限として予約しておく
    result.reserve(data_.objects.size());

    // ステージ内のオブジェクトを走査して、条件を満たすものを水ブロックとして AABB を計算して結果に追加する
    for (const auto& o : data_.objects) {
        // 水ブロックとして扱う条件をチェックする
        if (o.modelName != "Cube.obj") {
            continue;
        }

        // ブロックIDが Water のものを水ブロックとして扱う
        if (o.blockId != BlockID::Water) {
            continue;
        }

        // 条件を満たすオブジェクトは水ブロックとして扱うので、AABB を計算して結果に追加する
        CollisionUtility::AABB aabb;
        // AABB の最小点と最大点を計算するために、オブジェクトの位置とスケールを使用する
        Vector3 halfSize = { 1.0f * o.scale.x, 1.0f * o.scale.y, 1.0f * o.scale.z };
        aabb.min = { o.position.x - halfSize.x, o.position.y - halfSize.y, o.position.z - halfSize.z };
        aabb.max = { o.position.x + halfSize.x, o.position.y + halfSize.y, o.position.z + halfSize.z };
        // 計算した AABB を結果に追加する
        result.push_back(aabb);
    }

    return result;
}

/// <summary>
/// 虫スポーン位置一覧を返す
/// </summary>
std::vector<Vector3> StageEditor::GetBugSpawnPositions() const
{
    std::vector<Vector3> result;
    // 結果のベクターは、ステージ内のオブジェクト数を上限として予約しておく
    result.reserve(data_.objects.size());
    // ステージ内のオブジェクトを走査して、条件を満たすものを虫スポーン位置として結果に追加する
    for (const auto& o : data_.objects) {
        if (o.blockId == BlockID::BugSpawn) {
            result.push_back(o.position);
        }
    }

    return result;
}

/// <summary>
/// プレイヤー開始位置を返す（存在しない場合は std::nullopt）
/// </summary>
std::optional<Vector3> StageEditor::GetPlayerSpawnPosition() const
{
    for (const auto& o : data_.objects) {
        if (o.blockId == BlockID::PlayerSpawn) {
            return o.position;
        }
    }

    return std::nullopt;
}

/// <summary>
/// 現在のステージデータのスナップショットを履歴に保存する
/// </summary>
void StageEditor::SaveHistorySnapshot()
{
    if (historyIndex_ + 1 < (int)history_.size()) {
        history_.erase(history_.begin() + historyIndex_ + 1, history_.end());
    }

    Snapshot s;
    s.data = data_;
    s.nextId = nextId_;
    history_.push_back(std::move(s));
    historyIndex_ = (int)history_.size() - 1;
}

/// <summary>
/// 生成基準点を取得する
/// </summary>
Vector3 StageEditor::GetCreateOrigin() const
{
    Vector3 base {};
    switch (createReferenceIndex_) {
    case 0:
        base = controller_.GetCreatePosition();
        break;
    case 1:
        base = Vector3 { 0.0f, 0.0f, 0.0f };
        break;
    case 2:
        if (selectedObjectId_ != -1) {
            for (const auto& o : data_.objects) {
                if (o.id == selectedObjectId_) {
                    base = o.position;
                    break;
                }
            }
        } else {
            base = Vector3 { 0.0f, 0.0f, 0.0f };
        }
        break;
    case 3:
    default:
        base = createOrigin_;
        break;
    }

    return base + createOffset_;
}

/// <summary>
/// 一つ戻る (Undo)
/// </summary>
void StageEditor::Undo()
{
    if (historyIndex_ <= 0)
        return;
    --historyIndex_;
    const auto& s = history_[historyIndex_];
    data_ = s.data;
    nextId_ = s.nextId;
    loader_.CreateFromData(data_);
    selectedObjectId_ = -1;
}

/// <summary>
/// 一つ進む (Redo)
/// </summary>
void StageEditor::Redo()
{
    if (historyIndex_ + 1 >= (int)history_.size()) {
        return;
    }

    ++historyIndex_;
    const auto& s = history_[historyIndex_];
    data_ = s.data;
    nextId_ = s.nextId;
    loader_.CreateFromData(data_);
    selectedObjectId_ = -1;
}

/// <summary>
/// ImGui を使用して編集モードの UI を描画する
/// </summary>
void StageEditor::DrawImGui()
{
#ifdef USE_IMGUI

    ImGui::Begin("StageEditor");
    // ImGui のウィンドウを開始する
    if (ImGui::Button(isEditMode_ ? "Exit Edit Mode" : "Enter Edit Mode")) {
        ToggleEditMode();
    }

    ImGui::SameLine();
    // ードのときは、クリアボタンとライブエディットのオプションも表示する
    if (ImGui::Button("Clear")) {
        Clear();
    }

    ImGui::SameLine();
    // エディットのオプションを表示する
    if (ImGui::Checkbox("Live Edit", &liveEdit_)) {
        // ライブエディットのオプションを表示する
        if (liveEdit_ && selectedObjectId_ != -1) {
            loader_.UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
            for (auto& obj : data_.objects) {
                if (obj.id == selectedObjectId_) {
                    obj.position = editPosition_;
                    obj.rotation = editRotation_;
                    obj.scale = editScale_;
                    break;
                }
            }
        }
    }

    ImGui::Separator();

    // 生成原点の設定セクションを表示する
    ImGui::Text("Create Origin");
    const char* refs[] = { "Camera Forward", "World Origin", "Selected Object", "Custom" };
    ImGui::Combo("Reference", &createReferenceIndex_, refs, IM_ARRAYSIZE(refs));
    ImGui::SameLine();
    if (ImGui::Button("Set To Camera")) {
        createOrigin_ = controller_.GetCreatePosition();
        createReferenceIndex_ = 3;
    }

    ImGui::DragFloat3("Origin Pos", &createOrigin_.x, 0.1f);
    ImGui::DragFloat3("Create Offset", &createOffset_.x, 0.1f);
    ImGui::Checkbox("Show Create Preview", &showCreatePreview_);
    ImGui::SameLine();
    ImGui::Checkbox("Show Batch Preview", &showBatchPreview_);
    ImGui::SameLine();
    // バッチ生成のオプションを表示する
    if (ImGui::Checkbox("Move Origin Mode", &moveOriginMode_)) {
        if (moveOriginMode_)
            createReferenceIndex_ = 3;
    }

    if (moveOriginMode_) {
        ImGui::TextWrapped("Move Origin Mode forces Reference = Custom. Move the mouse to pan the origin; mouse wheel moves depth.");
        ImGui::SliderFloat("Move Sensitivity", &moveSensitivity_, 0.001f, 1.0f);
        ImGui::SliderFloat("Depth Sensitivity", &moveDepthSensitivity_, 0.001f, 1.0f);
    }

    if (ImGui::Checkbox("Enable Hotkey Toggle", &useHotkey_)) { }
    ImGui::SameLine();
    ImGui::InputText("Toggle Key (single char)", toggleKeyBuf_, sizeof(toggleKeyBuf_));

    ImGui::Separator();

    // オブジェクト生成のオプションを表示する
    if (ImGui::InputText("Default Model", defaultModelBuf_, sizeof(defaultModelBuf_))) {
        defaultModel_ = std::string(defaultModelBuf_);
    }

    ImGui::ColorEdit4("Placing Color", &placingColor_.x);

    // プレイスメントオプションを表示する
    const char* blockTypes[] = { "Normal", "Water", "BugSpawn", "PlayerSpawn" };
    int blockTypeIndex = static_cast<int>(placingBlockId_);
    // ブロックタイプの選択肢を表示する
    if (ImGui::Combo("Block Type", &blockTypeIndex, blockTypes, IM_ARRAYSIZE(blockTypes))) {
        placingBlockId_ = static_cast<BlockID>(blockTypeIndex);
    }

    ImGui::SameLine();
    // 生成ボタンを表示する
    if (ImGui::Button("Create")) {
        StageObject o;
        o.id = nextId_++;
        o.modelName = defaultModel_;
        o.position = GetCreateOrigin();
        o.blockId = placingBlockId_;
        o.color = placingColor_;
        data_.objects.push_back(o);
        loader_.UpdateOrCreateInstance(o);
        if (selectedObjectId_ == o.id) {
            loader_.SetInstanceColorById(o.id, { o.color.x, o.color.y, o.color.z, selectionBlinkAlpha_ });
        }
        SaveHistorySnapshot();
    }

    ImGui::Text("Create: Right-click in viewport or press Create button.");
    ImGui::Text("Delete: Press Delete key or use 'Delete Selected' in the list.");

    // オブジェクトの数
    ImGui::Text("Object Count: %d", (int)data_.objects.size());

    // セーブボタンの表示
    if (ImGui::Button("Save")) {
        std::filesystem::create_directories("resources");
        Save("resources/stage.json");
    }

    ImGui::SameLine();

    // ロードボタンの表示
    if (ImGui::Button("Load")) {
        Load("resources/stage.json");
    }

    ImGui::SameLine();

    // ひとつ戻る
    if (ImGui::Button("Undo")) {
        if (historyIndex_ > 0) {
            Undo();
        }
    }

    ImGui::SameLine();

    // ひとつ進む
    if (ImGui::Button("Redo")) {
        if (historyIndex_ + 1 < (int)history_.size()) {
            Redo();
        }
    }

    ImGui::Separator();

    // バッチ生成のオプションを表示する
    ImGui::Text("Batch Create");
    ImGui::InputText("Model for Batch", batchModelBuf_, sizeof(batchModelBuf_));
    ImGui::InputInt("Count A", &batchCountA_);
    ImGui::InputInt("Count B", &batchCountB_);
    ImGui::InputFloat("Spacing", &batchSpacing_);
    const char* axes[] = { "X", "Y(floor)", "Z" };
    ImGui::Combo("Normal Axis", &batchNormalIndex_, axes, IM_ARRAYSIZE(axes));
    ImGui::TextWrapped("Normal Axis selects the axis that is treated as the 'up' direction for the grid. For floor use Y.");

    // 一括生成用のボタン表示
    if (ImGui::Button("Batch Create")) {

        if (batchCountA_ < 1) {
            batchCountA_ = 1;
        }

        if (batchCountB_ < 1) {
            batchCountB_ = 1;
        }

        std::string model(batchModelBuf_);
        Vector3 origin = GetCreateOrigin();
        Vector3 dirA, dirB;

        if (batchNormalIndex_ == 0) {
            dirA = Vector3 { 0.0f, 0.0f, 1.0f };
            dirB = Vector3 { 0.0f, 1.0f, 0.0f };
        } else if (batchNormalIndex_ == 1) {
            dirA = Vector3 { 1.0f, 0.0f, 0.0f };
            dirB = Vector3 { 0.0f, 0.0f, 1.0f };
        } else {
            dirA = Vector3 { 1.0f, 0.0f, 0.0f };
            dirB = Vector3 { 0.0f, 1.0f, 0.0f };
        }

        float halfA = (batchCountA_ - 1) * 0.5f * batchSpacing_;
        float halfB = (batchCountB_ - 1) * 0.5f * batchSpacing_;

        for (int ia = 0; ia < batchCountA_; ++ia) {
            for (int ib = 0; ib < batchCountB_; ++ib) {
                Vector3 pos = origin + dirA * ((ia * batchSpacing_) - halfA) + dirB * ((ib * batchSpacing_) - halfB);
                StageObject o;
                o.id = nextId_++;
                o.modelName = model;
                o.position = pos;
                o.blockId = placingBlockId_;
                o.color = placingColor_;
                data_.objects.push_back(o);
                loader_.UpdateOrCreateInstance(o);
            }
        }

        SaveHistorySnapshot();
    }

    ImGui::Separator();

    // オブジェクトの選択と編集の UI を表示する
    ImGui::Text("Selection");
    ImGui::BeginChild("ObjectList", ImVec2(0, 200), true);
    for (const auto& o : data_.objects) {
        char label[128];
        snprintf(label, sizeof(label), "id:%d model:%s", o.id, o.modelName.c_str());
        bool isSelected = (selectedObjectId_ == o.id);
        if (ImGui::Selectable(label, isSelected)) {
            if (selectedObjectId_ != -1 && selectedObjectId_ != o.id) {
                for (const auto& prevObj : data_.objects) {
                    if (prevObj.id == selectedObjectId_) {
                        loader_.SetInstanceColorById(prevObj.id, prevObj.color);
                        break;
                    }
                }
            }

            selectedObjectId_ = o.id;
            editPosition_ = o.position;
            editRotation_ = o.rotation;
            editScale_ = o.scale;
            editBlockId_ = o.blockId;
            editColor_ = o.color;
            strncpy_s(selectedModelBuf_, sizeof(selectedModelBuf_), o.modelName.c_str(), _TRUNCATE);
            loader_.SetInstanceColorById(o.id, { o.color.x, o.color.y, o.color.z, selectionBlinkAlpha_ });
            selectionLastBlinkTime_ = std::chrono::steady_clock::now();
            selectionBlinkOn_ = true;
        }
    }

    ImGui::EndChild();

    // オブジェクトのパラメーターを編集するための UI を表示する
    if (selectedObjectId_ == -1) {
        ImGui::Text("No object selected");
    } else {
        // 位置、回転、スケール、ブロックID、カラーの編集UIは、オブジェクトが選択されているときにのみ表示する
        ImGui::Text("Selected ID: %d", selectedObjectId_);

        // 位置の編集UIを表示する
        if (ImGui::DragFloat3("Position", &editPosition_.x, 0.1f)) {
            if (liveEdit_) {
                loader_.UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                for (auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        obj.position = editPosition_;
                        break;
                    }
                }
            }
        }

        // 回転の編集UIを表示する
        if (ImGui::DragFloat3("Rotation", &editRotation_.x, 0.01f)) {
            if (liveEdit_) {
                loader_.UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                for (auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        obj.rotation = editRotation_;
                        break;
                    }
                }
            }
        }

        // スケールの編集UIを表示する
        if (ImGui::DragFloat3("Scale", &editScale_.x, 0.01f, 0.01f, 100.0f)) {
            if (liveEdit_) {
                loader_.UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                for (auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        obj.scale = editScale_;
                        break;
                    }
                }
            }
        }

        // ブロックIDの編集UIを表示する
        int editBlockTypeIndex = static_cast<int>(editBlockId_);
        if (ImGui::Combo("Selected Block Type", &editBlockTypeIndex, blockTypes, IM_ARRAYSIZE(blockTypes))) {
            editBlockId_ = static_cast<BlockID>(editBlockTypeIndex);
            if (liveEdit_) {
                for (auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        obj.blockId = editBlockId_;
                        break;
                    }
                }
            }
        }

        // 色の編集UIを表示する
        if (ImGui::ColorEdit4("Color", &editColor_.x)) {
            if (liveEdit_) {
                for (auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        obj.color = editColor_;
                        loader_.SetInstanceColorById(obj.id, editColor_);
                        break;
                    }
                }
            }
        }

        // トランスフォームの編集UIを表示する
        if (ImGui::Button("Apply Transform")) {
            loader_.UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
            for (auto& obj : data_.objects) {
                if (obj.id == selectedObjectId_) {
                    obj.position = editPosition_;
                    obj.rotation = editRotation_;
                    obj.scale = editScale_;
                    obj.blockId = editBlockId_;
                    obj.color = editColor_;
                    break;
                }
            }
            SaveHistorySnapshot();
            loader_.SetInstanceColorById(selectedObjectId_, selectionHighlightColor_);
        }

        ImGui::SameLine();

        // 削除と複製のボタンを表示する
        if (ImGui::Button("Delete Selected")) {
            data_.objects.erase(std::remove_if(data_.objects.begin(), data_.objects.end(), [this](const StageObject& o) { return o.id == selectedObjectId_; }), data_.objects.end());
            loader_.RemoveInstanceById(selectedObjectId_);
            selectedObjectId_ = -1;
            selectedModelBuf_[0] = '\0';
            SaveHistorySnapshot();
        }

        ImGui::SameLine();
        ImGui::InputInt("Duplicate Count", &duplicateCount_);
        if (duplicateCount_ < 1) {
            duplicateCount_ = 1;
        }

        ImGui::Checkbox("Use Half-Size Offset", &useHalfSizeOffset_);
        if (!useHalfSizeOffset_) {
            ImGui::DragFloat3("Duplicate Offset", &duplicateOffset_.x, 0.1f);
        } else {
            ImGui::Text("Offset will use half of source scale");
        }

        ImGui::SameLine();
        if (ImGui::Button("Duplicate Selected")) {
            if (selectedObjectId_ != -1) {
                for (const auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        StageObject base = obj;
                        StageObject lastCreated;
                        Vector3 perStepOffset = useHalfSizeOffset_ ? Vector3 { base.scale.x * 2.0f, 0.0f, 0.0f } : duplicateOffset_;
                        for (int i = 0; i < duplicateCount_; ++i) {
                            StageObject newObj = base;
                            newObj.id = nextId_++;
                            newObj.position = newObj.position + perStepOffset * static_cast<float>(i + 1);
                            newObj.color = base.color;
                            data_.objects.push_back(newObj);
                            loader_.UpdateOrCreateInstance(newObj);
                            lastCreated = newObj;
                        }
                        selectedObjectId_ = lastCreated.id;
                        loader_.SetInstanceColorById(selectedObjectId_, { lastCreated.color.x, lastCreated.color.y, lastCreated.color.z, selectionBlinkAlpha_ });
                        selectionLastBlinkTime_ = std::chrono::steady_clock::now();
                        selectionBlinkOn_ = true;
                        strncpy_s(selectedModelBuf_, sizeof(selectedModelBuf_), lastCreated.modelName.c_str(), _TRUNCATE);
                        SaveHistorySnapshot();
                        break;
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Model");
        // モデルの編集UIを表示する
        if (ImGui::InputText("Model Path", selectedModelBuf_, sizeof(selectedModelBuf_))) { }
        if (ImGui::Button("Apply Model")) {
            for (auto& obj : data_.objects) {
                if (obj.id == selectedObjectId_) {
                    obj.modelName = std::string(selectedModelBuf_);
                    loader_.UpdateOrCreateInstance(obj);
                    loader_.SetInstanceColorById(obj.id, selectionHighlightColor_);
                    break;
                }
            }
            SaveHistorySnapshot();
        }
    }

    ImGui::End();

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("StageEditor Debug", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Instances: %zu", loader_.GetInstanceCount());
    ImGui::End();
#endif // USE_IMGUI
}

/// <summary>
/// デバッグ用に、現在のインスタンス数を表示するウィンドウ
/// </summary>
std::string StageEditor::ResolveDisplayModelName(const StageObject& o) const
{
#ifdef USE_IMGUI
    switch (o.blockId) {
    case BlockID::BugSpawn:
        return "sphere.obj";
    case BlockID::PlayerSpawn:
        return "Cube.obj";
    default:
        return o.modelName;
    }

#endif // !USE_IMGUI
}

/// <summary>
/// ブロックIDに応じて、編集画面での表示用モデル名を返す。虫スポーンは球、プレイヤースポーンは立方体で表示する
/// </summary>
Vector3 StageEditor::ResolveDisplayScale(const StageObject& o) const
{
#ifdef USE_IMGUI
    // ブロックIDに応じて、編集画面での表示用スケールを返す
    switch (o.blockId) {
    case BlockID::BugSpawn:
        return { 0.35f, 0.35f, 0.35f };
    case BlockID::PlayerSpawn:
        return { 0.6f, 1.2f, 0.6f };
    default:
        return o.scale;
    }
#endif // !USE_IMGUI
}
