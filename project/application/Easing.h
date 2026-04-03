#pragma once
#include <algorithm>

namespace Easing{

	inline float Clamp01(float t){
		return std::clamp(t, 0.0f, 1.0f);
	}

	inline float Linear(float t){
		return Clamp01(t);
	}

	inline float EaseOutQuad(float t){
		t = Clamp01(t);
		return 1.0f - (1.0f - t) * (1.0f - t);
	}

	inline float EaseInOutCubic(float t){
		t = Clamp01(t);
		if(t < 0.5f){
			return 4.0f * t * t * t;
		}
		float u = -2.0f * t + 2.0f;
		return 1.0f - (u * u * u) / 2.0f;
	}

	inline float EaseInQuad(float t){
		t = Clamp01(t);
		return t * t;
	}

	inline float EaseInCubic(float t){
		t = Clamp01(t);
		return t * t * t;
	}

	inline float EaseInQuart(float t){
		t = Clamp01(t);
		return t * t * t * t;
	}

	inline float EaseInQuint(float t){
		t = Clamp01(t);
		return t * t * t * t * t;
	}

	inline float EaseInExpo(float t){
		t = Clamp01(t);
		return (t <= 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
	}

	inline float EaseInBack(float t){
		t = Clamp01(t);

		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;

		return c3 * t * t * t - c1 * t * t;
	}
}
