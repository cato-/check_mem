DESTDIR=/
INSTALL_LOCATION=$(DESTDIR)/usr/
CFLAGS:=$(shell dpkg-buildflags --get CFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)
all: check_mem
check_mem: check_mem.cpp
	c++ $(CXXFLAGS) $(LDFLAGS) -std=c++11 -Wall -static -O3 -flto -fwhole-program -o $@ $^
install: check_mem_install
check_mem_install:
	mkdir -p $(INSTALL_LOCATION)/lib/nagios/plugins
	install -m 0755 check_mem $(INSTALL_LOCATION)/lib/nagios/plugins/
clean:  
	rm -f check_mem   

