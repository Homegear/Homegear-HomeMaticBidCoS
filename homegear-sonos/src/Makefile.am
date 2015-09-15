AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear

libdir = $(datadir)/homegear/modules
lib_LTLIBRARIES = mod_sonos.la
mod_sonos_la_SOURCES = SonosPacket.cpp Sonos.cpp Factory.cpp GD.h SonosPeer.cpp SonosPacket.h SonosDevice.cpp SonosPeer.h Sonos.h DeviceTypes.h GD.cpp SonosDevice.h Factory.h PhysicalInterfaces PhysicalInterfaces/ISonosInterface.h PhysicalInterfaces/EventServer.h PhysicalInterfaces/ISonosInterface.cpp PhysicalInterfaces/EventServer.cpp LogicalDevices LogicalDevices/SonosCentral.h LogicalDevices/SonosCentral.cpp
mod_sonos_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_sonos.la
