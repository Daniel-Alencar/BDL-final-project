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
#include "display/ssd1306.c"

// Intervalo de envio
#define HID_INTERVAL_MS 100

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


#define TOTAL_FUNCTIONS 3
// 0: Mouse
// 1: Teclado
// 3: Controle
uint hid_function = 0;
uint last_hid_function = 0;

// Armazena o tempo do último evento (em microssegundos)
static volatile uint32_t last_time = 0;

uint8_t mouse_actions = 0;

uint keyboard_character = HID_KEY_A - 1;
uint8_t keycode[6] = {0};
uint8_t keycode_count = 0;


char convertHIDKeyToASCII(uint HID_key) {
  if(HID_key >= HID_KEY_A && HID_key <= HID_KEY_Z)
    return HID_key + 'A' - HID_KEY_A;
}


void gpio_irq_handler(uint gpio, uint32_t events) {
  // Obtém o tempo atual em microssegundos
  uint32_t current_time = to_us_since_boot(get_absolute_time());

  // Verifica se passou tempo suficiente desde o último evento
  // 200 ms de debouncing
  if (current_time - last_time > 500000) {
    // Atualiza o tempo do último evento
    last_time = current_time;

    if (gpio == JOYSTICK_BUTTON) {
      hid_function = hid_function + 1 == TOTAL_FUNCTIONS ? 0 : hid_function + 1;
    }

    if(hid_function == 0) {

      if(gpio == BUTTON_A) {
        mouse_actions |= MOUSE_BUTTON_RIGHT; 
      } else if (gpio == BUTTON_B) {
        mouse_actions |= MOUSE_BUTTON_LEFT;
      }

    } else if(hid_function == 1) {

      if(gpio == BUTTON_A) {
        if(keyboard_character > HID_KEY_A); {
          keyboard_character--;
          keycode[0] = keyboard_character;
        }
      } else if (gpio == BUTTON_B) {
        if(keyboard_character < HID_KEY_Z) {
          keyboard_character++;
          keycode[0] = keyboard_character;
        }
      }
      char character_message[50];
      sprintf(character_message, "CARACTERE %c", convertHIDKeyToASCII(keyboard_character));
      display_draw_string(character_message, 8, 22);
      display_send_data();

    } else if(hid_function == 2) {

      if(gpio == BUTTON_A) {
        keycode[keycode_count++] = HID_KEY_ENTER;
      } else if (gpio == BUTTON_B) {
        keycode[keycode_count++] = HID_KEY_SPACE;
      }
    }
  }
}




int main(void) {
  board_init();

  setup_joystick();
  setup_buttons();
  setup_led_RGB();
  setup_display_oled();

  // Inicializa a pilha USB
  tud_init(BOARD_TUD_RHPORT);

  gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_LEVEL_LOW, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_LEVEL_LOW, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(
    JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler
  );
  print_hid_function("MOUSE");

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
void hid_mouse_task() {

  if (!tud_hid_ready()) return;

  // Lê o ADC
  uint16_t adc_value_y = read_Y();
  uint16_t adc_value_x = read_X();

  // Converte ADC para movimento do cursor
  int8_t delta_x = adc_to_mouse_movement(adc_value_x);
  int8_t delta_y = adc_to_mouse_movement(adc_value_y);

  // Envia o relatório do mouse
  tud_hid_mouse_report(REPORT_ID_MOUSE, mouse_actions, delta_x, -delta_y, 0, 0);
  mouse_actions = 0;
}


void hid_keyboard_task(void) {

  static uint32_t start_ms = 0;
  if (board_millis() - start_ms < HID_INTERVAL_MS) return;
  start_ms = board_millis();

  // Verifica se o HID está pronto
  if (!tud_hid_ready()) return;

  // Lê o ADC
  uint16_t adc_value_y = read_Y();
  if(adc_value_y > ADC_CENTER + ADC_CENTER / 2) {
    uint8_t keycode[6] = {0};
    keycode[0] = HID_KEY_BACKSPACE;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  }

  tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  for(int i = 0; i < 6; i++) {
    keycode[i] = 0;
  }
  keycode_count = 0;
}

void hid_control_task(void) {

  static uint32_t start_ms = 0;
  if (board_millis() - start_ms < HID_INTERVAL_MS) return;
  start_ms = board_millis();

  // Verifica se o HID está pronto
  if (!tud_hid_ready()) return;

  // Lê o ADC
  uint16_t adc_value_y = read_Y();
  uint16_t adc_value_x = read_X();

  if(adc_value_y > ADC_CENTER + ADC_CENTER / 2) {
    uint8_t keycode[6] = {0};
    keycode[0] = HID_KEY_ARROW_UP;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  }
  if(adc_value_y < ADC_CENTER - ADC_CENTER / 2) {
    uint8_t keycode[6] = {0};
    keycode[0] = HID_KEY_ARROW_DOWN;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  }
  if(adc_value_x > ADC_CENTER + ADC_CENTER / 2) {
    uint8_t keycode[6] = {0};
    keycode[0] = HID_KEY_ARROW_RIGHT;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  }
  if(adc_value_x < ADC_CENTER - ADC_CENTER / 2) {
    uint8_t keycode[6] = {0};
    keycode[0] = HID_KEY_ARROW_LEFT;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  }

  tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
  for(int i = 0; i < 6; i++) {
    keycode[i] = 0;
  }
  keycode_count = 0;
}


// Tarefa para envio periódico dos relatórios HID
void hid_task(void) {
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if (board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();
  if(btn) {
    // Obtém o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    // 200 ms de debouncing
    if (current_time - last_time > 200000) {
      // Atualiza o tempo do último evento
      last_time = current_time;
      hid_function = hid_function + 1 == TOTAL_FUNCTIONS ? 0 : hid_function + 1;
    }
  }

  switch (hid_function) {
    case 0:
      if(last_hid_function != hid_function) {
        print_hid_function("MOUSE");
        last_hid_function = hid_function;
      }
      hid_mouse_task();
      break;
    case 1:
      if(last_hid_function != hid_function) {
        print_hid_function("TECLADO");
        last_hid_function = hid_function;
      }
      hid_keyboard_task();
      break;      
    case 2:
      if(last_hid_function != hid_function) {
        print_hid_function("CONTROLE");
        last_hid_function = hid_function;
      }
      hid_control_task();
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