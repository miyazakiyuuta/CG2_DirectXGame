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

#include "engine/base/Vector3.h"
//#include "engine/base/Matrix4x4.h"

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
#include <numbers>

#include "engine/3d/DebugCamera.h"

#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "ModelManager.h"
#include "Camera.h"

using namespace MatrixMath;
using namespace StringUtility;

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

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	D3DResourceLeakChecker leakCheck;

	//誰も捕捉しなかった場合に(Unhandled),捕捉する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

#pragma region 基盤システムの初期化

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

	TextureManager::GetInstance()->Initialize(dxCommon);

	SpriteCommon* spriteCommon = nullptr;
	// スプライト共通部の初期化
	spriteCommon = new SpriteCommon;
	spriteCommon->Initialize(dxCommon);

	Object3dCommon* object3dCommon = nullptr;
	// 3Dオブジェクト共通部の初期化
	object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);

#pragma endregion

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

	Camera* camera = new Camera();
	object3dCommon->SetDefaultCamera(camera);
	camera->SetRotate({ 0.3f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,4.0f,-10.0f });

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

	// TextureManager からテクスチャを読み込む
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture("resources/uvChecker.png");
	textureManager->LoadTexture("resources/monsterBall.png");
	textureManager->LoadTexture(modelData.material.textureFilePath);

	// あとで使いやすいようにインデックスとGPUハンドルを取っておく
	uint32_t uvCheckerIndex =
		textureManager->GetTextureIndexByFilePath("resources/uvChecker.png");
	uint32_t modelTexIndex =
		textureManager->GetTextureIndexByFilePath(modelData.material.textureFilePath);

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
		textureManager->GetSrvHandleGPU(uvCheckerIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
		textureManager->GetSrvHandleGPU(modelTexIndex);

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// デフォルト値
	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;

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


	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	const float kDeltaTime = 1.0f / 60.0f;

	bool useMonsterBall = false;

	DebugCamera debugCamera;
	debugCamera.Initialize();

	SoundManager sound;
	sound.Initialize();
	SoundData se = sound.LoadWave("resources/mokugyo.wav");
	//sound.PlayerWave(se);

	const uint32_t kMaxSprite = 5;
	std::vector<Sprite*> sprites;
	for (uint32_t i = 0; i < kMaxSprite; ++i) {
		Sprite* sprite = new Sprite();
		std::string texture = "resources/uvChecker.png";
		if (i % 2 == 0) {
			texture = "resources/uvChecker.png";
		} else {
			texture = "resources/monsterBall.png";
		}
		sprite->Initialize(spriteCommon, texture);
		sprite->SetPos({ i * 200.0f,0.0f });
		sprite->SetSize({ 100.0f,100.0f });
		sprite->SetTextureLeftTop({ 0.0f,0.0f });
		sprite->SetTextureSize({ 256.0f,256.0f });
		sprites.push_back(sprite);
	}

	Object3d* object3d = new Object3d();
	object3d->Initialize(object3dCommon);

	// 3Dモデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon);

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3d->SetModel("plane.obj");
	object3d->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d->SetCamera(camera);

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

#pragma region ImGui

		Vector3 translate = object3d->GetTranslate();
		Vector3 rotate = object3d->GetRotate();
		Vector3 scale = object3d->GetScale();

		Vector3 cameraTranslate = camera->GetTranslate();
		Vector3 cameraRotate = camera->GetRotate();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Window");
		//ImGui::Checkbox("useMonsterBall", &useMonsterBall);
		//ImGui::ColorEdit3("Color", &materialData->color->x); // RGBA
		if (ImGui::TreeNode("camera")) {
			ImGui::DragFloat3("Translate", &cameraTranslate.x, 0.1f);
			ImGui::DragFloat3("Rotate", &cameraRotate.x, 0.01f);

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("object")) {
			ImGui::DragFloat3("Translate", &translate.x, 0.01f);
			ImGui::DragFloat3("Rotate", &rotate.x, 0.01f);
			ImGui::DragFloat3("Scale", &scale.x, 0.01f);
			ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);
			ImGui::ColorEdit4("ObjectColor", &materialData->color.x);


			/*static const char* kBlendModeNames[] = {
				"kBlendModeNone",
				"kBlendModeNormal",
				"kBlendModeAdd",
				"kBlendModeSubtract",
				"kBlendModeMultiply",
				"kBlendModeScreen",
				"kCountOfBlendMode"
			};
			
			ImGui::Combo("BlendMode", &blendMode ,kBlendModeNames,7);*/
				//"kBlendModeNone\0kBlendModeNormal\0kBlendModeAdd\0kBlendModeSubtract\0kBlendModeMultiply\0kBlendModeScreen\0kCountOfBlendMode\0");
			ImGui::TreePop();
		}
		
		ImGui::End();

		// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		object3d->SetTranslate(translate);
		object3d->SetRotate(rotate);
		object3d->SetScale(scale);

		camera->SetTranslate(cameraTranslate);
		camera->SetRotate(cameraRotate);

#pragma endregion

		
		ImGuiIO& io = ImGui::GetIO();
		float wheelDelta = io.MouseWheel;
		
		camera->Update();

		object3d->Update();

		/* Spriteクラス 呼び出しの例
		Vector2 position = sprite->GetPos();
		position.x += 0.1f;
		position.y += 0.1f;
		sprite->SetPos(position);

		float rotation = sprite->GetRotation();
		rotation += 0.01f;
		sprite->SetRotation(rotation);

		Vector4 color = sprite->GetColor();
		color.x += 0.01f;
		if (color.x > 1.0f) {
			color.x -= 1.0f;
		}
		sprite->SetColor(color);
		
		Vector2 size = sprite->GetSize();
		size.x += 0.01f;
		size.y += 0.01f;
		sprite->SetSize(size);

		for (Sprite* sprite : sprites) {
			sprite->Update();
		}
		*/

		for (uint32_t i = 0; i < sprites.size(); ++i) {
			sprites[i]->Update();
			float rotation = sprites[i]->GetRotation();
			//rotation += 0.01f;
			sprites[i]->SetRotation(rotation);
		}
		

		ImGui::Render();

		// 描画前処理
		dxCommon->PreDraw();

		// 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
		object3dCommon->CommonDrawSetting();

		object3d->Draw();
	
		
		// スプライト描画

		spriteCommon->CommonDrawSetting();

		for (Sprite* sprite : sprites) {
			sprite->Draw();
		}

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

	TextureManager::GetInstance()->Finalize();

	// 3Dモデルマネージャーの終了
	ModelManager::GetInstance()->Finalize();

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// WindowAPIの終了処理
	winApp->Finalize();
	// WindowWPIの解放
	delete winApp;
	winApp = nullptr;

	delete spriteCommon;
	spriteCommon = nullptr;
	
	for (Sprite* sprite : sprites) {
		delete sprite;
		sprite = nullptr;
	}
	sprites.clear();

	delete object3d;
	object3d = nullptr;

	return 0;
}