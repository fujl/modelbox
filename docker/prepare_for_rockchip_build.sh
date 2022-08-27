#!/bin/sh

MINDSPORE_VER="1.7.0"
OSARCH="$(uname -m)"
MINDSPORE_FILE_ARCH="${OSARCH}"

install_mindspore_lite_packages() {
  if [ ! -e "/usr/local/mindspore-lite" ]; then
    wget https://ms-release.obs.cn-north-4.myhuaweicloud.com/${MINDSPORE_VER}/MindSpore/lite/release/linux/${OSARCH}/mindspore-lite-${MINDSPORE_VER}-linux-${MINDSPORE_FILE_ARCH}.tar.gz
    if [ $? -ne 0 ]; then
      echo "download mindspore lite failed."
      return 1
    fi

    tar xf mindspore-lite-${MINDSPORE_VER}-linux-${MINDSPORE_FILE_ARCH}.tar.gz
    if [ $? -ne 0 ]; then
      echo "extract mindspore failed."
      return 1
    fi

    mv mindspore-lite-${MINDSPORE_VER}-linux-${MINDSPORE_FILE_ARCH} /usr/local/
    if [ $? -ne 0 ]; then
      echo "move mindspore lite failed."
      return 1
    fi

    ln -s /usr/local/mindspore-lite-${MINDSPORE_VER}-linux-${MINDSPORE_FILE_ARCH} /usr/local/mindspore-lite
  fi
  
  return 0
}

install_mindspore_lite_packages