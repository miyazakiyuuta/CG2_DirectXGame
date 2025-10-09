#pragma once
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
class ResourceObject {
public:
	ResourceObject(ID3D12Resource* resource)
	:resource_(resource)
	{}
	~ResourceObject(){
		if (resource_) {
			resource_->Release();
		}
	}
	ID3D12Resource* Get() { return resource_; }

private:
	ID3D12Resource* resource_;
};

