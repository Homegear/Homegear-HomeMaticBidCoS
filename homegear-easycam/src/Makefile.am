AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear

libdir = $(datadir)/homegear/modules
lib_LTLIBRARIES = mod_easycam.la
mod_easycam_la_SOURCES = DeviceTypes.h EasyCam.cpp EasyCam.h EasyCamDevice.cpp EasyCamDevice.h EasyCamPacket.cpp EasyCamPacket.h EasyCamPeer.cpp EasyCamPeer.h Factory.cpp Factory.h GD.cpp GD.h LogicalDevices/EasyCamCentral.cpp LogicalDevices/EasyCamCentral.h PhysicalInterfaces/EventServer.cpp PhysicalInterfaces/EventServer.h PhysicalInterfaces/IEasyCamInterface.cpp PhysicalInterfaces/IEasyCamInterface.h
mod_easycam_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_easycam.la