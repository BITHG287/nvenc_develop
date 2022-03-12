#include "nv_encoder.h"

Nv_Encoder::Nv_Encoder()
{

}

Nv_Encoder::~Nv_Encoder()
{

}

int32_t Nv_Encoder::flush_encoder(AVFormatContext *fmt_ctx, uint32_t stream_idx)
{
    int ret, got_frame;
    AVPacket enc_pkt;
    if ( !(fmt_ctx->streams[stream_idx]->codec->codec->capabilities & AV_CODEC_CAP_DELAY) )
		return 0;
	while (1) 
    {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2(fmt_ctx->streams[stream_idx]->codec, &enc_pkt, NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame)
        {
			ret = 0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame, size: %5d\n", enc_pkt.size);
		/* mux encoded frame */
		ret = av_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
            break;
	}
	return ret;
}

void Nv_Encoder::YUV420P10LE_to_P010LE(uint16_t *src, uint16_t *dst, int w, int h)
{
    int ySize = w * h;
    int yuvSize = w * h * 3 / 2;
    int uIdx = ySize;
    int vIdx = ySize * 5 / 4;

    // 1. 移位
    for (int i = 0; i < yuvSize; i++)
    {
        src[i] = src[i] << 6;
    }

    // 2. YUV420P转为NV12
    memcpy(dst, src, 2 * ySize);  // 占两个字节
    for (int i = ySize; i < yuvSize; i += 2) {
        *(dst + i) = *(src + uIdx++);
        *(dst + i + 1) = *(src + vIdx++);
    }
}

int32_t Nv_Encoder::init(std::string out_file, int fps, int w, int h)
{
    _w = w;
    _h = h;

    _out_file = out_file;
    _fps = fps;

    int time_base_num, time_base_den;
    time_base_num = 100;
    time_base_den = _fps;
    // if (_fps == 2398) 
    // {
    //     time_base_num = 100;
    //     time_base_den = 2398;
    // }
    // else if (_fps == 2400)
    // {
    //     time_base_num = 100;
    //     time_base_den = 2400;
    // }
    // else if (_fps == 2500)
    // {
    //     time_base_num = 100;
    //     time_base_den = 2500;
    // }
    // else if (_fps == 3000)
    // {
    //     time_base_num = 100;
    //     time_base_den = 3000;
    // }
    // else
    // {
    //     time_base_num = 100;
    //     time_base_den = 2500;
    // }

    av_register_all();  // 注册ffmpeg所有编解码器
    avformat_alloc_output_context2(&_pFormatCtx, NULL, NULL, _out_file.c_str());  // 初始化输出码流的AVFormatContext(获取输出文件的编码格式)

    if (avio_open(&_pFormatCtx->pb, _out_file.c_str(), AVIO_FLAG_READ_WRITE) < 0)
    {
        std::cout << "failed open output file: " << out_file << std::endl;
        return -1;
    }

    _pVideoStream = avformat_new_stream(_pFormatCtx, 0);  //创建输出码流的AVStream

    // 一般fps在代码里这样表示fps = den / num，如果den = 15，num = 1，则fps = 15。如果帧率固定，pts * fps就表示当前是第几帧
    _pVideoStream->time_base.num = time_base_num;
    _pVideoStream->time_base.den = time_base_den;

    _pCodecCtx = _pVideoStream->codec;
    _pCodecCtx->codec_id = AV_CODEC_ID_HEVC;  // 直接指定为HEVC格式
    _pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    _pCodecCtx->pix_fmt = AV_PIX_FMT_P010LE;
    _pCodecCtx->width = _w;
    _pCodecCtx->height = _h;
    _pCodecCtx->time_base.num = time_base_num;
    _pCodecCtx->time_base.den = time_base_den;

    _pCodecCtx->framerate.num = time_base_den;
    _pCodecCtx->framerate.den = time_base_num;

    // _pCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
    int bit_rate = 20000000;
    _pCodecCtx->bit_rate = bit_rate;
    // _pCodecCtx->rc_min_rate = bit_rate;
    // _pCodecCtx->rc_max_rate = bit_rate;
    // _pCodecCtx->bit_rate_tolerance = bit_rate;
    // _pCodecCtx->rc_buffer_size = bit_rate;
    // _pCodecCtx->rc_initial_buffer_occupancy = _pCodecCtx->rc_buffer_size * 3 / 4;
    // _pCodecCtx->rc_buffer_aggressivity = (float)1.0;
    // _pCodecCtx->rc_initial_cplx = 0.5;

    _pCodecCtx->gop_size = 10;   // 每gop_size帧插入1个I帧，I帧越少，视频越小
    // _pCodecCtx->qmin = 0;  // 最大和最小量化系数
    // _pCodecCtx->qmax = 0;
    // _pCodecCtx->qcompress = (float)0.6;
    _pCodecCtx->max_b_frames = 1;  // 设置B帧最大的数量，B帧为视频图片空间的前后预测帧，B帧相对于I、P帧来说，压缩率比较大，采用多编码B帧提高清晰度

    _pEncodeParam = 0;
    av_dict_set(&_pEncodeParam, "preset", "slow", 0);
    // av_dict_set(&_pEncodeParam, "tune", "zero-latency", 0);
    av_dict_set(&_pEncodeParam, "color_primaries", "bt2020", 0);
    av_dict_set(&_pEncodeParam, "color_trc", "smpte2084", 0);
    av_dict_set(&_pEncodeParam, "colorspace", "bt2020nc", 0);

    av_dump_format(_pFormatCtx, 0, _out_file.c_str(), 1);

    _pCodec = avcodec_find_encoder(_pCodecCtx->codec_id);
    if (!_pCodec)
    {
        std::cout << "can not find encoder" << std::endl;
        return -2;
    }

    if (avcodec_open2(_pCodecCtx, _pCodec, &_pEncodeParam) < 0)
    {
        std::cout << "failed to open encoder" << std::endl;
        return -3;
    }

    _pFrame = av_frame_alloc();
    if (!_pFrame)
    {
        std::cout << "failed to alloc _pFrame" << std::endl;
        return -4;
    }
    _pFrame->format = _pCodecCtx->pix_fmt;
    _pFrame->width = _pCodecCtx->width;
    _pFrame->height = _pCodecCtx->height;

    _bufSize = avpicture_get_size(_pCodecCtx->pix_fmt, _pCodecCtx->width, _pCodecCtx->height);
    // _pBuf_YUV420P10LE = (uint8_t *)av_malloc(_bufSize);
    _pBuf_P010LE = (uint8_t *)av_malloc(_bufSize);

    avpicture_fill((AVPicture *)_pFrame, _pBuf_P010LE, _pCodecCtx->pix_fmt, _pCodecCtx->width, _pCodecCtx->height);
    avformat_write_header(_pFormatCtx, NULL);

    return 0;
}

int32_t Nv_Encoder::proceed(uint8_t *buf, int32_t idx)
{
    av_new_packet(&_pkt, _bufSize);

    YUV420P10LE_to_P010LE((uint16_t *)buf, (uint16_t *)_pBuf_P010LE, _w, _h);
    _pFrame->data[0] = _pBuf_P010LE;
    _pFrame->data[1] = _pBuf_P010LE + 2 * _w * _h;
    // _pFrame->pts = idx * (_pVideoStream->time_base.den) / ((_pVideoStream->time_base.num) * _fps);
    _pFrame->pts = idx;

    int got_pic = 0;
    int ret = avcodec_encode_video2(_pCodecCtx, &_pkt, _pFrame, &got_pic);
    if (ret < 0)
    {
        std::cout << idx <<": failed to encode" << std::endl;
        return -1;
    }
    // std::cout << _pkt.pts << ", " << _pkt.dts << std::endl;
    // _pkt.pts = _pFrame->pts;
    // _pkt.dts = _pFrame->pts;
    // if (_pkt.dts < _pkt.pts)
    // {
    //     _pkt.dts = _pkt.pts;
    // }

    if (got_pic == 1)
    {
        // printf("encode frame: %d, size: %d\n", idx, _pkt.size);
        _pkt.stream_index = _pVideoStream->index;
        av_packet_rescale_ts(&_pkt, _pCodecCtx->time_base, _pVideoStream->time_base);
        _pkt.pos = -1;
        int size = _pkt.size;
        int write_ret = av_interleaved_write_frame(_pFormatCtx, &_pkt);  // 将编码后的视频码流写入文件（fwrite）
        if (write_ret < 0)
        {
            std::cout << "cpp flush error " << write_ret << std::endl;
        }
        printf("encode frame %d, size %d, ret %d \n", idx, size, write_ret);
        av_free_packet(&_pkt);  // 释放内存
    }
    return 0;
}

int32_t Nv_Encoder::release()
{
    int ret = flush_encoder(_pFormatCtx, 0);
    if (ret < 0)
    {
        std::cout << "failed flush encoder" << std::endl;
        return -1;
    }
    av_write_trailer(_pFormatCtx);

    if (_pVideoStream)
    {
        avcodec_close(_pVideoStream->codec);
        av_free(_pFrame);
        av_free(_pBuf_P010LE);
    }

    avio_close(_pFormatCtx->pb);
    avformat_free_context(_pFormatCtx);

    return 0;
}
