AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear

libdir = $(datadir)/homegear/modules
lib_LTLIBRARIES = mod_philipshue.la
mod_philipshue_la_SOURCES = PhilipsHue.cpp Factory.cpp GD.h PhilipsHuePeer.h PhilipsHuePacket.cpp PhilipsHuePacket.h PhilipsHueDeviceTypes.h PhilipsHueDevice.h PhilipsHue.h GD.cpp PhilipsHueDevice.cpp PhilipsHuePeer.cpp Factory.h PhysicalInterfaces PhysicalInterfaces/HueBridge.h PhysicalInterfaces/HueBridge.cpp PhysicalInterfaces/IPhilipsHueInterface.h PhysicalInterfaces/IPhilipsHueInterface.cpp LogicalDevices LogicalDevices/PhilipsHueCentral.cpp LogicalDevices/PhilipsHueCentral.h PacketManager.h PacketManager.cpp
mod_philipshue_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_philipshue.la
