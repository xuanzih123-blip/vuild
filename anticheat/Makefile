# Makefile - Theos build for the TIZAntiCheat dylib.
# The real configuration lives in build.yml; this file only wires it up.

TARGET := iphone:clang:latest:13.0
ARCHS = arm64
INSTALL_TARGET_PROCESSES = TIZFFTipaExternal

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = TIZAntiCheat

TIZAntiCheat_FILES = source/main.mm \
                     source/Hooks.mm \
                     source/HookTable.cpp \
                     source/Il2CppResolver.cpp

TIZAntiCheat_CFLAGS = -fobjc-arc -fblocks -O2 -I source
TIZAntiCheat_CCFLAGS = -std=c++17
TIZAntiCheat_FRAMEWORKS = Foundation
TIZAntiCheat_LDFLAGS = -Wl,-install_name,TIZAntiCheat

include $(THEOS_MAKE_PATH)/tweak.mk

.PHONY: regenerate clean-build
regenerate:
	python3 tools/gen_hooks.py

clean-build:
	rm -rf build .theos packages
