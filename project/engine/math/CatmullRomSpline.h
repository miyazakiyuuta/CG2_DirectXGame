#pragma once
#include "math/Vector3.h"
#include <vector>

// Catmull-Romスプライン曲線
// 制御点列を管理し、tベース(0〜1)と距離ベース(先頭から◯m)の2通りで曲線上の点を評価できる。
// 距離ベース評価のために、制御点設定時に「累積距離→t」の変換テーブル（アークレングス・テーブル）を事前構築する。
// レール移動（等速で毎秒◯m進む）や railDistance 指定の敵出現に使う想定。
class CatmullRomSpline {
public:
	// 制御点を設定し、距離テーブルを構築する（制御点を変更するたびに呼ぶこと）
	void SetControlPoints(const std::vector<Vector3>& points);

	// スプライン全体を 0〜1 で評価する（tは区間ごとに等分割。速度は制御点間隔に依存する）
	Vector3 GetPosition(float t) const;

	// 先頭からの距離で評価する（制御点間隔によらず等間隔に進む）
	Vector3 GetPositionByDistance(float distance) const;

	// 進行方向の単位ベクトルを距離で取得する（レールカメラの向きなどに使う）
	Vector3 GetTangentByDistance(float distance) const;

	// スプライン全長
	float GetTotalLength() const { return totalLength_; }

	// --- 区間(制御点 i と i+1 の間)ごとの距離 ---
	// 制御点を足し引き・移動したとき、「曲線上のどのあたりか」を区間番号と割合で覚えておき、
	// 新しい曲線の距離へ戻す、といった使い方のためのもの

	// 区間の数(制御点の数-1)。制御点が2個未満なら0
	size_t GetSegmentCount() const;
	// segment番の区間が始まる距離[m](先頭からの累積)。範囲外の番号は端へ丸める
	float GetSegmentStartDistance(size_t segment) const;
	// segment番の区間の長さ[m]。範囲外の番号は端へ丸める
	float GetSegmentLength(size_t segment) const;

	const std::vector<Vector3>& GetControlPoints() const { return controlPoints_; }

private:
	// 距離テーブルを二分探索し、距離に対応する全体t(0〜1)を返す
	float DistanceToT(float distance) const;

	std::vector<Vector3> controlPoints_;

	// 距離テーブル: arcLengths_[i] = サンプル点i までの累積距離。
	// サンプルは全体tの等分割なので、添字から t を復元できる（t = i / (サンプル数-1)）
	std::vector<float> arcLengths_;
	float totalLength_ = 0.0f;

	// 1区間（制御点間）あたりのサンプル分割数。多いほど距離変換が正確になるが構築コストが増える
	static constexpr int kSamplesPerSegment = 32;
};
