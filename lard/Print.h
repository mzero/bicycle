#ifndef __INCLUDE_LARD_PRINT_H__
#define __INCLUDE_LARD_PRINT_H__

#include <cstdint>



class Print {
  public:
    void print(bool);
    void print(short int);
    void print(unsigned short int);
    void print(int);
    void print(unsigned int);
    void print(char);
    void print(const char*);
    
    void println(bool);
    void println(short int);
    void println(unsigned short int);
    void println(int);
    void println(unsigned int);
    void println(char);
    void println(const char*);
    
    void printf(const char*, ...);
    
    virtual size_t write(uint8_t) = 0;
};



#endif // __INCLUDE_LARD_PRINT_H__
