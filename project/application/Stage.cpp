#define NOMINMAX
#include "Stage.h"

Stage::Stage(Object3dCommon* objCommon, Camera* camera)
    : loader_(objCommon, camera){
    data_.name = "stage";
}

void Stage::SetStageData(const StageData& data){
    data_ = data;
    loader_.CreateFromData(data_);
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

void Stage::Draw(){
    loader_.DrawAndUpdate();
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
        if(o.modelName != "Cube.obj"){
            continue;
        }
        if(o.blockId == BlockID::Water ||
           o.blockId == BlockID::BugSpawn ||
           o.blockId == BlockID::PlayerSpawn){
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

std::vector<CollisionUtility::AABB> Stage::GetWaterBlockAABBs() const{
    std::vector<CollisionUtility::AABB> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        if(o.modelName != "Cube.obj"){
            continue;
        }
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

std::vector<CollisionUtility::OBB> Stage::GetBlockOBBs() const{
    std::vector<CollisionUtility::OBB> result;
    result.reserve(data_.objects.size());

    for(const auto& o : data_.objects){
        if(o.modelName != "Cube.obj"){
            continue;
        }
        if(o.blockId == BlockID::Water ||
           o.blockId == BlockID::BugSpawn ||
           o.blockId == BlockID::PlayerSpawn){
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
        if(o.modelName != "Cube.obj"){
            continue;
        }
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