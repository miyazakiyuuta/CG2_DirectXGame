#include "SoundManager.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

#include "utility/StringUtility.h"

using namespace Microsoft::WRL;
using namespace StringUtility;

SoundManager* SoundManager::instance = nullptr;

SoundManager* SoundManager::GetInstance() {
	if (!instance)instance = new SoundManager();
	return instance;
}

void SoundManager::Initialize() {
	HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	comInitialized_ = SUCCEEDED(result); // フラグを保存
	assert(comInitialized_);

	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));
	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(result));
	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));

	XAUDIO2_VOICE_DETAILS masterDetails{};
	masterVoice_->GetVoiceDetails(&masterDetails);

	result = xAudio2_->CreateSubmixVoice(
		&bgmSubmixVoice_,
		masterDetails.InputChannels,
		masterDetails.InputSampleRate);
	assert(SUCCEEDED(result));

	result = xAudio2_->CreateSubmixVoice(
		&seSubmixVoice_,
		masterDetails.InputChannels,
		masterDetails.InputSampleRate);
	assert(SUCCEEDED(result));
}

void SoundManager::Finalize() {

	for (auto& activeVoice : activeVoices_) {
		if (activeVoice.pVoice) {
			activeVoice.pVoice->Stop();
			activeVoice.pVoice->DestroyVoice();
		}
	}
	activeVoices_.clear();

	UnloadAll();

	if (bgmSubmixVoice_) { bgmSubmixVoice_->DestroyVoice(); bgmSubmixVoice_ = nullptr; }
	if (seSubmixVoice_) { seSubmixVoice_->DestroyVoice();  seSubmixVoice_ = nullptr; }

	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}

	if (xAudio2_) {
		xAudio2_->StopEngine();
		xAudio2_.Reset();
	}

	HRESULT result;
	// Windows Media Foundationの終了
	result = MFShutdown();
	assert(SUCCEEDED(result) && "MFShutdown failed");

	if (comInitialized_) {
		CoUninitialize();
		comInitialized_ = false;
	}

	delete instance;
	instance = nullptr;
}

void SoundManager::Update(float deltaTime) {
	for (auto it = activeVoices_.begin(); it != activeVoices_.end(); ) {
		// フェード処理
		if (it->fadeSpeed > 0.0f) {
			float current = 0.0f;
			it->pVoice->GetVolume(&current);

			float diff = it->fadeTargetVolume - current;
			float step = it->fadeSpeed * deltaTime;

			if (std::abs(diff) <= step) {
				// 目標音量に到達
				it->pVoice->SetVolume(it->fadeTargetVolume);
				it->fadeSpeed = 0.0f;

				// フェードアウト完了 → 停止して削除
				if (it->stopOnFadeOut) {
					it->pVoice->Stop();
					it->pVoice->DestroyVoice();
					it = activeVoices_.erase(it);
					continue;
				}
			} else {
				// 目標に向けて少しずつ変化
				it->pVoice->SetVolume(current + (diff > 0 ? step : -step));
			}
		}

		// 再生完了チェック（従来のまま）
		if (it->callback->isFinished) {
			it->pVoice->DestroyVoice();
			it = activeVoices_.erase(it);
		} else {
			++it;
		}
	}
}

SoundData SoundManager::LoadFile(const std::string& filename) {

	auto it = soundCache_.find(filename);
	if (it != soundCache_.end()) {
		return it->second;
	}

	// フルパスをワイド文字列に変換
	std::wstring filePathW = ConvertString(filename);
	HRESULT result;

	// SourceReader作成
	ComPtr<IMFSourceReader> pReader;
	result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
	assert(SUCCEEDED(result) && "MFCreateSourceReaderFromURL failed: ファイルが存在しないか形式が非対応");

	// PCM形式にフォーマット指定する
	ComPtr<IMFMediaType> pPCMType;
	MFCreateMediaType(&pPCMType);
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	result = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCMType.Get());
	assert(SUCCEEDED(result));

	// 実際にセットされたメディアタイプを取得する
	ComPtr<IMFMediaType> pOutType;
	pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);

	// Waveフォーマットを取得する
	WAVEFORMATEX* waveFormat = nullptr;
	MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);

	// コンテナに格納する音声データ
	SoundData soundData = {};
	soundData.wfex = *waveFormat;
	
	// 生成したWaveフォーマットを解放
	CoTaskMemFree(waveFormat);

	// PCMデータのバッファを構築
	while (true) {
		ComPtr<IMFSample> pSample;
		DWORD streamIndex = 0, flags = 0;
		LONGLONG llTimeStamp = 0;
		// サンプルを読み込む
		result = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
		// ストリームの末尾に達したら抜ける
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;

		if (pSample) {
			ComPtr<IMFMediaBuffer> pBuffer;
			// サンプルに含まれるサウンドデータのバッファを人つなぎにして取得
			pSample->ConvertToContiguousBuffer(&pBuffer);

			BYTE* pData = nullptr; // データ読み取り用ポインタ
			DWORD maxLength = 0, currentLength = 0;
			// バッファ読み込み用にロック
			pBuffer->Lock(&pData, &maxLength, &currentLength);
			// バッファの末尾にデータを追加
			soundData.buffer.insert(soundData.buffer.end(), pData, pData + currentLength);
			pBuffer->Unlock();
		}
	}

	soundCache_[filename] = soundData;
	return soundData;
}

void SoundManager::Unload(const std::string& filename) {
	soundCache_.erase(filename);
}

void SoundManager::UnloadAll() {
	soundCache_.clear();
}

SoundManager::SoundHandle SoundManager::PlayWave(const SoundData& soundData, bool loop, SoundCategory category) {
	if (soundData.buffer.empty()) return InvalidHandle;

	auto callbackPtr = std::make_unique<VoiceCallback>();
	VoiceCallback* rawCallback = callbackPtr.get();

	// 波形フォーマットをもとにSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;

	// 出力先をカテゴリに応じて切り替える
	IXAudio2SubmixVoice* targetSubmix =
		(category == SoundCategory::BGM) ? bgmSubmixVoice_ : seSubmixVoice_;

	XAUDIO2_SEND_DESCRIPTOR sendDesc = { 0, targetSubmix };
	XAUDIO2_VOICE_SENDS sendList = { 1, &sendDesc };

	HRESULT result = xAudio2_->CreateSourceVoice(
		&pSourceVoice, &soundData.wfex,
		0, XAUDIO2_DEFAULT_FREQ_RATIO, rawCallback,
		&sendList); // ← 出力先を指定
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	assert(SUCCEEDED(result) && "SubmitSourceBuffer failed");
	result = pSourceVoice->Start();
	assert(SUCCEEDED(result) && "Start failed");

	SoundHandle handle = nextHandle_++;
	activeVoices_.push_back({ handle, pSourceVoice, std::move(callbackPtr) });
	return handle;
}

void SoundManager::StopWave(SoundHandle handle) {
	for (auto it = activeVoices_.begin(); it != activeVoices_.end(); ++it) {
		if (it->handle == handle) {
			it->pVoice->Stop();
			it->pVoice->DestroyVoice();
			activeVoices_.erase(it);
			return;
		}
	}
}

void SoundManager::FadeIn(SoundHandle handle, float duration) {
	if (duration <= 0.0f) return;
	for (auto& av : activeVoices_) {
		if (av.handle == handle) {
			av.pVoice->SetVolume(0.0f); // 無音からスタート
			av.fadeTargetVolume = 1.0f;
			av.fadeSpeed = 1.0f / duration;
			av.stopOnFadeOut = false;
			return;
		}
	}
}

void SoundManager::FadeOut(SoundHandle handle, float duration) {
	if (duration <= 0.0f) return;
	for (auto& av : activeVoices_) {
		if (av.handle == handle) {
			av.fadeTargetVolume = 0.0f;
			av.fadeSpeed = 1.0f / duration;
			av.stopOnFadeOut = true; // 完了後に自動停止
			return;
		}
	}
}

void SoundManager::SetVolume(SoundHandle handle, float volume) {
	for (auto& av : activeVoices_) {
		if (av.handle == handle) {
			av.pVoice->SetVolume(volume);
			return;
		}
	}
}

bool SoundManager::IsPlaying(SoundHandle handle) const {
	for (const auto& av : activeVoices_) {
		if (av.handle == handle) return true;
	}
	return false;
}

void SoundManager::SetCategoryVolume(SoundCategory category, float volume) {
	if (category == SoundCategory::BGM && bgmSubmixVoice_) {
		bgmSubmixVoice_->SetVolume(volume);
	} else if (category == SoundCategory::SE && seSubmixVoice_) {
		seSubmixVoice_->SetVolume(volume);
	}
}

float SoundManager::GetCategoryVolume(SoundCategory category) const {
	float volume = 1.0f;
	if (category == SoundCategory::BGM && bgmSubmixVoice_) {
		bgmSubmixVoice_->GetVolume(&volume);
	} else if (category == SoundCategory::SE && seSubmixVoice_) {
		seSubmixVoice_->GetVolume(&volume);
	}
	return volume;
}
