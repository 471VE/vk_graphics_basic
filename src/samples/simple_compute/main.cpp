#include <chrono>

#include "random_array.h"
#include "simple_compute.h"

#define ARRAY_SIZE 100000000
#define RANDOM_SEED 42

void realizationOnCPU() {
  auto arr = generateRandomArray(RANDOM_SEED, ARRAY_SIZE);

  auto start = hr_clock::now();
  double average = 0;
  for (int i = 0; i < ARRAY_SIZE; ++i) {
    double new_val = 0.;
    for (int j = -3; j <=3; ++j) {
      int idx = i - j;
      if (idx >= 0 && idx < ARRAY_SIZE) {
        new_val += arr[idx];
      }
    }
    average += (arr[i] - new_val / 7.);
  }
  auto finish = hr_clock::now();

  average /= double(ARRAY_SIZE);
  std::cout << "Result on CPU: " << average << "." << std::endl;
  std::cout << "CPU time: " << (finish - start).count() / 1e9 << " s." << std::endl;
}

int main()
{
  constexpr int VULKAN_DEVICE_ID = 0;
  std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(ARRAY_SIZE, RANDOM_SEED);
  if(app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return 1;
  }

  app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

  std::cout << "\nArray size: " << ARRAY_SIZE << std::endl << std::endl;

  app->Execute();

  std::cout << std::endl;

  realizationOnCPU();

  return 0;
}
