/* Copyright 2013-2019 Homegear GmbH
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#include "../BidCoSPacket.h"
#include <homegear-base/BaseLib.h>
#include "../GD.h"
#include "Cunx.h"

namespace BidCoS {

Cunx::Cunx(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings) {
  _out.init(GD::bl);
  _out.setPrefix(GD::out.getPrefix() + "CUNX \"" + settings->id + "\": ");

  stackPrefix = "";
  for (uint32_t i = 1; i < settings->stackPosition; i++) {
    stackPrefix.push_back('*');
  }

  C1Net::TcpSocketInfo tcp_socket_info;
  auto dummy_socket = std::make_shared<C1Net::Socket>(-1);
  _socket = std::make_unique<C1Net::TcpSocket>(tcp_socket_info, dummy_socket);

  _hostname = settings->host;

  if (settings->listenThreadPriority == -1) {
    settings->listenThreadPriority = 45;
    settings->listenThreadPolicy = SCHED_FIFO;
  }
}

Cunx::~Cunx() {
  try {
    _stopCallbackThread = true;
    GD::bl->threadManager.join(_listenThread);
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

std::string Cunx::getIpAddress() {
  return _socket ? _socket->GetIpAddress() : "";
}

void Cunx::forceSendPacket(std::shared_ptr<BidCoSPacket> packet) {
  try {
    std::lock_guard<std::mutex> sendGuard(_forceSendPacketMutex);
    std::string packetString = packet->hexString();
    if (_bl->debugLevel >= 4) _out.printInfo("Info: Sending (" + _settings->id + "): " + packetString);
    send(stackPrefix + "As" + packet->hexString() + "\n" + (_updateMode ? "" : stackPrefix + "Ar\n"));
    if (packet->controlByte() & 0x10) std::this_thread::sleep_for(std::chrono::milliseconds(380));
    else std::this_thread::sleep_for(std::chrono::milliseconds(20));
    _lastPacketSent = BaseLib::HelperFunctions::getTime();
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::enableUpdateMode() {
  try {
    _updateMode = true;
    send(stackPrefix + "AR\n");
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::disableUpdateMode() {
  try {
    send(stackPrefix + "Ar\n");
    _updateMode = false;
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::send(std::string data) {
  try {
    if (data.size() < 3) return; //Otherwise error in printInfo
    _sendMutex.lock();
    if (!_socket->Connected() || _stopped) {
      _out.printWarning(std::string("Warning: !!!Not!!! sending: ") + data.substr(2, data.size() - 3));
      _sendMutex.unlock();
      return;
    }
    _socket->Send((uint8_t *)data.data(), data.size());
    _sendMutex.unlock();
    return;
  }
  catch (const C1Net::Exception &ex) {
    _out.printError(ex.what());
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  _stopped = true;
  _sendMutex.unlock();
}

void Cunx::startListening() {
  try {
    stopListening();
    if (!_aesHandshake) return; //AES is not initialized

    if (!GD::family->getCentral()) {
      _stopCallbackThread = true;
      _out.printError("Error: Could not get central address. Stopping listening.");
      return;
    }
    _myAddress = GD::family->getCentral()->getAddress();
    _aesHandshake->setMyAddress(_myAddress);

    IBidCoSInterface::startListening();

    C1Net::TcpSocketInfo tcp_socket_info;
    tcp_socket_info.read_timeout = 5000;
    tcp_socket_info.write_timeout = 5000;

    C1Net::TcpSocketHostInfo tcp_socket_host_info{
        .host = _settings->host,
        .port = (uint16_t)BaseLib::Math::getUnsignedNumber(_settings->port),
        .tls = _settings->ssl,
        .verify_certificate = _settings->verifyCertificate,
        .ca_file = _settings->caFile,
        .auto_connect = false
    };

    _socket = std::make_unique<C1Net::TcpSocket>(tcp_socket_info, tcp_socket_host_info);
    _out.printDebug("Connecting to CUNX with hostname " + _settings->host + " on port " + _settings->port + "...");
    _stopped = false;
    if (_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Cunx::listen, this);
    else GD::bl->threadManager.start(_listenThread, true, &Cunx::listen, this);
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::reconnect() {
  try {
    _socket->Shutdown();
    _out.printDebug("Connecting to CUNX device with hostname " + _settings->host + " on port " + _settings->port + "...");
    _socket->Open();
    _hostname = _settings->host;
    _ipAddress = _socket->GetIpAddress();
    _stopped = false;
    send(stackPrefix + "X21\n" + stackPrefix + "Ar\n");
    _out.printInfo("Connected to CUNX device with hostname " + _settings->host + " on port " + _settings->port + ".");
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::stopListening() {
  try {
    IBidCoSInterface::stopListening();
    if (_socket->Connected()) send(stackPrefix + "Ax\n" + stackPrefix + "X00\n");
    _stopCallbackThread = true;
    GD::bl->threadManager.join(_listenThread);
    _stopCallbackThread = false;
    _socket->Shutdown();
    _stopped = true;
    _sendMutex.unlock(); //In case it is deadlocked - shouldn't happen of course
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::listen() {
  try {
    uint32_t receivedBytes = 0;
    int32_t bufferMax = 2048;
    std::vector<char> buffer(bufferMax);
    bool more_data = false;

    while (!_stopCallbackThread) {
      if (_stopped || !_socket->Connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (_stopCallbackThread) return;
        if (_stopped) _out.printWarning("Warning: Connection to CUNX closed. Trying to reconnect...");
        reconnect();
        continue;
      }
      std::vector<uint8_t> data;
      try {
        do {
          receivedBytes = _socket->Read((uint8_t *)buffer.data(), buffer.size(), more_data);
          if (receivedBytes > 0) {
            data.insert(data.end(), buffer.begin(), buffer.begin() + receivedBytes);
            if (data.size() > 1000000) {
              _out.printError("Could not read from CUNX: Too much data.");
              break;
            }
          }
        } while (receivedBytes == (unsigned)bufferMax);
      }
      catch (const C1Net::TimeoutException &ex) {
        if (data.empty()) continue; //When receivedBytes is exactly 2048 bytes long, proofread will be called again, time out and the packet is received with a delay of 5 seconds. It doesn't matter as packets this big are only received at start up.
      }
      catch (const C1Net::ClosedException &ex) {
        _stopped = true;
        _out.printWarning("Warning: " + std::string(ex.what()));
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        continue;
      }
      catch (const C1Net::Exception &ex) {
        _stopped = true;
        _out.printError("Error: " + std::string(ex.what()));
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        continue;
      }
      if (data.empty() || data.size() > 1000000) continue;

      if (_bl->debugLevel >= 6) {
        _out.printDebug("Debug: Packet received from CUNX. Raw data: " + BaseLib::HelperFunctions::getHexString(data));
      }

      processData(data);

      _lastPacketReceived = BaseLib::HelperFunctions::getTime();
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void Cunx::processData(std::vector<uint8_t> &data) {
  try {
    if (data.empty()) return;
    std::string packets;
    packets.insert(packets.end(), data.begin(), data.end());

    std::istringstream stringStream(packets);
    std::string packetHex;
    while (std::getline(stringStream, packetHex)) {
      if (stackPrefix.empty()) {
        if (packetHex.size() > 0 && packetHex.at(0) == '*') return;
      } else {
        if (packetHex.size() + 1 <= stackPrefix.size()) return;
        if (packetHex.substr(0, stackPrefix.size()) != stackPrefix || packetHex.at(stackPrefix.size()) == '*') return;
        else packetHex = packetHex.substr(stackPrefix.size());
      }
      if (packetHex.size() > 21) //21 is minimal packet length (=10 Byte + CUNX "A" + "\n")
      {
        std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(packetHex, BaseLib::HelperFunctions::getTime()));
        processReceivedPacket(packet);
      } else if (!packetHex.empty()) {
        if (packetHex.compare(0, 4, "LOVF") == 0) _out.printWarning("Warning: CUNX with id " + _settings->id + " reached 1% limit. You need to wait, before sending is allowed again.");
        else if (packetHex == "A") continue;
        else _out.printInfo("Info: Ignoring too small packet: " + packetHex);
      }
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}
}
