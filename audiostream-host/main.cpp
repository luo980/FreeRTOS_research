#include <fstream>
#include <iostream>
#include <mpg123.h>
#include <out123.h>
#include <ao/ao.h>
#include <string.h>

#include <opus/opus.h>

#define MAX_PACKET 1500
#define MAX_FRAME_SIZE 6 * 960
#define MAX_PACKET_SIZE (1000000)

ao_device *ao_driver_init();
OpusEncoder *encoder_init(opus_int32 sampling_rate, int channels, int application);

int main()
{
    mpg123_init();

    int err;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;

    int channels, encoding;
    long rate;
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char *)malloc(buffer_size * sizeof(unsigned char));

    mpg123_open(mh, "./test.mp3");
    mpg123_getformat(mh, &rate, &channels, &encoding);

    std::cout << "rate is " << rate << ","
              << "channels is " << channels << ", "
              << "encoding is " << encoding << std::endl;

    ao_device *player = ao_driver_init();
    if (player == NULL)
    {
        fprintf(stderr, "Error opening device.\n");
        return -1;
    }

    std::ofstream out("res.txt");
    unsigned int counter = 0;
    std::cout << "Get buffer size " << buffer_size << std::endl;

    buffer_size = 480;
    unsigned char data[200][480] = {0};
    int max_payload_bytes = MAX_PACKET;
    int len_opus[4000] = {0};
    // int frame_duration_ms = 2.5;
    int sample_rate = 48000;
    int frame_size = sample_rate / 1000;
    unsigned char cbits[MAX_PACKET_SIZE];
    unsigned char *cbits_vtmp = cbits;

    OpusEncoder *enc = encoder_init(48000, 2, OPUS_APPLICATION_AUDIO);
    // decoding to buffer address with buffersize, one frame size.
    for (int totalBtyes = 0; (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) && (counter < 4000);)
    {
        short *tst = reinterpret_cast<short *>(buffer);
        // for (auto i = 0; i < buffer_size; i++)
        // {
        //     // out<< counter + i<<"\t"<< tst[i] << "\n";
        //     // std::cout << i << ": " <<  (int)buffer[i] << std::endl;
        //     out << i << "i: " << (int)buffer[i] << std::endl;
        // }
        // counter += buffer_size/2;

        ao_play(player, (char *)buffer, buffer_size);
        totalBtyes += done;

        len_opus[counter] = opus_encode(enc, (opus_int16*)buffer, 480, cbits_vtmp, MAX_PACKET_SIZE);
        if (len_opus[counter] < 0){
            std::cout << "failed to encode: " << opus_strerror(len_opus[counter]) << std::endl;
        }

        cbits_vtmp = cbits_vtmp + len_opus[counter];

        // out << "original: " << *tst << std::endl;
        // out << "encoded :" << *cbits << std::endl;
        // for (auto i = 0; i < len_opus[counter]; i++)
        // {

        //     // out<< counter + i<<"\t"<< tst[i] << "\n";
        //     // std::cout << i << ": " <<  (int)buffer[i] << std::endl;
        //     std::cout << i << "j: " << (int)cbits[i] << std::endl;
        // }

        // std::cout << "counter: " << counter << std::endl;
        // std::cout << "len_opus: " << len_opus[counter] << std::endl;
        // out << "counter" << " : " << counter << std::endl;

        counter = counter + 1;
    }

    OpusDecoder* decoder = opus_decoder_create(sample_rate, channels, &err);
    if (err < 0)
    {
        fprintf(stderr, "failed to create decoder: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }
    counter = 0;
    opus_int16 out1[MAX_FRAME_SIZE*2];
    cbits_vtmp = cbits;
    while(1){
        frame_size = opus_decode(decoder, cbits_vtmp, len_opus[counter], out1, 480*2, 0);
        //ao_play(player, (char*)tst, frame_size);
        ao_play(player, (char*)out1, frame_size);
        cbits_vtmp = cbits_vtmp + len_opus[counter];
        std::cout << "counter nb: " << counter << std::endl;
        counter++;
        if(counter > 4000){
            break;
        }
    }

    out.close();
    free(buffer);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    return 0;
}

ao_device *ao_driver_init()
{
    ao_device *device;
    ao_sample_format format;
    int default_driver;

    ao_initialize();

    /* -- Setup for default driver -- */
    default_driver = ao_default_driver_id();

    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.channels = 2;
    format.rate = 48000;
    format.byte_format = AO_FMT_LITTLE;

    device = ao_open_live(default_driver, &format, NULL /* no options */);

    return device;
}

OpusEncoder *encoder_init(opus_int32 sampling_rate, int channels, int application)
{
    int enc_err;
    OpusEncoder *enc = opus_encoder_create(sampling_rate, channels, application, &enc_err);
    if (enc_err != OPUS_OK)
    {
        fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(enc_err));
        return NULL;
    }

    int bitrate_bps = OPUS_BITRATE_MAX;
    int bandwidth = OPUS_BANDWIDTH_FULLBAND;
    int use_vbr = 1;
    int cvbr = 0;
    // int complexity = 10;
    int use_inbandfec = 0;
    int forcechannels = 0;
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
    opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));

    // IMPORTANT TO CONFIGURE DELAY
    int variable_duration = OPUS_FRAMESIZE_2_5_MS;
    opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(variable_duration));

    return enc;
}