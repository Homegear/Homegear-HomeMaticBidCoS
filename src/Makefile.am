AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_homematicbidcos.la
mod_homematicbidcos_la_SOURCES = BidCoSPeer.h BidCoSMessages.cpp BidCoSMessage.cpp Factory.cpp GD.h BidCoSPacketManager.cpp BidCoSMessages.h BidCoS.cpp PendingBidCoSQueues.cpp HomeMaticCentral.cpp HomeMaticCentral.h BidCoSPeer.cpp VirtualPeers/HmCcTc.cpp VirtualPeers/HcCcTc.h delegate.hpp GD.cpp BidCoSQueue.h BidCoSPacket.h Interfaces.cpp Interfaces.h BidCoSQueueManager.h delegate_template.hpp PendingBidCoSQueues.h Factory.h delegate_list.hpp PhysicalInterfaces/AesHandshake.h PhysicalInterfaces/Crc16.h PhysicalInterfaces/Crc16.cpp PhysicalInterfaces/HM-LGW.h PhysicalInterfaces/Hm-Mod-Rpi-Pcb.cpp PhysicalInterfaces/HomegearGateway.cpp PhysicalInterfaces/Cul.h PhysicalInterfaces/HM-CFG-LAN.h PhysicalInterfaces/Cunx.cpp PhysicalInterfaces/HM-CFG-LAN.cpp PhysicalInterfaces/Cunx.h PhysicalInterfaces/IBidCoSInterface.h PhysicalInterfaces/IBidCoSInterface.cpp PhysicalInterfaces/Cul.cpp PhysicalInterfaces/TICC1100.h PhysicalInterfaces/COC.h PhysicalInterfaces/TICC1100.cpp PhysicalInterfaces/AesHandshake.cpp PhysicalInterfaces/HM-LGW.cpp PhysicalInterfaces/COC.cpp BidCoSPacket.cpp BidCoSPacketManager.h BidCoSDeviceTypes.h BidCoS.h BidCoSQueueManager.cpp BidCoSMessage.h BidCoSQueue.cpp
mod_homematicbidcos_la_LDFLAGS =-module -avoid-version -shared

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_homematicbidcos.la
