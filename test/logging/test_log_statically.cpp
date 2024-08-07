#include "babylon/logging/logger.h"

// Do BABYLON_LOG at very begining of the program.
// Check whether log system is ensured to be ready in default state
__attribute__((init_priority(101))) static struct StaticallyInitialized {
  StaticallyInitialized() {
    BABYLON_LOG(INFO) << "log in static initialize stage is fine";
  }
} statically_initialized_instance;
