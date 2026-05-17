#ifndef __W25Q64_H__
#define __W25Q64_H__

#include "main.h"

#define W25Q64_CMD_WR_EN      0x06
#define W25Q64_CMD_RD_SR1     0x05
#define W25Q64_CMD_SEC_ERASE  0x20
#define W25Q64_CMD_PAGE_PROG  0x02
#define W25Q64_CMD_READ       0x03
#define W25Q64_CMD_JEDEC      0x9F

#define W25Q64_SECTOR_SIZE    4096
#define W25Q64_PAGE_SIZE      256
#define W25Q64_STORAGE_ADDR   0x7FF000

void     W25Q64_Init(void);
void     W25Q64_CS_L(void);
void     W25Q64_CS_H(void);
uint32_t W25Q64_ReadJEDEC(void);
void     W25Q64_WaitBusy(void);
void     W25Q64_WriteEnable(void);
void     W25Q64_SectorErase(uint32_t addr);
void     W25Q64_PageProgram(uint32_t addr, uint8_t *data, uint16_t len);
void     W25Q64_ReadData(uint32_t addr, uint8_t *data, uint16_t len);

#endif
