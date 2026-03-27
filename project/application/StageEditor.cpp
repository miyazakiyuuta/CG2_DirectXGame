#define NOMINMAX

#include "StageEditor.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <imgui.h>

/// <summary>
/// コンストラクタ
/// </summary>
StageEditor::StageEditor(Stage* stage, Object3dCommon* objCommon, Camera* camera)
    : stage_(stage)
    , controller_(camera){
    // Object3dCommon と Camera の参照を保存
    object3dCommon_ = objCommon;
    camera_ = camera;

    // デフォルトモデル名を初期化
    strcpy_s(defaultModelBuf_, sizeof(defaultModelBuf_), defaultModel_.c_str());

    previewMarker_ = std::make_unique<Object3d>();
    // Object3d の初期化は Object3dCommon が必要なので、コンストラクタ内で行う
    if(object3dCommon_){
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
void StageEditor::Initialize(const std::string& defaultModel){
    // デフォルトモデル名を設定
    defaultModel_ = defaultModel;
    // ImGui 用固定長バッファも更新
    strcpy_s(defaultModelBuf_, sizeof(defaultModelBuf_), defaultModel_.c_str());

    if(stage_){
        if(stage_->GetStageData().name.empty()){
            stage_->GetStageData().name = "stage";
        }

        int maxId = 0;
        for(const auto& o : stage_->GetStageData().objects){
            if(o.id > maxId){
                maxId = o.id;
            }
        }
        nextId_ = maxId + 1;
    } else{
        nextId_ = 1;
    }

    SaveHistorySnapshot();

    // プレビューマーカーのモデルを設定
    if(previewMarker_){
        previewMarker_->SetModel(defaultModel_);
    }
}

/// <summary>
/// 更新処理（入力チェックとオブジェクト生成・削除）
/// </summary>
void StageEditor::Update(){
#ifdef USE_IMGUI
    if(!stage_){
        return;
    }

    auto& data = stage_->GetStageData();

    if(createReferenceIndex_ != 3){
        moveOriginMode_ = false;
    }

    if(isEditMode_){
        if(controller_.IsCreateKeyPressed()){
            StageObject o;
            o.id = nextId_++;
            o.modelName = defaultModel_;
            o.position = GetCreateOrigin();
            o.rotation = createRotation_;
            o.scale = createScale_;
            o.blockId = placingBlockId_;
            o.color = placingColor_;

            data.objects.push_back(o);
            stage_->UpdateOrCreateInstance(o);

            if(selectedObjectId_ == o.id){
                stage_->SetInstanceColorById(o.id, selectionHighlightColor_);
            }
            // 履歴にスナップショットを保存
            SaveHistorySnapshot();
        }

        if(controller_.IsDeleteKeyPressed()){
            if(!data.objects.empty()){
                StageObject removed = data.objects.back();
                data.objects.pop_back();
                stage_->RemoveInstanceById(removed.id);
                SaveHistorySnapshot();
            }
        }
    }

    if(isEditMode_ && useHotkey_){
        if(!ImGui::GetIO().WantCaptureKeyboard){
            if(toggleKeyBuf_[0] != '\0'){
                int vk = toupper(static_cast<unsigned char>(toggleKeyBuf_[0]));
                bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
                if(down && !hotkeyPrevDown_){
                    moveOriginMode_ = !moveOriginMode_;
                    if(moveOriginMode_){
                        createReferenceIndex_ = 3;
                    }
                }
                hotkeyPrevDown_ = down;
            }
        }
    }

    if(isEditMode_ && createReferenceIndex_ == 3 && moveOriginMode_ && camera_){

        if(!ImGui::GetIO().WantCaptureMouse){
            POINT pt;
            long wheel = Input::GetInstance()->GetMouseWheel();
            bool skipXZUpdate = false;

            if(wheel != 0){
                createOrigin_.y += static_cast<float>(wheel) * createHeightSensitivity_ * 0.001f;
                skipXZUpdate = true;
            }

            if(GetCursorPos(&pt) && !skipXZUpdate){
                HWND hwnd = WinApp::GetInstance()->GetHwnd();
                POINT clientPt = pt;
                ScreenToClient(hwnd, &clientPt);

                const float w = static_cast<float>(WinApp::kClientWidth);
                const float h = static_cast<float>(WinApp::kClientHeight);
                float px = static_cast<float>(clientPt.x);
                float py = static_cast<float>(clientPt.y);
                float nx = (px / w) * 2.0f - 1.0f;
                float ny = -((py / h) * 2.0f - 1.0f);

                Matrix4x4 vp = camera_->GetViewProjectionMatrix();
                Matrix4x4 invVP = vp;
                invVP = invVP.Inverse();

                Vector3 nearClip = { nx, ny, 0.0f };
                Vector3 farClip = { nx, ny, 1.0f };

                Vector3 pNear = invVP.Transform(nearClip);
                Vector3 pFar = invVP.Transform(farClip);
                Vector3 dir = pFar - pNear;

                float planeY = createOrigin_.y;
                if(std::abs(dir.y) > 1e-6f){
                    float t = (planeY - pNear.y) / dir.y;
                    Vector3 intersect = pNear + dir * t;
                    createOrigin_.x = intersect.x;
                    createOrigin_.z = intersect.z;
                    createOrigin_.y = planeY;
                }
            }
        }
    }

    // Selection highlight handling: support single and multiple selection
    if(!selectedObjectIds_.empty()){
        if(selectedObjectIds_.size() == 1){
            int id = selectedObjectIds_[0];
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - selectionLastBlinkTime_).count();

            Vector4 originalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            bool found = false;
            for(const auto& obj : data.objects){
                if(obj.id == id){
                    originalColor = obj.color;
                    found = true;
                    break;
                }
            }

            if(found){
                float phase = fmodf(elapsed, selectionBlinkInterval_) / selectionBlinkInterval_;
                float t = 0.5f * (1.0f + std::sinf(2.0f * 3.14159265f * phase));
                float lerpedAlpha = originalColor.w * (1.0f - t) + selectionBlinkAlpha_ * t;

                // Throttle updates to stage_->SetInstanceColorById to avoid expensive per-frame GPU/resource updates.
                const float alphaEpsilon = 0.01f;
                const float minIntervalSec = 0.1f; // at most 10 updates per second
                auto timeSinceLastApplied = std::chrono::duration_cast<std::chrono::duration<float>>(now - selectionLastAppliedTime_).count();
                if(std::abs(lerpedAlpha - selectionLastAppliedAlpha_) > alphaEpsilon || timeSinceLastApplied > minIntervalSec){
                    stage_->SetInstanceColorById(id, { originalColor.x, originalColor.y, originalColor.z, lerpedAlpha });
                    selectionLastAppliedAlpha_ = lerpedAlpha;
                    selectionLastAppliedTime_ = now;
                }
            }
        } else {
            // Multiple selection: apply static highlight color
            for(int id : selectedObjectIds_){
                stage_->SetInstanceColorById(id, selectionHighlightColor_);
            }
        }
    }

    DrawImGui();
#endif
}

/// <summary>
/// 描画処理（エディタプレビュー描画）
/// </summary>
void StageEditor::Draw(){
    if(!isEditMode_ || !previewSphere_){
        return;
    }

    std::vector<Vector3> centers;

    // 選択中でも uiMode_ を元に生成プレビューを出す
    EditorUIMode previewMode = uiMode_;

    if(previewMode == EditorUIMode::SingleCreate){
        centers.push_back(GetCreateOrigin());
        {
            float scaleMax = std::max({ createScale_.x, createScale_.y, createScale_.z });
            float radius = previewRadius_ * scaleMax;
            previewSphere_->Draw(centers, radius, { 0.0f, 1.0f, 0.0f, 1.0f }, *camera_);
        }
    } else if(previewMode == EditorUIMode::BatchCreate){
        Vector3 dirA, dirB;

        if(batchNormalIndex_ == 0){
            dirA = { 0.0f, 0.0f, 1.0f };
            dirB = { 0.0f, 1.0f, 0.0f };
        } else if(batchNormalIndex_ == 1){
            dirA = { 1.0f, 0.0f, 0.0f };
            dirB = { 0.0f, 0.0f, 1.0f };
        } else{
            dirA = { 1.0f, 0.0f, 0.0f };
            dirB = { 0.0f, 1.0f, 0.0f };
        }

        float halfA = (batchCountA_ - 1) * 0.5f * batchSpacing_;
        float halfB = (batchCountB_ - 1) * 0.5f * batchSpacing_;
        Vector3 origin = GetCreateOrigin();

        for(int ia = 0; ia < batchCountA_; ++ia){
            for(int ib = 0; ib < batchCountB_; ++ib){
                Vector3 pos = origin
                    + dirA * ((ia * batchSpacing_) - halfA)
                    + dirB * ((ib * batchSpacing_) - halfB);
                centers.push_back(pos);
            }
        }

        if(!centers.empty()){
            float scaleMax = std::max({ createScale_.x, createScale_.y, createScale_.z });
            float radius = previewRadius_ * scaleMax;
            previewSphere_->Draw(centers, radius, { 0.0f, 0.5f, 1.0f, 1.0f }, *camera_);
        }
    }
}

/// <summary>
/// 編集モードの切り替え
/// </summary>
void StageEditor::ToggleEditMode(){ isEditMode_ = !isEditMode_; }

/// <summary>
/// 編集モードかどうかを返す
/// </summary>
bool StageEditor::IsEditMode() const{ return isEditMode_; }

/// <summary>
/// JSONファイルにステージデータを保存
/// </summary>
bool StageEditor::Save(const std::string& path){
    if(!stage_){
        return false;
    }
    return StageSerializer::SaveToFile(stage_->GetStageData(), path);
}

/// <summary>
/// JSONファイルからステージデータを読み込み、オブジェクトを生成
/// </summary>
bool StageEditor::Load(const std::string& path){
    if(!stage_){
        return false;
    }

    auto d = StageSerializer::LoadFromFile(path);
    if(!d){
        return false;
    }

    stage_->SetStageData(*d);

    // (visibility feature removed)

    int maxId = 0;
    for(auto& o : stage_->GetStageData().objects){
        if(o.id > maxId){
            maxId = o.id;
        }
    }

    nextId_ = maxId + 1;
    SaveHistorySnapshot();
    return true;
}

/// <summary>
/// ステージデータとオブジェクトをすべてクリア
/// </summary>
void StageEditor::Clear(){
    if(!stage_){
        return;
    }
    stage_->Clear();
    nextId_ = 1;
    SaveHistorySnapshot();
}

/// <summary>
/// ステージデータへの参照を返す
/// </summary>
StageData& StageEditor::GetStageData(){
    return stage_->GetStageData();
}

void StageEditor::SaveHistorySnapshot(){
    if(!stage_){
        return;
    }

    if(historyIndex_ + 1 < (int)history_.size()){
        history_.erase(history_.begin() + historyIndex_ + 1, history_.end());
    }

    Snapshot s;
    s.data = stage_->GetStageData();
    s.nextId = nextId_;
    history_.push_back(std::move(s));
    historyIndex_ = (int)history_.size() - 1;
}

Vector3 StageEditor::GetCreateOrigin() const{
    if(!stage_){
        return {};
    }

    Vector3 base{};
    const auto& data = stage_->GetStageData();

    switch(createReferenceIndex_){
        case 0:
            base = controller_.GetCreatePosition();
            break;
        case 1:
            base = Vector3{ 0.0f, 0.0f, 0.0f };
            break;
        case 2:
            if(!selectedObjectIds_.empty()){
                int sid = selectedObjectIds_[0];
                for(const auto& o : data.objects){
                    if(o.id == sid){
                        base = o.position;
                        break;
                    }
                }
            } else{
                base = Vector3{ 0.0f, 0.0f, 0.0f };
            }
            break;
        case 3:
        default:
            base = createOrigin_;
            break;
    }

    return base + createOffset_;
}

void StageEditor::Undo(){
    if(!stage_ || historyIndex_ <= 0){
        return;
    }

    --historyIndex_;
    const auto& s = history_[historyIndex_];
    stage_->SetStageData(s.data);
    nextId_ = s.nextId;
    ClearSelection();
}

void StageEditor::Redo(){
    if(!stage_ || historyIndex_ + 1 >= (int)history_.size()){
        return;
    }

    ++historyIndex_;
    const auto& s = history_[historyIndex_];
    stage_->SetStageData(s.data);
    nextId_ = s.nextId;
    ClearSelection();
}

void StageEditor::ClearSelection(){
    if(!stage_){
        selectedObjectId_ = -1;
        selectedObjectIds_.clear();
        selectedModelBuf_[0] = '\0';
        return;
    }

    auto& data = stage_->GetStageData();

    // Restore colors for any selected objects
    if(!selectedObjectIds_.empty()){
        for(int id : selectedObjectIds_){
            for(const auto& obj : data.objects){
                if(obj.id == id){
                    stage_->SetInstanceColorById(obj.id, obj.color);
                    break;
                }
            }
        }
    } else if(selectedObjectId_ != -1){
        for(const auto& obj : data.objects){
            if(obj.id == selectedObjectId_){
                stage_->SetInstanceColorById(obj.id, obj.color);
                break;
            }
        }
    }

    selectedObjectId_ = -1;
    selectedObjectIds_.clear();
    selectedModelBuf_[0] = '\0';
}

void StageEditor::DrawImGui(){
#ifdef USE_IMGUI
    if(!stage_){
        return;
    }

    auto& data = stage_->GetStageData();

    EditorUIMode currentMode;
    if(selectedObjectIds_.size() == 1){
        currentMode = EditorUIMode::SelectedEdit;
    } else if(selectedObjectIds_.size() > 1){
        currentMode = EditorUIMode::MultiSelected;
    } else {
        currentMode = uiMode_;
    }

    ImGui::Begin("StageEditor");

    if(ImGui::Button(isEditMode_ ? "Exit Edit Mode" : "Enter Edit Mode")){
        ToggleEditMode();
    }

    ImGui::SameLine();
    if(ImGui::Button("Clear")){
        Clear();
        ClearSelection();
    }

    ImGui::Text("Object Count: %d", (int)data.objects.size());

    if(ImGui::Button("Save")){
        std::filesystem::create_directories("resources");
        Save("resources/stage.json");
    }

    ImGui::SameLine();
    if(ImGui::Button("Load")){
        Load("resources/stage.json");
        ClearSelection();
    }

    ImGui::SameLine();
    if(ImGui::Button("Undo")){
        if(historyIndex_ > 0){
            Undo();
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Redo")){
        if(historyIndex_ + 1 < (int)history_.size()){
            Redo();
        }
    }

    ImGui::Separator();


        ImGui::Text("Editor Mode");
        int uiMode = static_cast<int>(uiMode_);
        const char* uiModes[] = { "Single Create", "Batch Create", "Selected Edit" };
        if(ImGui::Combo("Mode", &uiMode, uiModes, IM_ARRAYSIZE(uiModes))){
            uiMode_ = static_cast<EditorUIMode>(uiMode);
            ClearSelection();
        }


    ImGui::Separator();

    ImGui::Text("Objects");
    if(ImGui::Button("Clear Selection")){
        ClearSelection();
        uiMode_ = EditorUIMode::SingleCreate;
    }
    // Field visibility toggles for object list
    ImGui::SameLine();
    if(ImGui::Button("Fields")){
        // open small popup for field toggles
        ImGui::OpenPopup("FieldToggles");
    }
    if(ImGui::BeginPopup("FieldToggles")){
        ImGui::Checkbox("ID", &showFieldId_);
        ImGui::Checkbox("Model", &showFieldModel_);
        ImGui::Checkbox("Position", &showFieldPosition_);
        ImGui::Checkbox("Rotation", &showFieldRotation_);
        ImGui::Checkbox("Scale", &showFieldScale_);
        ImGui::Checkbox("Block Type", &showFieldBlockType_);
        ImGui::Checkbox("Color Alpha", &showFieldColor_);
        ImGui::EndPopup();
    }

    // show/hide all removed (visibility feature disabled)

    ImGui::BeginChild("ObjectList", ImVec2(0, 200), true);
    // Use ImGuiListClipper to avoid creating UI items for off-screen list entries
    int objCount = static_cast<int>(data.objects.size());
    ImGuiListClipper clip;
    clip.Begin(objCount);
    while(clip.Step()){
        for(int i = clip.DisplayStart; i < clip.DisplayEnd; ++i){
            const auto& o = data.objects[i];
            // Build label according to user-selected fields
            char label[512];
            int off = 0;
            if(showFieldId_){ off += snprintf(label + off, sizeof(label) - off, "id:%d ", o.id); }
            if(showFieldModel_){ off += snprintf(label + off, sizeof(label) - off, "model:%s ", o.modelName.c_str()); }
            if(showFieldPosition_){ off += snprintf(label + off, sizeof(label) - off, "pos:%.2f,%.2f,%.2f ", o.position.x, o.position.y, o.position.z); }
            if(showFieldRotation_){ off += snprintf(label + off, sizeof(label) - off, "rot:%.2f,%.2f,%.2f ", o.rotation.x, o.rotation.y, o.rotation.z); }
            if(showFieldScale_){ off += snprintf(label + off, sizeof(label) - off, "s:%.2f,%.2f,%.2f ", o.scale.x, o.scale.y, o.scale.z); }
            if(showFieldBlockType_){ off += snprintf(label + off, sizeof(label) - off, "type:%d ", static_cast<int>(o.blockId)); }
            if(showFieldColor_){ off += snprintf(label + off, sizeof(label) - off, "c:%.2f ", o.color.w); }
            bool isSelected = std::find(selectedObjectIds_.begin(), selectedObjectIds_.end(), o.id) != selectedObjectIds_.end();



            if(ImGui::Selectable(label, isSelected)){
            ImGui::SetItemDefaultFocus();
                ImGuiIO& io = ImGui::GetIO();
                bool ctrl = io.KeyCtrl;

                if(ctrl){
                    // Toggle selection membership
                    if(isSelected){
                        // Deselect this item
                        for(auto it = selectedObjectIds_.begin(); it != selectedObjectIds_.end(); ++it){
                            if(*it == o.id){
                                selectedObjectIds_.erase(it);
                                break;
                            }
                        }
                        // Restore color
                        stage_->SetInstanceColorById(o.id, o.color);
                    } else {
                        // Add to selection
                        selectedObjectIds_.push_back(o.id);
                        stage_->SetInstanceColorById(o.id, { o.color.x, o.color.y, o.color.z, selectionBlinkAlpha_ });
                    }

                    // Update single-selection compatibility
                    if(selectedObjectIds_.size() == 1){
                        selectedObjectId_ = selectedObjectIds_[0];
                    } else {
                        selectedObjectId_ = -1;
                    }
                } else {
                    // Single selection
                    ClearSelection();

                    selectedObjectIds_.push_back(o.id);
                    selectedObjectId_ = o.id;
                    uiMode_ = EditorUIMode::SelectedEdit;
                    editPosition_ = o.position;
                    editRotation_ = o.rotation;
                    editScale_ = o.scale;
                    editBlockId_ = o.blockId;
                    editColor_ = o.color;
                    editHp_ = o.hp;
                    editWarpTargetPosition_ = o.warpTargetPosition;
                    editWarpTargetSceneId_ = o.warpTargetSceneId;
                    editMoveDirection_ = o.moveDirection;
                    editMoveSpeed_ = o.moveSpeed;
                    editMoveRange_ = o.moveRange;
                    strncpy_s(selectedModelBuf_, sizeof(selectedModelBuf_), o.modelName.c_str(), _TRUNCATE);

                    stage_->SetInstanceColorById(o.id, { o.color.x, o.color.y, o.color.z, selectionBlinkAlpha_ });
                    selectionLastBlinkTime_ = std::chrono::steady_clock::now();
                    selectionBlinkOn_ = true;
                }
            }
        }
    }
    clip.End();
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::SameLine();
    if(ImGui::Checkbox("Live Edit", &liveEdit_)){
        if(liveEdit_ && selectedObjectId_ != -1){
            stage_->UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
            for(auto& obj : data.objects){
                if(obj.id == selectedObjectId_){
                    obj.position = editPosition_;
                    obj.rotation = editRotation_;
                    obj.scale = editScale_;
                    break;
                }
            }
        }
    }

    ImGui::Separator();

    const char* blockTypes[] = { "Normal", "Water", "BugSpawn", "PlayerSpawn", "Breakable", "Warp", "MovingPlatform" };

    if(currentMode != EditorUIMode::SelectedEdit){
        ImGui::Text("Create Origin");
        const char* refs[] = { "Camera Forward", "World Origin", "Selected Object", "Custom" };
        if(ImGui::Combo("Reference", &createReferenceIndex_, refs, IM_ARRAYSIZE(refs))){
            if(createReferenceIndex_ != 3){
                moveOriginMode_ = false;
            }
        }

        ImGui::SameLine();
        if(ImGui::Button("Set To Camera")){
            createOrigin_ = controller_.GetCreatePosition();
            createReferenceIndex_ = 3;
        }

        if(createReferenceIndex_ == 3){
            ImGui::Checkbox("Move Origin Mode", &moveOriginMode_);
        }
        ImGui::DragFloat3("Origin Pos", &createOrigin_.x, 0.1f);
        {
            // Preview Position shows the actual position used for creation (base + offset).
            Vector3 previewPos = GetCreateOrigin();
            if(ImGui::DragFloat3("Preview Position", &previewPos.x, 0.1f)){
                // When user edits the preview position, switch to Custom reference and apply it.
                createReferenceIndex_ = 3;
                createOrigin_ = previewPos;
            }
        }
        ImGui::DragFloat3("Origin Rotation", &createRotation_.x, 0.01f);
        ImGui::DragFloat3("Create Offset", &createOffset_.x, 0.1f);
        ImGui::DragFloat3("Create Scale", &createScale_.x, 0.01f, 0.01f, 100.0f);

        ImGui::Separator();

        if(ImGui::InputText("Model", defaultModelBuf_, sizeof(defaultModelBuf_))){
            defaultModel_ = std::string(defaultModelBuf_);
        }
        ImGui::ColorEdit4("Placing Color", &placingColor_.x);

        int blockTypeIndex = static_cast<int>(placingBlockId_);
        if(ImGui::Combo("Block Type", &blockTypeIndex, blockTypes, IM_ARRAYSIZE(blockTypes))){
            placingBlockId_ = static_cast<BlockID>(blockTypeIndex);
        }

        // Placing HP (only relevant for Breakable)
        if(placingBlockId_ == BlockID::Breakable){
            ImGui::InputInt("Placing HP", &placingHp_);
            if(placingHp_ < 0) placingHp_ = 0;
        }

        // Placing Warp settings
        if(placingBlockId_ == BlockID::Warp){
            ImGui::InputFloat3("Warp Target Pos", &placingWarpTargetPosition_.x);
            ImGui::InputInt("Warp Target SceneId", &placingWarpTargetSceneId_);
        }

        // Placing MovingPlatform settings
        if(placingBlockId_ == BlockID::MovingPlatform){
            const char* moveDirs[] = { "None", "Up/Down", "Down/Up", "Left/Right", "Right/Left" };
            ImGui::Combo("Move Direction", &placingMoveDirection_, moveDirs, IM_ARRAYSIZE(moveDirs));
            ImGui::InputFloat("Move Speed", &placingMoveSpeed_);
            ImGui::InputFloat("Move Range", &placingMoveRange_);
            ImGui::InputFloat("Move Phase (0..1)", &placingMovePhase_);
        }

        ImGui::Separator();
    }

    switch(currentMode){
        case EditorUIMode::SingleCreate:
        {
            ImGui::Text("Single Create");

            if(ImGui::Button("Create")){
                StageObject o;
                o.id = nextId_++;
                o.modelName = defaultModel_;
                o.position = GetCreateOrigin();
                o.rotation = createRotation_;
                o.scale = createScale_;
                o.blockId = placingBlockId_;
                o.color = placingColor_;
                o.hp = placingHp_;
                if(o.blockId == BlockID::Warp){
                    o.warpTargetPosition = placingWarpTargetPosition_;
                    o.warpTargetSceneId = placingWarpTargetSceneId_;
                }
                if(o.blockId == BlockID::MovingPlatform){
                    o.moveDirection = placingMoveDirection_;
                    o.moveSpeed = placingMoveSpeed_;
                    o.moveRange = placingMoveRange_;
                    o.movePhase = placingMovePhase_;
                }

                data.objects.push_back(o);
                stage_->UpdateOrCreateInstance(o);
                SaveHistorySnapshot();
            }
            break;
        }

        case EditorUIMode::MultiSelected:
        {
            ImGui::Text("Multiple Selected: %d", (int)selectedObjectIds_.size());
            ImGui::Separator();

            for(size_t si = 0; si < selectedObjectIds_.size(); ++si){
                int sid = selectedObjectIds_[si];
                StageObject* objPtr = nullptr;
                for(auto& o : data.objects){ if(o.id == sid){ objPtr = &o; break; } }
                if(!objPtr){ ImGui::Text("id:%d (not found)", sid); continue; }

                StageObject& sobj = *objPtr;
                char header[128];
                snprintf(header, sizeof(header), "id:%d model:%s", sobj.id, sobj.modelName.c_str());

                if(ImGui::TreeNode(header)){
                    // Show fields according to user toggles
                    if(showFieldPosition_){
                        if(ImGui::DragFloat3("Position", &sobj.position.x, 0.1f)){
                            if(liveEdit_) stage_->UpdateInstanceTransform(sobj.id, sobj.position, sobj.rotation, sobj.scale);
                        }
                    }

                    if(showFieldRotation_){
                        if(ImGui::DragFloat3("Rotation", &sobj.rotation.x, 0.01f)){
                            if(liveEdit_) stage_->UpdateInstanceTransform(sobj.id, sobj.position, sobj.rotation, sobj.scale);
                        }
                    }

                    if(showFieldScale_){
                        if(ImGui::DragFloat3("Scale", &sobj.scale.x, 0.01f, 0.01f, 100.0f)){
                            if(liveEdit_) stage_->UpdateInstanceTransform(sobj.id, sobj.position, sobj.rotation, sobj.scale);
                        }
                    }

                    if(showFieldBlockType_){
                        int blockTypeIndex = static_cast<int>(sobj.blockId);
                        if(ImGui::Combo("Block Type", &blockTypeIndex, blockTypes, IM_ARRAYSIZE(blockTypes))){
                            sobj.blockId = static_cast<BlockID>(blockTypeIndex);
                        }
                    }

                    // HP field for breakable
                    if(sobj.blockId == BlockID::Breakable){
                        if(ImGui::InputInt("HP", &sobj.hp)){
                            if(sobj.hp < 0) sobj.hp = 0;
                        }
                    }

                    if(showFieldColor_){
                        if(ImGui::ColorEdit4("Color", &sobj.color.x)){
                            stage_->SetInstanceColorById(sobj.id, sobj.color);
                        }
                    }

                    // Model field is a little different: show when model or model field enabled
                    if(showFieldModel_){
                        ImGui::Separator();
                        char modelBufLocal[256];
                        strncpy_s(modelBufLocal, sizeof(modelBufLocal), sobj.modelName.c_str(), _TRUNCATE);
                        ImGui::InputText("Model Path", modelBufLocal, sizeof(modelBufLocal));
                        ImGui::SameLine();
                        if(ImGui::Button("Apply To This")){
                            sobj.modelName = std::string(modelBufLocal);
                            stage_->UpdateOrCreateInstance(sobj);
                            SaveHistorySnapshot();
                        }
                    }

                    ImGui::SameLine();
                    if(ImGui::Button("Delete This")){
                        int delId = sobj.id;
                        stage_->RemoveInstanceById(delId);
                        data.objects.erase(std::remove_if(data.objects.begin(), data.objects.end(), [delId](const StageObject& o){ return o.id == delId; }), data.objects.end());
                        selectedObjectIds_.erase(std::remove(selectedObjectIds_.begin(), selectedObjectIds_.end(), delId), selectedObjectIds_.end());
                        ImGui::TreePop();
                        --si;
                        continue;
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::Separator();
            ImGui::Text("Common actions for selected items:");
            // Show/Hide Selected removed (visibility feature disabled)
            if(showFieldModel_){
                if(ImGui::Button("Apply Common Model")){
                    for(auto id : selectedObjectIds_){ for(auto& o : data.objects){ if(o.id == id){ o.modelName = std::string(selectedModelBuf_); stage_->UpdateOrCreateInstance(o); break; } } }
                    SaveHistorySnapshot();
                }
                ImGui::SameLine();
            }
            if(showFieldColor_){
                if(ImGui::Button("Apply Common Color")){
                    for(auto id : selectedObjectIds_){ for(auto& o : data.objects){ if(o.id == id){ o.color = editColor_; stage_->SetInstanceColorById(o.id, editColor_); break; } } }
                    SaveHistorySnapshot();
                }
            }

            break;
        }
        case EditorUIMode::BatchCreate:
        {
            ImGui::Text("Batch Create");

            ImGui::InputInt("Count A", &batchCountA_);
            ImGui::InputInt("Count B", &batchCountB_);
            ImGui::InputFloat("Spacing", &batchSpacing_);

            const char* axes[] = { "X", "Y(floor)", "Z" };
            ImGui::Combo("Normal Axis", &batchNormalIndex_, axes, IM_ARRAYSIZE(axes));

            if(ImGui::Button("Batch Create")){
                if(batchCountA_ < 1) batchCountA_ = 1;
                if(batchCountB_ < 1) batchCountB_ = 1;

                Vector3 origin = GetCreateOrigin();
                Vector3 dirA, dirB;

                if(batchNormalIndex_ == 0){
                    dirA = Vector3{ 0.0f, 0.0f, 1.0f };
                    dirB = Vector3{ 0.0f, 1.0f, 0.0f };
                } else if(batchNormalIndex_ == 1){
                    dirA = Vector3{ 1.0f, 0.0f, 0.0f };
                    dirB = Vector3{ 0.0f, 0.0f, 1.0f };
                } else{
                    dirA = Vector3{ 1.0f, 0.0f, 0.0f };
                    dirB = Vector3{ 0.0f, 1.0f, 0.0f };
                }

                float halfA = (batchCountA_ - 1) * 0.5f * batchSpacing_;
                float halfB = (batchCountB_ - 1) * 0.5f * batchSpacing_;

                for(int ia = 0; ia < batchCountA_; ++ia){
                    for(int ib = 0; ib < batchCountB_; ++ib){
                        Vector3 pos = origin + dirA * ((ia * batchSpacing_) - halfA) + dirB * ((ib * batchSpacing_) - halfB);

                        StageObject o;
                        o.id = nextId_++;
                        o.modelName = defaultModel_;
                        o.position = pos;
                        o.rotation = createRotation_;
                        o.scale = createScale_;
                        o.blockId = placingBlockId_;
                        o.color = placingColor_;
                        o.hp = placingHp_;
                        if(o.blockId == BlockID::Warp){
                            o.warpTargetPosition = placingWarpTargetPosition_;
                            o.warpTargetSceneId = placingWarpTargetSceneId_;
                        }
                        if(o.blockId == BlockID::MovingPlatform){
                            o.moveDirection = placingMoveDirection_;
                            o.moveSpeed = placingMoveSpeed_;
                            o.moveRange = placingMoveRange_;
                            o.movePhase = placingMovePhase_;
                        }
                        data.objects.push_back(o);
                        stage_->UpdateOrCreateInstance(o);
                    }
                }

                SaveHistorySnapshot();
            }
            break;
        }

        case EditorUIMode::SelectedEdit:
        {
            ImGui::Text("Selected Edit");

            if(selectedObjectId_ == -1){
                ImGui::Text("No object selected");
            } else{
                ImGui::Text("Selected ID: %d", selectedObjectId_);

                if(showFieldPosition_){
                    if(ImGui::DragFloat3("Position", &editPosition_.x, 0.1f)){
                        if(liveEdit_){
                            stage_->UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.position = editPosition_;
                                    break;
                                }
                            }
                        }
                    }
                }

                if(showFieldRotation_){
                    if(ImGui::DragFloat3("Rotation", &editRotation_.x, 0.01f)){
                        if(liveEdit_){
                            stage_->UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.rotation = editRotation_;
                                    break;
                                }
                            }
                        }
                    }
                }

                if(showFieldScale_){
                    if(ImGui::DragFloat3("Scale", &editScale_.x, 0.01f, 0.01f, 100.0f)){
                        if(liveEdit_){
                            stage_->UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.scale = editScale_;
                                    break;
                                }
                            }
                        }
                    }
                }

                if(showFieldBlockType_){
                    int editBlockTypeIndex = static_cast<int>(editBlockId_);
                    if(ImGui::Combo("Selected Block Type", &editBlockTypeIndex, blockTypes, IM_ARRAYSIZE(blockTypes))){
                        editBlockId_ = static_cast<BlockID>(editBlockTypeIndex);
                        if(liveEdit_){
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.blockId = editBlockId_;
                                    break;
                                }
                            }
                        }
                    }
                }

                // HP for breakable blocks
                if(editBlockId_ == BlockID::Breakable){
                    if(ImGui::InputInt("HP", &editHp_)){
                        if(editHp_ < 0) editHp_ = 0;
                        if(liveEdit_){
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.hp = editHp_;
                                    break;
                                }
                            }
                        }
                    }
                }

                // Warp fields
                if(editBlockId_ == BlockID::Warp){
                    if(ImGui::InputFloat3("Warp Target Pos", &editWarpTargetPosition_.x)){
                        if(liveEdit_){
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.warpTargetPosition = editWarpTargetPosition_;
                                    break;
                                }
                            }
                        }
                    }
                    if(ImGui::InputInt("Warp Target SceneId", &editWarpTargetSceneId_)){
                        if(liveEdit_){
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.warpTargetSceneId = editWarpTargetSceneId_;
                                    break;
                                }
                            }
                        }
                    }
                }

                // MovingPlatform fields
                if(editBlockId_ == BlockID::MovingPlatform){
                    const char* moveDirs[] = { "None", "Up/Down", "Down/Up", "Left/Right", "Right/Left" };
                    if(ImGui::Combo("Move Direction", &editMoveDirection_, moveDirs, IM_ARRAYSIZE(moveDirs))){
                        if(liveEdit_){
                            for(auto& obj : data.objects){ if(obj.id == selectedObjectId_){ obj.moveDirection = editMoveDirection_; break; } }
                        }
                    }
                    if(ImGui::InputFloat("Move Speed", &editMoveSpeed_)){
                        if(liveEdit_){ for(auto& obj : data.objects){ if(obj.id == selectedObjectId_){ obj.moveSpeed = editMoveSpeed_; break; } } }
                    }
                    if(ImGui::InputFloat("Move Range", &editMoveRange_)){
                        if(liveEdit_){ for(auto& obj : data.objects){ if(obj.id == selectedObjectId_){ obj.moveRange = editMoveRange_; break; } } }
                    }
                    if(ImGui::InputFloat("Move Phase (0..1)", &editMovePhase_)){
                        if(editMovePhase_ < 0.0f) editMovePhase_ = 0.0f;
                        if(editMovePhase_ > 1.0f) editMovePhase_ = 1.0f;
                        if(liveEdit_){ for(auto& obj : data.objects){ if(obj.id == selectedObjectId_){ obj.movePhase = editMovePhase_; break; } } }
                    }
                }

                if(showFieldColor_){
                    if(ImGui::ColorEdit4("Color", &editColor_.x)){
                        if(liveEdit_){
                            for(auto& obj : data.objects){
                                if(obj.id == selectedObjectId_){
                                    obj.color = editColor_;
                                    stage_->SetInstanceColorById(obj.id, editColor_);
                                    break;
                                }
                            }
                        }
                    }
                }

                ImGui::Separator();
                if(showFieldModel_){
                    ImGui::Text("Model");
                    ImGui::InputText("Model Path", selectedModelBuf_, sizeof(selectedModelBuf_));
                }

                bool anyTransformField = showFieldPosition_ || showFieldRotation_ || showFieldScale_ || showFieldBlockType_ || showFieldColor_;

                if(anyTransformField && ImGui::Button("Apply Transform")){
                    stage_->UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                    for(auto& obj : data.objects){
                        if(obj.id == selectedObjectId_){
                            obj.position = editPosition_;
                            obj.rotation = editRotation_;
                            obj.scale = editScale_;
                            obj.blockId = editBlockId_;
                            obj.color = editColor_;
                            obj.hp = editHp_;
                            if(obj.blockId == BlockID::Warp){
                                obj.warpTargetPosition = editWarpTargetPosition_;
                                obj.warpTargetSceneId = editWarpTargetSceneId_;
                            }
                            if(obj.blockId == BlockID::MovingPlatform){
                                obj.moveDirection = editMoveDirection_;
                                obj.moveSpeed = editMoveSpeed_;
                                obj.moveRange = editMoveRange_;
                        obj.movePhase = editMovePhase_;
                            }
                            break;
                        }
                    }
                    SaveHistorySnapshot();
                    stage_->SetInstanceColorById(selectedObjectId_, selectionHighlightColor_);
                }

                ImGui::SameLine();
                if(ImGui::Button("Apply Model")){
                    for(auto& obj : data.objects){
                        if(obj.id == selectedObjectId_){
                            obj.modelName = std::string(selectedModelBuf_);
                            stage_->UpdateOrCreateInstance(obj);
                            stage_->SetInstanceColorById(obj.id, selectionHighlightColor_);
                            break;
                        }
                    }
                    SaveHistorySnapshot();
                }

                ImGui::SameLine();
                if(ImGui::Button("Delete Selected")){
                data.objects.erase(
                    std::remove_if(data.objects.begin(), data.objects.end(),
                    [this](const StageObject& o){ return o.id == selectedObjectId_; }),
                    data.objects.end()
                );
                stage_->RemoveInstanceById(selectedObjectId_);
                ClearSelection();
                SaveHistorySnapshot();
                }

                ImGui::Separator();

                ImGui::InputInt("Duplicate Count", &duplicateCount_);
                if(duplicateCount_ < 1){
                    duplicateCount_ = 1;
                }

                ImGui::Checkbox("Use Half-Size Offset", &useHalfSizeOffset_);
                if(!useHalfSizeOffset_){
                    ImGui::DragFloat3("Duplicate Offset", &duplicateOffset_.x, 0.1f);
                } else{
                    ImGui::Text("Offset will use half of source scale");
                }

                if(ImGui::Button("Duplicate Selected")){
                    if(selectedObjectId_ != -1){
                        for(const auto& obj : data.objects){
                            if(obj.id == selectedObjectId_){
                                StageObject base = obj;
                                StageObject lastCreated;
                                Vector3 perStepOffset = useHalfSizeOffset_
                                    ? Vector3{ base.scale.x * 2.0f, 0.0f, 0.0f }
                                : duplicateOffset_;

                                for(int i = 0; i < duplicateCount_; ++i){
                                    StageObject newObj = base;
                                    newObj.id = nextId_++;
                                    newObj.position = newObj.position + perStepOffset * static_cast<float>(i + 1);
                                    newObj.color = base.color;
                        data.objects.push_back(newObj);
                        stage_->UpdateOrCreateInstance(newObj);
                                    lastCreated = newObj;
                                }

                                ClearSelection();
                                selectedObjectId_ = lastCreated.id;
                                stage_->SetInstanceColorById(
                                    selectedObjectId_,
                                    { lastCreated.color.x, lastCreated.color.y, lastCreated.color.z, selectionBlinkAlpha_ }
                                );
                                selectionLastBlinkTime_ = std::chrono::steady_clock::now();
                                selectionBlinkOn_ = true;
                                strncpy_s(selectedModelBuf_, sizeof(selectedModelBuf_), lastCreated.modelName.c_str(), _TRUNCATE);
                                SaveHistorySnapshot();
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    ImGui::End();

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("StageEditor Debug", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Instances: %zu", stage_->GetInstanceCount());
    ImGui::End();
#endif
}

std::string StageEditor::ResolveDisplayModelName(const StageObject& o) const{
    switch(o.blockId){
        case BlockID::BugSpawn:
            return "sphere.obj";
        case BlockID::PlayerSpawn:
            return "Cube.obj";
        default:
            return o.modelName;
    }
}

Vector3 StageEditor::ResolveDisplayScale(const StageObject& o) const{
    switch(o.blockId){
        case BlockID::BugSpawn:
            return { 0.35f, 0.35f, 0.35f };
        case BlockID::PlayerSpawn:
            return { 0.6f, 1.2f, 0.6f };
        default:
            return o.scale;
    }
}