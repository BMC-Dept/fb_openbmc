FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:${THISDIR}/../../../../recipes-bsp/uboot/files:"

SRCREV = "51264abe9348bb2eb39ace0db87cc38d9fcd7f72"

SRC_URI +="file://yosemite4-common.cfg \
           file://yosemite4.cfg \
           file://0001-dts-nuvoton-npcm845-evb-remove-un-used-device-and-ad.patch \
           file://0002-common-change-console-type-from-ttyS0-to-ttyS4.patch \
           file://0003-cmd-mem-set-test-result-of-mtest-to-enviroment-varia.patch \
           file://0004-Update-ENV_ADDR-and-uimage_flash_addr.patch \
           "
