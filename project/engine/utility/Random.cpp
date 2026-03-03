#include "Random.h"
#include <random>

namespace Random {

	std::mt19937& GetEngine() {
		thread_local std::random_device seedGenerator;
		thread_local std::mt19937 engine(seedGenerator());
		return engine;
	}

	int GetInt(int min, int max) {
		std::uniform_int_distribution<int> distribution(min, max);
		return distribution(GetEngine());
	}

	float GetFloat(float min, float max) {
		std::uniform_real_distribution<float> distribution(min, max);
		return distribution(GetEngine());
	}
}