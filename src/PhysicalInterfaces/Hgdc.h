/* Copyright 2013-2019 Homegear GmbH */

#ifndef HOMEGEAR_BIDCOS_HGDC_H
#define HOMEGEAR_BIDCOS_HGDC_H

#include "../BidCoSPacket.h"
#include "IBidCoSInterface.h"
#include <homegear-base/BaseLib.h>

namespace BidCoS
{

class Hgdc : public IBidCoSInterface
{
public:
    explicit Hgdc(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
    ~Hgdc() override;

    void startListening() override;
    void stopListening() override;

    void enableUpdateMode() override;
    void disableUpdateMode() override;

    bool isOpen() override { return !_stopped; }
protected:
    int32_t _packetReceivedEventHandlerId = -1;

    void forceSendPacket(std::shared_ptr<BidCoSPacket> packet) override;
    void processPacket(int64_t familyId, const std::string& serialNumber, const std::vector<uint8_t>& data);
};

}

#endif //HOMEGEAR_ENOCEAN_HOMEGEARGATEWAY_H
