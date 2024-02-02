FILESEXTRAPATHS:prepend :=  "${THISDIR}/file:"

SRC_URI:append = " file://BootBlockAndHeader_no_tip_test.xml"

do_install:append() {
    install -d ${DEST}
    install -m 0644 ${WORKDIR}/BootBlockAndHeader_no_tip_test.xml ${DEST}/BootBlockAndHeader_${DEVICE_GEN}_${IGPS_MACHINE}_NoTip.xml
}

