#include <fstream>
#include <iostream>
#include <mpg123.h>
#include <out123.h>

int main(){
    mpg123_init();

    int err;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;

    int channels, encoding;
    long rate;
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char*)malloc(buffer_size * sizeof(unsigned char));

    mpg123_open(mh, "/home/luo980/Desktop/FauKwa.mp3");
    mpg123_getformat(mh, &rate, &channels, &encoding);
    
    std::ofstream out("res.txt");
    unsigned int counter = 0;

    for (int totalBtyes = 0; mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK; ) {
        short* tst = reinterpret_cast<short*>(buffer);
        for (auto i = 0; i < buffer_size / 2; i++) {
            out<< counter + i<<"\t"<< tst[i] << "\n";
        }
        counter += buffer_size/2;
        totalBtyes += done;
    }
    out.close();
    free(buffer);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    return 0;
}