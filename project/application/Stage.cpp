#define NOMINMAX
#include "Stage.h"
#include "effect/ParticleManager.h"
#include "effect/ParticleConfig.h"
#include "utility/Logger.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

Stage::Stage(Object3dCommon* objCommon, Camera* camera)
    : loader_(objCommon, camera){
    data_.name = "stage";
}

void Stage::ApplyDamageAtSphere(const CollisionUtility::Sphere& sphere, int damage){
    // iterate over objects and find first breakable block that intersects sphere
    for(auto it = data_.objects.begin(); it != data_.objects.end(); ++it){
        const StageObject& o = *it;

        if(o.modelName != "Cube.obj") continue;
        if(o.blockId != BlockID::Breakable) continue;

        Transform t;
        t.translate = o.position;
        t.rotate = o.rotation;
        t.scale = o.scale;

        CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t, { 1.0f, 1.0f, 1.0f });
        auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(sphere, obb);
        if(!hit.hit) continue;

        // apply damage
        int newHp = o.hp - damage;

        int targetId = o.id;

        if(newHp <= 0){
            // remove from stage data and loader
            loader_.RemoveInstanceById(targetId);
            data_.objects.erase(it);

            // spawn simple particles (if group exists)
            ParticleConfig cfg;
            cfg.minScale = { 0.2f,0.2f,0.2f };
            cfg.maxScale = { 0.6f,0.6f,0.6f };
            cfg.minVelocity = { -1.0f, -1.0f, -1.0f };
            cfg.maxVelocity = { 1.0f, 1.5f, 1.0f };
            cfg.lifeTime = 1.0f;
            cfg.startColor = { 1.0f,1.0f,1.0f,1.0f };

            try{
                ParticleManager::GetInstance()->Emit("break", o.position, cfg, 16);
            } catch(...) {
                // if particle group not found, ignore
            }

        } else {
            // just update hp in data
            it->hp = newHp;
        }

        // only apply to the first hit object
        return;
    }
}

void Stage::SetStageData(const StageData& data){
    data_ = data;
    loader_.CreateFromData(data_);
}

void Stage::Update(float deltaTime){
    // Maintain runtime phase and base positions for moving platforms
    static std::unordered_map<int, float> movePhase;
    static std::unordered_map<int, Vector3> basePos;

    // Clean up maps for removed objects
    std::unordered_set<int> existingIds;
    for(const auto& o : data_.objects) existingIds.insert(o.id);
    for(auto it = movePhase.begin(); it != movePhase.end(); ){ if(existingIds.find(it->first)==existingIds.end()) it = movePhase.erase(it); else ++it; }
    for(auto it = basePos.begin(); it != basePos.end(); ){ if(existingIds.find(it->first)==existingIds.end()) it = basePos.erase(it); else ++it; }

    // clear previous frame deltas
    platformMoveDeltas_.clear();

    for(auto& o : data_.objects){
        if(o.blockId != BlockID::MovingPlatform) continue;
        // Debug: log platform params
        {
            std::string msg = "Stage::Update platform id:" + std::to_string(o.id) +
                " dir:" + std::to_string(o.moveDirection) +
                " speed:" + std::to_string(o.moveSpeed) +
                " range:" + std::to_string(o.moveRange) +
                " phase(norm):" + std::to_string(o.movePhase) + "\n";
            Logger::Log(msg);
        }
        if(o.moveRange <= 0.0f || o.moveSpeed == 0.0f) continue;

        // ensure base position is recorded
        if(basePos.find(o.id) == basePos.end()){
            basePos[o.id] = o.position;
            // initialize phase from object (normalized 0..1 -> radians)
            movePhase[o.id] = o.movePhase * 2.0f * 3.14159265f;
        }

        // compute previous position from base and current phase (don't rely on o.position when movementLocked)
        float halfRange = o.moveRange * 0.5f;
        float phaseBefore = movePhase[o.id];

        Vector3 axis{0.0f, 0.0f, 0.0f};
        if(o.moveDirection == 1 || o.moveDirection == 2){ // vertical
            axis = {0.0f, 1.0f, 0.0f};
        } else if(o.moveDirection == 3 || o.moveDirection == 4){ // horizontal (X)
            axis = {1.0f, 0.0f, 0.0f};
        }

        float prevOffset = std::sinf(phaseBefore) * halfRange;
        Vector3 prevPos = basePos[o.id] + axis * prevOffset;

        // advance phase
        movePhase[o.id] += deltaTime * o.moveSpeed;

        // compute new position using updated phase
        float newOffset = std::sinf(movePhase[o.id]) * halfRange;
        Vector3 newPos = basePos[o.id] + axis * newOffset;

        // record delta
        platformMoveDeltas_[o.id] = newPos - prevPos;

        // apply to instances; if movementLocked is false, also update data_.objects position
        if (!o.movementLocked) {
            o.position = newPos;
        }
        loader_.UpdateInstanceTransform(o.id, newPos, o.rotation, o.scale);
    }
}

StageData& Stage::GetStageData(){
    return data_;
}

const StageData& Stage::GetStageData() const{
    return data_;
}

void Stage::RefreshInstances(){
    loader_.CreateFromData(data_);
}
float Stage::GetHeightAt(const Vector3& pos) const{
    // 同じXZに複数の床（上下に別の足場）がある場合、
    // 「その位置(pos.y)より下にある一番高い床」を返す。
    // （経験値オーブの groundY が上の足場に吸われてワープするのを防ぐ）
    constexpr float kNoHit = -1e9f;
    constexpr float kEpsilon = 0.10f;

    float bestBelowY = kNoHit;
    float bestAnyY = kNoHit;

    for (const auto& o : data_.objects) {
        if (!(o.blockId == BlockID::Normal || o.blockId == BlockID::Breakable || o.blockId == BlockID::MovingPlatform)) {
            continue;
        }

        Vector3 halfSize = { 1.0f * o.scale.x, 1.0f * o.scale.y, 1.0f * o.scale.z };
        float minX = o.position.x - halfSize.x;
        float maxX = o.position.x + halfSize.x;
        float minZ = o.position.z - halfSize.z;
        float maxZ = o.position.z + halfSize.z;
        if (pos.x < minX || pos.x > maxX || pos.z < minZ || pos.z > maxZ) {
            continue;
        }

        float topY = o.position.y + halfSize.y;
        if (topY > bestAnyY) {
            bestAnyY = topY;
        }
        if (topY <= pos.y + kEpsilon && topY > bestBelowY) {
            bestBelowY = topY;
        }
    }

    if (bestBelowY > kNoHit * 0.5f) {
        return bestBelowY;
    }
    if (bestAnyY > kNoHit * 0.5f) {
        return bestAnyY;
    }
    return 0.0f;
}

void Stage::Draw(){
    loader_.DrawAndUpdate();
}

void Stage::DrawOpaque()
{
    loader_.DrawOpaqueAndUpdate();
}

void Stage::DrawTransparentSorted(const Vector3& cameraPos)
{
    loader_.DrawTransparentSortedAndUpdate(cameraPos);
}

void Stage::Clear(){
    data_.objects.clear();
    loader_.Clear();
}

void Stage::UpdateOrCreateInstance(const StageObject& o){
    loader_.UpdateOrCreateInstance(o);
}

void Stage::RemoveInstanceById(int id){
    loader_.RemoveInstanceById(id);
}

void Stage::UpdateInstanceTransform(int id, const Vector3& pos, const Vector3& rot, const Vector3& scale){
    loader_.UpdateInstanceTransform(id, pos, rot, scale);
}

void Stage::SetInstanceColorById(int id, const Vector4& color){
    loader_.SetInstanceColorById(id, color);
}

size_t Stage::GetInstanceCount() const{
    return loader_.GetInstanceCount();
}

std::vector<CollisionUtility::AABB> Stage::GetBlockAABBs() const{
    std::vector<CollisionUtility::AABB> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        // Include only solid block types for block AABBs
        if(!(o.blockId == BlockID::Normal ||
             o.blockId == BlockID::Breakable ||
             o.blockId == BlockID::Warp ||
             o.blockId == BlockID::MovingPlatform)){
            continue;
        }

        CollisionUtility::AABB aabb;
        Vector3 halfSize = { 1.0f * o.scale.x, 1.0f * o.scale.y, 1.0f * o.scale.z };
        aabb.min = { o.position.x - halfSize.x, o.position.y - halfSize.y, o.position.z - halfSize.z };
        aabb.max = { o.position.x + halfSize.x, o.position.y + halfSize.y, o.position.z + halfSize.z };
        result.push_back(aabb);
    }

    return result;
}

std::unordered_map<int, Vector3> Stage::ConsumePlatformDeltas(){
    auto copy = platformMoveDeltas_;
    platformMoveDeltas_.clear();
    return copy;
}

std::vector<CollisionUtility::AABB> Stage::GetWaterBlockAABBs() const{
    std::vector<CollisionUtility::AABB> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        // Include only water blocks
        if(o.blockId != BlockID::Water){
            continue;
        }

        CollisionUtility::AABB aabb;
        Vector3 halfSize = { 1.0f * o.scale.x, 1.0f * o.scale.y, 1.0f * o.scale.z };
        aabb.min = { o.position.x - halfSize.x, o.position.y - halfSize.y, o.position.z - halfSize.z };
        aabb.max = { o.position.x + halfSize.x, o.position.y + halfSize.y, o.position.z + halfSize.z };
        result.push_back(aabb);
    }

    return result;
}

std::vector<Vector3> Stage::GetBugSpawnPositions() const{
    std::vector<Vector3> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        if(o.blockId == BlockID::BugSpawn){
            result.push_back(o.position);
        }
    }

    return result;
}

std::optional<Vector3> Stage::GetPlayerSpawnPosition() const{
    for(const auto& o : data_.objects){
        if(o.blockId == BlockID::PlayerSpawn){
            return o.position;
        }
    }
    return std::nullopt;
}

std::vector<EnemySpawnPoint> Stage::GetEnemySpawnPoints() const{
    std::vector<EnemySpawnPoint> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        if(o.blockId != BlockID::EnemySpawn){
            continue;
        }

        EnemySpawnPoint spawn;
        spawn.position = o.position;
        spawn.rotation = o.rotation;
        spawn.enemyType = o.enemyType;
        // Use per-enemy-type default if present; fallback to object's value for compatibility
        auto it = data_.enemyTypeRespawnDefaults.find(o.enemyType);
        if (it != data_.enemyTypeRespawnDefaults.end()) {
            spawn.respawnInterval = it->second;
        } else {
            spawn.respawnInterval = o.enemyRespawnInterval;
        }
        result.push_back(spawn);
    }

    return result;
}

std::vector<CollisionUtility::OBB> Stage::GetBlockOBBs() const{
    std::vector<CollisionUtility::OBB> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        // Include only solid block types for block OBBs
        if(!(o.blockId == BlockID::Normal ||
             o.blockId == BlockID::Breakable ||
             o.blockId == BlockID::Warp ||
             o.blockId == BlockID::MovingPlatform)){
            continue;
        }

        Transform t;
        t.translate = o.position;
        t.rotate = o.rotation;
        t.scale = o.scale;

        result.push_back(
            CollisionUtility::MakeOBBFromTransform(
            t,
            { 1.0f, 1.0f, 1.0f }
        )
        );
    }

    return result;
}

std::vector<CollisionUtility::OBB> Stage::GetWaterBlockOBBs() const{
    std::vector<CollisionUtility::OBB> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        // Include only water blocks
        if(o.blockId != BlockID::Water){
            continue;
        }

        Transform t;
        t.translate = o.position;
        t.rotate = o.rotation;
        t.scale = o.scale;

        result.push_back(
            CollisionUtility::MakeOBBFromTransform(
            t,
            { 1.0f, 1.0f, 1.0f }
        )
        );
    }

    return result;
}