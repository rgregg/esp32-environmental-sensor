#pragma once
#include "publisher.h"
#include <NetworkClient.h>
#include <PubSubClient.h>

class MqttPublisher : public Publisher {
public:
  MqttPublisher() : client_(net_) {}
  void configure(const Config& cfg) override;
  void loop() override;
  void publish(const std::string& device, const ReadingSet& readings) override;
  bool connected() const override { return client_.connected(); }

private:
  bool reconnect();
  void publishDiscovery(const std::string& device, const ReadingSet& readings);
  NetworkClient net_;
  // mutable: PubSubClient::connected() is not const in this library version,
  // but connected() is logically a const query on Publisher's interface.
  mutable PubSubClient client_;
  Config cfg_;
  bool discoverySent_ = false;
};
