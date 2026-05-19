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
#include <unordered_map>

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

	using SoundHandle = uint32_t;
	static constexpr SoundHandle InvalidHandle = 0;

	enum class SoundCategory {
		BGM,
		SE
	};

	SoundData LoadFile(const std::string& filename); // wavファイル読み込み
	void Unload(const std::string& filename); // 特定ファイルをキャッシュから削除
	void UnloadAll(); // 全キャッシュ削除
	SoundHandle PlayWave(const SoundData& soundData, bool loop = false, SoundCategory category = SoundCategory::SE); // 再生

	void  StopWave(SoundHandle handle);
	void  SetVolume(SoundHandle handle, float volume);
	bool  IsPlaying(SoundHandle handle) const;

	void SetCategoryVolume(SoundCategory category, float volume); // 0.0f〜1.0f
	float GetCategoryVolume(SoundCategory category) const;

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
		SoundHandle handle = InvalidHandle;
		IXAudio2SourceVoice* pVoice = nullptr;
		std::unique_ptr<VoiceCallback> callback;
	};

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::list<ActiveVoice> activeVoices_;
	bool comInitialized_ = false;

	IXAudio2SubmixVoice* bgmSubmixVoice_ = nullptr;
	IXAudio2SubmixVoice* seSubmixVoice_ = nullptr;

	uint32_t nextHandle_ = 1;

	std::unordered_map<std::string, SoundData> soundCache_;
};

