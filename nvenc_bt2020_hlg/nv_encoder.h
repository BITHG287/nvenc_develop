#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif

class Nv_Encoder
{
public:
    Nv_Encoder();
    ~Nv_Encoder();
    int32_t init(std::string out_file, int fps, int w, int h);
    int32_t proceed(uint8_t *buf, int32_t idx);
    int32_t release();
    
    int32_t flush_encoder(AVFormatContext *fmt_ctx, uint32_t stream_idx);
    void YUV420P10LE_to_P010LE(uint16_t *src, uint16_t *dst, int w, int h);

private:
    AVFormatContext *_pFormatCtx;
    AVOutputFormat *_pOutputFmt;
    AVStream *_pVideoStream;
    AVCodecContext *_pCodecCtx;
    AVCodec *_pCodec;
    AVFrame *_pFrame;
    AVDictionary *_pEncodeParam;
    AVPacket _pkt;

    // uint8_t *_pBuf_YUV420P10LE;
    uint8_t *_pBuf_P010LE;  // 10bit的NV12
    int32_t _w;
    int32_t _h;
    int _fps;
    int32_t _bufSize;

    std::string _out_file;

};



