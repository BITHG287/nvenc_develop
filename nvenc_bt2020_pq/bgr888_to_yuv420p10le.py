import socket 
import os
import numpy as np
import cupy as cp
import cv2
import time
import json

fp = open("cfg.json", "rb")
cfgs = json.load(fp)
fp.close()

# src_video = "/media/hp/DATA/4k/RealBasicVSR/hugui/out.mp4"
# save_yuv = "/dev/shm/1.yuv"
src_video = cfgs["src_video"]
save_yuv = cfgs["share_file"]
if os.path.exists(save_yuv):
    os.remove(save_yuv)

PORT = cfgs["PORT"]
udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
localaddr = ("127.0.0.1", PORT)
start_msg = "start"
stop_msg = "stop"

M = cp.array([[0.2627, 0.6780, 0.0593], [-0.1396, -0.3604, 0.5000], [0.5000, -0.4598, -0.0402]])
M2 = cp.array([[0.6274039, 0.32928304, 0.04331307], [0.06909729, 0.9195404, 0.01136232], [0.01639144, 0.08801331, 0.89559525]])
m_1 = 2610 / 4096 * (1 / 4)
m_2 = 2523 / 4096 * 128
c_1 = 3424 / 4096
c_2 = 2413 / 4096 * 32
c_3 = 2392 / 4096 * 32

# 将8bit色域的视频，转换为10bit色域的yuv文件
def img888_to_yuvp10le(img, gain=300):    
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    base_rgb_8bit = img.astype(np.float32)

    base_rgb_8bit_gpu = cp.asarray(base_rgb_8bit)
    linear_bt709_gpu = (base_rgb_8bit_gpu / 255.0) ** 2.4

    linear_2020_gpu = cp.einsum('...ij,...j->...i', M2, linear_bt709_gpu)

    linear_2020_gain_gpu = linear_2020_gpu * gain

    # C = cp.clip(linear_2020_gain_gpu, 0, 1)
    C = linear_2020_gain_gpu / 10000 # ST2084峰值为10000
    Y_p = cp.sign(C) * cp.abs(C) ** m_1
    Y_p[cp.isnan(Y_p)] = 0

    tmp = (c_1 + c_2 * Y_p) / (c_3 * Y_p + 1)
    st2084_2020_gpu = cp.sign(tmp) * cp.abs(tmp) ** m_2
    # st2084_2020_gpu = cp.clip(tmp1, 0, 1)

    EYUV_GPU = cp.einsum('...ij,...j->...i', M, st2084_2020_gpu)
    
    Y_GPU = cp.clip((EYUV_GPU[:, :, 0] * 219 + 16 ) * 4, 0, 1023).astype(cp.uint16)
    U_GPU = cp.clip((EYUV_GPU[:, :, 1] * 224 + 128) * 4, 0, 1023).astype(cp.uint16)
    V_GPU = cp.clip((EYUV_GPU[:, :, 2] * 224 + 128) * 4, 0, 1023).astype(cp.uint16)

    Y = cp.asnumpy(Y_GPU.reshape(-1).astype(cp.uint16))
    U = cp.asnumpy(U_GPU[::2, ::2].reshape(-1).astype(cp.uint16))
    V = cp.asnumpy(V_GPU[::2, ::2].reshape(-1).astype(cp.uint16))

    return Y, U, V

def main(src_video, save_yuv, gain=250):
    # 1. 打开要写入的yuv文件
    fp = open(save_yuv, "wb")

    # 2. 获取视频信息
    video = cv2.VideoCapture(src_video)
    num_frames = int(video.get(7))
    print(num_frames)
    W = int(video.get(3))
    H = int(video.get(4))

    # 3. 开始转码
    # num_frames = 1500
    for i in range(num_frames):
        t1 = time.time()

        # 1. 读取一帧，转为10bit yuv420
        ret, img = video.read()
        img = cv2.resize(img, (3840, 2160))
        if not ret:
            print(i, "read video frame error")
            continue        

        Y, U, V = img888_to_yuvp10le(img, gain)
        data_bin = Y.astype('<u2').tostring()
        fp.write(data_bin)
        data_bin = U.astype('<u2').tostring()
        fp.write(data_bin)
        data_bin = V.astype('<u2').tostring()
        t2 = time.time()  # 转码结束时间戳

        # 2. 写入文件
        fp.write(data_bin)
        fp.flush()
        fp.seek(0, 0)
        t3 = time.time()  # 写入文件时间戳

        # 3. 发送start信号到cpp进程，开始压缩编码
        send_data = "start"
        udp_socket.sendto(send_data.encode('utf-8'), localaddr)

        # 4. 等待cpp进程编码结束
        recv_data = udp_socket.recvfrom(32)
        t4 = time.time()  # 转码结束时间戳
        
        # 3. 发送start信号到cpp进程，开始压缩编码
        send_data = "start"
        udp_socket.sendto(send_data.encode('utf-8'), localaddr)

        # 4. 等待cpp进程编码结束
        recv_data = udp_socket.recvfrom(32)
        t4 = time.time()  # 转码结束时间戳

        if i % 50 == 0:
            print("[%d], [%d] trans %.3fms, save %.3fms, encode %.3fms, total %.3fms" % (num_frames, i, 1000*(t2-t1), 1000*(t3-t2), 1000*(t4-t3), 1000*(t4-t1)))

    # 4. 释放资源、关闭文件
    send_data = "stop"
    udp_socket.sendto(send_data.encode('utf-8'), localaddr)
    time.sleep(1)

    video.release()
    fp.close()
    udp_socket.close()

if __name__ == '__main__':
    start_time = time.time()
    main(src_video, save_yuv, gain=250)
    stop_time = time.time()
    print("total cost %.3fs" % (stop_time - start_time))
