require meta-yosemite4/recipes-phosphor/images/fb-yosemite4-phosphor-image.inc

OBMC_IMAGE_EXTRA_INSTALL:append = " \
    bios-fw-update \
    cpld-fw-handler \
    cxl-fw-update \
    pldm-fw-update \
    pldm-package-re-wrapper \
    compute-software-id \
    loadsvf \
    mfg-tool \
    eid-auto-discovery \
"

