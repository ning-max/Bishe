#include "w25q64.h"

extern SPI_HandleTypeDef hspi2;

void W25Q64_CS_L(void) { HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET); }
void W25Q64_CS_H(void) { HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET); }

void W25Q64_Init(void)  { W25Q64_CS_H(); }

static uint8_t spi_xfer(uint8_t d)
{
    uint8_t r;
    HAL_SPI_TransmitReceive(&hspi2, &d, &r, 1, 100);
    return r;
}

uint8_t W25Q64_RdSR1(void)
{
    uint8_t cmd = W25Q64_CMD_RD_SR1, sr;
    W25Q64_CS_L();
    spi_xfer(cmd);
    sr = spi_xfer(0xFF);
    W25Q64_CS_H();
    return sr;
}

void W25Q64_WaitBusy(void)      { while (W25Q64_RdSR1() & 0x01) {} }
void W25Q64_WriteEnable(void)   { uint8_t c = W25Q64_CMD_WR_EN; W25Q64_CS_L(); spi_xfer(c); W25Q64_CS_H(); }

uint32_t W25Q64_ReadJEDEC(void)
{
    uint8_t buf[3];
    W25Q64_CS_L();
    spi_xfer(W25Q64_CMD_JEDEC);
    buf[0] = spi_xfer(0xFF); buf[1] = spi_xfer(0xFF); buf[2] = spi_xfer(0xFF);
    W25Q64_CS_H();
    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
}

void W25Q64_SectorErase(uint32_t addr)
{
    W25Q64_WriteEnable();
    W25Q64_CS_L();
    spi_xfer(W25Q64_CMD_SEC_ERASE);
    spi_xfer((addr >> 16) & 0xFF);
    spi_xfer((addr >> 8)  & 0xFF);
    spi_xfer(addr & 0xFF);
    W25Q64_CS_H();
    W25Q64_WaitBusy();
}

void W25Q64_PageProgram(uint32_t addr, uint8_t *data, uint16_t len)
{
    W25Q64_WriteEnable();
    W25Q64_CS_L();
    spi_xfer(W25Q64_CMD_PAGE_PROG);
    spi_xfer((addr >> 16) & 0xFF);
    spi_xfer((addr >> 8)  & 0xFF);
    spi_xfer(addr & 0xFF);
    for (uint16_t i = 0; i < len && i < W25Q64_PAGE_SIZE; i++)
        spi_xfer(data[i]);
    W25Q64_CS_H();
    W25Q64_WaitBusy();
}

void W25Q64_ReadData(uint32_t addr, uint8_t *data, uint16_t len)
{
    W25Q64_CS_L();
    spi_xfer(W25Q64_CMD_READ);
    spi_xfer((addr >> 16) & 0xFF);
    spi_xfer((addr >> 8)  & 0xFF);
    spi_xfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++)
        data[i] = spi_xfer(0xFF);
    W25Q64_CS_H();
}
