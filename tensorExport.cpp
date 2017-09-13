#include "mbed.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdio.h>
#include "tensor.hpp"
#include <stdlib.h>
#include "FATFileSystem.h"
#include "SDBlockDevice.h"
#include <errno.h>
#include "tensor.hpp"

using namespace std;

#define idx_ubyte 0x08
#define idx_byte  0x09
#define idx_short 0x0B
#define idx_int   0x0C
#define idx_float 0x0D
#define idx_double 0x0E

class HeaderMeta {
    public:
        unsigned char dataType;
        unsigned char numDim;
        vector<uint32_t> dim;
        long int dataPos;
};

//little endian to big endian
uint32_t htonl(uint32_t &val) {
    // const uint32_t mask = 0b11111111;
    uint32_t ret = 0;
    
    ret |= val >> 24;
    ret |= (val << 8) >> 16;
    ret |= (val << 16) >> 8;
    ret |= val << 24;

    // ret |= (val & (mask << 24)) >> 24;
    // ret |= (val & (mask << 16)) >> 8;
    // ret |= (val & (mask << 8)) << 8;
    // ret |= (val & (mask << 0)) << 24;

    return ret;
}


void return_error(int ret_val){
    if (ret_val) {
      printf(" [**Failure**] %d\r\n", ret_val);
      printf("Exiting...\r\n");
      exit(-1);
    } else {
      printf("  [DONE]\r\n");
    }
}

void errno_error(void* ret_val){
    if (ret_val == NULL) {
      printf(" [**Failure**] %d \r\n", errno);
      printf("Exiting...\r\n");
      exit(-1);
    } else {
      printf("  [DONE]\r\n");
    }
}


#define ON_ERR(FUNC, MSG)   printf(" * "); \
                            printf(MSG); \
                            return_error(FUNC);

                            
//big endian to little endian
uint32_t ntohl(uint32_t &val) {
    uint32_t ret = 0;
    
    ret |= val >> 24;
    ret |= (val << 8) >> 16;
    ret |= (val << 16) >> 8;
    ret |= val << 24;

    return ret;
}

uint32_t hton_f2int(float host_val) {
    uint32_t tmp = *((uint32_t*) &host_val);
    return htonl(tmp);
}

float ntoh_int2f(uint32_t net_val) {
    uint32_t tmp = ntohl(net_val);
    return *((float *) &tmp);
}


uint32_t getMagicNumber(unsigned char dtype, unsigned char dim) {
    uint32_t magic = 0;

    magic = (magic | dtype) << 8;
    magic = magic | dim;

    return magic;
}

void printVector(vector<uint32_t> vec) {
    printf("vector: \r\n");
    for(uint32_t i:vec) {
        printf("%d ", i);
    }

    printf("\r\n");

}

class TensorIdxImporter {
    private:
        FILE *fp;
        HeaderMeta header;
        HeaderMeta parseHeader(void);
        template<typename U>
        TensorBase<U> loader(string &filename, int idx_type);
        void open(string filename);
        //void open(FILE *fp);
        
    public:
        TensorBase<unsigned char> ubyte_import(string filename) { return loader<unsigned char>(filename, idx_ubyte);}
        TensorBase<char> byte_import(string filename)           { return loader<char>(filename, idx_byte);}
        TensorBase<short> short_import(string filename)         { return loader<short>(filename, idx_short);}
        TensorBase<int> int_import(string filename)             { return loader<int>(filename, idx_int);}
        TensorBase<float> float_import(string filename)         { return loader<float>(filename, idx_float);}
        //TensorBase<double> double_import(string filename);
};

// void TensorIdxImporter::open(FILE *_fp) {
//     fp = _fp;
//     header = parseHeader();
// }

HeaderMeta TensorIdxImporter::parseHeader(void) {
    unsigned char *buf = (unsigned char*) malloc(sizeof(unsigned char) * 4);

    fread(buf, 1, 4, fp);
    if(buf[0] != 0 || buf[0] != 0) {
        printf("Error, header magic number invalid\r\n");
    }

    HeaderMeta header;
    header.dataType = buf[2];
    header.numDim = buf[3];

    for(int i = 0; i < header.numDim; i++) {
        fread(buf, 1, 4, fp);
        uint32_t dimSize = ntohl(*(uint32_t*) buf);
        header.dim.push_back(dimSize);
    }

    free(buf);

    header.dataPos = ftell(fp);
    
    return header;
}

template<typename U>
TensorBase<U> TensorIdxImporter::loader(string &filename, int idx_type) {
    fp = fopen (filename.c_str(), "r" );
    errno_error(fp);

    header = parseHeader();

    fseek(fp, header.dataPos, SEEK_SET);  //need error  handling

    if(header.dataType != idx_type) {
        printf("TensorIdxImporter: header and tensor type mismatch\r\n");
        exit(-1);
    }

    TensorBase<U> t = TensorBase<U>(header.dim);  //tensor allocated
    const uint8_t unit_size = sizeof(U);
    U* val = (U *) malloc(unit_size);
    U* data = t.getPointer({});

    for(int i = 0; i < t.getSize(); i++) {
        fread(val, unit_size, 1, fp);
        //val = htonl((uint32_t) buff);  //NT: testing for uint8 only, deference error here
        data[i] = *val ;
    }

    free(val);

    ON_ERR(fclose(fp), "Closing file...");

    return t;
}
// TensorBase<double> TensorIdxImporter::double_import(string filename) {
    
//     open(filename);
//     t = TensorBase<double>(header.dim);  //tensor allocated

//     return loader(t);
// }

Serial pc(USBTX, USBRX, 115200);
SDBlockDevice bd(D11, D12, D13, D10);
FATFileSystem fs("fs");



int main(int argc, char** argv) {

    int error = 0;

    ON_ERR(fs.mount(&bd), "Mounting the filesystem on \"/fs\". ");

    printf(" * opening idx/uint8_4d_power2.idx");

    TensorIdxImporter t_import;
    TensorBase<unsigned char> t = t_import.ubyte_import("/fs/idx/uint8_4d_power2.idx");

    //TensorBase<unsigned char> t = idxTensor8bit(header, fp);
    printf("something\r\n");
    unsigned char* elem = t.getPointer({});

    printf("size: %d\r\n", (int) t.getSize());
    printf("data\r\n");
    for(int i = 0; i < t.getSize(); i++) {
        printf("%d ", elem[i]);
    }
    printf("\r\n");

    ON_ERR(fs.unmount(), "Unmounting the filesystem on \"/fs\". ");

    return 0;
}
//csv write
//or THE IDX FILE FORMAT

//TODO   try compiling the SD card example with C++11