#!/bin/bash

PACKAGE_NAME="kperf"
PACKAGE_DIR="kperf-package"

rm -r "${PACKAGE_DIR}"
make release

mkdir -p "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/etc/kperf"
mkdir -p "${PACKAGE_DIR}/usr/bin"
mkdir -p "${PACKAGE_DIR}/DEBIAN"
cp build/kperf "${PACKAGE_DIR}/usr/bin/"
cp assets/* "${PACKAGE_DIR}/etc/kperf/"
cp scripts/control "${PACKAGE_DIR}/DEBIAN/"

VERSION=$(grep "^Version:" "${PACKAGE_DIR}/DEBIAN/control" | awk '{print $2}')
OUTPUT_FILE="${PACKAGE_NAME}_${VERSION}_amd64.deb"

rm -f "${OUTPUT_FILE}"

chmod 755 "${PACKAGE_DIR}/usr/bin/kperf"
chmod 644 "${PACKAGE_DIR}/etc/kperf/"*

dpkg-deb --build "${PACKAGE_DIR}" "${OUTPUT_FILE}"

echo "Package built: ${OUTPUT_FILE}"
