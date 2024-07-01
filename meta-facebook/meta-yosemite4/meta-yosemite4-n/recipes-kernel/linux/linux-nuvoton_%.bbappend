FILESEXTRAPATHS:prepend := "${THISDIR}/linux-nuvoton:${THISDIR}/../../../../recipes-kernel/linux/files:"

KSRC = "git://github.com/Wiwynn/linux-nuvoton.git;protocol=https;branch=${KBRANCH}"
KBRANCH = "NPCM-6.1-OpenBMC"
LINUX_VERSION = "6.1.51"
SRCREV = "475d12279523a5c2ee7f657c8ea33ec9b2ebbcff"

SRC_URI += "file://yosemite4-common.cfg \
            file://yosemite4.cfg \
            file://0001-ARM64-dts-nuvoton-Add-initial-yosemitev4-device-tree.patch \
            file://0002-hwmon-max31790-support-to-config-PWM-as-TACH.patch \
            file://0003-hwmon-max31790-add-fanN_enable-for-all-fans.patch \
            file://0004-hwmon-ina233-Add-ina233-driver.patch \
            file://0005-Add-adm1281-driver.patch \
            file://0006-driver-i3c-handle-stop-signal-well.patch \
            file://0007-driver-rtc-nct3018-revert-to-pervious-version.patch \
            file://0008-arm64-dts-yosemite4-n-add-mac-config-property.patch \
            file://0009-arm64-dts-yosemite4-n-Change-IOE-i2c-address.patch \
            file://0010-arm64-dts-yosemite4-n-Add-nct7363-in-dts.patch \
            file://0011-yosemite4-n-dts-disabled-pcie-in-common-dts.patch \
            file://0012-drivers-spi-npcm-fiu-add-delay-before-reboot-to-avoi.patch \
            file://0013-arch-arm64-Kconfig-increase-the-default-GPIO-number.patch \
            file://0014-arm64-dts-yosemite4-n-increase-the-size-of-rwfs.patch \
            file://0015-Update-partitions-for-increasing-RWFS.patch \
            file://0016-driver-i3c-increase-DMA-transfer-timeout.patch \
            file://0017-arm64-dts-nuvoton-yv4-gpio-input-enable-92-183-184-1.patch \
            file://0018-i3c-send-7e-before-private-read.patch \
            file://0019-i3c-svc-add-error-handling.patch \
            file://0020-i3c-add-SDA-skew-before-private-read.patch \
            file://0021-drivers-i3c-svc-add-global-dma-mutex-locked.patch \
           "
