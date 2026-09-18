#pragma once
#include <string>
#include "reading.h"
#include "config.h"

std::string stateTopic(const std::string& base, const std::string& device,
                       const std::string& reading);
std::string discoveryTopic(const std::string& prefix, const std::string& device,
                           const std::string& reading);
std::string discoveryPayload(const std::string& device, const Reading& r,
                             const std::string& stateTopicStr);
// True when a config change means the existing broker connection must be
// dropped (a different broker, different credentials, or MQTT turned on/off).
bool mqttConnectionSettingsChanged(const Config& before, const Config& after);
