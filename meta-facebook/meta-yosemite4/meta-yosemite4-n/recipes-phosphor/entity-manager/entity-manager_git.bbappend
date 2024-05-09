FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://0001-configuration-add-yosemite4-config-for-Nuvoton-BMC.patch \
    file://0002-yosemite4-Support-CX7-NIC-card-temperature-sensor.patch \
    file://0003-configurations-yosemite4-Add-IANA-for-sentinel-dome.patch \
"
