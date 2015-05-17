#include <stdbool.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/usb/usbd.h>

#include "usbcdc.h"

#define PACKET_SZIE 32

static volatile uint32_t systick_ms = 0;
static volatile uint32_t freq = 0; /* 32bit = approx. 4.3G ticks per second. */
static volatile uint32_t freq_scratch = 0; /* scratch pad. */
static volatile bool updated = false;

static bool show_dot = false;


void systick_ms_setup(void) {
  /* 72MHz clock, interrupt for every 72,000 CLKs (1ms). */
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_set_reload(72000 - 1);
  systick_interrupt_enable();
  systick_counter_enable();
}

void timer_setup(void) {
  rcc_periph_clock_enable(RCC_TIM2);
  
}

int main(void) {
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
  rcc_periph_clock_enable(RCC_GPIOB);

  /* Setup GPIOB Pin 1 for the LED. */
  /* NOTE: Maple Mini is different from maple! */
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO1);
  gpio_set(GPIOB, GPIO1);

  /* Setup GPIOB Pin 9 to pull up the D+ high. The circuit is active low. */
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, GPIO9);
  gpio_clear(GPIOB, GPIO9);

  usbcdc_init();

  gpio_clear(GPIOB, GPIO1);
  systick_ms_setup();

  /* Wait 500ms for USB setup to complete before sending anything. */
  /* Takes ~ 130ms on my machine */
  while (systick_ms < 500);

  /* The loop. */

  while (true) {
    show_dot = !show_dot;

    

  }

  return 0;
}

/* Interrupts */

void tim2_isr(void) {
  freq_scratch += 65536;
  timer_clear_flag(TIM2, TIM_SR_CC1IF); /* Clear interrrupt flag. */
}

void sys_tick_handler(void) {
  systick_ms ++;

  if (systick_ms % 1000 == 0) {
    /* Scratch pad to finalized result */
    freq = freq_scratch + timer_get_counter(TIM2);
    freq_scratch = 0;
    updated = true;
    gpio_toggle(GPIOB, GPIO1);
  }
}
