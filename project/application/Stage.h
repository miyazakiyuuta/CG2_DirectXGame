#pragma once
#define NOMINMAX

#include <memory>
#include <optional>
#include <vector>

#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "utility/CollisionUtility.h"

#include "StageTypes.h"
#include "StageLoader.h"
#include <unordered_map>

class Stage{
public:
    Stage(Object3dCommon* objCommon, Camera* camera);

    void SetStageData(const StageData& data);
    StageData& GetStageData();
    const StageData& GetStageData() const;

    void RefreshInstances();
    void Draw();
    // Update runtime behaviors (moving platforms, etc.)
    void Update(float deltaTime);
    // Consume and return platform movement deltas computed during Update
    std::unordered_map<int, Vector3> ConsumePlatformDeltas();
    void Clear();

    void UpdateOrCreateInstance(const StageObject& o);
    void RemoveInstanceById(int id);
    void UpdateInstanceTransform(int id, const Vector3& pos, const Vector3& rot, const Vector3& scale);
    void SetInstanceColorById(int id, const Vector4& color);
    size_t GetInstanceCount() const;

    std::vector<CollisionUtility::AABB> GetBlockAABBs() const;
    std::vector<CollisionUtility::AABB> GetWaterBlockAABBs() const;
    std::vector<CollisionUtility::OBB> GetBlockOBBs() const;
    std::vector<CollisionUtility::OBB> GetWaterBlockOBBs() const;
    std::vector<Vector3> GetBugSpawnPositions() const;
    std::optional<Vector3> GetPlayerSpawnPosition() const;
    std::vector<EnemySpawnPoint> GetEnemySpawnPoints() const;

    // ダメージ判定: 指定した球に当たった破壊可能ブロックにダメージを与える
    void ApplyDamageAtSphere(const CollisionUtility::Sphere& sphere, int damage);

private:
    StageData data_;
    StageLoader loader_;
    // Runtime movement deltas for moving platforms (filled by Update)
    std::unordered_map<int, Vector3> platformMoveDeltas_;
};