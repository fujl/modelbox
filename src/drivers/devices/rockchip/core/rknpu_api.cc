
/*
 * Copyright 2021 The Modelbox Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "modelbox/device/rockchip/rknpu_api.h"

#include "modelbox/device/rockchip/device_rockchip.h"
#include "securec.h"

namespace modelbox {

#define JPEG_DEC_TIMEOUT 1000

const static std::map<MppFrameFormat, RgaSURF_FORMAT> mpp_rgb_fmt_map = {
    {MPP_FMT_YUV420SP, RK_FORMAT_YCbCr_420_SP},
    {MPP_FMT_YUV420SP_VU, RK_FORMAT_YCrCb_420_SP},
    {MPP_FMT_YUV422SP, RK_FORMAT_YCbCr_422_SP},
    {MPP_FMT_YUV422SP_VU, RK_FORMAT_YCrCb_422_SP},
    {MPP_FMT_YUV422P, RK_FORMAT_YCbCr_422_P},
    {MPP_FMT_RGB888, RK_FORMAT_RGB_888},
    {MPP_FMT_BGR888, RK_FORMAT_BGR_888}};

std::shared_ptr<modelbox::Buffer> CreateEmptyMppImg(
    int w, int h, RgaSURF_FORMAT fmt, std::shared_ptr<Device> &dev,
    rga_buffer_t &rga_buf) {
  int div_num = 1;
  if (fmt == RK_FORMAT_YCbCr_420_SP || fmt == RK_FORMAT_YCrCb_420_SP) {
    div_num = 2;
  }

  int ws = MPP_ALIGN(w, MPP_ALIGN_WIDTH);
  int hs = MPP_ALIGN(h, MPP_ALIGN_HEIGHT);
  auto buffer_ptr = std::make_shared<Buffer>(dev);
  int buf_size = ws * hs * 3 / div_num;

  auto ret = buffer_ptr->Build(buf_size);
  if (ret != STATUS_OK) {
    MBLOG_ERROR << "Create buffer fail, size=" << buf_size;
    return nullptr;
  }

  MppBuffer mpp_buf = (MppBuffer)(buffer_ptr->MutableData());
  if (mpp_buf == NULL) {
    MBLOG_ERROR << "MppBuffer is invalid.";
    return nullptr;
  }

  rga_buf = wrapbuffer_fd(mpp_buffer_get_fd(mpp_buf), w, h, fmt, ws, hs);
  return buffer_ptr;
}

RgaSURF_FORMAT GetRGAFormat(const std::string fmt_str) {
  const std::map<std::string, RgaSURF_FORMAT> fmt_map = {
      {"nv21", RK_FORMAT_YCbCr_420_SP},
      {"nv12", RK_FORMAT_YCrCb_420_SP},
      {"rgb", RK_FORMAT_RGB_888},
      {"bgr", RK_FORMAT_BGR_888}};

  auto iter = fmt_map.find(fmt_str);
  if (iter == fmt_map.end()) {
    MBLOG_ERROR << "Not support fmt: " << fmt_str;
    return RK_FORMAT_UNKNOWN;
  }

  return iter->second;
}

Status CopyNVMemory(uint8_t *psrc, uint8_t *pdst, int w, int h, int ws,
                    int hs) {
  // copy y
  uint8_t *ysrc = psrc;
  uint8_t *ydst = pdst;
  for (int i = 0; i < h; i++) {
    if (0 != memcpy_s(ydst, w, ysrc, w)) {
      MBLOG_ERROR << "memcpy_s fail";
      return STATUS_FAULT;
    }

    ysrc += ws;
    ydst += w;
  }

  uint8_t *uvsrc = psrc + ws * hs;
  uint8_t *uvdst = pdst + w * h;
  for (int i = 0; i < h / 2; i++) {
    if (0 != memcpy_s(uvdst, w, uvsrc, w)) {
      MBLOG_ERROR << "memcpy_s fail";
      return STATUS_FAULT;
    }

    uvsrc += ws;
    uvdst += w;
  }

  return STATUS_SUCCESS;
}

Status CopyRGBMemory(uint8_t *psrc, uint8_t *pdst, int w, int h, int ws,
                     int hs) {
  uint8_t *rgbsrc = psrc;
  uint8_t *rgbdst = pdst;

  for (int i = 0; i < h; i++) {
    if (0 != memcpy_s(rgbdst, w * 3, rgbsrc, w * 3)) {
      MBLOG_ERROR << "memcpy_s fail";
      return STATUS_FAULT;
    }

    rgbsrc += ws * 3;
    rgbdst += w * 3;
  }
  return STATUS_SUCCESS;
}

Status GetRGAFromImgBuffer(std::shared_ptr<Buffer> &in_img, RgaSURF_FORMAT fmt,
                           rga_buffer_t &rgb_buf) {
  int32_t in_width = 0;
  int32_t in_height = 0;
  int32_t in_wstride = 0;
  int32_t in_hstride = 0;

  in_img->Get("width", in_width);
  in_img->Get("height", in_height);
  in_img->Get("width_stride", in_wstride);
  in_img->Get("height_stride", in_hstride);
  if (RK_FORMAT_BGR_888 == fmt || RK_FORMAT_BGR_888 == fmt) {
    in_wstride = in_wstride / 3;
  }

  if (in_width == 0 || in_height == 0) {
    MBLOG_ERROR << "can not get input width or heigh";
    return STATUS_FAULT;
  }

  if (in_wstride == 0) {
    in_wstride = in_width;
  }

  if (in_hstride == 0) {
    in_hstride = in_height;
  }

  if (in_wstride % MPP_ALIGN_WIDTH != 0 || in_hstride % MPP_ALIGN_HEIGHT != 0) {
    rgb_buf = wrapbuffer_virtualaddr(
        mpp_buffer_get_ptr((MppBuffer)(in_img->ConstData())), in_width,
        in_height, fmt, in_wstride, in_hstride);
  } else {
    rgb_buf = wrapbuffer_fd(mpp_buffer_get_fd((MppBuffer)(in_img->ConstData())),
                            in_width, in_height, fmt, in_wstride, in_hstride);
  }

  return STATUS_SUCCESS;
}

std::shared_ptr<modelbox::Buffer> ColorChange(MppFrame &frame,
                                              RgaSURF_FORMAT fmt,
                                              std::shared_ptr<Device> device) {
  std::shared_ptr<Buffer> buffer = nullptr;
  int32_t w = (int32_t)mpp_frame_get_width(frame);
  int32_t h = (int32_t)mpp_frame_get_height(frame);
  int32_t ws = (int32_t)mpp_frame_get_hor_stride(frame);
  int32_t hs = MPP_ALIGN(h, MPP_ALIGN_HEIGHT);  // frame allign too large
  int32_t height = h;
  int32_t channel = 3;

  auto iter = mpp_rgb_fmt_map.find(mpp_frame_get_fmt(frame));
  if (iter == mpp_rgb_fmt_map.end()) {
    MBLOG_ERROR << "fmt not support: " << mpp_frame_get_fmt(frame);
    mpp_frame_deinit(&frame);
    return nullptr;
  }

  RgaSURF_FORMAT src_fmt = iter->second;
  if ((h % 2 != 0 || w % 2 != 0) &&
      (RK_FORMAT_RGB_888 != src_fmt && RK_FORMAT_BGR_888 != src_fmt)) {
    // mpp may give an odd height even in yuv format, fix it
    mpp_frame_set_height(frame, (h + 1) / 2 * 2);
    mpp_frame_set_width(frame, (w + 1) / 2 * 2);
  }

  if (src_fmt == fmt) {
    buffer = std::make_shared<Buffer>(device);
    MppBuffer mppbuf = mpp_frame_get_buffer(frame);
    buffer->Build((void *)(mppbuf), mpp_buffer_get_size(mppbuf),
                  [frame](void *p) {
                    MppFrame tmp = frame;
                    mpp_frame_deinit(&tmp);
                  });
    // todo  here maybe wrong  if input is not yuv420sp : liyuming5
    height = h * 3 / 2;
    channel = 1;
  } else {
    // others format need colorspace change
    rga_buffer_t src_buf = MPPFRAMETORGA(frame, src_fmt);
    rga_buffer_t dst_buf;
    buffer = CreateEmptyMppImg(w, h, fmt, device, dst_buf);
    if (buffer == nullptr) {
      MBLOG_ERROR << "create mpp img failed.";
      mpp_frame_deinit(&frame);
      return nullptr;
    }

    IM_STATUS status = imcvtcolor(src_buf, dst_buf, src_fmt, fmt);
    mpp_frame_deinit(&frame);
    if (status != IM_STATUS_SUCCESS) {
      MBLOG_ERROR << "rga convert color failed: " << status;
      return nullptr;
    }
  }

  buffer->Set("type", modelbox::ModelBoxDataType::MODELBOX_UINT8);
  buffer->Set("channel", channel);
  buffer->Set("shape",
              std::vector<size_t>{(size_t)height, (size_t)w, (size_t)channel});
  buffer->Set("width", w);
  buffer->Set("height", h);
  if (RK_FORMAT_BGR_888 == fmt || RK_FORMAT_BGR_888 == fmt) {
    buffer->Set("width_stride", ws * 3);
  } else {
    buffer->Set("width_stride", ws);
  }

  buffer->Set("height_stride", hs);
  buffer->Set("layout", std::string("hwc"));
  return buffer;
}

//-----------------------------------------------------------------------------------------------------------------
// https://github.com/rockchip-linux/gstreamer-rockchip/blob/master/gst/rockchipmpp/gstmppjpegdec.c
#define FFD8_OFFSET 2
#define SOFO_OFFSET 3
#define SEG_OFFSET 2
#define CHAR_SHORT 256
void MppJpegDec::GetJpegWH(int &nW, int &nH, unsigned char *buf, int bufLen) {
  nH = 0;
  nW = 0;
  int offset = FFD8_OFFSET;  // 跳过FFD8
  unsigned char type = 0xff;
  do {
    while (offset < bufLen && buf[offset] != 0xff) {
      offset++;
    }

    offset++;
    while (offset < bufLen && buf[offset] == 0xff) {
      offset++;
    }

    offset++;
    type = (unsigned char)buf[offset - 1];
    switch (type) {
      case 0x00:
      case 0x01:
      case 0xd0:
      case 0xd1:
      case 0xd2:
      case 0xd3:
      case 0xd4:
      case 0xd5:
      case 0xd6:
      case 0xd7:
        break;
      case 0xc0:  // SOF0段（图像基本信息）
      case 0xc2:  // JFIF格式的 SOF0段
      {
        // 找到SOFO段，解析宽度和高度信息
        offset += SOFO_OFFSET;
        // 高度 占2字节 低位高位互换
        nH = buf[offset++] * CHAR_SHORT;
        nH += buf[offset++];
        // 宽度 占2字节 低位高位互换
        nW = buf[offset++] * CHAR_SHORT;
        nW += buf[offset++];
        return;
      }
      default:  // 别的段都跳过
        // 获取段长度，直接跳过
        int offsetTmp = buf[offset++] * CHAR_SHORT;
        offsetTmp += offset + buf[offset] - SEG_OFFSET;
        offset = offsetTmp;
        break;
    }
  } while (type != 0xda && offset < bufLen);  // 扫描行开始
}

MppJpegDec::MppJpegDec() {}

MppJpegDec::~MppJpegDec() {
  ShutDown();
  if (rk_api_) {
    rk_api_->reset(codec_ctx_);
  }

  if (codec_ctx_) {
    mpp_destroy(codec_ctx_);
    codec_ctx_ = nullptr;
  }

  if (frm_grp_) {
    mpp_buffer_group_put(frm_grp_);
    frm_grp_ = NULL;
  }
}

Status MppJpegDec::Init() {
  CHECK_MPP_RET(mpp_create(&codec_ctx_, &rk_api_),
                "failed to run mpp_create: ", STATUS_FAULT);

  RK_U32 timeout = JPEG_DEC_TIMEOUT;
  CHECK_MPP_RET(rk_api_->control(codec_ctx_, MPP_SET_OUTPUT_TIMEOUT, &timeout),
                "Failed to set output timeout 0 fail: ", STATUS_FAULT);

  CHECK_MPP_RET(mpp_init(codec_ctx_, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG),
                "failed to run mpp_init.", STATUS_FAULT);

  CHECK_MPP_RET(mpp_buffer_group_get_internal(&frm_grp_, MPP_BUFFER_TYPE_DRM),
                "failed to get buffer group: ", STATUS_FAULT);

  return STATUS_OK;
}

Status MppJpegDec::ShutDown() {
  MppPacket eos_packet = nullptr;
  MppBuffer eos_buf = nullptr;

  /* Prepare EOS packet */
  mpp_buffer_get(frm_grp_, &eos_buf, 1);
  mpp_packet_init_with_buffer(&eos_packet, eos_buf);
  mpp_buffer_put(eos_buf);
  mpp_packet_set_size(eos_packet, 0);
  mpp_packet_set_length(eos_packet, 0);
  mpp_packet_set_eos(eos_packet);

  DecPkt(eos_packet);
  mpp_packet_deinit(&eos_packet);
  return STATUS_OK;
}

Status MppJpegDec::DecPkt(MppPacket &packet, int w, int h) {
  MppTask task = nullptr;

  if (rk_api_->poll(codec_ctx_, MPP_PORT_INPUT, MPP_POLL_BLOCK) != MPP_OK) {
    return STATUS_CONTINUE;
  }

  CHECK_MPP_RET(rk_api_->dequeue(codec_ctx_, MPP_PORT_INPUT, &task),
                "mpp task input dequeue failed: ", STATUS_FAULT);
  if (!task) {
    MBLOG_ERROR << "SendBuf get task fail";
    return STATUS_FAULT;
  }

  MppBuffer frm_buf = nullptr;
  MppFrame frame = nullptr;
  CHECK_MPP_RET(mpp_frame_init(&frame),
                "mpp_frame_init failed: ", STATUS_FAULT);

  if (w > 0 && h > 0) {
    CHECK_MPP_RET(mpp_buffer_get(frm_grp_, &frm_buf,
                                 MPP_ALIGN(w, MPP_ALIGN_MPP_WH) *
                                     MPP_ALIGN(h, MPP_ALIGN_MPP_WH) * 2),
                  "mpp_buffer_get failed: ", STATUS_FAULT);

    mpp_frame_set_buffer(frame, frm_buf);
    mpp_buffer_put(frm_buf);
  }

  mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet);
  auto meta = mpp_frame_get_meta(frame);
  mpp_meta_set_packet(meta, KEY_INPUT_PACKET, packet);
  mpp_task_meta_set_frame(task, KEY_OUTPUT_FRAME, frame);

  if (rk_api_->enqueue(codec_ctx_, MPP_PORT_INPUT, task) != MPP_OK) {
    MBLOG_ERROR << "mpp task input enqueue failed: ";
    mpp_frame_deinit(&frame);
    return STATUS_FAULT;
  }

  return STATUS_OK;
}

Status MppJpegDec::SendBuf(void *in_buf, int buf_len, int &w, int &h) {
  MppPacket packet = nullptr;
  if (w == 0 || h == 0) {
    GetJpegWH(w, h, (unsigned char *)in_buf, buf_len);
  }

  if (w == 0 || h == 0) {
    MBLOG_WARN << "get jpeg w or h fail";
    return STATUS_FAULT;
  }

  MppBuffer mpp_buf = nullptr;
  CHECK_MPP_RET(mpp_buffer_get(frm_grp_, &mpp_buf, buf_len),
                "mpp_buffer_get failed: ", STATUS_FAULT);

  mpp_buffer_write(mpp_buf, 0, in_buf, buf_len);
  mpp_packet_init_with_buffer(&packet, mpp_buf);
  mpp_buffer_put(mpp_buf);

  return DecPkt(packet, w, h);
}

Status MppJpegDec::SendBuf(MppBuffer &in_buf, int &w, int &h) {
  MppPacket packet = nullptr;
  if (w == 0 || h == 0) {
    GetJpegWH(w, h, (unsigned char *)mpp_buffer_get_ptr(in_buf),
              mpp_buffer_get_size(in_buf));
  }

  if (w == 0 || h == 0) {
    mpp_buffer_put(in_buf);
    MBLOG_ERROR << "get jpeg w or h fail";
    return STATUS_FAULT;
  }

  CHECK_MPP_RET(mpp_packet_init_with_buffer(&packet, in_buf),
                "mpp_packet_init failed: ", STATUS_FAULT);
  mpp_buffer_put(in_buf);

  return DecPkt(packet, w, h);
}

Status MppJpegDec::ReceiveFrame(MppFrame &out_frame) {
  MppTask task = nullptr;

  /* poll and wait here */
  CHECK_MPP_RET(rk_api_->poll(codec_ctx_, MPP_PORT_OUTPUT, MPP_POLL_BLOCK),
                "mpp output poll failed: ", STATUS_NODATA);
  CHECK_MPP_RET(rk_api_->dequeue(codec_ctx_, MPP_PORT_OUTPUT, &task),
                "mpp task output dequeue failed: ", STATUS_FAULT);
  if (!task) {
    MBLOG_ERROR << "ReceiveFrame get task fail";
    return STATUS_FAULT;
  }

  mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &out_frame);
  auto meta = mpp_frame_get_meta(out_frame);
  MppPacket packet = nullptr;
  mpp_meta_get_packet(meta, KEY_INPUT_PACKET, &packet);
  if (packet) {
    mpp_packet_deinit(&packet);
  }

  /* output queue */
  rk_api_->enqueue(codec_ctx_, MPP_PORT_OUTPUT, task);
  return STATUS_OK;
}

}  // namespace modelbox
