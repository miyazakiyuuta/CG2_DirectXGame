#pragma once
#include "DirectXCommon.h"

class SrvManager;

class ModelCommon {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

public:
	DirectXCommon* GetDxCommon() { return dxCommon_; }
	SrvManager* GetSrvManager() { return srvManager_; }

private:
	DirectXCommon* dxCommon_;
	SrvManager* srvManager_;
};

