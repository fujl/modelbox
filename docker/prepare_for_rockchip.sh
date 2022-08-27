#!/bin/bash
CODE_DIR=$(cd $(dirname $0)/..;pwd)

echo ${CODE_DIR}
mkdir -p ${CODE_DIR}/rockchip
unset https_proxy
unset http_proxy
apt update
apt-get install -y gnutls-bin
git config --global http.sslVerify false
git config --global http.postBuffer 524288000
git config -l

# download lib rga
function download_rga() {
  mkdir -p ${CODE_DIR}/rga
  cd ${CODE_DIR}/rga
  git clone -b 1.3.2_release https://github.com/airockchip/librga.git
  echo "clone librga finish"
  echo `ls -l librga`
  mkdir -p ${CODE_DIR}/rk-rga
  mkdir -p ${CODE_DIR}/rk-rga/lib
  cp -rfd ${CODE_DIR}/rga/librga/include ${CODE_DIR}/rk-rga
  cp -rfd ${CODE_DIR}/rga/librga/libs/Linux/gcc-aarch64/* ${CODE_DIR}/rk-rga/lib

  cd ${CODE_DIR}
  tar -czf rk-rga.tar.gz rk-rga
  mv rk-rga.tar.gz ${CODE_DIR}/rockchip/rk-rga.tar.gz

  rm -rf rk-rga rga
}

# download lib rknn
download_rknn() {
  mkdir -p ${CODE_DIR}/rknpu
  cd ${CODE_DIR}/rknpu
  git clone -b master https://github.com/rockchip-linux/RKNPUTools.git
  echo "clone RKNPUTools finish"
  echo `ls -l RKNPUTools`
  mkdir -p ${CODE_DIR}/rknn
  mkdir -p ${CODE_DIR}/rknn/lib
  cp -rfd RKNPUTools/rknn-api/Linux/rknn_api_sdk/rknn_api/arm/include ${CODE_DIR}/rknn
  cp -rfd RKNPUTools/rknn-api/Linux/rknn_api_sdk/rknn_api/arm/lib64/* ${CODE_DIR}/rknn/lib

  cd ${CODE_DIR}
  tar -czf rknn.tar.gz rknn
  mv rknn.tar.gz ${CODE_DIR}/rockchip/rknn.tar.gz

  rm -rf rknpu rknn
}

# download lib rknnrt
download_rknnrt() {
  mkdir -p ${CODE_DIR}/rknpu2
  cd ${CODE_DIR}/rknpu2
  git clone -b master https://github.com/rockchip-linux/rknpu2.git
  echo "clone rknpu2 finish"
  echo `ls -l rknpu2`
  mkdir -p ${CODE_DIR}/rknnrt
  mkdir -p ${CODE_DIR}/rknnrt/lib
  cp -rfd rknpu2/runtime/RK356X/Linux/librknn_api/include ${CODE_DIR}/rknnrt
  cp -rfd rknpu2/runtime/RK356X/Linux/librknn_api/aarch64/* ${CODE_DIR}/rknnrt/lib

  cd ${CODE_DIR}
  tar -czf rknnrt.tar.gz rknnrt
  mv rknnrt.tar.gz ${CODE_DIR}/rockchip/rknnrt.tar.gz

  rm -rf rknpu2 rknnrt
}

# download mpp and build lib mpp
download_rkmpp() {
  mkdir -p ${CODE_DIR}/mpp
  cd ${CODE_DIR}/mpp
  git clone -b develop https://github.com/rockchip-linux/mpp.git
  echo "clone mpp finish"
  echo `ls -l mpp`
  cd mpp
  if [ ! -d build ]; then
    mkdir build
  fi
  mkdir -p ${CODE_DIR}/rkmpp
  cd build
  cmake -DCMAKE_INSTALL_PREFIX=${CODE_DIR}/rkmpp ..
  make -j8
  make install
  cd ${CODE_DIR}
  tar -czf rkmpp.tar.gz rkmpp
  mv rkmpp.tar.gz ${CODE_DIR}/rockchip/rkmpp.tar.gz
  
  rm -rf mpp rkmpp
}

download() {
    url="$1"
    softName=${url##*/}
    echo -e "\n\nBegen to download ${softName}"
    curl -LO ${url}

    if [ "$(ls -lh ${softName}|awk '{print $5}')" == "0" ]; then
        echo "package download failed"
        exit 1
    fi
}

download_thirdparty_rk() {
  mkdir -p ${CODE_DIR}/rk_3rd
  cd ${CODE_DIR}/rk_3rd
  download http://download.modelbox-ai.com/third-party/aarch64/rockchip/rk-rga.tar.gz
  download http://download.modelbox-ai.com/third-party/aarch64/rockchip/rknn.tar.gz
  download http://download.modelbox-ai.com/third-party/aarch64/rockchip/rknnrt.tar.gz

  mv rk-rga.tar.gz ${CODE_DIR}/rockchip/rk-rga.tar.gz
  mv rknn.tar.gz ${CODE_DIR}/rockchip/rknn.tar.gz
  mv rknnrt.tar.gz ${CODE_DIR}/rockchip/rknnrt.tar.gz

  cd ${CODE_DIR}
  rm -rf rk_3rd
}

#download_rga
#download_rknn
#download_rknnrt
download_thirdparty_rk
download_rkmpp
