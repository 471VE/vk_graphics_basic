#include "random_array.h"
#include <random>

std::vector<double> generateRandomArray(int seed, size_t size) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0, 1);

    std::vector<double> randomArray(size);
    for (size_t i = 0; i < size; ++i) {
        randomArray[i] = dist(gen);
    }
    return randomArray;
}