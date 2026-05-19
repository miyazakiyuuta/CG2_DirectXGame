#include "3d/Model.h"
#include "2d/TextureManager.h"
#include "3d/ModelCommon.h"
#include "3d/Object3dCommon.h"
#include "3d/Skeleton.h"
#include "base/SrvManager.h"
#include <cassert>
#include <fstream>


void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename) {
	modelCommon_ = modelCommon;
	dxCommon_ = modelCommon_->GetDxCommon();

	modelData_ = LoadModelFile(directoryPath, filename);
	animations_ = LoadAnimationFile(directoryPath, filename);

	CreateVertexData();
	CreateIndexData();

	if (modelData_.material.textureFilePath.empty()) {
		modelData_.material.textureFilePath = TextureManager::kDefaultTextureName;
	}

	// .objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
	modelData_.material.srvIndex = TextureManager::GetInstance()->GetSrvIndex(modelData_.material.textureFilePath);

	if (TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureFilePath).ptr != 0) {
		// 読み込んだテクスチャの番号を取得
		modelData_.material.srvIndex = TextureManager::GetInstance()->GetSrvIndex(modelData_.material.textureFilePath);
	} else {
		modelData_.material.srvIndex = TextureManager::GetInstance()->GetSrvIndex(TextureManager::kDefaultTextureName);
	}
}

void Model::Update(const Skeleton& skeleton) {
	if (skinCluster_.inverseBindPoseMatrices.empty()) {
		return;
	}
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
		assert(jointIndex < skinCluster_.inverseBindPoseMatrices.size());
		skinCluster_.mappedPalette[jointIndex].skeletonSpaceMatrix = skinCluster_.inverseBindPoseMatrices[jointIndex] * skeleton.joints[jointIndex].skeletonSpaceMatrix;
		skinCluster_.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = skinCluster_.mappedPalette[jointIndex].skeletonSpaceMatrix.Inverse().Transpose();
	}
}

void Model::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	auto* srvManager = modelCommon_->GetSrvManager();

	bool isSkinned = !skinCluster_.inverseBindPoseMatrices.empty();
	if (isSkinned) {
		D3D12_VERTEX_BUFFER_VIEW skinnedVBV{};
		skinnedVBV.BufferLocation = skinCluster_.skinnedVertexResource->GetGPUVirtualAddress();
		skinnedVBV.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData_.vertices.size());
		skinnedVBV.StrideInBytes = sizeof(VertexData);
		commandList->IASetVertexBuffers(0, 1, &skinnedVBV);
	} else {
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	}

	commandList->IASetIndexBuffer(&indexBufferView_);

	// SRVのDescriptorTableの先頭を設定
	commandList->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(modelData_.material.srvIndex));

	// 描画!(DrawCall/ドローコール)。
	commandList->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
}

std::map<std::string, Animation> Model::LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
	std::map<std::string, Animation> animations;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
	if (!scene || !scene->HasAnimations()) { // アニメーションがない
		return animations;
	}

	for (int i = 0; i < (int)scene->mNumAnimations; ++i) {
		aiAnimation* animationAssimp = scene->mAnimations[i]; // 最初のアニメーションだけ採用
		Animation& animation = animations[animationAssimp->mName.C_Str()];
		animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // 時間の単位を秒に変換
		// animations.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);

		// assimpでは個々のNodeのAnimationをchannelと呼んでいるのでchannnelを回してNodeAnimationの情報をとってくる
		for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
			aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
			NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
			// Position(Translate)の解析
			for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
				aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
				KeyframeVector3 keyframe;
				keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // 秒に変換
				keyframe.value = {-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
				nodeAnimation.translate.push_back(keyframe);
			}
			// Rotationの解析
			for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
				aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
				KeyframeQuaternion keyframe;
				keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
				keyframe.value = {keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w};
				nodeAnimation.rotate.push_back(keyframe);
			}
			// Scaleの解析
			for (uint32_t j = 0; j < nodeAnimationAssimp->mNumScalingKeys; ++j) {
				aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[j];
				KeyframeVector3 keyframe;
				keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
				keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
				nodeAnimation.scale.push_back(keyframe);
			}
		}
	}

	return animations;
}

void Model::CreateSkinCluster(const Skeleton& skeleton) {
	auto device = dxCommon_->GetDevice();
	auto srvManager = modelCommon_->GetSrvManager();

	// palette用のResourceを確保
	skinCluster_.paletteResource = dxCommon_->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
	WellForGPU* mappedPalette = nullptr;
	skinCluster_.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster_.mappedPalette = {mappedPalette, skeleton.joints.size()};
	skinCluster_.paletteSrvIndex = srvManager->Allocate();

	skinCluster_.paletteSrvHandle.first = srvManager->GetCPUDescriptorHandle(skinCluster_.paletteSrvIndex);
	skinCluster_.paletteSrvHandle.second = srvManager->GetGPUDescriptorHandle(skinCluster_.paletteSrvIndex);

	// palette用のsrvを作成。StructuredBufferでアクセスできるようにする
	srvManager->CreateSRVForStructuredBuffer(skinCluster_.paletteSrvIndex, skinCluster_.paletteResource.Get(), static_cast<UINT>(skeleton.joints.size()), sizeof(WellForGPU));

	// influence用のResourceを確保。頂点ごとにinfluence情報を追加できるようにする
	skinCluster_.influenceResource = dxCommon_->CreateBufferResource(sizeof(VertexInfluence) * modelData_.vertices.size());
	VertexInfluence* mappedInfluence = nullptr;
	skinCluster_.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData_.vertices.size());
	skinCluster_.mappedInfluence = {mappedInfluence, modelData_.vertices.size()};

	// Influence用のVBVを作成
	skinCluster_.influenceBufferView.BufferLocation = skinCluster_.influenceResource->GetGPUVirtualAddress();
	skinCluster_.influenceBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexInfluence) * modelData_.vertices.size());
	skinCluster_.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);
	// influence用SRVの作成
	skinCluster_.influenceSrvIndex = srvManager->Allocate();
	skinCluster_.influenceSrvHandle.first = srvManager->GetCPUDescriptorHandle(skinCluster_.influenceSrvIndex);
	skinCluster_.influenceSrvHandle.second = srvManager->GetGPUDescriptorHandle(skinCluster_.influenceSrvIndex);
	srvManager->CreateSRVForStructuredBuffer(skinCluster_.influenceSrvIndex, skinCluster_.influenceResource.Get(), static_cast<UINT>(modelData_.vertices.size()), sizeof(VertexInfluence));

	// inputVertex用SRVの作成
	skinCluster_.inputVertexSrvIndex = srvManager->Allocate();
	skinCluster_.inputVertexSrvHandle.first = srvManager->GetCPUDescriptorHandle(skinCluster_.inputVertexSrvIndex);
	skinCluster_.inputVertexSrvHandle.second = srvManager->GetGPUDescriptorHandle(skinCluster_.inputVertexSrvIndex);
	srvManager->CreateSRVForStructuredBuffer(skinCluster_.inputVertexSrvIndex, vertexResource_.Get(), static_cast<UINT>(modelData_.vertices.size()), sizeof(VertexData));

	// InverseBindPoseMatrixを格納する場所を作成して、単位行列で埋める
	skinCluster_.inverseBindPoseMatrices.resize(skeleton.joints.size());
	for (auto& matrix : skinCluster_.inverseBindPoseMatrices) {
		matrix = Matrix4x4::Identity();
	}

	for (const auto& jointWeight : modelData_.skinClusterData) { // ModelのSkinClusterの情報を解析
		auto it = skeleton.jointMap.find(jointWeight.first);     // jointWeight.firstはjoint名なので、skeletonに対象となるjointが含まれているか判断
		if (it == skeleton.jointMap.end()) {                     // そんな名前のJointは存在しないので次に回す
			continue;
		}
		// (*it).secondにはjointのindexが入っているので、該当のindexのinverseBindPoseMatrixを代入
		skinCluster_.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
			auto& currentInfluence = skinCluster_.mappedInfluence[vertexWeight.vertexIndex]; // 該当のvertexIndexのinfluence情報を参照しておく
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {                    // 空いているところに入れる
				if (currentInfluence.weights[index] == 0.0f) {                               // weight==0が空いている状態なので、その場所にweightとjointのindexを代入
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;
				}
			}
		}
	}

	/*skinCluster_.skinnedVertexResource = dxCommon_->CreateBufferResource(
	    sizeof(VertexData) * modelData_.vertices.size());*/

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(VertexData) * modelData_.vertices.size();
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // ← これが重要

	HRESULT hr =
	    dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&skinCluster_.skinnedVertexResource));
	assert(SUCCEEDED(hr));

	skinCluster_.skinningInformationResource = dxCommon_->CreateBufferResource(sizeof(SkinningInformation));
	SkinningInformation* skinningInfo = nullptr;
	skinCluster_.skinningInformationResource->Map(0, nullptr, reinterpret_cast<void**>(&skinningInfo));
	skinningInfo->numVertices = static_cast<uint32_t>(modelData_.vertices.size());

	skinCluster_.uavIndex = srvManager->Allocate();
	skinCluster_.uavHandle.first = srvManager->GetCPUDescriptorHandle(skinCluster_.uavIndex);
	skinCluster_.uavHandle.second = srvManager->GetGPUDescriptorHandle(skinCluster_.uavIndex);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(modelData_.vertices.size());
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Buffer.StructureByteStride = sizeof(VertexData);

	device->CreateUnorderedAccessView(skinCluster_.skinnedVertexResource.Get(), nullptr, &uavDesc, skinCluster_.uavHandle.first);
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData;                          // 構築するMaterialData
	std::string line;                                   // ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open());                             // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

Model::ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData

	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes()); // メッシュがないのは対応しない

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());        // 法線がないMeshは非対応
		assert(mesh->HasTextureCoords(0)); // TexcoordがないMeshは非対応
		modelData.vertices.resize(mesh->mNumVertices);

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
			// 右手系->左手系への変換
			modelData.vertices[vertexIndex].position = {-position.x, position.y, position.z, 1.0f};
			modelData.vertices[vertexIndex].normal = {-normal.x, normal.y, normal.z};
			modelData.vertices[vertexIndex].texcoord = {texcoord.x, texcoord.y};
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);
			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				modelData.indices.push_back(vertexIndex);
			}
		}

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse(); // BindPoseMatrixに戻す
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate); // 成分を抽出
			// 左手系のBindPoseMatrixを作る
			Matrix4x4 bindPoseMatrix = Matrix4x4::Affine({scale.x, scale.y, scale.z}, {rotate.x, -rotate.y, -rotate.z, rotate.w}, {-translate.x, translate.y, translate.z});
			// InverseBindPoseMatrixにする
			jointWeightData.inverseBindPoseMatrix = bindPoseMatrix.Inverse();

			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId});
			}
		}
	}

	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}
	}

	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

void Model::CreateVertexData() {
	// VertexResourceを作る
	vertexResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());
	// VertexBufferViewを作成する(値を設定するだけ)
	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 使用するリソースサイズは頂点のサイズ
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* ptr = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&ptr)); // 書き込むためのアドレスを取得
	vertexData_ = {ptr, modelData_.vertices.size()};                  // 範囲確定
	// VertexResourceにデータを書き込むためのアドレスを取得してvertexDataに割り当てる
	std::memcpy(vertexData_.data(), modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void Model::CreateIndexData() {
	indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * modelData_.indices.size());

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* ptr = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
	indexData_ = {ptr, modelData_.indices.size()};
	std::memcpy(indexData_.data(), modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
}

Model::Node Model::ReadNode(aiNode* node) {
	Node result;

	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate);

	result.transform.scale = {scale.x, scale.y, scale.z};
	result.transform.rotate = {rotate.x, -rotate.y, -rotate.z, rotate.w};
	result.transform.translate = {-translate.x, translate.y, translate.z};

	result.localMatrix = Matrix4x4::Affine(result.transform.scale, result.transform.rotate, result.transform.translate);

	result.name = node->mName.C_Str();          // Node名を格納
	result.children.resize(node->mNumChildren); // 子供の数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		// 再帰的に読んで階層構造を作っていく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}
	return result;
}
