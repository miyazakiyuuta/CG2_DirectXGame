#pragma once
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#include <wrl.h>
#include <vector>
#include <fstream>
#include <cassert>
#include <cstring>

// 音声データ
struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファ
	std::vector<BYTE> buffer;
};


class SoundManager {
public:
	static SoundManager* GetInstance();

	void Initialize();
	void Finalize();

	SoundData LoadFile(const std::string& filename); // wavファイル読み込み
	void Unload(SoundData* soundData); // バッファ解放
	void PlayerWave(const SoundData& soundData); // 再生

private:
	static SoundManager* instance;

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_;
};

