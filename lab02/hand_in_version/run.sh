set -e
set -x

ROOT_DIR="/Users/changchenchen/Desktop/312552011_lab02"
DIST_DIR="${ROOT_DIR}/dist"
ROOTFS="${DIST_DIR}/rootfs"

# cpio -i
# if [ -d "$ROOTFS" ]; then
#     # ./dist/rootfs exit
#     rm -rf $ROOTFS
# fi

# mkdir -p $ROOTFS
# bzip2 -dc $ROOT_DIR/origin_rootfs.cpio.bz2 | (cd $ROOTFS && cpio -idm)

# -------------------------

# compile code and add .ko and executable to rootfs
docker exec x86_test /bin/bash -c "cd /build/mazemod && make ARCH=x86_64 CROSS_COMPILE=x86_64-linux-gnu-"
cp -r $ROOT_DIR/mazemod $ROOTFS
# cp -r $ROOT_DIR/hellomod $ROOTFS
# cp -r $ROOT_DIR/verify $ROOTFS

cd $ROOTFS
find . | cpio -o -H newc | bzip2 > $DIST_DIR/rootfs.cpio.bz2

# /bin/bash -c $ROOT_DIR/qemu.sh


