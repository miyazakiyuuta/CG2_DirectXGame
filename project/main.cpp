#include <Windows.h>

#include <cstdint>

#include <filesystem> // ファイルやディレクトリに関する操作を行うライブラリ
#include <fstream> // ファイルに書いたり読んだりするライブラリ
#include <chrono> // 時間を扱うライブラリ

#include <string>
#include <format>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#include <dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")
#include <strsafe.h>

#include <dxgidebug.h>
#pragma comment(lib,"dxguid.lib")

#include "engine/base/Matrix4x4.h"

#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"



// #include"C:\\Users\\k024g\\Desktop\\CG2\\CG2_DirectXGame\\externals\\DirectXTex\\DirectXTex.h"
//#include <DirectXTex.h>
// 修正案: "DirectXTex.h" のインクルードパスをプロジェクトの相対パスに変更します。
// 既存コードを維持しつつ、正しいパスでインクルードすることでエラーを解消します。

#include <vector>

#include <fstream>
#include <sstream>

#include "engine/io/ResourceObject.h"
#include <wrl.h>
#include <iostream>

#include "engine/audio/SoundManager.h"

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include <random>
#include <numbers>

#include "engine/3d/DebugCamera.h"

#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"

using namespace MatrixMath;
using namespace StringUtility;

const float pi = 3.14159265f;

enum BlendMode {
	//!< ブレンドなし
	kBlendModeNone,
	//!< 通常aブレンド。デフォルト。 Src * SrcA + Dest * (1 - SrcA)
	kBlendModeNormal,
	//!< 加算。Src * SrcA + Dest * 1
	kBlendModeAdd,
	//!< 減算。Dest * 1 - Src * SrcA
	kBlendModeSubtract,
	//!< 乗算。Src * 0 + Dest * Src
	kBlendModeMultiply,
	//!< スクリーン。Src * (1 - Dest) + Dest * 1
	kBlendModeScreen,
	// 利用してはいけない
	kCountOfBlendMode,
};

struct Vector2 {
	float x;
	float y;
};

/*struct Vector3 {
	float x;
	float y;
	float z;
};*/

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color; //!< ライトの色
	Vector3 direction; //!< ライトの向き
	float intensity; //!< 輝度
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02%02d-%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE |
		FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	// processID(このexeのID)とクラッシュ(例外)の発生したthreadIDを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	// 他に関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

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

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= -1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				//VertexData vertex = { position,texcoord,normal };
				//modelData.vertices.push_back(vertex);
				triangle[faceVertex] = { position,texcoord,normal };
			}
			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}

Particle MakeNewParticle(std::mt19937& randomEngine) {
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	Particle particle;
	particle.transform.scale = { 1.0f,1.0f,1.0f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };
	particle.transform.translate = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.color = { distColor(randomEngine),distColor(randomEngine),distColor(randomEngine),1.0f };
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;
	return particle;
}

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	D3DResourceLeakChecker leakCheck;

	//誰も捕捉しなかった場合に(Unhandled),捕捉する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

	WinApp* winApp = nullptr;
	winApp = new WinApp();
	winApp->Initialize();

	Input* input = nullptr;
	input = new Input();
	input->Initialize(winApp);

	POINT lastMouse = {};
	GetCursorPos(&lastMouse);
	ScreenToClient(winApp->GetHwnd(), &lastMouse);

	HRESULT hr = S_OK;

	DirectXCommon* dxCommon = nullptr;
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	ID3D12Device* device = dxCommon->GetDevice();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	//depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Depthの書き込みを行わない
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// モデル読み込み
	//ModelData modelData;
	ModelData modelData = LoadObjFile("resources", "plane.obj");
	//ModelData modelData = LoadObjFile("resources/fence", "fence.obj");
	//ModelData modelData = LoadObjFile("resources", "axis.obj");
	/*
	modelData.vertices.push_back({ .position = {1.0f,1.0f,0.0f,1.0f},.texcoord = {0.0f,0.0f},.normal = {0.0f,0.0f,1.0f} }); // 左上
	modelData.vertices.push_back({ .position = {-1.0f,1.0f,0.0f,1.0f},.texcoord = {1.0f,0.0f},.normal = {0.0f,0.0f,1.0f} }); // 右上
	modelData.vertices.push_back({ .position = {1.0f,-1.0f,0.0f,1.0f},.texcoord = {0.0f,1.0f},.normal = {0.0f,0.0f,1.0f} }); // 左下
	modelData.vertices.push_back({ .position = {1.0f,-1.0f,0.0f,1.0f},.texcoord = {0.0f,1.0f},.normal = {0.0f,0.0f,1.0f} }); // 左下
	modelData.vertices.push_back({ .position = {-1.0f,1.0f,0.0f,1.0f},.texcoord = {1.0f,0.0f},.normal = {0.0f,0.0f,1.0f} }); // 右上
	modelData.vertices.push_back({ .position = {-1.0f,-1.0f,0.0f,1.0f},.texcoord = {1.0f,1.0f},.normal = {0.0f,0.0f,1.0f} }); // 右下
	modelData.material.textureFilePath = "./resources/uvChecker.png";
	*/
	
	const int kNumMaxInstance = 10; // instanceCount

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = dxCommon->LoadTexture("resources/circle.png");
	//DirectX::ScratchImage mipImages = dxCommon->LoadTexture("resources/uvChecker.png");
	//DirectX::ScratchImage mipImages = dxCommon->LoadTexture("resources/fence/fence.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = dxCommon->CreateTextureResource(metadata);
	//UpLoadTextureData(textureResource, mipImages);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon->UpLoadTextureData(textureResource, mipImages);

	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = dxCommon->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = dxCommon->CreateTextureResource(metadata2);
	//UploadTextureData(textureResource2, mipImages2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = dxCommon->UpLoadTextureData(textureResource2, mipImages2);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);
	/*
	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	// 先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	*/
	// ImGui が 0 番を使ってる前提で、1 番に uvChecker を割り当て
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(1);
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	// 2枚目のテクスチャ（modelData のやつ）が必要なら 2番を使う
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetSRVCPUDescriptorHandle(2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetSRVGPUDescriptorHandle(2);


	// SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	//device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

#pragma region Object用PSO

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC objectRootSignatureDesc{};
	objectRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE objectSrvRange[1] = {};
	objectSrvRange[0].BaseShaderRegister = 0; // 0から始まる
	objectSrvRange[0].NumDescriptors = 1; // 数は1つ
	objectSrvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	objectSrvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// RootParameter作成。PixelShaderのMaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER objectRootParameters[4] = {};
	objectRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	objectRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	objectRootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド (PS)
	
	objectRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	objectRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	objectRootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド (VS)

	objectRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	objectRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	objectRootParameters[2].DescriptorTable.pDescriptorRanges = objectSrvRange; // Tableの中身の配列を指定
	objectRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(objectSrvRange); // Tableで利用する数

	objectRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	objectRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	objectRootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	D3D12_STATIC_SAMPLER_DESC objectStaticSamplers[1] = {};
	objectStaticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	objectStaticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	objectStaticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	objectStaticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	objectStaticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	objectStaticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	objectStaticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	objectStaticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

	objectRootSignatureDesc.pStaticSamplers = objectStaticSamplers;
	objectRootSignatureDesc.NumStaticSamplers = _countof(objectStaticSamplers);

	objectRootSignatureDesc.pParameters = objectRootParameters; // ルートパラメータ配列へのポインタ
	objectRootSignatureDesc.NumParameters = _countof(objectRootParameters); // 配列の長さ

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> objectSignatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> objectErrorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&objectRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &objectSignatureBlob, &objectErrorBlob);
	if (FAILED(hr)) {
		Log(std::cerr, reinterpret_cast<char*>(objectErrorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> objectRootSignature = nullptr;
	hr = device->CreateRootSignature(0, objectSignatureBlob->GetBufferPointer(), objectSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&objectRootSignature));
	assert(SUCCEEDED(hr));

	D3D12_INPUT_ELEMENT_DESC objectInputElementDescs[3] = {};
	objectInputElementDescs[0].SemanticName = "POSITION";
	objectInputElementDescs[0].SemanticIndex = 0;
	objectInputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	objectInputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	
	objectInputElementDescs[1].SemanticName = "TEXCOORD";
	objectInputElementDescs[1].SemanticIndex = 0;
	objectInputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	objectInputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	objectInputElementDescs[2].SemanticName = "NORMAL";
	objectInputElementDescs[2].SemanticIndex = 0;
	objectInputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	objectInputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC objectInputLayoutDesc{};
	objectInputLayoutDesc.pInputElementDescs = objectInputElementDescs;
	objectInputLayoutDesc.NumElements = _countof(objectInputElementDescs);
	
	int blendMode = kBlendModeNone;
	int prevBlendMode = -1;
	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	

	// RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> objectVertexShaderBlob = dxCommon->CompileShader(L"resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
	assert(objectVertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> objectPixelShaderBlob = dxCommon->CompileShader(L"resources/shaders/Object3D.PS.hlsl", L"ps_6_0");
	assert(objectPixelShaderBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC objectPipelineStateDesc{};
	objectPipelineStateDesc.pRootSignature = objectRootSignature.Get(); // RootSignature
	objectPipelineStateDesc.InputLayout = objectInputLayoutDesc; // InputLayout
	objectPipelineStateDesc.VS = { objectVertexShaderBlob->GetBufferPointer(),objectVertexShaderBlob->GetBufferSize() }; // VertexShader
	objectPipelineStateDesc.PS = { objectPixelShaderBlob->GetBufferPointer(),objectPixelShaderBlob->GetBufferSize() }; // PixelShader
	objectPipelineStateDesc.BlendState = blendDesc; // BlendState
	objectPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	objectPipelineStateDesc.NumRenderTargets = 1;
	objectPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ。三角形
	objectPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	objectPipelineStateDesc.SampleDesc.Count = 1;
	objectPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilの設定
	objectPipelineStateDesc.DepthStencilState = depthStencilDesc;
	objectPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> objectPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&objectPipelineStateDesc, IID_PPV_ARGS(&objectPipelineState));
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region Particle用PSO

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC particleRootSignatureDesc{};
	particleRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE particleRangeForInstancing[1] = {};
	particleRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	particleRangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	particleRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	particleRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_DESCRIPTOR_RANGE particleSrvRange[1] = {};
	particleSrvRange[0].BaseShaderRegister = 0; // 0から始まる
	particleSrvRange[0].NumDescriptors = 1; // 数は1つ
	particleSrvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	particleSrvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_ROOT_PARAMETER particleRootParameters[4] = {};
	particleRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	particleRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	particleRootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド

	particleRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	particleRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	particleRootParameters[1].DescriptorTable.pDescriptorRanges = particleRangeForInstancing; // Tableの中身の配列を指定
	particleRootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(particleRangeForInstancing); // Tableで利用する数

	particleRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	particleRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	particleRootParameters[2].DescriptorTable.pDescriptorRanges = particleSrvRange; // Tableの中身の配列を指定
	particleRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(particleSrvRange); // Tableで利用する数

	particleRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	particleRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	particleRootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	D3D12_STATIC_SAMPLER_DESC particleStaticSamplers[1] = {};
	particleStaticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	particleStaticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	particleStaticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleStaticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleStaticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	particleStaticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	particleStaticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	particleStaticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

	particleRootSignatureDesc.pStaticSamplers = particleStaticSamplers;
	particleRootSignatureDesc.NumStaticSamplers = _countof(particleStaticSamplers);

	particleRootSignatureDesc.pParameters = particleRootParameters; // ルートパラメータ配列へのポインタ
	particleRootSignatureDesc.NumParameters = _countof(particleRootParameters); // 配列の長さ

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> particleSignatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> particleErrorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&particleRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &particleSignatureBlob, &particleErrorBlob);
	if (FAILED(hr)) {
		Log(std::cerr, reinterpret_cast<char*>(particleErrorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSignature = nullptr;
	hr = device->CreateRootSignature(0, particleSignatureBlob->GetBufferPointer(), particleSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&particleRootSignature));
	assert(SUCCEEDED(hr));

	D3D12_INPUT_ELEMENT_DESC particleInputElementDescs[3] = {};
	particleInputElementDescs[0].SemanticName = "POSITION";
	particleInputElementDescs[0].SemanticIndex = 0;
	particleInputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	particleInputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	particleInputElementDescs[1].SemanticName = "TEXCOORD";
	particleInputElementDescs[1].SemanticIndex = 0;
	particleInputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	particleInputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	particleInputElementDescs[2].SemanticName = "NORMAL";
	particleInputElementDescs[2].SemanticIndex = 0;
	particleInputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	particleInputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC particleInputLayoutDesc{};
	particleInputLayoutDesc.pInputElementDescs = particleInputElementDescs;
	particleInputLayoutDesc.NumElements = _countof(particleInputElementDescs);

	int particleBlendMode = kBlendModeNormal;
	int particlePrevBlendMode = -1;
	// BlendStateの設定
	D3D12_BLEND_DESC particleBlendDesc{};
	// すべての色要素を書き込む
	particleBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	particleBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	particleBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	particleBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	particleBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// ブレンドモード
	/* ノーマル
	particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	*/
	particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShaderBlob = dxCommon->CompileShader(
		L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(particleVertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShaderBlob = dxCommon->CompileShader(
		L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(particlePixelShaderBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePipelineStateDesc{};
	particlePipelineStateDesc.pRootSignature = particleRootSignature.Get(); // RootSignature
	particlePipelineStateDesc.InputLayout = particleInputLayoutDesc; // InputLayout
	particlePipelineStateDesc.VS = { particleVertexShaderBlob->GetBufferPointer(),particleVertexShaderBlob->GetBufferSize() }; // VertexShader
	particlePipelineStateDesc.PS = { particlePixelShaderBlob->GetBufferPointer(),particlePixelShaderBlob->GetBufferSize() }; // PixelShader
	particlePipelineStateDesc.BlendState = particleBlendDesc; // BlendState
	particlePipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	particlePipelineStateDesc.NumRenderTargets = 1;
	particlePipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ。三角形
	particlePipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	particlePipelineStateDesc.SampleDesc.Count = 1;
	particlePipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilの設定
	particlePipelineStateDesc.DepthStencilState = depthStencilDesc;
	particlePipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> particlePipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&particlePipelineStateDesc, IID_PPV_ARGS(&particlePipelineState));
	assert(SUCCEEDED(hr));

#pragma endregion

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// デフォルト値
	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;

	std::random_device seedGenerator;
	std::mt19937 randomEngine(seedGenerator());

	// 分割数
	//const uint32_t kSubdivision = 16;

	// マテリアル用のリソースを作る。
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 今回は赤を書き込んでみる
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();

	// WVP用のリソースを作る。Matrix4x4 １つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

	// Instancing用のTransformationMatrixリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = 
		dxCommon->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

	Particle particles[kNumMaxInstance];
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		particles[index] = MakeNewParticle(randomEngine);
	}

	// 書き込むためのアドレスを取得
	ParticleForGPU* instancingData = nullptr;
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		instancingData[index].WVP = MakeIdentity4x4();
		instancingData[index].World = MakeIdentity4x4();
		instancingData[index].color = { 0.0f,0.0f,0.0f,0.0f };
	}


	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(3);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(3);
	device->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);

	// データを書き込む
	//Matrix4x4* wvpData = nullptr;
	TransformationMatrix* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();


	// 頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースサイズは頂点のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData)); // 書き込むためのアドレスを取得
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size()); // 頂点データをリソースにコピー


	/* sphere
	// 経度分割1つ分の角度φ
	const float kLonEvery = pi * 2.0f / float(kSubdivision);
	// 緯度分割1つ分の角度θ
	const float kLatEvery = pi / float(kSubdivision);
	// 緯度の方向に分割
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -pi / 2.0f + kLatEvery * latIndex; // θ
		float latNext = lat + kLatEvery;
		// 経度の方向に分割しながら線を描く
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
			float lon = lonIndex * kLonEvery; // φ
			float lonNext = lon + kLonEvery;
			// 頂点にデータを入力する。

			// テクスチャ座標
			float u = float(lonIndex) / float(kSubdivision);
			float uNext = float(lonIndex + 1) / float(kSubdivision);
			float v = 1.0f - float(latIndex) / float(kSubdivision);
			float vNext = 1.0f - float(latIndex + 1) / float(kSubdivision);

			// 基準点a
			vertexData[start].position.x = cos(lat) * cos(lon);
			vertexData[start].position.y = sin(lat);
			vertexData[start].position.z = cos(lat) * sin(lon);
			vertexData[start].position.w = 1.0f;
			vertexData[start].texcoord = { u,v };
			vertexData[start].normal.x = vertexData[start].position.x;
			vertexData[start].normal.y = vertexData[start].position.y;
			vertexData[start].normal.z = vertexData[start].position.z;
			// b
			vertexData[start + 1].position.x = cos(latNext) * cos(lon);
			vertexData[start + 1].position.y = sin(latNext);
			vertexData[start + 1].position.z = cos(latNext) * sin(lon);
			vertexData[start + 1].position.w = 1.0f;
			vertexData[start + 1].texcoord = { u,vNext };
			vertexData[start + 1].normal.x = vertexData[start + 1].position.x;
			vertexData[start + 1].normal.y = vertexData[start + 1].position.y;
			vertexData[start + 1].normal.z = vertexData[start + 1].position.z;
			// d
			vertexData[start + 2].position.x = cos(latNext) * cos(lonNext);
			vertexData[start + 2].position.y = sin(latNext);
			vertexData[start + 2].position.z = cos(latNext) * sin(lonNext);
			vertexData[start + 2].position.w = 1.0f;
			vertexData[start + 2].texcoord = { uNext,vNext };
			vertexData[start + 2].normal.x = vertexData[start + 2].position.x;
			vertexData[start + 2].normal.y = vertexData[start + 2].position.y;
			vertexData[start + 2].normal.z = vertexData[start + 2].position.z;
			// a2
			vertexData[start + 3].position.x = cos(lat) * cos(lon);
			vertexData[start + 3].position.y = sin(lat);
			vertexData[start + 3].position.z = cos(lat) * sin(lon);
			vertexData[start + 3].position.w = 1.0f;
			vertexData[start + 3].texcoord = { u,v };
			vertexData[start + 3].normal.x = vertexData[start + 3].position.x;
			vertexData[start + 3].normal.y = vertexData[start + 3].position.y;
			vertexData[start + 3].normal.z = vertexData[start + 3].position.z;
			// d2
			vertexData[start + 4].position.x = cos(latNext) * cos(lonNext);
			vertexData[start + 4].position.y = sin(latNext);
			vertexData[start + 4].position.z = cos(latNext) * sin(lonNext);
			vertexData[start + 4].position.w = 1.0f;
			vertexData[start + 4].texcoord = { uNext,vNext };
			vertexData[start + 4].normal.x = vertexData[start + 4].position.x;
			vertexData[start + 4].normal.y = vertexData[start + 4].position.y;
			vertexData[start + 4].normal.z = vertexData[start + 4].position.z;
			// c
			vertexData[start + 5].position.x = cos(lat) * cos(lonNext);
			vertexData[start + 5].position.y = sin(lat);
			vertexData[start + 5].position.z = cos(lat) * sin(lonNext);
			vertexData[start + 5].position.w = 1.0f;
			vertexData[start + 5].texcoord = { uNext,v };
			vertexData[start + 5].normal.x = vertexData[start + 5].position.x;
			vertexData[start + 5].normal.y = vertexData[start + 5].position.y;
			vertexData[start + 5].normal.z = vertexData[start + 5].position.z;
		}
	}
	*/

	// Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = dxCommon->CreateBufferResource(sizeof(VertexData) * 6);

	// 頂点バッファ―ビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	// 1枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f }; // 左上
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[1].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[2].normal = { 0.0f,0.0f,-1.0f };
	// 2枚目の三角形
	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f }; // 左上
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };
	vertexDataSprite[3].normal = { 0.0f,0.0f,1.0f };
	vertexDataSprite[4].position = { 640.0f,0.0f,0.0f,1.0f }; // 右上
	vertexDataSprite[4].texcoord = { 1.0f,0.0f };
	vertexDataSprite[4].normal = { 0.0f,0.0f,1.0f };
	vertexDataSprite[5].position = { 640.f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[5].texcoord = { 1.0f,1.0f };
	vertexDataSprite[5].normal = { 0.0f,0.0f,1.0f };

	// Sprite用のマテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));

	materialDataSprite->color = Vector4{ 1.0f,1.0f,1.0f,1.0f };
	// SpriteはLightingしないのでfalseを設定する
	materialDataSprite->enableLighting = false;

	materialDataSprite->uvTransform = MakeIdentity4x4();

	Transform uvTransformSprite{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};

	// Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	// 単位行列を書き込んでおく
	//*transformationMatrixDataSprite = MakeIdentity4x4();
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };


	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	const float kDeltaTime = 1.0f / 60.0f;

	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Transform cameraTransform{
		{1.0f,1.0f,1.0f},
		{std::numbers::pi_v<float> / 3.0f,std::numbers::pi_v<float>,0.0f},
		{0.0f,23.0f,10.0f} };

	bool useMonsterBall = false;

	

	DebugCamera debugCamera;
	debugCamera.Initialize();

	SoundManager sound;
	sound.Initialize();
	SoundData se = sound.LoadWave("resources/mokugyo.wav");
	//sound.PlayerWave(se);


	// ウィンドウの×ボタンが押されるまでループ
	while (true) {
		// Windowsのメッセージ処理
		if (winApp->ProcessMessage()) {
			// ゲームループを抜ける
			break;
		}

		input->Update();
		// 数字の0キーが押されていたら
		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringA("Hit 0\n"); // 出力ウィンドウに「Hit 0」と表示
		}

		POINT currentMouse;
		GetCursorPos(&currentMouse);
		ScreenToClient(winApp->GetHwnd(), &currentMouse);

		POINT delta = {
			currentMouse.x - lastMouse.x,
			currentMouse.y - lastMouse.y
		};

		lastMouse = currentMouse;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Window");
		//ImGui::Checkbox("useMonsterBall", &useMonsterBall);
		//ImGui::ColorEdit3("Color", &materialData->color->x); // RGBA
		if (ImGui::TreeNode("camera")) {
			ImGui::DragFloat3("Translate", &cameraTransform.translate.x, 0.1f);
			ImGui::DragFloat3("Rotate", &cameraTransform.rotate.x, 0.01f);

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("object")) {
			ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
			ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
			ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);
			ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);
			ImGui::ColorEdit4("ObjectColor", &materialData->color.x);
			static const char* kBlendModeNames[] = {
				"kBlendModeNone",
				"kBlendModeNormal",
				"kBlendModeAdd",
				"kBlendModeSubtract",
				"kBlendModeMultiply",
				"kBlendModeScreen",
				"kCountOfBlendMode"
			};
			
			ImGui::Combo("BlendMode", &blendMode ,kBlendModeNames,7);
				//"kBlendModeNone\0kBlendModeNormal\0kBlendModeAdd\0kBlendModeSubtract\0kBlendModeMultiply\0kBlendModeScreen\0kCountOfBlendMode\0");
			ImGui::TreePop();
		}
		/*if (ImGui::TreeNode("particle")) {
			
		}*/
		if (ImGui::TreeNode("sprite")) {
			ImGui::DragFloat3("TranslateSprite", &transformSprite.translate.x, 1.0f);
			ImGui::DragFloat3("RotateSprite", &transformSprite.rotate.x, 0.01f);
			ImGui::DragFloat3("ScaleSprite", &transformSprite.scale.x, 0.01f);
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
			ImGui::ColorEdit4("SpriteColor", &materialDataSprite->color.x);
			

			ImGui::TreePop();
		}
		ImGui::End();

		// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		if(blendMode!=prevBlendMode){
			prevBlendMode = blendMode;
			if (blendMode == kBlendModeNone) {
				blendDesc.RenderTarget[0].BlendEnable = FALSE;
			} else {
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

				if (blendMode == kBlendModeNormal) {
					blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
					blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
					blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				}
				if (blendMode == kBlendModeAdd) {
					blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
					blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
					blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				}
				if (blendMode == kBlendModeSubtract) {
					blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
					blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
					blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				}
				if (blendMode == kBlendModeMultiply) {
					blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
					blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
					blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
				}
				if (blendMode == kBlendModeScreen) {
					blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
					blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
					blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				}
				objectPipelineStateDesc.BlendState = blendDesc;
				hr = device->CreateGraphicsPipelineState(
					&objectPipelineStateDesc, IID_PPV_ARGS(&objectPipelineState));
				assert(SUCCEEDED(hr));
			}
		}
		
		ImGuiIO& io = ImGui::GetIO();
		float wheelDelta = io.MouseWheel;
		//debugCamera.Update(keyboard, delta, wheelDelta);

		/*ゲームの処理*/
		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		//Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
		// WVPMatrixを作る
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
		//Matrix4x4 worldViewProjectionMatrix = Multiply(Multiply(worldMatrix, viewMatrix), projectionMatrix);
		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;

		// instancing用のWVP行列を作る
		uint32_t numInstance = 0;
		Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);
		for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
			if (particles[index].lifeTime <= particles[index].currentTime) {
				continue;
			}
			particles[index].transform.translate.x += particles[index].velocity.x * kDeltaTime;
			particles[index].transform.translate.y += particles[index].velocity.y * kDeltaTime;
			particles[index].transform.translate.z += particles[index].velocity.z * kDeltaTime;
			Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
			Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, cameraMatrix);
			billboardMatrix.m[3][0] = 0.0f; // 平行移動成分はいらない
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;
			//Matrix4x4 worldMatrix = MakeAffineMatrix(particles[index].transform.scale, particles[index].transform.rotate, particles[index].transform.translate);
			Matrix4x4 worldMatrix =
				Multiply(Multiply(MakeScaleMatrix(particles[index].transform.scale), billboardMatrix), MakeTranslateMatrix(particles[index].transform.translate));
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
			particles[index].currentTime += kDeltaTime;
			instancingData[numInstance].WVP = worldViewProjectionMatrix;
			instancingData[numInstance].World = worldMatrix;
			instancingData[numInstance].color = particles[index].color;
			float alpha = 1.0f - (particles[index].currentTime / particles[index].lifeTime);
			instancingData[numInstance].color.w = alpha;
			++numInstance;
		}

		// Sprite用のWorldViewProjectionMatrixを作る
		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
		Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
		//*transformationMatrixDataSprite = worldViewProjectionMatrixSprite;
		transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
		transformationMatrixDataSprite->World = worldMatrixSprite;

		Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
		materialDataSprite->uvTransform = uvTransformMatrix;

		

		ImGui::Render();

		// 描画前処理
		dxCommon->PreDraw();

		// RootSignatureを設定。PSOに設定しているけど別途設定が必要
		commandList->SetGraphicsRootSignature(objectRootSignature.Get());
		commandList->SetPipelineState(objectPipelineState.Get()); // PSOを設定
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
		// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// マテリアルCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		//// wvp用のCBufferの場所を設定 // 0or1
		commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

		commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

		// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
		commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
		// 描画!(DrawCall/ドローコール)。
		commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

		// パーティクル描画
		// パーティクル用 RootSignature / PSO に切り替え
		commandList->SetGraphicsRootSignature(particleRootSignature.Get());
		commandList->SetPipelineState(particlePipelineState.Get());
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
		commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

		commandList->DrawInstanced(UINT(modelData.vertices.size()), numInstance, 0, 0);
		
		commandList->SetGraphicsRootSignature(objectRootSignature.Get());

		// マテリアルCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
		// Spriteの描画。変更が必要なものだけ変更する
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // VBVを設定
		// TransformationMatrixCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
		// 描画! (DrawCall/ドローコール)
		//commandList->DrawInstanced(6, 1, 0, 0);

		commandList->IASetIndexBuffer(&indexBufferViewSprite);
		// 描画! (DrawCall/ドローコール) 6個のインデックスを使用し1つのインスタンスを描画。
		//commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

		// 実際のcommandListのImGuiの描画コマンドを積む
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

		// 描画後処理
		dxCommon->PostDraw();

	}

	//ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	// 現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	// ログファイルの名前にコンマ何秒はいらないので、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	// 日本時間(PCの設定時間)に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	// formatを使って年月日_時分秒の文字に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	// 時刻を使ってファイル名を決定
	std::string logFilePath = std::string("log/") + dateString + ".log";
	// ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	sound.Unload(&se);
	sound.Finalize();

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// WindowAPIの終了処理
	winApp->Finalize();
	// WindowWPIの解放
	delete winApp;
	winApp = nullptr;


	return 0;
}