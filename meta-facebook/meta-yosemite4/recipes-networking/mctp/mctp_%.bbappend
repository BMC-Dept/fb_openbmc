FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://0001-mctpd-Change-the-type-of-NetworkId-to-uint32.patch \
    file://0002-CHANGELOG-Start-a-changelog.patch \
    file://0003-mctpd-Add-support-for-endpoint-recovery.patch \
    file://0004-mctpd-Allow-recovery-of-devices-reporting-a-nil-UUID.patch \
    file://0005-mctpd-Allow-configuring-.Connectivity-as-writable.patch \
    file://0006-mctpd-Fix-resource-handling-over-recovery-of-exchang.patch \
    file://0007-mctpd-Cope-with-devices-that-don-t-support-Get-Endpo.patch \
    file://0008-mctp-introduce-ops-wrapper-for-socket-operations.patch \
    file://0009-mctpd-Exit-on-control-socket-EOF.patch \
    file://0010-tests-introduce-MCTP-infrastructure-test-facility.patch \
    file://0011-tests-Add-venv-setup-for-tests.patch \
    file://0013-mctpd-Handle-SIGTERM-to-enable-test-coverage-reports.patch \
    file://0014-tests-Add-netlink-implementations-for-DELNEIGH-and-D.patch \
    file://0015-tests-utils-Add-mctpd_mctp_endpoint_obj-helper.patch \
    file://0016-tests-Add-endpoint-removal-test.patch \
    file://0017-tests-mctpd-endpoint-recovery.patch \
    file://0018-add-setup-param-to-meson-command-in-README.patch \
    file://0019-mctpd-handle-EEXIST-outside-of-get_endpoint_peer.patch \
    file://0020-mctpd-Add-AssignEndpointStatic-dbus-interface.patch \
    file://0021-Support-get-static-eid-config-from-entity-manager.patch \
    file://0022-Add-method-for-setting-up-MCTP-EID-by-config-path.patch \
    file://0023-mctpd-fix-mctpd-crash-issue.patch \
"
