#include "3d/ModelCommon.h"
#include "base/SrvManager.h"

void ModelCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
}