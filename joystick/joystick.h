// GPIO para eixo X
#define JOYSTICK_X_PIN 27 
// GPIO para eixo Y
#define JOYSTICK_Y_PIN 26  
// GPIO para botão do Joystick
#define JOYSTICK_BUTTON 22

// Valor em repouso do eixo x
#define JOYSTICK_MIDDLE_X 2118
// Valor em repouso do eixo y
#define JOYSTICK_MIDDLE_Y 1997

void setup_joystick() {
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);  

    gpio_init(JOYSTICK_BUTTON);
    gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON); 
}

uint16_t read_Y() {
    adc_select_input(0); 
    return adc_read();
}

uint16_t read_X() {
    adc_select_input(1); 
    return adc_read();
}
