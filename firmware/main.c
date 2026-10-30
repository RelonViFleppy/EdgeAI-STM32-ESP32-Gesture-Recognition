#include <stdint.h>

// Register Adresleri
#define RCC_BASE        0x40023800UL
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))

// GPIOA (SPI1: PA5, PA6, PA7 | USART2: PA2 TX)
#define GPIOA_BASE      0x40020000UL
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

// GPIOE (CS: PE3)
#define GPIOE_BASE      0x40021000UL
#define GPIOE_MODER     (*(volatile uint32_t *)(GPIOE_BASE + 0x00))
#define GPIOE_OSPEEDR   (*(volatile uint32_t *)(GPIOE_BASE + 0x08))
#define GPIOE_BSRR      (*(volatile uint32_t *)(GPIOE_BASE + 0x18))

// SPI1
#define SPI1_BASE       0x40013000UL
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

// USART2
#define USART2_BASE     0x40004400UL
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0C))

#define LIS3DSH_CTRL_REG4   0x20
#define CS_LOW()        (GPIOE_BSRR = (1UL << (3 + 16)))
#define CS_HIGH()       (GPIOE_BSRR = (1UL << 3))

void periph_init(void)
{
    RCC_AHB1ENR |= (1UL << 0) | (1UL << 4);
    RCC_APB2ENR |= (1UL << 12);
    RCC_APB1ENR |= (1UL << 17);

    // PE3 Output
    GPIOE_MODER &= ~(3UL << (3 * 2));
    GPIOE_MODER |=  (1UL << (3 * 2));
    GPIOE_OSPEEDR |= (3UL << (3 * 2));
    CS_HIGH();

    // PA5, PA6, PA7 -> SPI1 (AF5)
    GPIOA_MODER &= ~((3UL << 10) | (3UL << 12) | (3UL << 14));
    GPIOA_MODER |=  ((2UL << 10) | (2UL << 12) | (2UL << 14));
    GPIOA_AFRL  &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOA_AFRL  |=  ((5UL << 20) | (5UL << 24) | (5UL << 28));

    // PA2 -> USART2 TX (AF7)
    GPIOA_MODER &= ~(3UL << (2 * 2));
    GPIOA_MODER |=  (2UL << (2 * 2));
    GPIOA_AFRL  &= ~(0xF << (2 * 4));
    GPIOA_AFRL  |=  (7UL << (2 * 4));

    // SPI1 Master Modu
    SPI1_CR1 = (1UL << 2) | (5UL << 3) | (1UL << 1) | (1UL << 0) | (1UL << 9) | (1UL << 8) | (1UL << 6);

    // USART2: 115200 Baud (16 MHz clock için)
    USART2_BRR = 0x008B;
    USART2_CR1 = (1UL << 3) | (1UL << 13);
}

uint8_t spi1_transfer(uint8_t data)
{
    while (!(SPI1_SR & (1UL << 1)));
    SPI1_DR = data;
    while (!(SPI1_SR & (1UL << 0)));
    return (uint8_t)SPI1_DR;
}

void lis3dsh_write(uint8_t reg, uint8_t val)
{
    CS_LOW();
    spi1_transfer(reg & 0x7F);
    spi1_transfer(val);
    CS_HIGH();
}

uint8_t lis3dsh_read(uint8_t reg)
{
    uint8_t val;
    CS_LOW();
    spi1_transfer(reg | 0x80);
    val = spi1_transfer(0x00);
    CS_HIGH();
    return val;
}

void uart2_write_char(char c)
{
    while (!(USART2_SR & (1UL << 7)));
    USART2_DR = (uint8_t)c;
}

void uart2_write_string(const char *str)
{
    while (*str) uart2_write_char(*str++);
}

void uart2_print_int(int16_t num)
{
    char buf[7];
    int i = 0;
    if (num < 0) {
        uart2_write_char('-');
        num = -num;
    }
    if (num == 0) {
        uart2_write_char('0');
        return;
    }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    while (i > 0) {
        uart2_write_char(buf[--i]);
    }
}

void delay_cycles(volatile uint32_t count)
{
    while (count--) __asm__("nop");
}

int main(void)
{
    periph_init();
    lis3dsh_write(LIS3DSH_CTRL_REG4, 0x67); // 100 Hz, tüm eksenler aktif

    while (1)
    {
        int16_t x = (int16_t)((lis3dsh_read(0x29) << 8) | lis3dsh_read(0x28));
        int16_t y = (int16_t)((lis3dsh_read(0x2B) << 8) | lis3dsh_read(0x2A));
        int16_t z = (int16_t)((lis3dsh_read(0x2D) << 8) | lis3dsh_read(0x2C));

        uart2_print_int(x);
        uart2_write_char(',');
        uart2_print_int(y);
        uart2_write_char(',');
        uart2_print_int(z);
        uart2_write_string("\r\n");

        delay_cycles(160000); // ~50 Hz örnekleme
    }
}
