#include "CatmullRomSpline.h"
#include "math/mathFunction.h"
#include <algorithm>

void CatmullRomSpline::SetControlPoints(const std::vector<Vector3>& points) {
	controlPoints_ = points;
	arcLengths_.clear();
	totalLength_ = 0.0f;

	// 制御点が2個未満なら曲線を作れないのでテーブルは構築しない
	if (controlPoints_.size() < 2) {
		return;
	}

	// スプライン全体を等分割サンプリングし、隣接サンプル間の直線距離を積算して
	// 「累積距離→t」の変換テーブルを作る（弧長の逆関数は解析的に解けないため事前計算する）
	const int segmentCount = static_cast<int>(controlPoints_.size()) - 1;
	const int sampleCount = segmentCount * kSamplesPerSegment + 1;

	arcLengths_.reserve(sampleCount);
	arcLengths_.push_back(0.0f);

	Vector3 prevPos = GetPosition(0.0f);
	for (int i = 1; i < sampleCount; ++i) {
		float t = static_cast<float>(i) / static_cast<float>(sampleCount - 1);
		Vector3 pos = GetPosition(t);
		totalLength_ += (pos - prevPos).Length();
		arcLengths_.push_back(totalLength_);
		prevPos = pos;
	}
}

Vector3 CatmullRomSpline::GetPosition(float t) const {
	// エッジケース: 制御点0個はゼロベクトル、1個はその点をそのまま返す
	if (controlPoints_.empty()) {
		return Vector3{};
	}
	if (controlPoints_.size() == 1) {
		return controlPoints_[0];
	}

	t = std::clamp(t, 0.0f, 1.0f);

	// 全体t → 区間番号 + 区間内t に変換
	const int segmentCount = static_cast<int>(controlPoints_.size()) - 1;
	float scaledT = t * static_cast<float>(segmentCount);
	int index = std::min(static_cast<int>(scaledT), segmentCount - 1); // t=1.0 のとき最終区間に収める
	float localT = scaledT - static_cast<float>(index);

	// 4制御点を選ぶ。端の区間は隣が無いので端点を複製して代用する（定番の端点処理）
	const int lastIndex = static_cast<int>(controlPoints_.size()) - 1;
	const Vector3& p0 = controlPoints_[std::max(index - 1, 0)];
	const Vector3& p1 = controlPoints_[index];
	const Vector3& p2 = controlPoints_[index + 1];
	const Vector3& p3 = controlPoints_[std::min(index + 2, lastIndex)];

	return CatmullRomInterpolation(p0, p1, p2, p3, localT);
}

Vector3 CatmullRomSpline::GetPositionByDistance(float distance) const {
	return GetPosition(DistanceToT(distance));
}

Vector3 CatmullRomSpline::GetTangentByDistance(float distance) const {
	// 距離を前後に少しずらした2点の差分から進行方向を求める
	const float kDelta = 0.01f; // 差分幅[m]。レール規模(数百m)に対して十分小さい値
	Vector3 front = GetPositionByDistance(distance + kDelta);
	Vector3 back = GetPositionByDistance(distance - kDelta);

	Vector3 diff = front - back;
	// 端でクランプされ2点が一致した場合などのゼロ除算を防ぐ
	if (diff.LengthSquared() <= 0.0f) {
		return Vector3{ 0.0f, 0.0f, 1.0f };
	}
	return diff.Normalize();
}

float CatmullRomSpline::DistanceToT(float distance) const {
	// テーブル未構築（制御点2個未満）なら先頭を返す
	if (arcLengths_.size() < 2 || totalLength_ <= 0.0f) {
		return 0.0f;
	}

	distance = std::clamp(distance, 0.0f, totalLength_);

	// 二分探索で distance を挟むサンプル区間を特定する
	auto it = std::lower_bound(arcLengths_.begin(), arcLengths_.end(), distance);
	if (it == arcLengths_.begin()) {
		return 0.0f;
	}
	size_t upper = static_cast<size_t>(std::distance(arcLengths_.begin(), it));
	size_t lower = upper - 1;

	// サンプル区間内は線形補間で t を復元する
	float segmentLength = arcLengths_[upper] - arcLengths_[lower];
	float ratio = (segmentLength > 0.0f) ? (distance - arcLengths_[lower]) / segmentLength : 0.0f;

	// サンプルは全体tの等分割なので、添字→t の変換は (添字 + 割合) / (サンプル数-1)
	float sampleT = (static_cast<float>(lower) + ratio) / static_cast<float>(arcLengths_.size() - 1);
	return sampleT;
}
