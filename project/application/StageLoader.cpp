#include "StageLoader.h"
#include <algorithm>
#include "base/DirectXCommon.h"

/// <summary>
/// コンストラクタ
/// </summary>
StageLoader::StageLoader(Object3dCommon* objCommon, Camera* camera)
    : object3dCommon_(objCommon)
    , camera_(camera)
{
}

/// <summary>
/// StageData の内容を元に Object3d インスタンスを生成
/// </summary>
void StageLoader::CreateFromData(const StageData& data)
{
    // 差分更新のために既存インスタンスのマップを作成
    std::unordered_map<int, size_t> idToIndex;
    for (size_t i = 0; i < instances_.size(); ++i) {
        idToIndex[instances_[i].id] = i;
    }

    // 新しいインスタンスリストを作成
    std::vector<Instance> newInstances;
    // 事前にサイズを予約しておく（パフォーマンス向上のため）
    newInstances.reserve(data.objects.size());

    // データ内のオブジェクトをすべて処理
    for (const auto& o : data.objects) {
        // オブジェクトIDで既存インスタンスを検索
        auto it = idToIndex.find(o.id);
        // 既存インスタンスが見つかった場合は更新、見つからない場合は新規作成
        if (it != idToIndex.end()) {
            // 既存インスタンスを更新
            Instance inst = std::move(instances_[it->second]);

            // ID は同じなのでそのまま、オブジェクトの内容を更新
            if (inst.object) {
                inst.object->SetModel(o.modelName); // モデルは変更される可能性があるので毎回セットする
                inst.object->SetCamera(camera_); // カメラも毎回セットする必要があるかもしれない
                inst.object->SetTranslate(o.position); // 位置は毎回セットする必要がある
                inst.object->SetRotate(o.rotation); // 回転も毎回セットする必要がある
                inst.object->SetScale(o.scale); // 拡縮率も毎回セットする必要がある
                inst.object->SetColor(o.color); // 色も毎回セットする必要がある
                inst.object->Update(); // 更新も呼び出しておく
            }

            // 更新したインスタンスを新しいリストに追加
            newInstances.push_back(std::move(inst));

        } else {
            // 新規作成
            Instance inst;
            // オブジェクトIDをセット
            inst.id = o.id;
            // Object3d を生成して初期化
            inst.object = std::make_unique<Object3d>();
            // 初期化
            inst.object->Initialize(object3dCommon_);
            // データをセット
            inst.object->SetModel(o.modelName);
            // カメラもセットしておく
            inst.object->SetCamera(camera_);
            // 位置をセット
            inst.object->SetTranslate(o.position);
            // 回転をセット
            inst.object->SetRotate(o.rotation);
            // 拡縮率をセット
            inst.object->SetScale(o.scale);
            // 色をセット
            inst.object->SetColor(o.color);
            // 更新も呼び出しておく
            inst.object->Update();
            // 新規作成したインスタンスを新しいリストに追加
            newInstances.push_back(std::move(inst));
        }
    }

    // 消去されたオブジェクトを GPU の描画完了後に削除するためのリスト
    std::vector<std::unique_ptr<Object3d>> toDelete;
    for (auto &oldInst : instances_) {
        if (oldInst.object) {
            // 既存インスタンスのIDが新しいデータに存在しない場合は削除対象
            toDelete.push_back(std::move(oldInst.object));
        }
    }

    // 新しいインスタンスリストに置き換える
    instances_ = std::move(newInstances);

    // GPU の描画完了後に削除するためのリストが空でない場合は DirectXCommon にデフード削除を登録
    if (!toDelete.empty() && DirectXCommon::GetInstance()) {
        for (auto &ptr : toDelete) {
            // ptr を shared_ptr に移動してキャプチャすることで、lambda 内で安全にリソースを保持できるようにする
            std::shared_ptr<Object3d> sp = std::move(ptr);
            DirectXCommon::GetInstance()->EnqueueDeferredDeletion([sp]() mutable {
                // ここで sp がスコープから外れると、Object3d のデストラクタが呼び出される
                sp.reset();
            });
        }
    }
}

/// <summary>
/// StageObject の内容を元に Object3d インスタンスを更新・作成する
/// </summary>
void StageLoader::UpdateOrCreateInstance(const StageObject& o)
{
    // 既存を検索して更新
    for (auto& inst : instances_) {
        // オブジェクトIDで既存インスタンスを検索
        if (inst.id == o.id) {
            // ID は同じなのでそのまま、オブジェクトの内容を更新
            if (inst.object) {
                inst.object->SetModel(o.modelName); // モデルは変更される可能性があるので毎回セットする
                inst.object->SetTranslate(o.position); // 位置は毎回セットする必要がある
                inst.object->SetRotate(o.rotation); // 回転も毎回セットする必要がある
                inst.object->SetScale(o.scale); // 拡縮率も毎回セットする必要がある
                inst.object->SetColor(o.color); // 色も毎回セットする必要がある
                inst.object->Update(); // 更新も呼び出しておく
            }
            return;
        }
    }

    /// 既存インスタンスが見つからなかったので新規作成
    Instance inst;
    // オブジェクトIDをセット
    inst.id = o.id;
    // Object3d を生成して初期化
    inst.object = std::make_unique<Object3d>();
    // 初期化
    inst.object->Initialize(object3dCommon_);
    // データをセット
    inst.object->SetModel(o.modelName);
    // カメラもセットしておく
    inst.object->SetCamera(camera_);
    // 位置をセット
    inst.object->SetTranslate(o.position);
    // 回転をセット
    inst.object->SetRotate(o.rotation);
    // 拡縮率をセット
    inst.object->SetScale(o.scale);
    // 色をセット
    inst.object->SetColor(o.color);
    // 更新も呼び出しておく
    inst.object->Update();

    // 新規作成したインスタンスをリストに追加
    instances_.push_back(std::move(inst));
}

/// <summary>
/// ID でインスタンスを照合して削除
/// </summary>
void StageLoader::RemoveInstanceById(int id)
{
    // 削除対象のインスタンスを GPU の描画完了後に削除するためのリスト
    std::vector<std::unique_ptr<Object3d>> toDelete;
    // 指定IDのインスタンスを検索して削除対象リストに移動
    for (auto &inst : instances_) {
        if (inst.id == id && inst.object) {
            toDelete.push_back(std::move(inst.object));
        }
    }

    // インスタンスリストから指定IDのインスタンスを削除
    instances_.erase(std::remove_if(instances_.begin(), instances_.end(), [id](const Instance& a) { return a.id == id; }), instances_.end());

    // GPU の描画完了後に削除するためのリストが空でない場合は DirectXCommon にデフード削除を登録
    if (!toDelete.empty() && DirectXCommon::GetInstance()) {
        for (auto &ptr : toDelete) {
            std::shared_ptr<Object3d> sp = std::move(ptr);
            DirectXCommon::GetInstance()->EnqueueDeferredDeletion([sp]() mutable {
                sp.reset();
            });
        }
    }
}

/// <summary>
/// ID でインスタンスを照合して位置・回転・拡縮率を更新
/// </summary>
void StageLoader::UpdateInstanceTransform(int id, const Vector3& pos, const Vector3& rot, const Vector3& scale)
{
    // 指定IDのインスタンスを検索して更新
    for (auto& inst : instances_) {
        // オブジェクトIDで既存インスタンスを検索
        if (inst.id == id && inst.object) {
            // ID は同じなのでそのまま、位置・回転・拡縮率を更新
            inst.object->SetTranslate(pos);
            inst.object->SetRotate(rot);
            inst.object->SetScale(scale);
            // 更新も呼び出しておく
            inst.object->Update();
            return;
        }
    }
}

/// <summary>
/// 色を直接設定する（選択ハイライト用）
/// </summary>
void StageLoader::SetInstanceColorById(int id, const Vector4& color)
{
    // 指定IDのインスタンスを検索して更新
    for (auto& inst : instances_) {
        // オブジェクトIDで既存インスタンスを検索
        if (inst.id == id && inst.object) {
            // ID は同じなのでそのまま、色を更新
            inst.object->SetColor(color);
            return;
        }
    }
}

/// <summary>
/// すべてのインスタンスを破棄
/// </summary>
void StageLoader::Clear()
{
    // すべてのインスタンスを GPU の描画完了後に削除するためのリスト
    std::vector<std::unique_ptr<Object3d>> toDelete;
    for (auto &inst : instances_) {
        if (inst.object) toDelete.push_back(std::move(inst.object));
    }
    instances_.clear();

    // GPU の描画完了後に削除するためのリストが空でない場合は DirectXCommon にデフード削除を登録
    if (!toDelete.empty() && DirectXCommon::GetInstance()) {
        for (auto &ptr : toDelete) {
            std::shared_ptr<Object3d> sp = std::move(ptr);
            DirectXCommon::GetInstance()->EnqueueDeferredDeletion([sp]() mutable {
                sp.reset();
            });
        }
    }
}

/// <summary>
/// 管理している全インスタンスの Update と Draw を呼び出す
/// </summary>
void StageLoader::DrawAndUpdate()
{
    // すべてのインスタンスに対して Update と Draw を呼び出す
    for (auto& i : instances_) {
        if (i.object) {
            // 更新と描画を呼び出す
            i.object->Update();
            i.object->Draw();
        }
    }
}

void StageLoader::DrawOpaqueAndUpdate()
{
    for (auto& i : instances_) {
        if (!i.object) {
            continue;
        }

        const Vector4 color = i.object->GetColor();

        // 透過しているものは後で描く
        if (color.w < 0.999f) {
            continue;
        }

        i.object->Update();
        i.object->Draw();
    }
}

void StageLoader::DrawTransparentSortedAndUpdate(const Vector3& cameraPos)
{
    struct DrawItem {
        Instance* instance = nullptr;
        float distanceSq = 0.0f;
    };

    std::vector<DrawItem> drawItems;
    drawItems.reserve(instances_.size());

    for (auto& i : instances_) {
        if (!i.object) {
            continue;
        }

        const Vector4 color = i.object->GetColor();

        // 不透明は先に描いているのでここでは描かない
        if (color.w >= 0.999f) {
            continue;
        }

        const Vector3 pos = i.object->GetTranslate();
        const float dx = pos.x - cameraPos.x;
        const float dy = pos.y - cameraPos.y;
        const float dz = pos.z - cameraPos.z;

        drawItems.push_back({
            &i,
            dx * dx + dy * dy + dz * dz
            });
    }

    // 透過はカメラから遠い順
    std::sort(
        drawItems.begin(),
        drawItems.end(),
        [](const DrawItem& a, const DrawItem& b) {
            return a.distanceSq > b.distanceSq;
        }
    );

    for (auto& item : drawItems) {
        if (!item.instance || !item.instance->object) {
            continue;
        }

        item.instance->object->Update();
        item.instance->object->Draw();
    }
}

/// <summary>
/// Debug: インスタンス数を返す
/// </summary>
size_t StageLoader::GetInstanceCount() const
{
    return instances_.size();
}
