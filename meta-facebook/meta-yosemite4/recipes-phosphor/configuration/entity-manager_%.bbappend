FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://0001-yosemite4-Support-CX7-NIC-card-temperature-sensor.patch \
    file://0002-configurations-Revise-the-BRCM-NIC-sensor-name.patch \
    file://0003-configuration-yosemite4-support-different-fan-table-.patch \
    file://0004-configurations-yosemite4-Add-Sentinel-Dome-with-Reti.patch \
    file://0005-Add-mctp-eids-configuration-for-Yosemite-4.patch \
    file://0006-configurations-yosemite4-Add-IANA-for-sentinel-dome.patch \
    file://0007-configurations-yosemite4-Remove-the-redundant-NIC-se.patch \
    file://0008-configurations-yosemite4-Add-second-sources-support-.patch \
    file://0009-configurations-yosemite4-Add-12V-EFFUSE-Power-Input-.patch \
    file://0010-configurations-yosemite4-Rename-Medusa-FRU-config-by.patch \
    file://0011-configurations-yosemite4-Remove-the-duplicate-code-i.patch \
    file://0012-configurations-yosemite4-Support-second-sources-fan-.patch \
    file://0013-configurations-yosemite4-Remove-duplicate-Fan-Board-.patch \
    file://0014-configs-terminus-reduce-capitalization.patch \
    file://0015-configurations-yosemite4-Include-board-info-into-Dec.patch \
    file://0016-configurations-yosemite4-align-the-FRU-naming-of-Med.patch \
    file://0017-configurations-yosemite4-Rename-the-fan-board-config.patch \
    file://0018-configurations-yosemite4-distinguish-sensor-naming-f.patch \
    file://0019-configurations-yosemite4-Support-third-sources-Medus.patch \
    file://0020-configurations-yosemite4-Support-fourth-sources-Medu.patch \
    file://0021-configurations-yosemite4-Support-third-sources-Fan-B.patch \
    file://0022-configurations-yosemite4-Support-fourth-sources-Fan-.patch \
    file://0023-configurations-yosemite4-set-fail-safe-percent-to-90.patch \
    file://0024-configurations-yosemite4-Rename-the-spider-board-wit.patch \
    file://0025-configurations-yosemite4-Set-fan-board-current-senso.patch \
    file://0026-configurations-yosemite4-Support-fifth-sources-Medus.patch \
    file://0027-configurations-yosemite4-Support-sixth-sources-Medus.patch \
    file://0028-configurations-yosemite4-Support-seventh-sources-Med.patch \
    file://0029-configurations-yosemite4-Support-eighth-sources-Medu.patch \
    file://0030-configurations-yosemite4-Support-second-sources-Spid.patch \
    file://0031-configurations-yosemite4-Add-PowerState-to-Medusa-Bo.patch \
"

