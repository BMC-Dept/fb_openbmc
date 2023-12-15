FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:${THISDIR}/../../../../recipes-bsp/uboot/files:"

SRCREV = "51264abe9348bb2eb39ace0db87cc38d9fcd7f72"

SRC_URI +="file://yosemite4-common.cfg \
           file://yosemite4.cfg"
