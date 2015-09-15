AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear

libdir = $(datadir)/homegear/modules
lib_LTLIBRARIES = mod_max.la
mod_max_la_SOURCES = MAXDevice.cpp Makefile.am MAXMessages.cpp MAXDeviceTypes.h MAXPacket.cpp PendingQueues.cpp Factory.cpp GD.h MAXPeer.h MAXMessage.cpp MAXPeer.cpp PacketQueue.cpp QueueManager.h delegate.hpp GD.cpp MAX.cpp delegate_template.hpp Factory.h MAXPacket.h MAXMessage.h delegate_list.hpp PhysicalInterfaces PhysicalInterfaces/CUL.h PhysicalInterfaces/CUL.cpp PhysicalInterfaces/TICC1100.h PhysicalInterfaces/COC.h PhysicalInterfaces/TICC1100.cpp PhysicalInterfaces/COC.cpp LogicalDevices LogicalDevices/MAXCentral.cpp LogicalDevices/MAXCentral.h LogicalDevices/MAXSpyDevice.h LogicalDevices/MAXSpyDevice.cpp PacketQueue.h PendingQueues.h PacketManager.h PacketManager.cpp QueueManager.cpp MAXDevice.h MAXMessages.h MAX.h
mod_max_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_max.la
