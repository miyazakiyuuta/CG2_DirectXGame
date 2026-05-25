#pragma once
#include "../../../engine/math/Vector3.h"
#include "../../../engine/utility/CollisionUtility.h"
#include "math/Vector4.h"
#include <memory>
#include <vector>
#include <cstring>
#include "../../Ability.h"
#include <random>
#include <unordered_map>

class Object3d;
class Object3dCommon;
class Camera;
class Player;

class BaseEnemy {
public:
    BaseEnemy();
    virtual ~BaseEnemy();

    virtual void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) = 0;
    virtual void Update(float deltaTime, const Vector3& playerPos) = 0;
    virtual void Draw() = 0;

    void SetBlockColliders(const std::vector<CollisionUtility::OBB>* colliders) { blockColliders_ = colliders; }

    void SetKeepInsideCylinder(const CollisionUtility::Cylinder* cylinder){ keepInsideCylinder_ = cylinder; }

    const Vector3& GetPosition() const { return position_; }
    bool IsDead() const { return isDead_; }
    virtual void Kill() { isDead_ = true; } // 仮想関数に変更

    // 当たり判定の部位（パーツ）情報を表す構造体
    struct TargetPart {
        Vector3 position;
        CollisionUtility::OBB obb;
        int partId;
    };

    // この敵が持つ当たり判定のリストを取得する（デフォルトは自身全体の1つのみ）
    virtual std::vector<TargetPart> GetTargetParts(float radius) const {
        return { { position_, GetOBB(position_, radius), 0 } };
    }

    // 指定した部位を破壊（撃破）する（デフォルトは敵全体を撃破）
    virtual void KillPart(int partId) {
        (void)partId;
        Kill();
    }

    // 色設定ヘルパー：派生クラスは Initialize 内で SetColor を呼ぶこと
    void SetColor(const Vector4& color);
    void SetAlpha(float a);
    // 元のアルファ値を返す（SetColor 呼び出し時に保存される）
    float GetOriginalAlpha() const;

    // プレイヤーへの速度補正値を取得
    float GetPlayerSpeedMultiplier() const { return playerSpeedMultiplier_; }

    // 4. マネージャーからプレイヤー情報を受け取るための追加
    void SetPlayer(class Player* p) { player_ = p; }

    // 能力 XP のドロップ定義
    struct DropEntry {
        AbilityId ability = AbilityId::Unknown;
        float weight = 1.0f; // sampling weight
        int minAmount = 1;
        int maxAmount = 1;
        float chance = 1.0f; // per-entry chance (0..1)
    };

    // この敵インスタンス用のドロップテーブルを設定
    void SetDropTable(const std::vector<DropEntry>& table) { dropTable_ = table; }
    // 添付されたプレイヤーへドロップ配布（死亡時に呼ばれる）
    // 戻り値: 配布した総ドロップ量（整数）。0 の場合はドロップ無し。
    int DistributeDrops();
    AbilityId GetLastDropAbility() const { return lastDropAbility_; }

    // ソナー（エコー）が当たった時に呼ばれる
    virtual void OnSonarHit() {}

    // 舌で掴める（フックできる）対象かどうかを返す。デフォルトは false。
    // フェイズゴーストなどの特殊な敵だけ、状態に合わせて true を返すようにオーバーライドする。
    virtual bool IsGrappable() const { return false; }

    // 【修正】当たり判定用のOBB取得を public に移動し、Playerから参照可能にする
    virtual CollisionUtility::OBB GetOBB(const Vector3& pos, float radius) const;

    // 舌が当たった時の通知（当たった方向を受け取る）
    virtual void OnTongueHit(const Vector3& direction) { (void)direction; }

    // 慣性ジャンプ用に現在の移動速度を取得する
    virtual Vector3 GetVelocity() const { return velocity_; }

protected:
    // 横方向の衝突解決（接地中だけ足場判定を行う）
    void ResolveHorizontalCollisions(const Vector3& previousPosition);
    void ResolveVerticalCollisions();

    // 横方向衝突判定（isOnGround が true のときだけ足場チェック）
    // 現在乗っているブロックの範囲から外れたら横移動をキャンセルする
    // isOnGround が true かつ着地ブロックが記録済みの場合のみ XZ 範囲制限を行う
    void ResolveHorizontalCollisionsForPos(Vector3& pos,
                                            const Vector3& prevPos,
                                            float radius,
                                            bool isOnGround);

    // 縦方向衝突判定（着地時にブロック XZ 範囲を記録、ジャンプ時にクリア）
    void ResolveVerticalCollisionsForPos(Vector3& pos, Vector3& vel, float collisionRadius, float visualRadius, bool& outOnGround);

    // 接地判定ユーティリティ（足元にブロックがあれば true）
    bool HasGroundBelow(const Vector3& pos, float checkHeight = 0.4f) const;

    // --------------------------------------------------------
    // 着地ブロック外部管理用（ClusterSlime など複数体管理クラス向け）
    // 各メンバーごとに着地ブロックを保存・復元するために使用する
    // --------------------------------------------------------
    // 着地ブロックが記録されているか・ XZ + 上面 Y を取得する
    bool HasLandingBlock() const { return hasLandingBlock_; }
    void GetLandingBlockRange(float& minX, float& maxX, float& minZ, float& maxZ, float& topY) const {
        minX = landingBlockMinX_; maxX = landingBlockMaxX_;
        minZ = landingBlockMinZ_; maxZ = landingBlockMaxZ_;
        topY = landingBlockTopY_;
    }
    // 着地ブロックを外部から設定する（メンバー切り替え時に前のメンバーの情報を復元）
    void SetLandingBlockRange(bool has, float minX, float maxX, float minZ, float maxZ, float topY) {
        hasLandingBlock_ = has;
        landingBlockMinX_ = minX; landingBlockMaxX_ = maxX;
        landingBlockMinZ_ = minZ; landingBlockMaxZ_ = maxZ;
        landingBlockTopY_ = topY;
    }
    // 着地ブロックをクリアする
    void ClearLandingBlock() { hasLandingBlock_ = false; }

protected:
    std::unique_ptr<Object3d> object_ = nullptr;
    Vector4 originalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 position_ = {0.0f, 0.0f, 0.0f};
    Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
    bool isDead_ = false;
    bool isOnGround_ = false;

    const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;

    const CollisionUtility::Cylinder* keepInsideCylinder_ = nullptr;

    float gravity_ = -0.04f;
    float groundY_ = 0.0f;

    // プレイヤーの移動速度倍率（1.0 が通常）
    float playerSpeedMultiplier_ = 1.0f;

    class Player* player_ = nullptr;
    std::vector<DropEntry> dropTable_;
    AbilityId lastDropAbility_ = AbilityId::Unknown;

    // 着地ブロック記録方式：着地した瞬間のブロックの XYZ 範囲を保持する
    // ジャンプ中は無効化し、接地中のみ横移動制限に使用する
    bool hasLandingBlock_ = false;      // 着地ブロックが記録されているか
    float landingBlockMinX_ = 0.0f;     // 着地ブロックの X 最小値
    float landingBlockMaxX_ = 0.0f;     // 着地ブロックの X 最大値
    float landingBlockMinZ_ = 0.0f;     // 着地ブロックの Z 最小値
    float landingBlockMaxZ_ = 0.0f;     // 着地ブロックの Z 最大値
    float landingBlockTopY_ = 0.0f;     // 着地ブロックの上面 Y 座標（プレイヤーが実際に赤に超えているかの判定に使う）
};