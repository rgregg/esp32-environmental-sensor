#pragma once
#include <string>
#include "reading.h"

std::string formatLineProtocol(const std::string& measurement,
                               const std::string& device,
                               const ReadingSet& readings);
