#pragma once

class Camera;

/// <summary>
/// パーティクルの仕組みのインターフェース。CPU版(ParticleManager)とGPU版(GPUParticleManager)が実装する。
/// Framework(毎フレームの更新・描画)とシーン(描画に使うカメラの受け渡し)は、この型のポインタで両方をまとめて扱う
/// (「CPU版の後にGPU版」と同じ呼び出しを2行ずつ並べない)。
///
/// 基底に置かないもの:
///  - 発生のさせ方: CPU版はEmit/RegisterEffectで単発も出せるが、GPU版はエミッタの継続放出だけ。共通の機能ではない
///  - DispatchAll(計算シェーダの実行): GPU版にしか無い。CPU版に「何もしないDispatch」を持たせるのは、
///    飛べない鳥に何もしないFlyを持たせるのと同じ誤りなので、Frameworkが具象の型のまま呼ぶ
///  - 初期化・終了: 引数は同じでも、Frameworkの初期化・解放の順番そのものに意味があるので、1つずつ書いた方が読める
/// </summary>
class IParticleSystem {
public:
	// 基底のポインタで扱うので仮想デストラクタにする(実体の解放は各シングルトンのFinalizeが行う)
	virtual ~IParticleSystem() = default;

	// 粒のシミュレーション(Frameworkが毎フレーム、シーンの更新でカメラが確定した後に呼ぶ)
	virtual void Update(float deltaTime) = 0;

	// 粒を描く。半透明・加算なのでシーンの不透明物の後に呼ぶ(Frameworkが毎フレーム呼ぶ)
	virtual void Draw() = 0;

	// 描画に使うカメラ。nullptrなら描かない(シーンを抜けるときは必ずnullptrへ戻す。解放済みのカメラを読ませないため)
	virtual void SetCamera(const Camera* camera) = 0;
};
