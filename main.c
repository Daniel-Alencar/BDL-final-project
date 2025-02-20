#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "hardware/adc.h"

#include "joystick/joystick.h"
#include "buttons/buttons.h"
#include "led_rgb/led_rgb.h"

// Definições para ADC
#define ADC_MAX 4095

// Configuração de escala para movimento do cursor
// Velocidade máxima do mouse
#define MOUSE_MAX_SPEED 10
// Valor central do ADC
#define ADC_CENTER (ADC_MAX / 2)

// Protótipos das funções
void led_blinking_task(void);
void hid_task(void);

// Configuração do intervalo de piscar do LED
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;


#define TOTAL_FUNCTIONS 2
// 0: Mouse
// 1: Teclado
uint hid_function = 0;

int main(void) {
  board_init();

  setup_joystick();
  setup_buttons();
  setup_led_RGB();

  // Inicializa a pilha USB
  tud_init(BOARD_TUD_RHPORT);

  while (1) {
    // Tarefa do TinyUSB
    tud_task(); 
    led_blinking_task();
    // Envia os relatórios HID
    hid_task(); 
  }
}

// Callbacks de status USB
void tud_mount_cb(void) { blink_interval_ms = BLINK_MOUNTED; }
void tud_umount_cb(void) { blink_interval_ms = BLINK_NOT_MOUNTED; }
void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}
void tud_resume_cb(void) { 
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED; 
}

// Função para mapear valores do ADC para deslocamento do cursor
int8_t adc_to_mouse_movement(uint16_t adc_value) {
  int16_t mapped = ((int16_t)adc_value - ADC_CENTER) * MOUSE_MAX_SPEED / ADC_CENTER;
  return (int8_t)mapped;
}

// Envia um relatório HID de movimento do mouse baseado no ADC
void send_hid_mouse_report() {
  if (!tud_hid_ready()) return;

  // Lê o ADC
  uint16_t adc_value_y = read_Y();
  uint16_t adc_value_x = read_X();

  // Converte ADC para movimento do cursor
  int8_t delta_x = adc_to_mouse_movement(adc_value_x);
  int8_t delta_y = adc_to_mouse_movement(adc_value_y);

  // Lê os botões (nível baixo = pressionado)
  uint8_t buttons = 0;
  if(!gpio_get(BUTTON_A)) {
    buttons |= MOUSE_BUTTON_LEFT;
  }
  if(!gpio_get(BUTTON_B)) {
    buttons |= MOUSE_BUTTON_RIGHT;
  }

  // Envia o relatório do mouse
  tud_hid_mouse_report(REPORT_ID_MOUSE, buttons, delta_x, -delta_y, 0, 0);
}

uint8_t keyboard_character = HID_KEY_A;

// Intervalo de envio
#define HID_INTERVAL_MS 100

void hid_keyboard_task(void) {

  // Buffer de teclas pressionadas
  uint8_t keycode[6] = {0};
  uint8_t key_count = 0;

  bool value_A = gpio_get(BUTTON_A);
  bool value_B = gpio_get(BUTTON_B);

  // Se nenhum dos dois foi pressionado
  if(value_A && value_B) {
    // Mantêm tudo como está
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, 0);
    set_red(!gpio_get(LED_RED));
  } else {
    // Verifica os botões
    if (!value_A) {
      keyboard_character--;
    }
    
    if (!value_B) {
      keyboard_character++;
    }

    // keycode[key_count++] = HID_KEY_BACKSPACE;
    // keycode[key_count++] = keyboard_character;

    // Envia Backspace
    keycode[0] = HID_KEY_BACKSPACE;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    sleep_ms(50);

    // Libera as teclas (necessário para evitar teclas presas)
    keycode[0] = 0;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    sleep_ms(50);

    keycode[0] = keyboard_character;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    sleep_ms(50);

    // Libera as teclas (necessário para evitar teclas presas)
    keycode[0] = 0;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  }
  sleep_ms(500);
}


// Tarefa para envio periódico dos relatórios HID
void hid_task(void) {
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if (board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();
  if(btn) {
    hid_function = ++hid_function % TOTAL_FUNCTIONS;
  }

  switch (hid_function)
  {
  case 0:
    send_hid_mouse_report();
    break;
  case 1:
    hid_keyboard_task();
    break;
  default:
    break;
  }
}

// Tarefa para piscar o LED
void led_blinking_task(void) {
  static uint32_t start_ms = 0;
  static bool led_state = false;

  if (!blink_interval_ms) return;
  if (board_millis() - start_ms < blink_interval_ms) return;

  start_ms += blink_interval_ms;
  board_led_write(led_state);
  led_state = !led_state;
}

// Callback para receber um relatório HID do host (opcional)
void tud_hid_set_report_cb(
  uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
  uint8_t const* buffer, uint16_t bufsize
) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}

// Callback para enviar um relatório HID ao host (opcional)
uint16_t tud_hid_get_report_cb(
  uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
  uint8_t* buffer, uint16_t reqlen
) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  // Sem processamento por enquanto
  return 0; 
}