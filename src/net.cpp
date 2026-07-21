#include "net.h"
#include <Arduino.h>
#include <ETH.h>
#include <ESPmDNS.h>

static volatile bool s_gotIp = false;

static void onEthEvent(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_ETH_GOT_IP) s_gotIp = true;
  if (event == ARDUINO_EVENT_ETH_DISCONNECTED) s_gotIp = false;
}

bool netBegin(const std::string& hostname) {
  Network.onEvent(onEthEvent);
  ETH.begin();               // uses board pin defaults for esp32-poe-iso
  ETH.setHostname(hostname.c_str());
  uint32_t start = millis();
  while (!s_gotIp && millis() - start < 15000) delay(100);
  if (!s_gotIp) return false;
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }
  return true;
}

std::string netIp() {
  if (!s_gotIp) return "0.0.0.0";
  return std::string(ETH.localIP().toString().c_str());
}

bool netConnected() { return s_gotIp; }
