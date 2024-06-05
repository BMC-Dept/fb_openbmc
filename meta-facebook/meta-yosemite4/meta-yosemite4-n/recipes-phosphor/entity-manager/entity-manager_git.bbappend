FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://0001-configuration-add-yosemite4-config-for-Nuvoton-BMC.patch \
    file://0002-yosemite4-Support-CX7-NIC-card-temperature-sensor.patch \
    file://0003-configurations-Revise-the-BRCM-NIC-sensor-name.patch \
    file://0004-schemas-Add-CheckHysterWithSetpt-for-PID.patch \
    file://0005-configuration-add-fan-table-in-yosemite4-config-file.patch \
"
