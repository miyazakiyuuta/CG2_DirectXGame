#pragma once
#define NOMINMAX

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "3d/Camera.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "io/Input.h"
#include "math/Vector3.h"
#include <../externals/nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <unordered_map>

/// <summary>
/// ステージ内のオブジェクトを表す構造体
/// </summary>
struct StageObject {

    // 一意の識別ID
    // ・削除対象判定
    // ・ロード後の整合性保持
    // ・将来的な選択機能追加時に利用
    int id = -1;

    // このオブジェクトが使用するモデルファイル名
    // 例: "Cube.obj"
    std::string modelName;

    // ワールド座標
    Vector3 position {};

    // オイラー角（ラジアン想定）
    Vector3 rotation {};

    // 拡縮率（初期値は等倍）
    Vector3 scale { 1.0f, 1.0f, 1.0f };
};

/// <summary>
/// ステージ全体のデータ構造
/// </summary>
struct StageData {

    // ステージ名（ファイル名UI表示用）
    std::string name;

    // ステージ内の全オブジェクト
    std::vector<StageObject> objects;
};

/// <summary>
/// StageData を JSON 形式で保存・読み込みするクラス
/// </summary>
class StageSerializer {
public:
    /// <summary>
    /// JSONファイルに StageData を保存
    /// </summary>
    static bool SaveToFile(const StageData& data, const std::string& path)
    {
        // nlohmann::json を使って保存

        // JSONオブジェクトを構築
        nlohmann::json j;
        // ステージ名を保存
        j["name"] = data.name;

        // オブジェクト一覧を保存
        for (const auto& o : data.objects) {
            // オブジェクトごとにJSONオブジェクトを作成
            nlohmann::json jo;
            // ID、モデル名、位置、回転、拡縮率を保存
            jo["id"] = o.id; // ID を保存
            jo["modelName"] = o.modelName; // モデル名を保存
            nlohmann::json pos;
            pos["x"] = o.position.x;
            pos["y"] = o.position.y;
            pos["z"] = o.position.z;
            nlohmann::json rot;
            rot["x"] = o.rotation.x;
            rot["y"] = o.rotation.y;
            rot["z"] = o.rotation.z;
            nlohmann::json scl;
            scl["x"] = o.scale.x;
            scl["y"] = o.scale.y;
            scl["z"] = o.scale.z;

            // 位置、回転、拡縮率をそれぞれJSONオブジェクトとして保存
            jo["position"] = pos; // 位置を保存
            jo["rotation"] = rot; // 回転を保存
            jo["scale"] = scl; // 拡縮率を保存
            j["objects"].push_back(jo); // オブジェクトを配列に追加
        }

        // ファイルに保存
        std::ofstream ofs(path);
        // ファイルが開けない場合は失敗
        if (!ofs.is_open()) {
            return false;
        }

        // JSONオブジェクトをファイルに書き込む（インデント2スペースで整形）
        ofs << std::setw(2) << j << std::endl;

        // 書き込み成功
        return true;
    }

    /// <summary>
    /// JSONファイルから StageData を読み込み
    /// </summary>
    static std::optional<StageData> LoadFromFile(const std::string& path)
    {
        // nlohmann::json を使って読み込み
        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }

        // ファイルを開く
        std::ifstream ifs(path);
        // ファイルが開けない場合は失敗
        if (!ifs.is_open()) {
            return std::nullopt;
        }

        // JSONオブジェクトを読み込む
        try {
            // JSONオブジェクトを構築
            nlohmann::json j;
            ifs >> j;
            // JSONオブジェクトから StageData を構築
            StageData data;

            // ステージ名を読み込む
            if (j.contains("name")) {
                data.name = j["name"].get<std::string>();
            }

            // オブジェクト一覧を読み込む
            if (j.contains("objects")) {
                // オブジェクト配列をループして StageObject を構築
                for (auto& jo : j["objects"]) {
                    // JSONオブジェクトから StageObject を構築
                    StageObject o;

                    // ID はオプションとする（デフォルトは-1のまま）
                    if (jo.contains("id")) {
                        o.id = jo["id"].get<int>();
                    }

                    // モデル名は必須とする
                    if (jo.contains("modelName")) {
                        o.modelName = jo["modelName"].get<std::string>();
                    }

                    // 位置はオプションとする（デフォルトは原点のまま）
                    if (jo.contains("position")) {
                        o.position.x = jo["position"]["x"].get<float>();
                        o.position.y = jo["position"]["y"].get<float>();
                        o.position.z = jo["position"]["z"].get<float>();
                    }

                    // 回転はオプションとする（デフォルトは0のまま）
                    if (jo.contains("rotation")) {
                        o.rotation.x = jo["rotation"]["x"].get<float>();
                        o.rotation.y = jo["rotation"]["y"].get<float>();
                        o.rotation.z = jo["rotation"]["z"].get<float>();
                    }

                    // 拡縮率はオプションとする（デフォルトは等倍のまま）
                    if (jo.contains("scale")) {
                        o.scale.x = jo["scale"]["x"].get<float>();
                        o.scale.y = jo["scale"]["y"].get<float>();
                        o.scale.z = jo["scale"]["z"].get<float>();
                    }

                    // 構築した StageObject を StageData に追加
                    data.objects.push_back(o);
                }
            }

            return data; // 読み込み成功

        } catch (...) {
            return std::nullopt; // 例外が発生した場合は失敗
        }
    }
};

/// <summary>
/// StageData の内容を元に Object3d インスタンスを生成・管理するクラス
/// </summary>
class StageLoader {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    StageLoader(Object3dCommon* objCommon, Camera* camera)
        : object3dCommon_(objCommon)
        , camera_(camera)
    {
    }

    /// <summary>
    /// StageData の内容を元に Object3d インスタンスを生成
    /// </summary>
    void CreateFromData(const StageData& data)
    {
        // 差分更新: 既存インスタンスを ID で照合し、更新・追加・削除を行う
        // マップを作成して既存インスタンスを参照しやすくする
        std::unordered_map<int, size_t> idToIndex;
        for (size_t i = 0; i < instances_.size(); ++i) {
            idToIndex[instances_[i].id] = i;
        }

        // 新しい一覧を保持
        std::vector<Instance> newInstances;
        newInstances.reserve(data.objects.size());

        for (const auto& o : data.objects) {
            auto it = idToIndex.find(o.id);
            if (it != idToIndex.end()) {
                // 既存インスタンスがある -> 更新
                Instance inst = std::move(instances_[it->second]);
                if (inst.object) {
                    inst.object->SetModel(o.modelName); // モデルは変更する可能性があるので毎回セットする
                    inst.object->SetCamera(camera_); // カメラも毎回セットする
                    inst.object->SetTranslate(o.position); // 位置は毎回セットする
                    inst.object->SetRotate(o.rotation); // 回転は毎回セットする
                    inst.object->SetScale(o.scale); // 拡縮率は毎回セットする
                    inst.object->Update(); // ワールド行列などの更新を行う
                }

                // 更新したインスタンスを新しい一覧に追加
                newInstances.push_back(std::move(inst));

            } else {
                // 新規作成
                Instance inst;
                inst.id = o.id; // ID をセット
                inst.object = std::make_unique<Object3d>(); // Object3d を生成
                inst.object->Initialize(object3dCommon_); // Object3d を初期化
                inst.object->SetModel(o.modelName); // モデルをセット
                inst.object->SetCamera(camera_); // カメラをセット
                inst.object->SetTranslate(o.position); // 位置をセット
                inst.object->SetRotate(o.rotation); // 回転をセット
                inst.object->SetScale(o.scale); // 拡縮率をセット
                inst.object->Update(); // ワールド行列などの更新を行う

                // 新しいインスタンスを新しい一覧に追加
                newInstances.push_back(std::move(inst));
            }
        }

        // 置き換え
        instances_ = std::move(newInstances);
    }

    /// <summary>
    /// StageObject の内容を元に Object3d インスタンスを更新・作成する
    /// </summary>
    void UpdateOrCreateInstance(const StageObject& o)
    {

        // ID で既存インスタンスを照合
        for (auto& inst : instances_) {
            // ID が一致するインスタンスが見つかった場合は更新して終了
            if (inst.id == o.id) {
                // モデル名、位置、回転、拡縮率を更新
                if (inst.object) {
                    inst.object->SetModel(o.modelName); // モデルは変更する可能性があるので毎回セットする
                    inst.object->SetTranslate(o.position); // 位置は毎回セットする
                    inst.object->SetRotate(o.rotation); // 回転は毎回セットする
                    inst.object->SetScale(o.scale); // 拡縮率は毎回セットする
                    inst.object->Update(); // ワールド行列などの更新を行う
                }

                return;
            }
        }

        // 一致するインスタンスが見つからなかった場合は新規作成
        Instance inst; // 新しいインスタンスを作成
        inst.id = o.id; // ID をセット
        inst.object = std::make_unique<Object3d>(); // Object3d を生成
        inst.object->Initialize(object3dCommon_); // Object3d を初期化
        inst.object->SetModel(o.modelName); // モデルをセット
        inst.object->SetCamera(camera_); // カメラをセット
        inst.object->SetTranslate(o.position); // 位置をセット
        inst.object->SetRotate(o.rotation); // 回転をセット
        inst.object->SetScale(o.scale); // 拡縮率をセット
        inst.object->Update(); // ワールド行列などの更新を行う

        // 新しいインスタンスを一覧に追加
        instances_.push_back(std::move(inst));
    }

    /// <summary>
    /// ID でインスタンスを照合して削除
    /// </summary>
    void RemoveInstanceById(int id)
    {
        instances_.erase(std::remove_if(instances_.begin(), instances_.end(), [id](const Instance& a) { return a.id == id; }), instances_.end());
    }

    /// <summary>
    /// ID でインスタンスを照合して位置・回転・拡縮率を更新
    /// </summary>
    void UpdateInstanceTransform(int id, const Vector3& pos, const Vector3& rot, const Vector3& scale)
    {
      
        for (auto& inst : instances_) {
            // ID で既存インスタンスを照合
            if (inst.id == id && inst.object) {
                // ID が一致するインスタンスが見つかった場合は位置・回転・拡縮率を更新
                inst.object->SetTranslate(pos); 
                inst.object->SetRotate(rot);
                inst.object->SetScale(scale);
                inst.object->Update();

                return;
            }
        }
    }

    /// <summary>
    /// すべてのインスタンスを破棄
    /// </summary>
    void Clear()
    {
        // unique_ptr なので clear するだけで全オブジェクトは自動的に破棄される
        instances_.clear();
    }

    /// <summary>
    /// 管理している全インスタンスの Update と Draw を呼び出す
    /// </summary>
    void DrawAndUpdate()
    {
        // すべてのインスタンスに対して Update と Draw を呼び出す
        for (auto& i : instances_) {
            // オブジェクトが有効な場合のみ Update と Draw を呼び出す
            if (i.object) {
                // ワールド行列などの更新
                i.object->Update();
                // 描画呼び出し
                i.object->Draw();
            }
        }
    }

    // Debug: インスタンス数を返す
    size_t GetInstanceCount() const { return instances_.size(); }

private:
    /// <summary>
    /// 管理するオブジェクトインスタンスの構造体
    /// </summary>
    struct Instance {
        int id = -1; // オブジェクトID（StageObjectのIDと対応させる）
        std::unique_ptr<Object3d> object; // 描画用オブジェクト
    };

    // Object3d初期化用
    Object3dCommon* object3dCommon_ = nullptr;

    // 描画時に使用するカメラ
    Camera* camera_ = nullptr;

    // 現在描画中のオブジェクト一覧
    std::vector<Instance> instances_;
};

/// <summary>
/// エディタの入力制御クラス
/// </summary>
class EditorController {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    EditorController(Camera* camera)
        : camera_(camera)
    {
    }

    /// <summary>
    /// カメラの前方にオブジェクトを生成するための位置を計算して返す
    /// </summary>
    Vector3 GetCreatePosition(float distance = 5.0f) const
    {
        if (!camera_) {
            return {};
        }

        // カメラの位置と回転を取得
        Vector3 pos = camera_->GetTranslate(); // カメラの位置を取得
        Vector3 rot = camera_->GetRotate(); // カメラの回転を取得

        // pitch(x) と yaw(y) を使用
        float cp = std::cos(rot.x); // pitchのcos
        float sp = std::sin(rot.x); // pitchのsin
        float cy = std::cos(rot.y); // yawのcos
        float sy = std::sin(rot.y); // yawのsin

        // カメラの前方ベクトルを計算
        Vector3 forward = {
            -sy * cp,
            sp,
            -cy * cp
        };

        // 前方ベクトルを距離分だけ伸ばして、カメラ位置に加算して生成位置を計算
        return pos + forward * distance;
    }

    /// <summary>
    /// 生成キー（マウス右クリック）が押されたか
    /// </summary>
    bool IsCreateKeyPressed() const
    {
        // 右クリックのトリガーをチェック
        return Input::GetInstance()->IsTriggerMouse(1);
    }

    /// <summary>
    /// 削除キー（Delete）が押されたか
    /// </summary>
    /// <returns></returns>
    bool IsDeleteKeyPressed() const
    {
        // Deleteキーのトリガーをチェック
        return Input::GetInstance()->IsTriggerKey(DIK_DELETE);
    }

private:
    // 描画時に使用するカメラ
    Camera* camera_ = nullptr;
};

/// <summary>
/// ステージエディタのメインクラス
/// </summary>
class StageEditor {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    StageEditor(Object3dCommon* objCommon, Camera* camera)
        : loader_(objCommon, camera)
        , controller_(camera)
    {
        // Object3dCommon と Camera を StageLoader に渡して初期化
        object3dCommon_ = objCommon;
        camera_ = camera;
    }

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(const std::string& defaultModel)
    {
        defaultModel_ = defaultModel; // デフォルトモデル名を保存
        // defaultModelBuf_ に初期文字列をコピー（安全な strcpy_s を使用）
        strcpy_s(defaultModelBuf_, sizeof(defaultModelBuf_), defaultModel_.c_str());
        data_.name = "stage"; // ステージ名の初期値を設定
    }

    /// <summary>
    /// 更新処理（入力チェックとオブジェクト生成・削除）
    /// </summary>
    void Update()
    {
        // 生成・削除などの入力処理は編集モード時のみ行う
        if (isEditMode_) {
            // 生成処理（右クリックでカメラ前方にオブジェクトを生成）
            if (controller_.IsCreateKeyPressed()) {
                StageObject o;
                // 一意ID付与
                o.id = nextId_++; // 生成するたびにIDをインクリメントして割り当てる
                // デフォルトモデルを設定
                o.modelName = defaultModel_;
                // カメラ前方に配置
                o.position = controller_.GetCreatePosition();
                // 回転はゼロ、拡縮は等倍のまま
                data_.objects.push_back(o);
                // 変更を反映してオブジェクトを再生成
                loader_.UpdateOrCreateInstance(o);
            }

            // 削除処理（Deleteキーで最後に生成したオブジェクトを削除）
            if (controller_.IsDeleteKeyPressed()) {
                // オブジェクトが存在する場合のみ削除処理を行う
                if (!data_.objects.empty()) {
                    // 最後のオブジェクトを削除
                    StageObject removed = data_.objects.back();
                    data_.objects.pop_back();
                    // 削除インスタンスのみ破棄
                    loader_.RemoveInstanceById(removed.id);
                }
            }
        }

        // ImGuiウィンドウは常に描画して、編集モードの切り替えやステージ確認を可能にする
        DrawImGui(); // ImGuiの描画呼び出し
    }

    /// <summary>
    /// 描画処理（StageLoaderの描画呼び出し）
    /// </summary>
    void Draw()
    {
        // すべてのオブジェクトの描画を呼び出す
        loader_.DrawAndUpdate();
    }

    /// <summary>
    /// 編集モードの切り替え
    /// </summary>
    void ToggleEditMode() { isEditMode_ = !isEditMode_; }

    /// <summary>
    /// 編集モードかどうかを返す
    /// </summary>
    bool IsEditMode() const { return isEditMode_; }

    /// <summary>
    /// JSONファイルにステージデータを保存
    /// </summary>
    bool Save(const std::string& path)
    {
        // StageSerializer を使用して現在のステージデータを指定されたパスに保存する
        return StageSerializer::SaveToFile(data_, path);
    }

    /// <summary>
    /// JSONファイルからステージデータを読み込み、オブジェクトを生成
    /// </summary>
    bool Load(const std::string& path)
    {
        // StageSerializer を使用して指定されたパスからステージデータを読み込む
        auto d = StageSerializer::LoadFromFile(path);

        // 読み込み失敗（ファイル無しや解析エラーなど）の場合は false を返す
        if (!d) {
            return false;
        }

        // 読み込んだデータを現在のステージデータにコピー
        data_ = *d;

        // 読み込んだデータ内のオブジェクトのIDを確認して、次に付与するIDを決定する
        int maxId = 0;
        // 読み込んだオブジェクトのIDをすべて確認して、最大のIDを見つける
        for (auto& o : data_.objects) {
            // 現在のオブジェクトのIDとこれまでの最大IDを比較して、より大きい方を新しい最大IDとして保持する
            if (o.id > maxId)
                maxId = o.id;
        }

        // 読み込んだデータのIDを考慮して、次に付与するIDを最大IDの次の値に設定する
        nextId_ = maxId + 1;
        // 読み込んだデータを元にオブジェクトを生成
        loader_.CreateFromData(data_);

        // 読み込み成功
        return true;
    }

    /// <summary>
    /// ステージデータとオブジェクトをすべてクリア
    /// </summary>
    void Clear()
    {
        // ステージデータ内のオブジェクト配列をクリアして空にする
        data_.objects.clear();
        // オブジェクト管理クラスのインスタンスもすべて破棄してクリアする
        loader_.Clear();
    }

    /// <summary>
    /// ステージデータへの参照を返す（外部から直接データを操作したい場合に使用）
    /// </summary>
    StageData& GetStageData() { return data_; }

private:
    /// <summary>
    /// ImGuiを使用して編集モードのUIを描画する関数
    /// </summary>
    void DrawImGui()
    {
        // ImGuiウィンドウの開始
        ImGui::Begin("StageEditor");

        // 編集モード切り替えボタン
        if (ImGui::Button(isEditMode_ ? "Exit Edit Mode" : "Enter Edit Mode")) {
            // ボタンが押されたら編集モードを切り替える
            ToggleEditMode();
        }

        // クリアボタン
        ImGui::SameLine();

        // クリアボタンが押されたらステージデータとオブジェクトをすべてクリアする
        if (ImGui::Button("Clear")) {
            // すべてのデータとオブジェクトをクリアする
            Clear();
        }

        // 区切り線を描画してUIを整理する
        ImGui::Separator();

        // デフォルトモデル名の編集（安全な固定長バッファを使用）
        if (ImGui::InputText("Default Model", defaultModelBuf_, sizeof(defaultModelBuf_))) {
            // 変更があった場合のみ std::string に反映
            defaultModel_ = std::string(defaultModelBuf_);
        }

        // 簡易作成ボタン（右クリックでも作成できます）
        ImGui::SameLine();
        if (ImGui::Button("Create")) {
            StageObject o;
            o.id = nextId_++;
            o.modelName = defaultModel_;
            o.position = controller_.GetCreatePosition();
            data_.objects.push_back(o);
            loader_.UpdateOrCreateInstance(o);
        }

        // ヘルプ表示
        ImGui::Text("Create: Right-click in viewport or press Create button.");
        ImGui::Text("Delete: Press Delete key or use 'Delete Selected' in the list.");

        // オブジェクト数の表示
        ImGui::Text("Object Count: %d", (int)data_.objects.size());
        if (ImGui::Button("Save")) {
            // Saveボタンが押されたら現在のステージデータを "stage.json" というファイル名で保存する
            Save("stage.json");
        }

        // Saveボタンと Loadボタンを同じ行に配置するために SameLine を呼び出す
        ImGui::SameLine();

        // Loadボタンが押されたら "stage.json" というファイルからステージデータを読み込む
        if (ImGui::Button("Load")) {
            // Load関数は成功・失敗に関わらずステージデータを更新する可能性があるため、ここでは戻り値をチェックせずに呼び出す
            Load("stage.json");
        }

        // 選択・編集パネル
        ImGui::Separator();
        ImGui::Text("Selection");
        // オブジェクト一覧から選択できるリストを表示
        ImGui::BeginChild("ObjectList", ImVec2(0, 200), true);
        for (const auto& o : data_.objects) {
            char label[128];
            snprintf(label, sizeof(label), "id:%d model:%s", o.id, o.modelName.c_str());
            bool isSelected = (selectedObjectId_ == o.id);
            if (ImGui::Selectable(label, isSelected)) {
                // 選択状態のトグル
                selectedObjectId_ = o.id;
                // 選択内容を編集バッファにコピー
                editPosition_ = o.position;
                editRotation_ = o.rotation;
                editScale_ = o.scale;
            }
        }
        ImGui::EndChild();

        if (selectedObjectId_ == -1) {
            ImGui::Text("No object selected");
        } else {
            ImGui::Text("Selected ID: %d", selectedObjectId_);
            // 編集バッファを表示・編集
            ImGui::DragFloat3("Position", &editPosition_.x, 0.1f);
            ImGui::DragFloat3("Rotation", &editRotation_.x, 0.01f);
            ImGui::DragFloat3("Scale", &editScale_.x, 0.01f, 0.01f, 100.0f);
            if (ImGui::Button("Apply Transform")) {
                // 編集内容を反映
                loader_.UpdateInstanceTransform(selectedObjectId_, editPosition_, editRotation_, editScale_);
                // データモデルにも反映
                for (auto& obj : data_.objects) {
                    if (obj.id == selectedObjectId_) {
                        obj.position = editPosition_;
                        obj.rotation = editRotation_;
                        obj.scale = editScale_;
                        break;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Selected")) {
                // データ削除
                data_.objects.erase(std::remove_if(data_.objects.begin(), data_.objects.end(), [this](const StageObject& o) { return o.id == selectedObjectId_; }), data_.objects.end());
                // インスタンス削除
                loader_.RemoveInstanceById(selectedObjectId_);
                selectedObjectId_ = -1;
            }
        }

        // ImGuiウィンドウの終了
        ImGui::End();

        // Debug: インスタンス数表示（下に小さく表示）
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("StageEditor Debug", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Instances: %zu", loader_.GetInstanceCount());
        ImGui::End();
    }

    // 保存データ本体
    StageData data_;

    // 描画インスタンス管理
    StageLoader loader_;

    // 入力処理
    EditorController controller_;

    // Object3d初期化用
    Object3dCommon* object3dCommon_ = nullptr;

    // 描画時に使用するカメラ
    Camera* camera_ = nullptr;

    // 新規生成オブジェクトのデフォルトモデル名
    std::string defaultModel_ = "Cube.obj"; // デフォルトモデル名
    // ImGui 用固定長バッファ: std::string の内部バッファを直接渡さない安全な実装
    char defaultModelBuf_[256] = {};

    // 次に付与するID
    int nextId_ = 1;

    // 編集モード状態
    bool isEditMode_ = false;
    // 現在選択中のオブジェクトID（編集用）
    int selectedObjectId_ = -1;
    // 選択中オブジェクトの編集用ワークバッファ
    Vector3 editPosition_ {}; // 直接編集はせず、選択時にコピーして編集後に反映する方式
    Vector3 editRotation_ {}; // 直接編集はせず、選択時にコピーして編集後に反映する方式
    Vector3 editScale_ { 1.0f, 1.0f, 1.0f }; // 直接編集はせず、選択時にコピーして編集後に反映する方式
};