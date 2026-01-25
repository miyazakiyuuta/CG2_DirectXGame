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
#include "SrvManager.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "ImGuiManager.h"

#include <imgui.h>

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

	SrvManager* srvManager = nullptr;
	srvManager = new SrvManager();
	srvManager->Initialize(dxCommon);

	TextureManager::GetInstance()->Initialize(dxCommon, srvManager);

	SpriteCommon* spriteCommon = nullptr;
	// スプライト共通部の初期化
	spriteCommon = new SpriteCommon;
	spriteCommon->Initialize(dxCommon, srvManager);

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
	camera->InitializeGPU(device);
	camera->SetRotate({ 0.3f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,4.0f,-10.0f });
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });

	// TextureManager からテクスチャを読み込む
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture("resources/uvChecker.png");
	textureManager->LoadTexture("resources/monsterBall.png");

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

	std::string testTexture = "resources/monsterBall.png";
	Sprite* testSprite = new Sprite();
	testSprite->Initialize(spriteCommon, testTexture);
	testSprite->SetPos({ 100.0f,100.0f });
	testSprite->SetSize({ 500.0f,500.0f });
	testSprite->SetAnchorPoint({ 0.0f,0.0f });
	testSprite->SetTextureSize({ 1200.0f,600.0f });

	Object3d* object3d = new Object3d();
	object3d->Initialize(object3dCommon);

	// 3Dモデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon, srvManager);

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3d->SetModel("plane.obj");
	object3d->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d->SetCamera(camera);

	ModelManager::GetInstance()->LoadModel("sphere.obj");
	Object3d* monsterBall = new Object3d();
	monsterBall->Initialize(object3dCommon);
	monsterBall->SetModel("sphere.obj");
	monsterBall->SetRotate({ 0.0f, -std::numbers::pi_v<float> / 2.0f, 0.0f });

	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);
	ParticleManager::GetInstance()->SetCamera(camera);

	ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");

	ParticleEmitter* testParticle = new ParticleEmitter(dxCommon, srvManager, camera);
	testParticle->transform_ = {};
	testParticle->name_ = "test";
	testParticle->count_ = 10;
	testParticle->frequencyTime_ = 1.0f;

	ImGuiManager* imGuiManager = nullptr;
	imGuiManager = new ImGuiManager();
	imGuiManager->Initialize(winApp, dxCommon, srvManager);


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

		imGuiManager->Begin();

#ifdef USE_IMGUI
		// デモウィンドウ(使い方紹介)
		ImGui::ShowDemoWindow();

		Vector2 testSpritePos = testSprite->GetPos();

		ImGui::Begin("Window");

		camera->DrawImGui();

		if (ImGui::TreeNode("Sprite")) {
			ImGui::DragFloat2("position", &testSpritePos.x, 1.0f, 0.0f, 0.0f, "%4.3f");
			ImGui::TreePop();
		}

		ImGui::End();

		testSprite->SetPos(testSpritePos);
#endif

		/*
#pragma region ImGui

		Vector3 translate = object3d->GetTranslate();
		Vector3 rotate = object3d->GetRotate();
		Vector3 scale = object3d->GetScale();

		Vector3 monsterBallTranslate = monsterBall->GetTranslate();
		Vector3 monsterBallRotate = monsterBall->GetRotate();
		Vector3 monsterBallScale = monsterBall->GetScale();

		Vector4 lightColor = monsterBall->GetLightColor();
		Vector3 lightDirection = monsterBall->GetLightDirection();
		float lightIntensity = monsterBall->GetLightIntensity();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Window");
		if (ImGui::TreeNode("object")) {
			ImGui::DragFloat3("Translate", &translate.x, 0.01f);
			ImGui::DragFloat3("Rotate", &rotate.x, 0.01f);
			ImGui::DragFloat3("Scale", &scale.x, 0.01f);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("monsterBall")) {
			ImGui::DragFloat3("Translate", &monsterBallTranslate.x, 0.01f);
			ImGui::DragFloat3("Rotate", &monsterBallRotate.x, 0.01f);
			ImGui::DragFloat3("Scale", &monsterBallScale.x, 0.01f);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("MonsterBallLight")) {
			ImGui::DragFloat4("LightColor", &lightColor.x, 0.01f);
			ImGui::DragFloat3("LightDirection", &lightDirection.x, 0.01f);
			ImGui::DragFloat("LightIntensity", &lightIntensity, 0.01f);
			ImGui::TreePop();
		}
		ImGui::End();

		camera->ImGui();

		// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		object3d->SetTranslate(translate);
		object3d->SetRotate(rotate);
		object3d->SetScale(scale);

		monsterBall->SetTranslate(monsterBallTranslate);
		monsterBall->SetRotate(monsterBallRotate);
		monsterBall->SetScale(monsterBallScale);

		monsterBall->SetLightColor(lightColor);
		monsterBall->SetLightDirection(lightDirection);
		monsterBall->SetLightIntensity(lightIntensity);

#pragma endregion


		ImGuiIO& io = ImGui::GetIO();
		float wheelDelta = io.MouseWheel;
		*/

		

		camera->Update();
		camera->TransferToGPU();

		object3d->Update();

		monsterBall->Update();
		//monsterBall->SetRotate({ 0.0f,monsterBall->GetRotate().y + 0.02f,0.0f });

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

		testSprite->Update();

		testParticle->Update(1.0f / 60.0f);

		ParticleManager::GetInstance()->Update(1.0f / 60.0f); // すべてのパーティクルの更新

		imGuiManager->End();

		// 描画前処理
		dxCommon->PreDraw();
		srvManager->PreDraw();

		// 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
		object3dCommon->CommonDrawSetting();

		//object3d->Draw();

		monsterBall->Draw();

		// スプライト描画
		spriteCommon->CommonDrawSetting();

		/*for (Sprite* sprite : sprites) {
			sprite->Draw();
		}*/
		testSprite->Draw();

		ParticleManager::GetInstance()->Draw();

		imGuiManager->Draw();

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
	//ImGui_ImplDX12_Shutdown();
	//ImGui_ImplWin32_Shutdown();
	//ImGui::DestroyContext();

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

	delete srvManager;
	srvManager = nullptr;

	imGuiManager->Finalize();
	delete imGuiManager;
	imGuiManager = nullptr;

	return 0;
}