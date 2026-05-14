#pragma once
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#include <wrl.h>
#include <vector>
#include <string>
#include <list>
#include <memory>
#include <atomic>
#include <cassert>

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
	void Update();

	SoundData LoadFile(const std::string& filename); // wavファイル読み込み
	void Unload(SoundData* soundData); // バッファ解放
	void PlayWave(const SoundData& soundData); // 再生

private:
	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	static SoundManager* instance;

	class VoiceCallback : public IXAudio2VoiceCallback {
	public:
		std::atomic<bool> isFinished{ false };
		void STDMETHODCALLTYPE OnStreamEnd() override { isFinished = true; }
		void STDMETHODCALLTYPE OnVoiceProcessingPassEnd()    override {}
		void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
		void STDMETHODCALLTYPE OnBufferEnd(void*)            override {}
		void STDMETHODCALLTYPE OnBufferStart(void*)          override {}
		void STDMETHODCALLTYPE OnLoopEnd(void*)              override {}
		void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT)  override {}
	};

	struct ActiveVoice {
		IXAudio2SourceVoice* pVoice = nullptr;
		std::unique_ptr<VoiceCallback> callback;
	};

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::list<ActiveVoice> activeVoices_;
	bool comInitialized_ = false;
};

