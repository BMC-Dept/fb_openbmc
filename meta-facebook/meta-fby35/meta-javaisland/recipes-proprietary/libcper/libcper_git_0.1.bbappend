FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Skip the first 4 bytes while parsing the CPER raw data file by libcper 
# because NVIDIA SatMC sends additional 4 bytes at the beginning of 
# each CPER raw data. These 4 bytes are formatVersion (uint8), 
# formatType (uint8) and eventDataLength (uint16), which are the 
# header of the add event data for CPEREvent request command, 
# but are incompatible with libcper.
SRC_URI:append = " \
    file://0001-Skip-parsing-extra-4-bytes-of-NVIDIA-header-raw-data.patch \
"