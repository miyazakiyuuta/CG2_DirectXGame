#include "direction/PhasedSequence.h"

#include "effect/ParticleManager.h"

void PhasedSequence::Update(float deltaTime, const Camera& camera) {
	// 始まる前と終わった後は何も触らない。終わった後にシーンが結果画面への遷移を頼むと、
	// ChangeSceneが全ポストエフェクトを切る。ここで点け直すと次のシーンへ点いたまま渡ってしまう
	if (!IsRunning()) {
		return;
	}

	// 時計を進める。段階の長さはすべて実時間なので、スローモーションの間も演出の長さは変わらない
	phaseTimer_ += deltaTime;
	elapsed_ += deltaTime;

	// 段階を進める(入った瞬間の出来事=爆発なども、ここで派生クラスが起こす)
	AdvancePhase();

	// ゲーム時間の倍率。シーンは次のフレームから、自機・敵・弾のdeltaTimeにもこの倍率を掛ける
	// (1フレーム(約16ms)の遅れは体感できない)
	timeScale_ = ComputeTimeScale();
	// 粒にも同じ倍率を掛け、スロー中は煙と火花も遅くする。ただしヒットストップ(倍率0)の間は等速のまま流す:
	// 通常の被弾と同じく「止まった画面の中で火花だけが散る」見せ方にするため
	ParticleManager::GetInstance()->SetTimeScale(IsHitStop() ? 1.0f : timeScale_);

	UpdateContents(deltaTime, camera);

	// 終わったフレームも、その時点の状態をここで1度だけ書き込む(次のフレームからは上の早期returnで触らない)
	ApplyPostEffects(camera);
}

void PhasedSequence::ResetClocks() {
	phaseTimer_ = 0.0f;
	elapsed_ = 0.0f;
	timeScale_ = 1.0f;
}
