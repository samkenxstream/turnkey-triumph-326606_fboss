/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/test/ConfigFactory.h"
#include "fboss/agent/hw/test/HwLinkStateDependentTest.h"
#include "fboss/agent/hw/test/HwTestPacketUtils.h"
#include "fboss/agent/hw/test/dataplane_tests/HwTestOlympicUtils.h"
#include "fboss/agent/hw/test/dataplane_tests/HwTestQosUtils.h"
#include "fboss/agent/test/EcmpSetupHelper.h"
#include "fboss/agent/test/ResourceLibUtil.h"

namespace facebook::fboss {

class HwDscpMarkingTest : public HwLinkStateDependentTest {
 protected:
  void SetUp() override {
    HwLinkStateDependentTest::SetUp();
    helper_ = std::make_unique<utility::EcmpSetupAnyNPorts6>(
        getProgrammedState(), RouterID(0));
  }
  cfg::SwitchConfig initialConfig() const override {
    auto cfg = utility::onePortPerVlanConfig(
        getHwSwitch(), masterLogicalPortIds(), cfg::PortLoopbackMode::MAC);
    return cfg;
  }

  void verifyDscpMarking(bool /*frontPanel*/) {
    if (!isSupported(HwAsic::Feature::L3_QOS)) {
      return;
    }

    // TODO
  }

  uint8_t kIcpDscp() const {
    return utility::kOlympicQueueToDscp()
        .at(utility::kOlympicICPQueueId)
        .front();
  }

  std::string
  getDscpAclName(IP_PROTO proto, std::string direction, uint32_t port) const {
    return folly::to<std::string>(
        "dscp-mark-for-proto-",
        static_cast<int>(proto),
        "-L4-",
        direction,
        "-port-",
        port);
  }

 private:
  void sendAllPackets(
      uint8_t dscp,
      bool frontPanel,
      IP_PROTO proto,
      const std::vector<uint32_t>& ports) {
    for (auto port : ports) {
      sendPacket(
          dscp,
          frontPanel,
          proto,
          port /* l4SrcPort */,
          std::nullopt /* l4DstPort */);
      sendPacket(
          dscp,
          frontPanel,
          proto,
          std::nullopt /* l4SrcPort */,
          port /* l4DstPort */);
    }
  }

  void sendPacket(
      uint8_t dscp,
      bool frontPanel,
      IP_PROTO proto,
      std::optional<uint16_t> l4SrcPort,
      std::optional<uint16_t> l4DstPort) {
    auto vlanId = VlanID(*initialConfig().vlanPorts_ref()[0].vlanID_ref());
    auto intfMac = utility::getInterfaceMac(getProgrammedState(), vlanId);
    auto srcMac = utility::MacAddressGenerator().get(intfMac.u64NBO() + 1);

    std::unique_ptr<facebook::fboss::TxPacket> txPacket;
    if (proto == IP_PROTO::IP_PROTO_UDP) {
      txPacket = utility::makeUDPTxPacket(
          getHwSwitch(),
          vlanId,
          srcMac, // src mac
          intfMac, // dst mac
          folly::IPAddressV6("2620:0:1cfe:face:b00c::3"), // src ip
          folly::IPAddressV6("2620:0:1cfe:face:b00c::4"), // dst ip
          l4SrcPort.has_value() ? l4SrcPort.value() : 8000,
          l4DstPort.has_value() ? l4DstPort.value() : 8001,
          dscp << 2, // shifted by 2 bits for ECN
          255 // ttl
      );
    } else if (proto == IP_PROTO::IP_PROTO_TCP) {
      txPacket = utility::makeTCPTxPacket(
          getHwSwitch(),
          vlanId,
          srcMac, // src mac
          intfMac, // dst mac
          folly::IPAddressV6("2620:0:1cfe:face:b00c::3"), // src ip
          folly::IPAddressV6("2620:0:1cfe:face:b00c::4"), // dst ip
          l4SrcPort.has_value() ? l4SrcPort.value() : 8000,
          l4DstPort.has_value() ? l4DstPort.value() : 8001,
          dscp << 2, // shifted by 2 bits for ECN
          255 // ttl
      );
    } else {
      CHECK(false);
    }

    // port is in LB mode, so it will egress and immediately loop back.
    // Since it is not re-written, it should hit the pipeline as if it
    // ingressed on the port, and be properly queued.
    if (frontPanel) {
      auto outPort = helper_->ecmpPortDescriptorAt(kEcmpWidth).phyPortID();
      getHwSwitchEnsemble()->getHwSwitch()->sendPacketOutOfPortSync(
          std::move(txPacket), outPort);
    } else {
      getHwSwitchEnsemble()->getHwSwitch()->sendPacketSwitchedSync(
          std::move(txPacket));
    }
  }

  static inline constexpr auto kEcmpWidth = 1;
  std::unique_ptr<utility::EcmpSetupAnyNPorts6> helper_;
};

// Verify that the DSCP unmarked traffic to specific L4 src/dst ports that
// arrives on a forntl panel port is DSCP marked correctly as well as egresses
// via the right QoS queue.
TEST_F(HwDscpMarkingTest, VerifyDscpMarkingFrontPanel) {
  verifyDscpMarking(true);
}

// Verify that the DSCP unmarked traffic to specific L4 src/dst ports that
// originates from a CPU port is  DSCP marked correctly as well as egresses
// via the right QoS queue.
TEST_F(HwDscpMarkingTest, VerifyDscpMarkingCpu) {
  verifyDscpMarking(false);
}

} // namespace facebook::fboss