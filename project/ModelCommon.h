#pragma once
#include "DirectXCommon.h"

class ModelCommon {
public:
	void Initialize(DirectXCommon* dxCommon);

public:
	DirectXCommon* GetDxCommon() { return dxCommon_; }

private:
	DirectXCommon* dxCommon_;
};

