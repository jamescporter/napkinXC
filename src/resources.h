// Time & resources utils

#include <chrono>
#include <thread>

struct Resources {
    std::chrono::steady_clock::time_point timePoint;
    double cpuTime;
    double userCpuTime;
    double systemCpuTime;
    double currentRealMem;
    double peakRealMem;
    double currentVirtualMem;
    double peakVirtualMem;
    double dataMemory;
    double stackMemory;
};

// Returns Resources structure
Resources getResources();

// Returns number of available cpus
int getCpuCount();

// Returns size of available RAM
unsigned long long getSystemMemory();
