#include "stm32l4xx.h"
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/unistd.h>

#include "eeng1030_lib.h"
#include "i2c.h"

#define NUM_LEDS   16
#define WS_PIN     7
#define BTN_PIN    9

#define TOP_LED    0
#define DEAD_ZONE  180
#define SAVE_THRESHOLD 500

uint16_t response;
int32_t X_g, Y_g, Z_g;
int32_t X_filt = 0, Y_filt = 0, Z_filt = 0;

int saved_mode = 0;
int saved_centre_led = 0;
int save_latched = 0;

static const int16_t dir_x[NUM_LEDS] = {
     0,  383,  707,  924, 1000,  924,  707,  383,
     0, -383, -707, -924,-1000, -924, -707, -383
};

static const int16_t dir_y[NUM_LEDS] = {
   1000,  924,  707,  383,    0, -383, -707, -924,
  -1000, -924, -707, -383,    0,  383,  707,  924
};

void setup(void);
void delay(volatile uint32_t dly);
void initSerial(uint32_t baudrate);
void eputc(char c);

static void clock_init_80mhz(void);
static void gpio_init_ws2812(void);
static void gpio_init_button(void);
static int button_pressed(void);
static void dwt_init(void);
static void delay_us(uint32_t us);

static void initADC(void);
static int readADC(int chan);
static void initTimer2PWM(void);
static void setTimer2Duty(int duty);

static void ws2812_reset(void);
static void ws2812_send_bit(uint8_t bit);
static void ws2812_send_byte(uint8_t byte);
static void ws2812_send_pixel(uint8_t g, uint8_t r, uint8_t b);

static int32_t read_bmi160_axis(uint8_t reg_low);
static int wrap_led(int n);
static int get_pointer_led(int32_t x, int32_t y, int32_t z);
static void show_live_and_saved(int live_centre_led, int saved_centre_led, int saved_active, uint8_t g, uint8_t r, uint8_t b);

int main(void)
{
    uint8_t colour_index = 0;
    uint8_t last_button_state = 0;

    uint8_t g_val = 0x00;
    uint8_t r_val = 0x08;
    uint8_t b_val = 0x00;

    clock_init_80mhz();
    setup();
    initSerial(9600);
    gpio_init_ws2812();
    gpio_init_button();
    dwt_init();

    pinMode(GPIOA, 0, 3);
    pinMode(GPIOA, 3, 2);
    GPIOA->AFR[0] &= ~(0xF << (4 * 3));
    GPIOA->AFR[0] |=  (1U << (4 * 3));

    initADC();
    initTimer2PWM();

    ResetI2C();

    I2CStart(0x69, WRITE, 2);
    I2CWrite(0x7E);
    I2CWrite(0x11);
    I2CStop();

    delay(1000000);

    while (1)
    {
        uint8_t current_button_state = button_pressed();

        if ((current_button_state == 1) && (last_button_state == 0))
        {
            delay(30000);

            if (button_pressed() == 1)
            {
                colour_index++;
                if (colour_index >= 4)
                    colour_index = 0;

                while (button_pressed() == 1) {}

                delay(30000);
            }
        }

        last_button_state = current_button_state;

        X_g = read_bmi160_axis(0x12);
        Y_g = read_bmi160_axis(0x14);
        Z_g = read_bmi160_axis(0x16);

        X_filt = (3 * X_filt + X_g) / 4;
        Y_filt = (3 * Y_filt + Y_g) / 4;
        Z_filt = (3 * Z_filt + Z_g) / 4;

        int pot_value = readADC(5);
        int centre_led = get_pointer_led(X_filt, Y_filt, Z_filt);

        if (pot_value <= SAVE_THRESHOLD)
        {
            setTimer2Duty(0);

            if (save_latched == 0)
            {
                saved_centre_led = centre_led;
                saved_mode = 1;
                save_latched = 1;
            }
        }
        else
        {
            setTimer2Duty(pot_value);
            saved_mode = 0;
            save_latched = 0;
        }

        if (colour_index == 0)
        {
            g_val = 0x00;
            r_val = 0x08;
            b_val = 0x00;
        }
        else if (colour_index == 1)
        {
            g_val = 0x08;
            r_val = 0x08;
            b_val = 0x00;
        }
        else if (colour_index == 2)
        {
            g_val = 0x02;
            r_val = 0x08;
            b_val = 0x00;
        }
        else
        {
            g_val = 0x08;
            r_val = 0x00;
            b_val = 0x00;
        }

        printf("POT=%d X=%ld Y=%ld Z=%ld colour=%d saved=%d saved_led=%d live_led=%d\r\n",
               pot_value, X_filt, Y_filt, Z_filt, colour_index, saved_mode, saved_centre_led, centre_led);

        show_live_and_saved(centre_led, saved_centre_led, saved_mode, g_val, r_val, b_val);

        delay(80000);
    }
}

static int32_t read_bmi160_axis(uint8_t reg_low)
{
    int16_t accel_raw;
    int32_t value;

    I2CStart(0x69, WRITE, 1);
    I2CWrite(reg_low);
    I2CReStart(0x69, READ, 2);

    response = I2CRead();
    accel_raw = response;

    response = I2CRead();
    accel_raw = accel_raw + (response << 8);

    I2CStop();

    value = accel_raw;
    value = (value * 981) / 16384;

    return value;
}

static int wrap_led(int n)
{
    while (n < 0) n += NUM_LEDS;
    while (n >= NUM_LEDS) n -= NUM_LEDS;
    return n;
}

static int get_pointer_led(int32_t x, int32_t y, int32_t z)
{
    int32_t mag2_xy = x * x + y * y;

    if (mag2_xy < (DEAD_ZONE * DEAD_ZONE))
    {
        return TOP_LED;
    }

    int best_led = 0;
    int32_t best_dot = -2147483647;

    for (int i = 0; i < NUM_LEDS; i++)
    {
        int32_t dot = x * dir_x[i] + y * dir_y[i];

        if (dot > best_dot)
        {
            best_dot = dot;
            best_led = i;
        }
    }

    return wrap_led(TOP_LED + best_led);
}

static void show_live_and_saved(int live_centre_led, int saved_centre_led, int saved_active, uint8_t g, uint8_t r, uint8_t b)
{
    int live_a = wrap_led(live_centre_led - 1);
    int live_b = wrap_led(live_centre_led);
    int live_c = wrap_led(live_centre_led + 1);

    int save_a = wrap_led(saved_centre_led - 1);
    int save_b = wrap_led(saved_centre_led);
    int save_c = wrap_led(saved_centre_led + 1);

    ws2812_reset();

    for (int i = 0; i < NUM_LEDS; i++)
    {
        uint8_t gg = 0x00;
        uint8_t rr = 0x00;
        uint8_t bb = 0x00;

        if (saved_active && (i == save_a || i == save_b || i == save_c))
        {
            gg = 0x00;
            rr = 0x00;
            bb = 0x08;
        }

        if (i == live_a || i == live_b || i == live_c)
        {
            gg = g;
            rr = r;
            bb = b;
        }

        ws2812_send_pixel(gg, rr, bb);
    }

    ws2812_reset();
}

void setup(void)
{
    RCC->AHB2ENR |= (1 << 0) | (1 << 1);
    initI2C();
}

void delay(volatile uint32_t dly)
{
    while (dly--) {}
}

void initSerial(uint32_t baudrate)
{
    RCC->AHB2ENR |= (1 << 0);

    pinMode(GPIOA, 2, 2);
    selectAlternateFunction(GPIOA, 2, 7);

    pinMode(GPIOA, 15, 2);
    selectAlternateFunction(GPIOA, 15, 3);

    RCC->APB1ENR1 |= (1 << 17);

    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = (1 << 12);
    USART2->BRR = 80000000 / baudrate;
    USART2->CR1 = (1 << 3);
    USART2->CR1 |= (1 << 2);
    USART2->CR1 |= (1 << 0);
}

int _write(int file, char *data, int len)
{
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO))
    {
        errno = EBADF;
        return -1;
    }

    int original_len = len;

    while (len--)
    {
        eputc(*data);
        data++;
    }

    return original_len;
}

void eputc(char c)
{
    while ((USART2->ISR & (1 << 6)) == 0) {}
    USART2->TDR = c;
}

static void clock_init_80mhz(void)
{
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}

    FLASH->ACR |= FLASH_ACR_LATENCY_4WS;

    RCC->PLLCFGR =
        RCC_PLLCFGR_PLLSRC_HSI |
        (0 << RCC_PLLCFGR_PLLM_Pos) |
        (10 << RCC_PLLCFGR_PLLN_Pos) |
        (0 << RCC_PLLCFGR_PLLR_Pos) |
        RCC_PLLCFGR_PLLREN;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    SystemCoreClock = 80000000;
}

static void gpio_init_ws2812(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    GPIOA->MODER &= ~(3U << (WS_PIN * 2));
    GPIOA->MODER |=  (1U << (WS_PIN * 2));

    GPIOA->OTYPER &= ~(1U << WS_PIN);
    GPIOA->OSPEEDR &= ~(3U << (WS_PIN * 2));
    GPIOA->OSPEEDR |=  (3U << (WS_PIN * 2));
    GPIOA->PUPDR &= ~(3U << (WS_PIN * 2));

    GPIOA->BSRR = (1U << (WS_PIN + 16));
}

static void gpio_init_button(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    GPIOA->MODER &= ~(3U << (BTN_PIN * 2));
    GPIOA->PUPDR &= ~(3U << (BTN_PIN * 2));
    GPIOA->PUPDR |=  (1U << (BTN_PIN * 2));
}

static int button_pressed(void)
{
    if ((GPIOA->IDR & (1U << BTN_PIN)) == 0)
        return 1;
    else
        return 0;
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks) {}
}

static void ws2812_reset(void)
{
    GPIOA->BSRR = (1U << (WS_PIN + 16));
    delay_us(80);
}

static void ws2812_send_bit(uint8_t bit)
{
    uint32_t start = DWT->CYCCNT;

    if (bit)
    {
        GPIOA->BSRR = (1U << WS_PIN);
        while ((DWT->CYCCNT - start) < 64) {}
        GPIOA->BSRR = (1U << (WS_PIN + 16));
        while ((DWT->CYCCNT - start) < 100) {}
    }
    else
    {
        GPIOA->BSRR = (1U << WS_PIN);
        while ((DWT->CYCCNT - start) < 32) {}
        GPIOA->BSRR = (1U << (WS_PIN + 16));
        while ((DWT->CYCCNT - start) < 100) {}
    }
}

static void ws2812_send_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        ws2812_send_bit((byte >> i) & 0x01);
    }
}

static void ws2812_send_pixel(uint8_t g, uint8_t r, uint8_t b)
{
    ws2812_send_byte(g);
    ws2812_send_byte(r);
    ws2812_send_byte(b);
}

static void initADC(void)
{
    RCC->AHB2ENR |= (1 << 13);
    RCC->CCIPR |= (1 << 29) | (1 << 28);
    ADC1_COMMON->CCR = ((0b01) << 16) + (1 << 22);

    ADC1->CR = (1 << 28);
    delay(100);

    ADC1->CR |= (1 << 31);
    while (ADC1->CR & (1 << 31)) {}

    ADC1->CFGR = (1 << 31);
}

static int readADC(int chan)
{
    ADC1->SQR1 = 0;
    ADC1->SQR1 |= (chan << 6);

    ADC1->ISR = (1 << 3);
    ADC1->CR |= (1 << 0);
    while ((ADC1->ISR & (1 << 0)) == 0) {}

    ADC1->CR |= (1 << 2);
    while ((ADC1->ISR & (1 << 3)) == 0) {}

    return ADC1->DR;
}

static void initTimer2PWM(void)
{
    RCC->APB1ENR1 |= (1 << 0);

    TIM2->CR1 = 0;
    TIM2->CCMR2 = (0b110 << 12) | (1 << 11);
    TIM2->CCER |= (1 << 12);

    TIM2->PSC = 79;
    TIM2->ARR = 1000 - 1;
    TIM2->CCR4 = 0;

    TIM2->EGR |= (1 << 0);
    TIM2->CR1 |= (1 << 7);
    TIM2->CR1 |= (1 << 0);
}

static void setTimer2Duty(int duty)
{
    int arrvalue;

    if (duty < 0)
        duty = 0;

    if (duty > 4095)
        duty = 4095;

    if (duty <= SAVE_THRESHOLD)
    {
        TIM2->CCR4 = 0;
        return;
    }

    arrvalue = (duty * (TIM2->ARR + 1)) / 4095;
    TIM2->CCR4 = arrvalue;
}