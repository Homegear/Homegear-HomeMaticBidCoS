/* Copyright 2013-2019 Homegear GmbH */

#include "../GD.h"
#include "Hgdc.h"

namespace BidCoS
{

Hgdc::Hgdc(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
    _settings = settings;
    _out.init(GD::bl);
    _out.setPrefix(GD::out.getPrefix() + "EnOcean HGDC \"" + settings->id + "\": ");

    signal(SIGPIPE, SIG_IGN);

    _stopped = true;
}

Hgdc::~Hgdc()
{
    stopListening();
}

void Hgdc::startListening()
{
    try
    {
        GD::bl->hgdc->unregisterPacketReceivedEventHandler(_packetReceivedEventHandlerId);
        _packetReceivedEventHandlerId = GD::bl->hgdc->registerPacketReceivedEventHandler(BIDCOS_FAMILY_ID, std::function<void(int64_t, const std::string&, const std::vector<uint8_t>&)>(std::bind(&Hgdc::processPacket, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

        if(!_aesHandshake) return; //AES is not initialized

        if(!GD::family->getCentral())
        {
            _stopCallbackThread = true;
            _out.printError("Error: Could not get central address. Stopping listening.");
            return;
        }
        _myAddress = GD::family->getCentral()->getAddress();
        _aesHandshake->setMyAddress(_myAddress);

        IBidCoSInterface::startListening();

        _stopped = false;
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Hgdc::stopListening()
{
    try
    {
        _stopped = true;
        IBidCoSInterface::stopListening();
        GD::bl->hgdc->unregisterPacketReceivedEventHandler(_packetReceivedEventHandlerId);
        _packetReceivedEventHandlerId = -1;
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Hgdc::forceSendPacket(std::shared_ptr<BidCoSPacket> packet)
{
    try
    {
        auto binaryPacket = packet->byteArray();
        if(!GD::bl->hgdc->sendPacket(_settings->serialNumber, binaryPacket))
        {
            _out.printError("Error sending packet " + BaseLib::HelperFunctions::getHexString(binaryPacket) + ".");
        }

        if(_bl->debugLevel > 3)
        {
            if(packet->getTimeSending() > 0)
            {
                _out.printInfo("Info: Sending (" + _settings->id + "): " + BaseLib::HelperFunctions::getHexString(binaryPacket) + " Planned sending time: " + BaseLib::HelperFunctions::getTimeString(packet->getTimeSending()));
            }
            else
            {
                _out.printInfo("Info: Sending (" + _settings->id + "): " + BaseLib::HelperFunctions::getHexString(binaryPacket));
            }
        }
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Hgdc::processPacket(int64_t familyId, const std::string& serialNumber, const std::vector<uint8_t>& data)
{
    try
    {
        if(serialNumber != _settings->serialNumber) return;
        std::shared_ptr<BidCoSPacket> packet = std::make_shared<BidCoSPacket>(data, true, BaseLib::HelperFunctions::getTime());
        processReceivedPacket(packet);
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Hgdc::enableUpdateMode()
{
    GD::bl->hgdc->setMode(_settings->serialNumber, 1);
}

void Hgdc::disableUpdateMode()
{
    GD::bl->hgdc->setMode(_settings->serialNumber, 0);
}

}