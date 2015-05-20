#include <stdbool.h>
#include <stdio.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/usb/usbd.h>

#include "usbcdc.h"

#define PACKET_SZIE 64

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
  /* NOTE: libopencm3 will automatically setup related GPIO and AFIO. */

  rcc_periph_clock_enable(RCC_TIM2);
  timer_reset(TIM2);

  /* Disable outputs. */
  timer_disable_oc_output(TIM2, TIM_OC1);
  timer_disable_oc_output(TIM2, TIM_OC2);
  timer_disable_oc_output(TIM2, TIM_OC3);
  timer_disable_oc_output(TIM2, TIM_OC4);

  /* Timer mode: no divider, edge, count up */
  timer_disable_preload(TIM2);
  timer_continuous_mode(TIM2);
  timer_set_period(TIM2, 65535);
  timer_slave_set_mode(TIM2, TIM_SMCR_SMS_ECM1);
  timer_slave_set_filter(TIM2, TIM_IC_CK_INT_N_2);
  timer_slave_set_polarity(TIM2, TIM_ET_RISING);
  timer_slave_set_prescaler(TIM2, TIM_IC_PSC_OFF);
  timer_slave_set_trigger(TIM2, TIM_SMCR_TS_ETRF);
  timer_update_on_overflow(TIM2);

  nvic_enable_irq(NVIC_TIM2_IRQ);
  timer_enable_counter(TIM2);
  timer_enable_irq(TIM2, TIM_DIER_CC1IE);
}

void mco_setup(void) {
  /* NOTE: AFIO already enabled by timer setup. If not, you need to enable it here. */

  /* WTF: needs to "re-enable" GPIOA clock **separately** to get the MCO work.
   * Maybe a bug in libopencm3 or STM32 or both? */

  rcc_periph_clock_enable(RCC_GPIOA);

  /* Outputs 72MHz clock on PA8, for calibration. */
  // TODO: let user switch with button, or even cycle through different clock sources.
  // Available: NOCLK, SYSCLK, HSICLK, HSECLK, PLL / 2, and a bunch of debug outputs.
  // SYSCLK available only when SYSCLK < 50MHz.
  gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO8);
  rcc_set_mco(RCC_CFGR_MCO_HSECLK); /* This merely sets RCC_CFGR, and does not care about GPIO or AFIO. */
}

int main(void) {
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
  rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB);

  /* Setup GPIOB Pin 1 for the LED. */
  /* NOTE: Maple Mini is different from maple! */
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO1);
  gpio_set(GPIOB, GPIO1);

  /* Setup GPIOB Pin 9 to pull up the D+ high. The circuit is active low. */
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, GPIO9);
  gpio_clear(GPIOB, GPIO9);

  usbcdc_init();

  gpio_clear(GPIOB, GPIO1);

  timer_setup();
  systick_ms_setup();
  mco_setup();

  /* Wait 500ms for USB setup to complete before sending anything. */
  /* Takes ~ 130ms on my machine */
  while (systick_ms < 500);

  /* The loop. */
  char buffer[PACKET_SZIE];
  int len;

  /* Skip the first sample: no resets happened and if we do the usual subtract here it will underflow. */
  while (!updated);
  updated = false;

  /* The loop (for real). */
  while (true) {
    if (!updated) {
      continue;
    }

    updated = false;
    show_dot = !show_dot;
    /* Subtract one extra overflow occurred during counter reset. */
    freq -= 65536;
    /* TODO: The following line costs approx. 20KB. Find an alternative. */
    len = snprintf(buffer, PACKET_SZIE, "%4lu.%06lu MHz %c\r", freq / 1000000, freq % 1000000, show_dot ? '.' : ' ');
    if (len > 0) {
      len ++;
      usbcdc_write(buffer, len);
    }
  }

  return 0;
}

/* Interrupts */

void tim2_isr(void) {
  if (timer_get_flag(TIM2, TIM_SR_CC1IF)) {
    freq_scratch += 65536; /* TIM2 is 16-bit and overflows every 65536 events. */
    timer_clear_flag(TIM2, TIM_SR_CC1IF); /* Clear interrrupt flag. */
  }
}

void sys_tick_handler(void) {
  systick_ms ++;

  if (systick_ms % 1000 == 0) {
    /* Scratch pad to finalized result */
    freq = freq_scratch + timer_get_counter(TIM2);
    /* Reset the counter. This will generate one extra overflow. */
    /* In case of nothing got counted, manually generate a reset. */
    timer_set_counter(TIM2, 1);
    timer_set_counter(TIM2, 0);
    freq_scratch = 0;
    updated = true;
    gpio_toggle(GPIOB, GPIO1);
  }
}
