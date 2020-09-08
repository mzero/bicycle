#ifndef __INCLUDE_LARD_SPI_H__
#define __INCLUDE_LARD_SPI_H__

class SPIClass {
  public:
    void begin();
    void transfer(uint8_t);
};

extern SPIClass SPI;

#endif // __INCLUDE_LARD_SPI_H__
