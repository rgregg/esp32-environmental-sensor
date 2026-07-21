#pragma once
#include "config.h"
#include "sensor_manager.h"

void webBegin(Config& cfg, SensorManager& sensors, bool (*onConfigChanged)());
void webLoop();
