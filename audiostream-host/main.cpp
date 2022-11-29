#include <ao/ao.h>
#include <mpg123.h>
#include <out123.h>
#include <string.h>
#include <fstream>
#include <iostream>

#include <opus/opus.h>
#include <sys/time.h>

#define MAX_PACKET 1500
#define MAX_FRAME_SIZE 6 * 960
#define MAX_PACKET_SIZE (3840000)
#define COUNTERLEN 4000

#define RATE 48000
#define BITS 16
#define FRAMELEN 2.5
#define CHANNELS 2
#define FRAMESIZE (RATE * CHANNELS * 2 * FRAMELEN / 1000)
long rate;

ao_device* ao_driver_init(int rate);
OpusEncoder* encoder_init(opus_int32 sampling_rate,
                          int channels,
                          int application);

int main() {
    // 初始化mpg123解码器
    mpg123_init();

    // 获得解码器句柄
    int err;
    mpg123_handle* mh = mpg123_new(NULL, &err);

    // 根据播放器句柄获得buffer大小参数，这里在创建时没有定制参数
    unsigned char* buffer;
    size_t buffer_size;
    size_t done;
    buffer_size = mpg123_outblock(mh);
    std::cout << "Get buffer size " << buffer_size << std::endl;
    buffer = (unsigned char*)malloc(buffer_size * sizeof(unsigned char));

    // 从mp3文件中获得通道数和编码方法
    int channels, encoding;
    mpg123_open(mh, "./test2.mp3");
    mpg123_getformat(mh, &rate, &channels, &encoding);
    std::cout << "rate is " << rate << ","
              << "channels is " << channels << ", "
              << "encoding is " << encoding << std::endl;

    // 初始化libao，可认为是播放器
    ao_device* player = ao_driver_init(rate);
    if (player == NULL) {
        fprintf(stderr, "Error opening device.\n");
        return -1;
    }

    unsigned int counter = 0;

    // 这里简单计算，其实就是2.5ms的framesize对应多少采样点
    // must be multiple times of bits
    // 2.5MS -> 48000 samples per second / 1000 MS per second * 2 Channels *
    // 2bytes per sample * 2.5MS = 480 bytes per 2.5MS 2.5MS 120 per channel
    // samples, 240 per channel size, 240 samples per 2.5MS
    buffer_size = 480;
    int max_payload_bytes = MAX_PACKET;
    int len_opus[COUNTERLEN] = {0};
    // int frame_duration_ms = 2.5;
    int sample_rate = rate;
    int frame_size = sample_rate / 1000;
    unsigned char cbits[MAX_PACKET_SIZE];
    unsigned char* cbits_vtmp = cbits;

    //初始化Opus编码器
    OpusEncoder* enc = encoder_init(RATE, CHANNELS, OPUS_APPLICATION_AUDIO);

    timeval start, end;
    timeval start4, end4;
    gettimeofday(&start, NULL);
    int totalBytes;
    // decoding to buffer address with buffersize, one frame size.
    //循环到解码mp3文件完成，done表示实际得到解码长度，buffer_size表示目标解码长度
    //只在最后一次读取时，done可能会小于buffer_size
    for (totalBytes = 0;
         (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) &&
         (counter < COUNTERLEN);) {
        short* tst = reinterpret_cast<short*>(buffer);

        //将每次解码得到的2.5ms framesize长度pcm音频使用libao接口进行播放
        ao_play(player, (char*)buffer, done);
        totalBytes += done;
        if (buffer_size != done) {
            std::cout << "last done size : " << done << std::endl;
        }

        // std::cout << "FRAMESIZE is " << FRAMESIZE << std::endl;
        gettimeofday(&start4, NULL);

        // 使用opus编码器对pcm音频进行编码，得到opus编码结果存到cbits_vtmp数组
        //len_opus数组中存放每次编码得到的字节长度
        len_opus[counter] = opus_encode(enc,
                                        // (opus_int16*)buffer,
                                        tst, 120, cbits_vtmp, MAX_PACKET_SIZE);

        gettimeofday(&end4, NULL);
        std::cout << "1 time encoding cost :" << (end4.tv_usec - start4.tv_usec)
                  << "us" << std::endl;
        if (len_opus[counter] < 0) {
            std::cout << "failed to encode: "
                      << opus_strerror(len_opus[counter]) << std::endl;
        }

        // 数组地址向前移动编码长度
        cbits_vtmp = cbits_vtmp + len_opus[counter];
        counter = counter + 1;
    }

    std::cout << "total pcm bytes is " << totalBytes << std::endl;
    gettimeofday(&end, NULL);
    std::cout << 1000 * (end.tv_sec - start.tv_sec) +
                     (end.tv_usec - start.tv_usec) / 1000
              << std::endl;

    ao_close(player);

    // 重新初始化libao播放器，采样率根据实际情况定
    ao_device* player2 = ao_driver_init(44100);

    if (player2 == NULL) {
        fprintf(stderr, "Error opening device.\n");
        return -1;
    }

    // 初始化opus解码器
    OpusDecoder* decoder = opus_decoder_create(RATE, CHANNELS, &err);
    if (err < 0) {
        fprintf(stderr, "failed to create decoder: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }
    counter = 0;
    opus_int16 out1[MAX_FRAME_SIZE * 2];
    cbits_vtmp = cbits;

    timeval start2, end2;
    timeval start3, end3;
    int decodebytes = 0;
    gettimeofday(&start2, NULL);
    int decodeSamples;
    while (1) {
        gettimeofday(&start3, NULL);
        // 将之前编码得到的数组进行解码，每次长度为单个2.5ms的framesize
        // 返回pcm样本数量
        decodeSamples =
            opus_decode(decoder, cbits_vtmp, len_opus[counter], out1, 12000, 0);
        // std::cout << "The decoded decodeSamples is " << decodeSamples <<
        // std::endl;
        char* tst = reinterpret_cast<char*>(out1);
        gettimeofday(&end3, NULL);
        std::cout << "1 time decoding cost :" << (end3.tv_usec - start3.tv_usec)
                  << "us" << std::endl;

        // 需要注意的是，这里得到的decodeSamples的数字和实际解码得到pcm长度之间有4倍的换算关系
        // 具体原因就检查一下吧
        ao_play(player2, tst, decodeSamples * 4);
        cbits_vtmp = cbits_vtmp + len_opus[counter];
        // std::cout << "counter nb: " << counter << std::endl;
        counter++;

        // 解码字节数统计
        decodebytes += decodeSamples * 4;
        if (counter > COUNTERLEN - 1) {
            std::cout << "counter nb: " << counter << std::endl;
            break;
        }
    }
    std::cout << "total decoded pcm bytes is " << decodebytes << std::endl;
    gettimeofday(&end2, NULL);
    std::cout << 1000 * (end2.tv_sec - start2.tv_sec) +
                     (end2.tv_usec - start2.tv_usec) / 1000
              << std::endl;

    ao_close(player2);

    free(buffer);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    return 0;
}

ao_device* ao_driver_init(int rate) {
    ao_device* device;
    ao_sample_format format;
    int default_driver;

    ao_initialize();

    /* -- Setup for default driver -- */
    default_driver = ao_default_driver_id();

    memset(&format, 0, sizeof(format));
    format.bits = BITS;
    format.channels = 2;
    format.rate = rate;
    std::cout << "ao rate is " << rate << std::endl;
    format.byte_format = AO_FMT_LITTLE;

    device = ao_open_live(default_driver, &format, NULL /* no options */);

    return device;
}

OpusEncoder* encoder_init(opus_int32 sampling_rate,
                          int channels,
                          int application) {
    int enc_err;
    std::cout << "Here the rate is" << sampling_rate << std::endl;
    OpusEncoder* enc =
        opus_encoder_create(sampling_rate, channels, application, &enc_err);
    if (enc_err != OPUS_OK) {
        fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(enc_err));
        return NULL;
    }

    int bitrate_bps = OPUS_BITRATE_MAX;
    int bandwidth = OPUS_BANDWIDTH_FULLBAND;
    int use_vbr = 1;
    int cvbr = 0;
    // int complexity = 10;
    int use_inbandfec = 0;
    int forcechannels = 2;
    int use_dtx = 0;
    int packet_loss_perc = 0;

    opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_bps));
    opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(bandwidth));
    opus_encoder_ctl(enc, OPUS_SET_VBR(use_vbr));
    opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(cvbr));
    // opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(use_inbandfec));
    opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(forcechannels));
    opus_encoder_ctl(enc, OPUS_SET_DTX(use_dtx));
    opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(packet_loss_perc));

    // opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&skip));
    opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(BITS));

    // IMPORTANT TO CONFIGURE DELAY
    int variable_duration = OPUS_FRAMESIZE_2_5_MS;
    opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(variable_duration));

    return enc;
}