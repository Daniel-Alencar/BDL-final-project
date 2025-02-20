
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12

void setup_led_RGB() {
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
}

void set_red(bool value) {
    gpio_put(LED_RED, value);
}

void set_green(bool value) {
    gpio_put(LED_GREEN, value);
}

void set_blue(bool value) {
    gpio_put(LED_BLUE, value);
}
