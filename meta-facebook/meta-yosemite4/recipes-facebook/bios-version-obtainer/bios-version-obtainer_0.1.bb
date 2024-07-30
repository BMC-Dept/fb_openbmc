SUMMARY = "OpenBMC BIOS Version Obtainer"
DESCRIPTION = "OpenBMC BIOS Version Obtainer"
PR = "r1"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit systemd

LOCAL_URI = " \
    file://bios-version-obtainer.service \
    file://bios-version-obtainer.sh \
"

DEPENDS += "systemd"
RDEPENDS:${PN} = "bash"

S = "${WORKDIR}"
SYSTEMD_SERVICE:${PN} = "bios-version-obtainer.service"

do_install() {
    install -d ${D}/${bindir}
    install -m 0755 ${S}/bios-version-obtainer.sh ${D}/${bindir}/bios-version-obtainer.sh
    install -d ${D}${systemd_system_unitdir}
    install -m 644 bios-version-obtainer.service ${D}${systemd_system_unitdir}/
}
