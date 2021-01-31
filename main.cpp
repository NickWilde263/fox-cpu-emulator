#include <SDL2/SDL.h>
#include <pthread.h>
#include <iostream>
#include <dlfcn.h>
#include <stdio.h>
#include <time.h>
#include <mutex>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "io/File.h"
#include "util/base.h"
/* #include "iniparser/dictionary.h" */
#include "iniparser/iniparser.h" 

using namespace std;
lua_State* L = luaL_newstate();
std::mutex ready;

uint8_t portStateA[65536]; //External lib writes here and CPU read here
bool stateChangeA[65536];
std::mutex CPUOnRead[65536];

uint8_t portStateB[65536]; //External lib read here and CPU write here
bool stateChangeB[65536];
std::mutex LibOnRead[65536];

extern "C" { //Only loaded libary use this
  void writePort(uint8_t portNumber, uint8_t data) {
    CPUOnRead[portNumber].lock();
    portStateA[portNumber] = data;
    stateChangeA[portNumber] = true;
    CPUOnRead[portNumber].unlock();
  }

  uint8_t readPort(uint8_t portNumber) {
    uint8_t data;
    while(stateChangeB[portNumber] == false) { //Wait undefinitely
      SDL_Delay(1);
    }
    
    LibOnRead[portNumber].lock();
    data = portStateB[portNumber];
    stateChangeB[portNumber] = false;
    LibOnRead[portNumber].unlock();
    return data;
  } 
}

constexpr float busClock = 1; //Maybe this emulating bus clock on motherboards
void* busSync(void* unused) {return NULL;}

uint8_t RAMB[65536 * 255];
constexpr uint8_t* RAM = RAMB;

typedef void (*InsFormat)();
#define INSFORMAT_NO_CONSTEXPR(name) void name()
#define INSFORMAT(name) constexpr void name()

mutex the_lock;
uint16_t IPb = 0x0000;

uint16_t SPb = 0xFFFF; //Top of the stack
uint16_t DIb = 0x0000;
uint16_t SIb = 0x0000;
uint16_t BPb = 0x0000;

constexpr uint16_t* SI = &SIb;
constexpr uint16_t* DI = &DIb;
constexpr uint16_t* BP = &BPb;
constexpr uint16_t* SP = &SPb;

constexpr uint16_t* IP = &IPb;

constexpr int segment_CS = 0;
constexpr int segment_SS = 1;
constexpr int segment_DS = 2;
constexpr int segment_ES = 3;

uint8_t segmentsB[256] = {
  0x0000, //CS
  0x0000, //SS
  0x0000, //DS
  0x0000  //ES
};
constexpr uint8_t* segments = segmentsB;

/*
 * 16 8-bit registers
 * 8 16-bit registers
 */
uint8_t regsB[2^4];
constexpr uint8_t* regs = regsB;

//Convert segment and offset to RAM location
constexpr int getAbs(uint8_t segment, uint16_t offset) {
  return (((int) segment) << 16) + ((int) offset) % (65536 * 256);
}

//RAM operations
constexpr uint8_t readRAM_uint8(uint8_t segment, uint16_t offset) {
  return RAM[getAbs(segment, offset)];
}

constexpr uint16_t readRAM_uint16(uint8_t segment, uint16_t offset) {
  return ((uint16_t) RAM[getAbs(segment, offset)] * (uint16_t) 256) + (uint16_t) RAM[getAbs(segment, offset + 1)];
}

constexpr void writeRAM_uint8(uint8_t segment, uint16_t offset, uint8_t value) {
  RAM[getAbs(segment, offset)] = value;
}

constexpr void writeRAM_uint16(uint8_t segment, uint16_t offset, uint16_t value) {
  RAM[getAbs(segment, offset)] = value / 256 % 256;
  RAM[getAbs(segment, offset + 1)] = value % 256;
}

//Register operations
constexpr uint8_t readReg_uint8(uint8_t reg) {
  return regs[reg % 2^4];
}

constexpr uint16_t readReg_uint16(uint8_t reg) {
  return regs[reg % 2^3] * 256 + regs[reg % 2^3 + 1];
}

constexpr void writeReg_uint8(uint8_t reg, uint8_t value) {
  regs[reg % 2^4] = value;
}

constexpr void writeReg_uint16(uint8_t reg, uint16_t value) {
  regs[reg % 2^3] = value / 256 % 256;
  regs[reg % 2^3 + 1] = value % 256;
}

float getCPUTime() {
  return (float) clock() / (float) CLOCKS_PER_SEC;
}

char* readable(double num, double* output) {
  static char ret[3];
  double* out = output;
  
  ret[1] = 'h';
  ret[2] = 'z';
  
  if (num >= 1000) { //KiB
    num = num / 1000;
    if (num >= 1000) { //MiB
      num = num / 1000;
      if (num >= 1000) { //GiB
        num = num / 1000;
        if (num >= 1000) { //TiB
          num = num / 1000;
          if (num >= 1000) { //PiB
            num = num / 1000;
            if (num >= 1000) { //EiB
              num = num / 1000;
              if (num >= 1000) { //ZiB
                num = num / 1000;
                ret[0] = 'Z';
                *out = num;
                return ret;
              };
              ret[0] = 'E';
              *out = num;
              return ret;
            };
            ret[0] = 'P';
            *out = num;
            return ret;
          };
          ret[0] = 'T';
          *out = num;
          return ret;
        };
        ret[0] = 'G';
        *out = num;
        return ret;
      };
      ret[0] = 'M';
      *out = num;
      return ret;
    };
    ret[0] = 'K';
    *out = num;
    return ret;
  };
  
  ret[0] = 'h';
  ret[1] = 'z';
  ret[2] = '\0';
  *out = num;
  return ret;
}

double cycles;
void *frequencyPrinter(void *tmpvar) {
  double num;
  double tmp;
  char* text;
  while (true) {
    tmp = cycles;
    
    sleep((unsigned int) 1);
    
    tmp = (double) cycles - tmp;
    
    text = readable(tmp, &num);
    cout << "Speed: " << double(floor(num * 100 + 0.5)) / 100 << " " << text[0] << text[1] << text[2] << " " << "\n";
  }
  pthread_exit(NULL);
}

void unknown() {
  cout << "Unknown instruction encountered " << endl;
  printf("At %ld:%ld\n", segments[segment_CS], *IP);
  cout << "Instruction = " << (int) readRAM_uint8(segments[segment_CS], *IP) << endl;
  exit(0);
}

INSFORMAT(nop) {}

void hlt() {
  cout << "Halted ";
  printf("at %ld:%ld\n", segments[segment_CS], *IP);
  exit(0);
}

//BitOps
constexpr uint8_t bit_set(uint8_t number, size_t index) {
  return number | (uint8_t) 1 << index;
}

constexpr uint8_t bit_unset(uint8_t number, size_t index) {
  return number & ~((uint8_t) 1 << index);
}

//Flags
uint8_t flagB;
constexpr uint8_t* flag = &flagB;
/*

Bit
0 = Carry
1 = Overflow
2 = Zero
3 = Larger
4 = Equal
5 = Smaller
6 = Interrupt flag
7 = Direction flag
*/
INSFORMAT_NO_CONSTEXPR(uint16_add) {
  int res = (int) readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) + (int) readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2));
  if (res > 65535) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), (uint16_t) (res % 65536));
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(uint16_sub) {
  uint16_t res = (uint16_t) ~(readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1))) + (uint16_t) readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2));
  res = ~res;
  if (readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2)) > readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1))) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), res);
  *IP = *IP + 2;
}

INSFORMAT(uint8_mult) {
  uint16_t res = (uint16_t) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1)) * (uint16_t) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2));
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 3), res);
  *IP = *IP + 3;
}

INSFORMAT_NO_CONSTEXPR(uint8_add) {
  int res = (int) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1)) + (int) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2));
  if (res > 255) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), (uint16_t) (res % 256));
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(uint8_sub) {
  uint8_t res = (uint8_t) ~(readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1))) + (uint8_t) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2));
  res = ~res;
  if (readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2)) > readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1))) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), res);
  *IP = *IP + 2;
}

INSFORMAT(imm_jmp) {
  *IP = readRAM_uint8(segments[segment_CS], *IP + 1) * 256 + readRAM_uint8(segments[segment_CS], *IP + 2) - 1;
}

INSFORMAT(reg_move_uint16) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2)));
  *IP = *IP + 2;
}

INSFORMAT(reg_move_uint8) {
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2)));
  *IP = *IP + 2;
}

INSFORMAT(imm_move_uint8) {
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), readRAM_uint8(segments[segment_CS], *IP + 2));
  *IP = *IP + 2;
}

INSFORMAT(imm_move_uint16) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), readRAM_uint8(segments[segment_CS], *IP + 2) * 256
                                                                + readRAM_uint8(segments[segment_CS], *IP + 3));
  *IP = *IP + 3;
}

INSFORMAT_NO_CONSTEXPR(prn_uint16) {
  std::cout << (int) readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) << std::endl;
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(uint16_cmp) {
  uint16_t A = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1));
  uint16_t B = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2));
  if (A == 0) {
    *flag = bit_set(*flag, 2);
  } else {
    *flag = bit_unset(*flag, 2);
  }
  
  if (A > B) {
    *flag = bit_set(*flag, 3);
  } else {
    *flag = bit_unset(*flag, 3);
  }
  
  if (A == B) {
    *flag = bit_set(*flag, 4);
  } else {
    *flag = bit_unset(*flag, 4);
  }
  
  if (A < B) {
    *flag = bit_set(*flag, 5);
  } else {
    *flag = bit_unset(*flag, 5);
  }
  
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(uint8_cmp) {
  uint8_t A = readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1));
  uint8_t B = readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2));
  if (A == 0) {
    *flag = bit_set(*flag, 2);
  } else {
    *flag = bit_unset(*flag, 2);
  }
  
  if (A > B) {
    *flag = bit_set(*flag, 3);
  } else {
    *flag = bit_unset(*flag, 3);
  }
  
  if (A == B) {
    *flag = bit_set(*flag, 4);
  } else {
    *flag = bit_unset(*flag, 4);
  }
  
  if (A < B) {
    *flag = bit_set(*flag, 5);
  } else {
    *flag = bit_unset(*flag, 5);
  }
  
  *IP = *IP + 2;
}

INSFORMAT(clf) {
  *flag = bit_unset(*flag, 2);
  *flag = bit_unset(*flag, 3);
  *flag = bit_unset(*flag, 4);
  *flag = bit_unset(*flag, 5);
}

/*

Bit
0 = Carry
1 = Overflow
2 = Zero
3 = Larger
4 = Equal
5 = Smaller
6 = Interrupt *flag
7 = reserved
*/

INSFORMAT_NO_CONSTEXPR(jc) {
  if (*flag == bit_set(*flag, 0)) {
    *IP = readRAM_uint8(segments[segment_CS], *IP + 1) * 256 + readRAM_uint8(segments[segment_CS], *IP + 2) - 1;
    return;
  }
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(jz) {
  if (*flag == bit_set(*flag, 2)) {
    *IP = readRAM_uint8(segments[segment_CS], *IP + 1) * 256 + readRAM_uint8(segments[segment_CS], *IP + 2) - 1;
    return;
  }
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(jg) {
  if (*flag == bit_set(*flag, 3)) {
    *IP = readRAM_uint8(segments[segment_CS], *IP + 1) * 256 + readRAM_uint8(segments[segment_CS], *IP + 2) - 1;
    return;
  }
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(je) {
  if (*flag == bit_set(*flag, 4)) {
    *IP = readRAM_uint8(segments[segment_CS], *IP + 1) * 256 + readRAM_uint8(segments[segment_CS], *IP + 2) - 1;
    return;
  }
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(js) {
  if (*flag == bit_set(*flag, 5)) {
    *IP = readRAM_uint8(segments[segment_CS], *IP + 1) * 256 + readRAM_uint8(segments[segment_CS], *IP + 2) - 1;
    return;
  }
  *IP = *IP + 2;
}

//Reg jmps
INSFORMAT_NO_CONSTEXPR(reg_jc) {
  if (*flag == bit_set(*flag, 0)) {
    *IP = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) - 1;
    return;
  }
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(reg_jz) {
  if (*flag == bit_set(*flag, 2)) {
    *IP = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) - 1;
    return;
  }
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(reg_jg) {
  if (*flag == bit_set(*flag, 3)) {
    *IP = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) - 1;
    return;
  }
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(reg_je) {
  if (*flag == bit_set(*flag, 4)) {
    *IP = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) - 1;
    return;
  }
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(reg_js) {
  if (*flag == bit_set(*flag, 5)) {
    *IP = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) - 1;
    return;
  }
  *IP = *IP + 1;
}

INSFORMAT(reg_jmp) {
  *IP = readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) - 1;
}

INSFORMAT(pop_uint8) {
  *SP = *SP + 1;
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1),
                 readRAM_uint8(segments[segment_SS], *SP));
  *IP = *IP + 1;
}

INSFORMAT(push_uint8) {
  writeRAM_uint8(segments[segment_SS], *SP, readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1)));
  *SP = *SP - 1;
  *IP = *IP + 1;
}

INSFORMAT(pop_uint16) {
  *SP = *SP + 2;
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1),
                  readRAM_uint16(segments[segment_SS], *SP));
  *IP = *IP + 1;
}

INSFORMAT(push_uint16) {
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)));
  *SP = *SP - 2;
  *IP = *IP + 1;
}

INSFORMAT(pusha) {
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(0));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(1));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(2));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(3));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(4));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(5));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(6));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, readReg_uint16(7));
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, *BP);
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, *SI);
  *SP = *SP - 2;
  writeRAM_uint16(segments[segment_SS], *SP, *DI);
  *SP = *SP - 2;
}

INSFORMAT(popa) {
  *SP = *SP + 2;
  *DI = readRAM_uint16(segments[segment_SS], *SP);
  *SP = *SP + 2;
  *SI = readRAM_uint16(segments[segment_SS], *SP);
  *SP = *SP + 2;
  *BP = readRAM_uint16(segments[segment_SS], *SP);
  
  *SP = *SP + 2;
  writeReg_uint16(7, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(6, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(5, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(4, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(3, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(2, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(1, readRAM_uint16(segments[segment_SS], *SP));
  *SP = *SP + 2;
  writeReg_uint16(0, readRAM_uint16(segments[segment_SS], *SP));
}

INSFORMAT_NO_CONSTEXPR(imm_uint8_add) {
  int res = (int) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1)) + (int) readRAM_uint8(segments[segment_CS], *IP + 2);
  if (res > 255) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), (uint16_t) (res % 256));
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(imm_uint8_sub) {
  uint8_t res = (uint8_t) ~(readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1))) + (uint8_t) readRAM_uint8(segments[segment_CS], *IP + 2);
  res = ~res;
  if (readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2)) > readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1))) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), res);
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(imm_uint16_add) {
  int res = (int) readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) + (int) readRAM_uint16(segments[segment_CS], *IP + 2);
  if (res > 65535) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), (uint16_t) (res % 65536));
  *IP = *IP + 3;
}

INSFORMAT_NO_CONSTEXPR(imm_uint16_sub) {
  uint16_t res = (uint16_t) ~(readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1))) + (uint16_t) readRAM_uint16(segments[segment_CS], *IP + 2);
  res = ~res;
  if (readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2)) > readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1))) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), res);
  *IP = *IP + 3;
}

INSFORMAT_NO_CONSTEXPR(uint8_inc) {
  int res = (int) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1)) + 1;
  if (res > 255) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), (uint16_t) (res % 256));
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(uint8_dec) {
  uint8_t res = (uint8_t) ~(readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1))) + (uint8_t) 1;
  res = ~res;
  if (readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2)) > readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1))) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1), res);
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(uint16_inc) {
  int res = (int) readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1)) + 1;
  if (res > 65535) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), (uint16_t) (res % 65536));
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(uint16_dec) {
  uint16_t res = (uint16_t) ~(readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1))) + (uint16_t) 1;
  res = ~res;
  if (readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 2)) > readReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1))) {
    *flag = bit_set(*flag, 1);
  } else {
    *flag = bit_unset(*flag, 1);
  }
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), res);
  *IP = *IP + 1;
}

INSFORMAT(reg_getIP) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), *IP);
  *IP = *IP + 1;
}

INSFORMAT(reg_getCS) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), segments[segment_CS]);
  *IP = *IP + 1;
}

INSFORMAT(reg_getSS) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), segments[segment_SS]);
  *IP = *IP + 1;
}

INSFORMAT(reg_getDS) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), segments[segment_DS]);
  *IP = *IP + 1;
}

INSFORMAT(reg_getES) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), segments[segment_ES]);
  *IP = *IP + 1;
}

INSFORMAT(reg_getSP) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), *SP);
  *IP = *IP + 1;
}

INSFORMAT(reg_getBP) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), *BP);
  *IP = *IP + 1;
}

INSFORMAT(reg_getDI) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), *DI);
  *IP = *IP + 1;
}

INSFORMAT(reg_getSI) {
  writeReg_uint16(readRAM_uint8(segments[segment_CS], *IP + 1), *SI);
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(tele) {
  cout << (int) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1));
  *IP = *IP + 1;
}

INSFORMAT_NO_CONSTEXPR(short_out) {
  uint8_t portNumber = readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1));
  LibOnRead[portNumber].lock();
  portStateB[portNumber] = readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2));
  stateChangeB[portNumber] = true;
  LibOnRead[portNumber].unlock();
  *IP = *IP + 2;
}

INSFORMAT_NO_CONSTEXPR(printch) {
  cout << (int) readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1)) << endl;
  *IP = *IP + 1;
}

INSFORMAT(ret) {
  *SP = *SP + 2;
  *IP = readRAM_uint16(segments[segment_SS], *SP) - 1;
}

INSFORMAT(call) {
  writeRAM_uint16(segments[segment_SS], *SP, *IP + 3);
  *SP = *SP - 2;
  *IP = readRAM_uint16(segments[segment_CS], *IP + 1) - 1;
}

constexpr int TIMEOUT = 40;
INSFORMAT_NO_CONSTEXPR(short_in) {
  uint8_t data;
  int time = 0;
  uint8_t portNumber = readReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 1));
  while(stateChangeA[portNumber] == false and time <= TIMEOUT) {
    SDL_Delay(1);
    time = time + 1;
  }
  
  if (time < TIMEOUT) {
    CPUOnRead[portNumber].lock();
    data = portStateA[portNumber];
    stateChangeA[portNumber] = false;
    CPUOnRead[portNumber].unlock();
    writeReg_uint8(readRAM_uint8(segments[segment_CS], *IP + 2), data);
    *flag = bit_unset(*flag, 2);
  } else {
    *flag = bit_set(*flag, 2);
  }
  
  *IP = *IP + 2;
}

typedef int(*mypselect_t)(int, fd_set*, fd_set*, fd_set*, const timespec*, const __sigset_t*);
constexpr mypselect_t mypselect = pselect;
constexpr void sleep(int64_t nsec) {
  struct timespec delay = {nsec / 1000000000, nsec % 1000000000};
  mypselect(0, NULL, NULL, NULL, &delay, NULL);
}

/*
Memory allocation (based on IBM PC (0xF00000 + <physical address in IBM PC memory allocation doc>))
0x000000 - 0xF9FFFF = Conventional RAM 15,936k or 249x64k segment 
0xFA0000 - 0xFBFFFF = Video RAM 128k or 2x64k segment
0xFC0000 - 0xFFFFFF = ROM 256k or 4x64k segment
*/

struct DevArguments {
  void (*func)(int, const char**);
  int argc;
  const char** argv;
};

void* device(void* arg) {
  ready.lock();
  ready.unlock();
  
  DevArguments* args = (DevArguments*) arg;
  args->func(args->argc, args->argv);
  return NULL;
}

constexpr uint8_t ROMSegment = 0xFC;
int main() {
  luaL_openlibs(L);

  dictionary* ini = iniparser_load("_config.ini");
  int64_t delay = (uint64_t) 1e+9 / double(iniparser_getint(ini, "System:Clock", 1) * 1024);
  
  int status;
  int abc;
  File rom = File("rom.exe", &status);
  size_t fsize = rom.getFilesize();
  segments[segment_CS] = ROMSegment;
  uint16_t j = 0;
  while (j < fsize) {
    char tmp = '\0';
    rom.fread(&tmp, 1);
    writeRAM_uint8(segments[segment_CS], j,tmp);
    j++;
  }
  
  InsFormat instructions[] =  {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  
  struct timespec delaySpec = {delay / 1000000000, delay % 1000000000};
  
  int i = 0;
  while (i < 256) {instructions[i] = unknown;i++;};
  
  instructions[0] = nop;
  instructions[1] = uint16_add;
  instructions[2] = uint16_sub;
  instructions[3] = reg_move_uint16;
  instructions[4] = imm_move_uint16;
  instructions[5] = uint8_mult;
  instructions[6] = uint8_add;
  instructions[7] = uint8_sub;
  instructions[8] = reg_move_uint8;
  instructions[9] = imm_move_uint8;
  instructions[10] = uint8_cmp;
  instructions[11] = uint16_cmp;
  instructions[12] = clf;
  instructions[13] = imm_jmp;
  instructions[14] = je;
  instructions[15] = js;
  instructions[16] = jg;
  instructions[17] = jc;
  instructions[18] = jz;
  instructions[19] = reg_jmp;
  instructions[20] = reg_je;
  instructions[21] = reg_js;
  instructions[22] = reg_jg;
  instructions[23] = reg_jc;
  instructions[24] = reg_jz;
  instructions[25] = pop_uint8;
  instructions[26] = push_uint8;
  instructions[27] = pop_uint16;
  instructions[28] = push_uint16;
  instructions[29] = pusha;
  instructions[30] = popa;
  instructions[31] = imm_uint16_add;
  instructions[32] = imm_uint16_sub;
  instructions[33] = imm_uint8_add;
  instructions[34] = imm_uint8_sub;
  instructions[35] = uint8_inc;
  instructions[36] = uint16_inc;
  instructions[37] = uint8_dec;
  instructions[38] = uint16_dec;
  instructions[39] = reg_getCS;
  instructions[40] = reg_getSS;
  instructions[41] = reg_getDS;
  instructions[42] = reg_getES;
  instructions[43] = reg_getSP;
  instructions[44] = reg_getBP;
  instructions[45] = reg_getDI;
  instructions[46] = reg_getSI;
  instructions[47] = short_out;
  instructions[48] = short_in;
  instructions[49] = ret;
  instructions[50] = call;
  
  instructions[252] = printch;
  instructions[253] = reg_getIP;
  instructions[254] = prn_uint16;
  instructions[255] = hlt;
  
  ready.lock();
  pthread_t fake;
  if (iniparser_getboolean(ini, "System:printClock", 0) == 1) {
    pthread_create(&fake, NULL, frequencyPrinter, NULL);
  }
  pthread_create(&fake, NULL, busSync, NULL);
  
  const char* parser = "output = {}\
     local i = 1 \
     for str in _G.string.gmatch(input, \"[^ \\n]+\") do \
       output[i] = str\
       i = i + 1 \
     end \
  ";
  
  luaL_loadstring(L, parser);
  lua_setglobal(L, "func_cache");
  
  dlopen(NULL, RTLD_GLOBAL | RTLD_NOW);
  printf("Loading devices...\n");
  int k = 0;
  int nsec = iniparser_getnsec(ini);
  std::string otherThan("system");
  while (k < nsec) {
    const char* str = iniparser_getsecname(ini, k);
    const char* resultStr;
    if (std::string(str) != otherThan) {
      lua_pushstring(L, str);
      lua_pushstring(L, ":");
      lua_pushstring(L, "EntryPoint");
      lua_concat(L, 3);
      resultStr = lua_tostring(L, -1);
      lua_pop(L, 1);
      
      const char* entryPoint = iniparser_getstring(ini, resultStr, NULL);
      if (entryPoint == NULL) {
        printf("EntryPoint for %s cannot be found either missing or not a string\n", resultStr);
      } else {
        lua_pushstring(L, str);
        lua_pushstring(L, ":");
        lua_pushstring(L, "Path");
        lua_concat(L, 3);
        resultStr = lua_tostring(L, -1);
        lua_pop(L, 1);
        const char* path = iniparser_getstring(ini, resultStr, NULL);
        if (path == NULL) {
          printf("Path for %s cannot be found either missing or not a string\n", resultStr);
        } else {
          void* handle = dlopen(path, RTLD_NOW);
          if (handle == NULL) {
            printf("Failed to load %s reason:\n%s\n", path, dlerror());
          } else {
            void* ptr_to_func = dlsym(handle, entryPoint);
            if (ptr_to_func == NULL) {
              printf("%s\n", dlerror());
            } else {
              lua_pushstring(L, str);
              lua_pushstring(L, ":");
              lua_pushstring(L, "Argument");
              lua_concat(L, 3);
              resultStr = lua_tostring(L, -1);
              lua_pop(L, 1);
        
              const char* buffer = iniparser_getstring(ini, resultStr, NULL);
              const char** arguments = (const char**) malloc(sizeof(const char*));
              
              lua_pushstring(L, buffer);
              lua_setglobal(L, "input");
              lua_getglobal(L, "func_cache");
              lua_call(L, 0, 0);
              
              int pos = 0;
              lua_pushglobaltable(L);
              lua_getfield(L, 1, "output");
              const char** tmp;
              while (true) {
                lua_pushnumber(L, pos + 1);
                if (lua_gettable(L, -2) == LUA_TNIL) {
                  break;
                }
              
                arguments[pos] = lua_tostring(L, -1);
                lua_pop(L, 1);
                pos++;
                
                //Reallocation
                tmp = (const char**) malloc(sizeof(const char*) * pos);
                memcpy((void*) tmp, (void*) arguments, sizeof(const char*) * pos);
                free(arguments);
                
                arguments = (const char**) malloc(sizeof(const char*) * (pos + 1));
                memcpy((void*) arguments, (void*) tmp, sizeof(const char*) * pos);
                free(tmp);
              }
              lua_pop(L, 1);
              
              DevArguments* args = new DevArguments;
              args->func = (void (*)(int, const char**)) ptr_to_func;
              args->argc = pos;
              args->argv = arguments;
              
              pthread_t fake;
              pthread_create(&fake, NULL, device, (void*) args);
            }
          }
        }
      }
    }
    k++;
  }
  printf("Complete!\n");
  
  ready.unlock();
  SDL_Delay(2);
  while (true) {
    instructions[readRAM_uint8(segments[segment_CS], *IP)]();
    *IP = *IP + 1;
    cycles++;
    pselect(0, NULL, NULL, NULL, &delaySpec, NULL);
  }

}




















