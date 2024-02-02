FILESEXTRAPATHS:prepend :=  "${THISDIR}/file:"

SRC_URI:append = " file://arbel_a35_bootblock_no_tip_test.bin"

do_deploy:append() {
    install -D -m 0644 ${WORKDIR}/arbel_a35_bootblock_no_tip_test.bin ${DEPLOYDIR}/arbel_a35_bootblock_no_tip.bin
}

