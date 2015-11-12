AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_homematicbidcos.la
mod_homematicbidcos_la_SOURCES = HomeMaticDevice.h BidCoSPeer.h BidCoSMessages.cpp BidCoSMessage.cpp Factory.cpp GD.h BidCoSPacketManager.cpp BidCoSMessages.h BidCoS.cpp PendingBidCoSQueues.cpp Devices Devices/HM-CC-TC.cpp Devices/HM-CC-VD.h Devices/HM-SD.h Devices/HM-CC-VD.cpp Devices/HM-LC-SwX-FM.cpp Devices/HM-SD.cpp Devices/HM-LC-SwX-FM.h Devices/HomeMaticCentral.cpp Devices/HM-CC-TC.h Devices/HomeMaticCentral.h BidCoSPeer.cpp delegate.hpp GD.cpp BidCoSQueue.h BidCoSPacket.h HomeMaticDevice.cpp BidCoSQueueManager.h delegate_template.hpp PendingBidCoSQueues.h Factory.h delegate_list.hpp PhysicalInterfaces/AesHandshake.h PhysicalInterfaces/HM-LGW.h PhysicalInterfaces/CUL.h PhysicalInterfaces/CulAes.h PhysicalInterfaces/HM-CFG-LAN.h PhysicalInterfaces/CUNO.cpp PhysicalInterfaces/HM-CFG-LAN.cpp PhysicalInterfaces/RTLSDR-LAN.h PhysicalInterfaces/RTLSDR-LAN.cpp PhysicalInterfaces/CUNO.h PhysicalInterfaces/IBidCoSInterface.h PhysicalInterfaces/IBidCoSInterface.cpp PhysicalInterfaces/CUL.cpp PhysicalInterfaces/CulAes.cpp PhysicalInterfaces/TICC1101.cpp PhysicalInterfaces/TICC1100.h PhysicalInterfaces/TICC1101.h PhysicalInterfaces/COC.h PhysicalInterfaces/TICC1100.cpp PhysicalInterfaces/AesHandshake.cpp PhysicalInterfaces/HM-LGW.cpp PhysicalInterfaces/COC.cpp BidCoSPacket.cpp BidCoSPacketManager.h BidCoSDeviceTypes.h BidCoS.h BidCoSQueueManager.cpp BidCoSMessage.h BidCoSQueue.cpp
mod_homematicbidcos_la_LDFLAGS =-module -avoid-version -shared

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_homematicbidcos.la
