FILESEXTRAPATHS:prepend := "${THISDIR}/linux-nuvoton:${THISDIR}/../../../../recipes-kernel/linux/files:"

KSRC = "git://github.com/Wiwynn/linux-nuvoton.git;protocol=https;branch=${KBRANCH}"
KBRANCH = "NPCM-6.1-OpenBMC"
LINUX_VERSION = "6.1.51"
SRCREV = "df1b40757950401d55129cae6c888c8899fd3981"

SRC_URI += "file://yosemite4-common.cfg \
            file://yosemite4.cfg \
            file://0001-ARM64-dts-nuvoton-Add-initial-yosemitev4-device-tree.patch \
           "
