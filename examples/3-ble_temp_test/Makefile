# Set the name of your application:
APPLICATION = 3-ble_temp_test

# Which board to compile to
BOARD ?= nrf52840dk

# are the default shell commands of RIOT-OS
USEMODULE += xtimer

# Include NimBLE
USEPKG += nimble
USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt

# Use automated advertising
USEMODULE += nimble_autoadv
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_DEVICE_NAME='"RIOT Temp Sensor"'
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_START_MANUALLY=1

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

include $(RIOTBASE)/Makefile.include