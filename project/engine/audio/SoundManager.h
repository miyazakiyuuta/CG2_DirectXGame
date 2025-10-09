#pragma once
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#include <wrl.h>

// 音声データ
struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBufer;
	// バッファのサイズ
	unsigned int bufferSize;
};


class SoundManager {
public:
	void Initialize();
	void Finalize();

	SoundData LoadWave(const char* filename); // wavファイル読み込み
	void Unload(SoundData* soundData); // バッファ解放
	void PlayerWave(const SoundData& soundData); // 再生

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_;
};

