#ifndef _FILE_H
#define _FILE_H

//Instruct File.cpp to use unistd.h open, close, write, read, lseek, and fsync
#define LOW_LEVEL

#ifndef LOW_LEVEL
#include <cstdio>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#endif
#include "util/base.h"

#define F_INVALID_MODE 14
#define F_UNKNOWN_ERR 15
#define F_NOT_SUPPORTED_WRITE 16
#define F_CLOSED 17

class File {
  private:
    #ifndef LOW_LEVEL
    FILE* h;
    #else
    int h;
    int mode;
    #endif
    int _open;
    void init(const char* filename, const char* mode, int* status);
  public:
    File(const char* filename, const char* mode, int* status);
    File(const char* filename, int* status);
    int fread(char* buffer, int size);
    int fwrite(char* data, int size);
    int fwrite(char* data);
    int seek(long int offset, int origin);
    int seek(long int* output);
    int seek(long int offset);
    int seek();
    int fclose();
    int getFilesize();
    int flush();
};

#endif
