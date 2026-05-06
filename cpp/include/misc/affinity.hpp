#pragma once

#include <thread>
#include <vector>

#include <pthread.h>
#include <sched.h>

namespace misc {

class Affinity {
public:
  // Set current thread to single core
  static bool pin_to_core(unsigned int core_id) {
    return pin_to_cores({core_id});
  }

  // Set current thread to multiple cores
  static bool pin_to_cores(const std::vector<unsigned int> &cores) {
    if (!validate_cores(cores))
      return false;
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), make_cpuset(cores)) == 0;
  }

  // Set specific thread to single core
  static bool pin_thread(std::thread::native_handle_type thread, unsigned int core_id) {
    return pin_thread_cores(thread, {core_id});
  }

  // Set specific thread to multiple cores
  static bool pin_thread_cores(std::thread::native_handle_type thread, const std::vector<unsigned int> &cores) {
    if (!validate_cores(cores))
      return false;
    return pthread_setaffinity_np(thread, sizeof(cpu_set_t), make_cpuset(cores)) == 0;
  }

  // Get number of CPU cores
  static unsigned int core_count() {
    unsigned int count = std::thread::hardware_concurrency();
    return count > 0 ? count : 1;
  }

  // Check if affinity is supported
  static bool supported() { return true; }

private:
  static bool validate_cores(const std::vector<unsigned int> &cores) {
    if (cores.empty())
      return false;
    unsigned int max_cores = core_count();
    for (unsigned int core : cores) {
      if (core >= max_cores)
        return false;
    }
    return true;
  }

  static cpu_set_t *make_cpuset(const std::vector<unsigned int> &cores) {
    static cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (unsigned int core : cores) {
      CPU_SET(core, &cpuset);
    }
    return &cpuset;
  }
};

} // namespace misc
