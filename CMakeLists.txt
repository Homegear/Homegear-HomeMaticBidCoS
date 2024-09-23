cmake_minimum_required(VERSION 3.8)
project(homegear_homematicbidcos)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        misc/CoverityModeling.cpp
        src/PhysicalInterfaces/AesHandshake.cpp
        src/PhysicalInterfaces/AesHandshake.h
        src/PhysicalInterfaces/COC.cpp
        src/PhysicalInterfaces/COC.h
        src/PhysicalInterfaces/Crc16.cpp
        src/PhysicalInterfaces/Crc16.h
        src/PhysicalInterfaces/Cul.cpp
        src/PhysicalInterfaces/Cul.h
        src/PhysicalInterfaces/Cunx.cpp
        src/PhysicalInterfaces/Cunx.h
        src/PhysicalInterfaces/Hgdc.cpp
        src/PhysicalInterfaces/Hgdc.h
        src/PhysicalInterfaces/HM-CFG-LAN.cpp
        src/PhysicalInterfaces/HM-CFG-LAN.h
        src/PhysicalInterfaces/HM-LGW.cpp
        src/PhysicalInterfaces/HM-LGW.h
        src/PhysicalInterfaces/Hm-Mod-Rpi-Pcb.cpp
        src/PhysicalInterfaces/Hm-Mod-Rpi-Pcb.h
        src/PhysicalInterfaces/IBidCoSInterface.cpp
        src/PhysicalInterfaces/IBidCoSInterface.h
        src/PhysicalInterfaces/TICC1100.cpp
        src/PhysicalInterfaces/TICC1100.h
        src/VirtualPeers/HmCcTc.cpp
        src/VirtualPeers/HmCcTc.h
        src/BidCoS.cpp
        src/BidCoS.h
        src/BidCoSDeviceTypes.h
        src/BidCoSMessage.cpp
        src/BidCoSMessage.h
        src/BidCoSMessages.cpp
        src/BidCoSMessages.h
        src/BidCoSPacket.cpp
        src/BidCoSPacket.h
        src/BidCoSPacketManager.cpp
        src/BidCoSPacketManager.h
        src/BidCoSPeer.cpp
        src/BidCoSPeer.h
        src/BidCoSQueue.cpp
        src/BidCoSQueue.h
        src/BidCoSQueueManager.cpp
        src/BidCoSQueueManager.h
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/HomeMaticCentral.cpp
        src/HomeMaticCentral.h
        src/Interfaces.cpp
        src/Interfaces.h
        src/PendingBidCoSQueues.cpp
        src/PendingBidCoSQueues.h
        src/PhysicalInterfaces/HomegearGateway.cpp src/PhysicalInterfaces/HomegearGateway.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})

add_library(homegear_homematicbidcos ${SOURCE_FILES})