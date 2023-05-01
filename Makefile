# Set the name of your application:
APPLICATION = BLE-hangboard

# Which board to compile to
BOARD ?= nrf52840dk

# Include timer modules
USEMODULE += xtimer
USEMODULE += event_timeout_ztimer

# Include NimBLE
USEPKG += nimble
USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt

# Use automated advertising
USEMODULE += nimble_autoadv
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_DEVICE_NAME='"BLE Hangboard"'   # This is the name that appears on the BLE scanner.
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_START_MANUALLY=1

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/RIOT

include $(RIOTBASE)/Makefile.include