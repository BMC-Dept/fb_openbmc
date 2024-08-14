FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

EXTRA_OEMESON:append = " -Dtransport-implementation=af-mctp -Dmaximum-transfer-size=150 -Doem-meta=enabled -Dsensor-polling-time=2000"

SRC_URI += " \
    file://host_eid \
    file://0001-pldm-Implement-PCIeSlot-interface.patch \
    file://0002-pldm-Implement-PCIeDevice-interface.patch \
    file://0003-pldm-Implement-Cable-interface.patch \
    file://0004-pldm-Adding-support-to-find-system-type.patch \
    file://0005-platform-mc-add-more-exception-type.patch \
    file://0006-libpldmresponder-fix-an-eternal-hang-in-requesting-b.patch  \
    file://0007-Skip-endpoint-check-when-responding-to-GetPDR-reques.patch  \
    file://0008-skip-asking-for-PDR-s-from-remote-endpoint.patch \
    file://0009-host-bmc-Logging-PEL-for-PDR-Exchange-Failure.patch \
    file://0010-oem-ibm-Implement-Host-lamp-test-interface.patch       \
    file://0011-Add-support-for-the-coreCount-property-in-DBus.patch \
    file://0012-Implementing-Motherboard-interface.patch \
    file://0013-terminus_manager-fix-spelling-of-Terminus.patch \
    file://0014-sdbusplus-use-shorter-type-aliases.patch \
    file://0015-OWNERS-Add-Thu-Nguyen-as-a-reviewer.patch \
    file://0016-platform-mc-PDR-handling.patch \
    file://0017-pldmd-fix-crash-when-re-request-D-Bus-name.patch \
    file://0018-requester-Use-return-code-instead-of-throwing-except.patch \
    file://0019-pldmtool-support-GetNextPart-for-GetPDR-command.patch      \
    file://0022-Adding-exception-handling-while-hosting-Dbus-path.patch \
    file://0023-platform-mc-Terminus-name-handling.patch \
    file://0024-platform-mc-Sensor-handling.patch \
    file://0025-platform-mc-Added-EventManager.patch \
    file://0026-requester-support-multi-host-MCTP-devices-hot-plug.patch \
    file://0027-Support-OEM-META-write-file-request-for-post-code-hi.patch \
    file://0028-platform-mc-Add-OEM-Meta-event-handler.patch \
    file://0029-Support-OEM-META-command-for-host-BIOS-version.patch \
    file://0030-platform-mc-Monitor-all-sensors-once-upon-the-first-.patch \
    file://0031-pldm-Revise-image-path-for-update.patch \
    file://0032-Support-OEM-META-command-for-Event-Logs-from-BIC.patch \
    file://0033-Support-OEM-META-command-for-power-control.patch \
    file://0034-oem-meta-Add-APML-alert-handler.patch \
    file://0035-Support-OEM-META-command-for-NIC-power-cycle.patch \
    file://0036-Add-event-log-type-for-PMIC-error-VR-alert.patch \
    file://0037-platform-mc-Handle-Connectivity-propertiesChanged-si.patch \
    file://0038-platform-mc-Call-MCTP-.Recover-in-the-request-timeou.patch \
    file://0039-requester-Use-return-code-instead-of-throwing-except.patch \
    file://0040-platform-mc-Provide-a-default-TerminusName.patch \
    file://pldm-restart.sh \
    file://pldm-slow-restart.service \
    file://pldmd.service \
"

FILES:${PN}:append = " \
    ${systemd_system_unitdir}/pldm-restart.sh \
"

SYSTEMD_SERVICE:${PN} += " \
    pldm-slow-restart.service \
    pldmd.service \
"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}/usr/share/pldm
    install -m 0444 ${WORKDIR}/host_eid ${D}/usr/share/pldm
    install -m 0755 ${WORKDIR}/pldm-restart.sh ${D}${datadir}/pldm/
    install -m 0644 ${WORKDIR}/pldm-slow-restart.service ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/pldmd.service ${D}${systemd_system_unitdir}
}

