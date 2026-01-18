set -e
set -x

ROOT_DIR="/Users/changchenchen/Desktop/312552011_lab03"
DIST_DIR="${ROOT_DIR}/lab03_dist"

docker exec x86_lab03 /bin/bash -c "cd /build/lab03_dist && make"
# x86_64-linux-gnu-gcc -o libsolver.so -shared -fPIC libsolver.c

cd ${DIST_DIR}
python3 submit.py libsolver.so
