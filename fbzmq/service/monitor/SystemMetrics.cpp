// Copyright 2004-present Facebook. All Rights Reserved.

#include "SystemMetrics.h"

namespace fbzmq {

/* Return RSS memory the process currently used from /proc/[pid]/status.
 / The /proc is a pseudo-filesystem providing an API to kernel data
 / structures.
*/
folly::Optional<size_t>
SystemMetrics::getRSSMemBytes() {
  folly::Optional<size_t> rss{folly::none};
  // match the line like: "VmRSS:      9028 kB"
  std::regex rssRegex("VmRSS:\\s+(\\d+)\\s+(\\w+)");
  std::smatch rssMatched;
  std::string line;
  std::ifstream input;
  try {
    // "/proc/self/" allows a process to look at itself without knowing the PID.
    std::ifstream input("/proc/self/status");
    if (input.is_open()) {
      while (std::getline(input, line)) {
        if (std::regex_search(line, rssMatched, rssRegex) &&
            rssMatched[2] == "kB") {
          rss = std::stoull(rssMatched[1]) * 1024;
          break;
        }
      }
    }
  } catch (const std::exception& ex) {
    LOG(ERROR)
        << "Fail to read the \"/proc/self/status\" of current process to get the memory usage: "
        << ex.what();
  }
  return rss;
}

// Return CPU% the process used
folly::Optional<double>
SystemMetrics::getCPUpercentage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);

  ProcCpuTime nowCpuTime(usage);
  folly::Optional<double> cpuPct{folly::none};

  // calculate the CPU% = (process time diff) / (time elapsed) * 100
  if (prevCpuTime.timestamp != 0 && // has cached before
      nowCpuTime.timestamp > prevCpuTime.timestamp &&
      nowCpuTime.totalTime > prevCpuTime.totalTime) {
    uint64_t timestampDiff = nowCpuTime.timestamp - prevCpuTime.timestamp;
    uint64_t procTimeDiff = nowCpuTime.totalTime - prevCpuTime.totalTime;
    cpuPct = ((double)procTimeDiff / (double)timestampDiff) * 100;
  }

  // update the cache for next CPU% update
  prevCpuTime = nowCpuTime;

  return cpuPct;
}

// get current timestamp
uint64_t
SystemMetrics::getCurrentNanoTime() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

} // namespace fbzmq