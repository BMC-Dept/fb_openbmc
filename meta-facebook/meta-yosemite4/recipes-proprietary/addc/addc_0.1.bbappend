# Copyright 2022-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

EXTRA_OECMAKE = "-DNO_SYSTEMD=ON -DBIC_APML_INTF=ON -DENABLE_PLDM=ON"

DEPENDS += " libbic cli11 apml"
DEPENDS:remove = "libgpiod"

inherit systemd

RDEPENDS:${PN}:append = " bash"

SRC_URI += " \
    file://ras-polling.sh \
    file://ras-polling@.service \
"

FILES:${PN}:append = " \
    ${libexecdir}/* \
    ${systemd_system_unitdir}/* \
"

SYSTEMD_SERVICE:${PN} += "${@' '.join(['ras-polling@{}.service'.format(i) for i in (d.getVar('OBMC_HOST_INSTANCES', True) or '').split()])}"

do_install:append() {
    install -d ${D}${libexecdir}/amd-ras
    install -m 0755 ${WORKDIR}/ras-polling.sh ${D}${libexecdir}/amd-ras/
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/ras-polling@.service ${D}${systemd_system_unitdir}/ras-polling@.service
}
