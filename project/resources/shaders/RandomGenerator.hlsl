#include "Random.hlsl"

class RandomGenerator
{
    float3 seed;

    float3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float Generate1d()
    {
        float result = rand3dTo1d(seed);
        seed.x = result;
        return result;
    }
};