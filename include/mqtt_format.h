#pragma once
#include <string>
#include "reading.h"

std::string stateTopic(const std::string& base, const std::string& device,
                       const std::string& reading);
std::string discoveryTopic(const std::string& prefix, const std::string& device,
                           const std::string& reading);
std::string discoveryPayload(const std::string& device, const Reading& r,
                             const std::string& stateTopicStr);
