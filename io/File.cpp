#include <unistd.h>
#include "io/File.h"

#define CHECK_FILE if (File::_open == 0) {return 1;};

#ifdef LOW_LEVEL
#include <fcntl.h>
#endif

void File::init(const char* filename, const char* mode, int* status)  {
  #ifndef LOW_LEVEL
  File::h = fopen(filename, mode);
  if (File::h == NULL) {
    *status = F_UNKNOWN_ERR;
    File::_open = 0;
    return;
  }
  *status = 0;
  File::open = 1;
  #else
  errno = 0;
  int tmp;
  if (mode[0] == 'r') {
    if (mode[1] == '+') {
      tmp = open(filename, O_RDWR);
      File::h = tmp;
      File::mode = O_RDWR;
      File::_open = 0;
      *status = 0;
    } else {
      tmp = open(filename, O_RDONLY);
      File::h = tmp;
      File::mode = O_RDONLY;
      File::_open = 0;
      *status = 0;
    }
  } else if(mode[0] == 'w') {
    if (mode[1] == '+') {
      tmp = open(filename, O_RDWR | O_CREAT | O_TRUNC);
      File::h = tmp;
      File::mode = O_RDWR;
      File::_open = 0;
      *status = 0;
    } else {
      tmp = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
      File::h = tmp;
      File::mode = O_WRONLY;
      File::_open = 0;
      *status = 0;
    }
  } else if(mode[0] == 'a') {
    if (mode[1] == '+') {
      tmp = open(filename, O_APPEND | O_CREAT);
      File::h = tmp;
      File::mode = O_APPEND;
      File::_open = 0;
      *status = 0;
    } else {
      tmp = open(filename, O_APPEND | O_CREAT | O_TRUNC);
      File::h = tmp;
      File::mode = O_APPEND;
      File::_open = 0;
      *status = 0;
    }
  } else {
    File::_open = 0;
    *status = F_INVALID_MODE;
    return;
  }
  if (tmp == -1) {
    *status = errno;
    File::_open = 0;
    return;
  }
  File::_open = 1;
  #endif
}

File::File(const char* filename, int* status) {
  File::init(filename, "r", status);
}

File::File(const char* filename, const char* mode, int* status) {
  File::init(filename, mode, status);
}

int File::fwrite(char* data) {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  fputs((const char*)data, File::h);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  return F_NOT_SUPPORTED_WRITE;
  #endif
}

int File::fwrite(char* data, int size) {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  fwrite((const char*)data, sizeof(char), size, File::h);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  int stat = write(File::h, (void*) data, size);
  if (stat == -1) {
    return errno;
  }
  return 0;
  #endif
}

int File::fread(char* buffer, int size) {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  //char* tmp = (char*) malloc(sizeof(char) * size + 1);
  fread(buffer, sizeof(char), size, File::h);
  /*memcpy((void*) buffer, (void*) tmp, size);
  free(tmp);*/
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  int stat = read(File::h, (void*) buffer, size);
  if (stat == -1) {
    return errno;
  }
  return 0;
  #endif
}

int File::seek(long int* output) {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  *output = ftell(File::h);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  int status = (int) lseek(File::h, 0, SEEK_CUR);
  if (status == -1) {
    *output = -1;
    return errno;
  }
  *output = (long int) status;
  return 0;
  #endif
}

int File::seek() {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  int tmp = (int) ftell(File::h);
  if (ferror(File::h)) {
    return 0;
  }
  return tmp;
  #else
  long int tmp;
  int status = File::seek(&tmp);
  if (tmp == -1) {
    errno = status;
    return -1;
  }
  errno = 0;
  return (int) tmp;
  #endif
}

int File::seek(long int offset, int origin) {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  fseek(File::h, offset, origin);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  int status = (int) lseek(File::h, (int) offset, origin);
  if (status = -1) {
    return errno;
  }
  return 0;
  #endif
}

int File::seek(long int offset) {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  fseek(File::h, offset, SEEK_CUR);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  return File::seek(offset, SEEK_CUR);
  #endif
}

int File::fclose() {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  fclose(File::h);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  int status = (int) close(File::h);
  if (status = -1) {
    return errno;
  }
  return 0;
  #endif
}

int File::flush() {
  CHECK_FILE;
  #ifndef LOW_LEVEL
  fflush(File::h);
  if (ferror(File::h)) {
    return F_UNKNOWN_ERR;
  }
  return 0;
  #else
  int status = (int) fsync(File::h);
  if (status = -1) {
    return errno;
  }
  return 0;
  #endif
}

int File::getFilesize() {
  if (File::_open == 0) {return F_CLOSED;};
  
  errno = 0;
  
  int ol = File::seek();
  File::seek(0, SEEK_END);
  
  if (errno != 0) {
    return -1;
  }
  
  errno = 0;
  
  int tmp = File::seek();
  File::seek(ol, SEEK_SET);
  
  if (errno != 0) {
    return -1;
  }
  
  #ifndef LOW_LEVEL
  if (ferror(File::h)) {
    errno = F_UNKNOWN_ERROR;
    return -1;
  }
  #endif
  return tmp;
}
