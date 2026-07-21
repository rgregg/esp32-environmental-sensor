#pragma once
#include <string>

bool netBegin(const std::string& hostname);
std::string netIp();
bool netConnected();
