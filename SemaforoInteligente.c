/*
 *  Autor: Daniel Porto Braz
 *
 *  Este projeto apresenta um semáforo inteligente que opera através dos 
 *  conceitos de FreeRTOS e tasks.
*/


#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "ws2818b.pio.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdio.h>

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define ledG 11
#define ledB 12
#define ledR 13

#define botaoA 5

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

// Definição de pixel GRB
struct pixel_t {
    uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {
    
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    
    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }
    
    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    
    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

ssd1306_t ssd;                                                // Inicializa a estrutura do display

// Buzzer
const uint8_t BUZZER_PIN = 21;
const uint16_t PERIOD = 59609; // WRAP
const float DIVCLK = 16.0; // Divisor inteiro
static uint slice_21;
const uint16_t dc_values[] = {PERIOD * 0.3, 0}; // Duty Cycle de 30% e 0%

void setup_pwm(){

    // PWM do BUZZER
    // Configura para soar 440 Hz
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_21 = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_clkdiv(slice_21, DIVCLK);
    pwm_set_wrap(slice_21, PERIOD);
    pwm_set_gpio_level(BUZZER_PIN, 0);
    pwm_set_enabled(slice_21, true);
}



// >>>>>>>>> TAREFAS <<<<<<<<<

static volatile bool night_mode = false; // Modo de funcionamento. (Normal/Noturno)

// Handles para gerenciar a prioridade e execução das tarefas
TaskHandle_t xHandleNightModeTask = NULL;
TaskHandle_t xHandleBlinkLedsTask = NULL;
TaskHandle_t xHandleBuzzerTask = NULL;
TaskHandle_t xHandleMatrixLedsTask = NULL;
TaskHandle_t xHandleDisplayTask = NULL;

// Tarefa para ler o botão A
void vReadButtonTask(void *parameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Inicia somente as tarefas do modo normal
    vTaskSuspend(xHandleNightModeTask);
    vTaskResume(xHandleBlinkLedsTask);
    vTaskResume(xHandleBuzzerTask);
    vTaskResume(xHandleMatrixLedsTask);
    vTaskResume(xHandleDisplayTask);

    bool last_mode = night_mode; // Armazena o último modo que o semáforo estava

    while (true){
        // Leitura do botão com debounce
        if (!gpio_get(botaoA)){
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); 
            if (!gpio_get(botaoA))
                night_mode = !night_mode; 
        }

        // Se o modo mudou, reconfigura as tarefas
        if (night_mode != last_mode){
            if (night_mode){
                vTaskSuspend(xHandleBlinkLedsTask);
                vTaskSuspend(xHandleBuzzerTask);
                vTaskSuspend(xHandleMatrixLedsTask);
                vTaskSuspend(xHandleDisplayTask);
                vTaskResume(xHandleNightModeTask);
            } else {
                vTaskSuspend(xHandleNightModeTask);
                vTaskResume(xHandleBlinkLedsTask);
                vTaskResume(xHandleBuzzerTask);
                vTaskResume(xHandleMatrixLedsTask);
                vTaskResume(xHandleDisplayTask);
            }

            last_mode = night_mode;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}


// Tarefa para executar a rotina do modo noturno
void vNightModeTask(void *parameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(true){

        // Desliga o display, a matriz, o buzzer e os LEDs
        ssd1306_fill(&ssd, false); 
        ssd1306_send_data(&ssd);
        npClear();
        npWrite();
        pwm_set_gpio_level(BUZZER_PIN, dc_values[1]);
        gpio_put(ledR, false);
        gpio_put(ledG, false);

        // Pisca o LED na cor amarela e apita o buzzer por 2 segundos
        gpio_put(ledR, true);
        gpio_put(ledG, true);
        npWrite();
        pwm_set_gpio_level(BUZZER_PIN, dc_values[0]);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
        gpio_put(ledR, false);
        gpio_put(ledG, false);
        npClear();
        pwm_set_gpio_level(BUZZER_PIN, dc_values[1]);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500)); 
    }
}

// Tarefa que pisca os leds na sequência do semáforo p/ pedestres
void vBlinkLedsTask(void* parameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true)
    {
        // Sinal Verde 
        gpio_put(ledG, true);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(3000));

        // Sinal Amarelo
        gpio_put(ledR, true);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
        gpio_put(ledG, false);

        // Sinal Vermelho
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
        gpio_put(ledR, false);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

// Tarefa para apitar o buzzer durante o modo Normal
void vBuzzerTask(void *parameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(true){
        // Apito de 1 segundo para indicar sinal verde p/ pedestre
        pwm_set_gpio_level(BUZZER_PIN, dc_values[0]);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
        pwm_set_gpio_level(BUZZER_PIN, dc_values[1]);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));

        // Apito intermmitente por 1 segundo para indicar sinal amarelo p/ pedestre
        for (int i = 0; i < 5; i++){
            pwm_set_gpio_level(BUZZER_PIN, dc_values[0]);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
            pwm_set_gpio_level(BUZZER_PIN, dc_values[1]);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
        }

        // Apito de 0.5 segundo para indicar sinal vermelho p/ pedestre
        pwm_set_gpio_level(BUZZER_PIN, dc_values[0]);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
        pwm_set_gpio_level(BUZZER_PIN, dc_values[1]);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); // Pequeno atraso para evitar dessincronização com o display
    }
}

// Tarefa para exibir o semáforo na matriz de LEDs
void vMatrixLedsTask(void *parameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Sequência: vermelho -> verde -> amarelo
    while(true){
        npSetLED(17, 1.0, 0.0, 0.0); // Pixel 17 como cor vermelha do semáforo
        npWrite();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(4000));
        npClear();

        npSetLED(7, 0.0, 1.0, 0.0); // Pixel 7 como cor verde do semáforo
        npWrite();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
        npClear();
        
        npSetLED(12, 1.0, 1.0, 0.0); // Pixel 12 como cor amarela do semáforo
        npWrite();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
        npClear();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); // Pequeno atraso para evitar dessincronização com o display
    }
}

// Tarefa para exibir a mensagem p/ pedestre no display
void vDisplayTask(void *parameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();

    char msg_verde[] = "Pode atravessar";
    char msg_amarelo[] = "Atencao";
    char msg_vermelho[] = "Pare";
    char apagador[] = "                 "; // Apaga o espaço de mensagens do display baseado na mensagem de maior tamanho possível

    bool cor = true;

    // Sequência: verde -> amarelo -> vermelho
    while (true)
    {
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo

        // Escreve a mensagem para o sinal verde
        ssd1306_draw_string(&ssd, msg_verde, 4, 32); 
        ssd1306_send_data(&ssd);                           
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(3000));
        ssd1306_draw_string(&ssd, apagador, 4, 32); // Apaga a mensagem 
        ssd1306_send_data(&ssd);       

        // Escreve a mensagem para o sinal amarelo
        ssd1306_draw_string(&ssd, msg_amarelo, 30, 32); 
        ssd1306_send_data(&ssd);                           
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
        ssd1306_draw_string(&ssd, apagador, 4, 32);
        ssd1306_send_data(&ssd);   
        
        // Escreve a mensagem para o sinal vermelho
        ssd1306_draw_string(&ssd, msg_vermelho, 30, 32); 
        ssd1306_send_data(&ssd);                          
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
        ssd1306_draw_string(&ssd, apagador, 4, 32);
        ssd1306_send_data(&ssd);       
    }
}


int main()
{
    stdio_init_all();

    gpio_init(botaoA);
    gpio_set_dir(botaoA, GPIO_IN);
    gpio_pull_up(botaoA);

    gpio_init(ledG);
    gpio_set_dir(ledG, GPIO_OUT);

    gpio_init(ledR);
    gpio_set_dir(ledR, GPIO_OUT);

    // Inicializa matriz de LEDs NeoPixel.
    npInit(LED_PIN);
    npClear();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    setup_pwm(); // Configura o PWM do Buzzer

    xTaskCreate(vReadButtonTask, "Read Button Task", configMINIMAL_STACK_SIZE,
        NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vNightModeTask, "Night Mode Task", configMINIMAL_STACK_SIZE,
        NULL, tskIDLE_PRIORITY, &xHandleNightModeTask);
    xTaskCreate(vBlinkLedsTask, "Leds Task", configMINIMAL_STACK_SIZE,
        NULL, tskIDLE_PRIORITY, &xHandleBlinkLedsTask);
    xTaskCreate(vBuzzerTask, "Buzzer Task", configMINIMAL_STACK_SIZE,
        NULL, tskIDLE_PRIORITY, &xHandleBuzzerTask);
    xTaskCreate(vMatrixLedsTask, "Matrix LEDs Task", configMINIMAL_STACK_SIZE,
        NULL, tskIDLE_PRIORITY, &xHandleMatrixLedsTask);    
    xTaskCreate(vDisplayTask, "Display Task", configMINIMAL_STACK_SIZE,
        NULL, tskIDLE_PRIORITY, &xHandleDisplayTask);
    vTaskStartScheduler();
    panic_unsupported();
}