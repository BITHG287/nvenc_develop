#include "nv_encoder.h"

#include <jsoncpp/json/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

// #define PORT 9999
// #define W 3840
// #define H 2160
// #define SHARE_FILE "/dev/shm/1.yuv"
// #define OUT_FILE "/media/hp/DATA/4k/RealBasicVSR/hugui/out.ts"
// #define FPS 2500  // 2398, 2400, 2500

std::string OUT_FILE = "1.ts";
std::string SHARE_FILE = "/dev/shm/1.yuv";
int PORT = 9999;
int W = 3840;
int H = 2160;
int FPS = 2500;

int parse_json(std::string fname)
{
    Json::Reader json_reader;
    Json::Value root;
    std::ifstream json_fp("cfg.json", std::ios::binary);
    if ( !json_fp.is_open() )
    {
        std::cout << "error opening json file" << std::endl;
        return -1;
    }

    if (json_reader.parse(json_fp, root))
    {
        OUT_FILE = root["dst_video"].asString();
        SHARE_FILE = root["share_file"].asString();
        PORT = root["PORT"].asInt();
        W = root["W"].asInt();
        H = root["H"].asInt();
        FPS = root["FPS"].asInt();
        std::cout << "parse json complete!" << std::endl;
    }
    else
    {
        std::cout << "parse json error" << std::endl;
        return -2;
    }

    json_fp.close();

    return 0;
}

int main(int argc, char **argv)
{
    int ret = parse_json("cfg.json");

    // 1. 创建socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0)
    {
        std::cout << "cpp create socket failed" << std::endl;
        return 0;
    }
    struct sockaddr_in addr_serv;
    int len;
    memset(&addr_serv, 0, sizeof(struct sockaddr_in));
    addr_serv.sin_family = AF_INET;
    addr_serv.sin_port = htons(PORT);
    addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
    len = sizeof(addr_serv);

    if (bind(sock_fd, (struct sockaddr *)&addr_serv, sizeof(addr_serv)) < 0)
    {
        std::cout << "cpp socket bind error" << std::endl;
        return 0;
    }

    int recv_num;
    int send_num;
    char send_buf[32] = "done";
    char recv_buf[32];
    std::string recv_msg;
    struct sockaddr_in addr_client;
    
    // 2. "/de/shm/share"
    int size = 3 * H * W;
    unsigned char *buf = (unsigned char *)malloc(size);
    std::string filename = SHARE_FILE;
    FILE *shared_fd;
    int open_flag = 0;
    
    // 3. 创建编码器
    int fps = FPS;  // 2500, 2400, 2398
    int frame_num = 250;
    Nv_Encoder nvenc;
    nvenc.init(OUT_FILE, fps, W, H);
    std::cout << "please start python..." << std::endl;

    // 4. 进入编码循环
    int cnt = 0;
    while (1)
    {   
        memset(recv_buf, 0, sizeof(recv_buf));
        recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr_client, (socklen_t *)&len);
        if (recv_num < 0)
        {
            std::cout << "cpp recv error" << std::endl;
            return 0;
        }
        recv_msg = recv_buf;
        // std::cout << "recv_msg is " << recv_msg << std::endl;

        if (recv_msg == "stop")
            break;

        if (recv_msg == "start")
        {
            if (open_flag == 0)  // 第一次收到编码信息
            {
                shared_fd = fopen(filename.c_str(), "rb");
                if (shared_fd == NULL)
                {
                    std::cout << "cpp can't get shared mem" << std::endl;
                    return 0;
                }
                open_flag = 1;
            }
            // std::cout << "cpp nvenc start..." << std::endl;
            fseek(shared_fd, 0, SEEK_SET);
            int ret = fread(buf, size, 1, shared_fd);
            ret = nvenc.proceed(buf, cnt);
            // std::cout << cnt << " : " << "nvenc proceed ret = " << ret << std::endl;
            cnt += 1;
            len = sizeof(addr_client);
            send_num = sendto(sock_fd, send_buf, sizeof(send_buf), 0, (struct sockaddr *)&addr_client, len);
            if (send_num < 0)
            {
                std::cout << "cpp send error, send_num = " << send_num << std::endl;
                return 0;
            }
            // std::cout << "cpp nvenc end..." << std::endl;
        }
    }
    
    // 5. 退出
    nvenc.release();
    free(buf);
    close(sock_fd);
    fclose(shared_fd);

    return 0;
}

#if 0
int main(int argc, char **argv)
{
    if (argc < 3) {
        std::cout << "input params not correct, example: main.out input.yuv output.ts" << std::endl;
        return 0;
    }

    // std::string in_file = "/home/hugui/mnt/work/test.yuv";
    // std::string out_file = "test.ts";
    std::string in_file = std::string(argv[1]);
    std::string out_file = std::string(argv[2]);
    
    float fps = 25.0;
    int frame_num = 250;
    int w = 3840;
    int h = 2160;
    int size = w * h * 3;
    uint8_t *p_buf = (uint8_t *)malloc(size);

    FILE *p_in_file = fopen(in_file.c_str(), "rb");

    Nv_Encoder nvenc;
    nvenc.init(out_file, fps);

    for (int i = 0; i < frame_num; i++)
    {
        if ( fread(p_buf, 1, size, p_in_file) <= 0 )
        {
            std::cout << "failed to read raw data" << std::endl;
            fclose(p_in_file);
            return -1;
        }
        else if(feof(p_in_file))
        {
            break;
        }

        nvenc.proceed(p_buf, i);
    }
    
    nvenc.release();

    fclose(p_in_file);
    free(p_buf);

    return 0;
}
#endif
