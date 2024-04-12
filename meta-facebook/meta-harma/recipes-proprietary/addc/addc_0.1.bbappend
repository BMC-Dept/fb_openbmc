# Copyright 2023-present Facebook. All Rights Reserved.
FILESEXTRAPATHS:append := "${THISDIR}/files:"
DEFAULT_PREFERENCE = "1"

inherit systemd obmc-phosphor-systemd

DEPENDS += "cli11 boost apml libgpiod sdbusplus systemd phosphor-logging"
EXTRA_OECMAKE = " \
                  -DNO_SYSTEMD="" \
                "

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN}:append = " com.amd.crashdump.service"
SYSTEMD_OVERRIDE:${PN}:harma += " \
                                  addc-init.conf:com.amd.crashdump.service.d/addc-init.conf \
                                "

FILES:${PN} += "${systemd_system_unitdir}"
