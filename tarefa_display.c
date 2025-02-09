#include <stdio.h>  
#include <stdlib.h>
#include "pico/stdlib.h"

#include <math.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "hardware/timer.h"

#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

// Arquivo que contém o programa PIO
#include "pio_matrix.pio.h"

// Arquivo que contém os desenhos dos números
#include "inc/desenho.h"

// Definição de constantes
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define NUM_PIXELS 25 // Número de LEDs na matriz
#define btA 5        // Pino do botão A
#define btB 6        // Pino do botão B
#define Matrix_LED 7 // Pino de controle da matriz de LEDs
#define led_g 11 //LED verde
#define led_b 12 //LED azul

// Variáveis globais
static volatile uint32_t verde = 0;    // Estado LED verde
static volatile uint32_t azul = 0;     // Estado LED azul    
static volatile uint32_t last_time = 0;  // Armazena o tempo do último evento (em microssegundos)
static volatile uint intensidade = 50; // Armazena a intensidade da matriz de LEDs
ssd1306_t ssd; // Inicializa a estrutura do display
bool cor = true; 
double r = 0.0, b = 0.0, g = 0.0;
PIO pio = pio0;  // Seleciona o primeiro bloco de PIO
bool ok; // verificação de clock
uint16_t i; //variavel auxiliar
uint32_t valor_led; // cor dos LEDs
uint sm; // maquina de estado da PIO

//Prototupação da função de interrupção 
static void gpio_irq_handler(uint gpio, uint32_t events);

// Função para definir a intensidade das cores do LED
uint32_t matrix_rgb(double b, double r, double g) {
  unsigned char R, G, B;
  R = r * intensidade;
  G = g * intensidade;
  B = b * intensidade;
  return (G << 24) | (R << 16) | (B << 8); // Retorna a cor em formato de 32 bits
}
// Função que escreve na matriz de LEDs usando PIO
void desenho_pio(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b) {
  for (int16_t i = 0; i < NUM_PIXELS; i++) {
    if(azul == 1){
      valor_led = matrix_rgb(desenho[24 - i], r=0.0, g=0.0); // Define a cor do LED
      pio_sm_put_blocking(pio, sm, valor_led); // Envia o valor para a PIO
    }else if(verde == 1){
      valor_led = matrix_rgb(b=0.0, r=0.0, desenho[24 - i]); // Define a cor do LED
      pio_sm_put_blocking(pio, sm, valor_led); // Envia o valor para a PIO
    }else{
      valor_led = matrix_rgb(b, desenho[24 - i], g=0.0); // Define a cor do LED
      pio_sm_put_blocking(pio, sm, valor_led); // Envia o valor para a PIO
    }
  }
}

int main()
{
  // Inicializa a biblioteca padrão

  // Configura a frequência de clock para 128 MHz
  ok = set_sys_clock_khz(128000, false);

  //Inicializa a comunicação via UART
  stdio_init_all();

  if (ok) printf("clock set to %ld\n", clock_get_hz(clk_sys));

  // Configuração da PIO para controle da matriz de LEDs
  uint offset = pio_add_program(pio, &pio_matrix_program); // Carrega o programa PIO
  sm = pio_claim_unused_sm(pio, true);               // Adquire uma máquina de estado
  pio_matrix_program_init(pio, sm, offset, Matrix_LED);   // Inicializa a PIO

  // Inicialização do barramento I2C
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  
  //Inicializa o display OLED
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  // Inicialização do botão btA
  gpio_init(btA);
  gpio_set_dir(btA, GPIO_IN);
  gpio_pull_up(btA);

  // Inicialização do botão btB
  gpio_init(btB);
  gpio_set_dir(btB, GPIO_IN);
  gpio_pull_up(btB);

  // Inicialização do LED verde
  gpio_init(led_g);
  gpio_set_dir(led_g, GPIO_OUT);

  // Inicialização do LED azul
  gpio_init(led_b);
  gpio_set_dir(led_b, GPIO_OUT);

  // Configuração das interrupções das GPIOs
  gpio_set_irq_enabled_with_callback(btA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled(btB, GPIO_IRQ_EDGE_FALL, true);
  
  // Mensagem inicial
  printf("RP2040 inicializado. Digite algum Caractere ou Numero.\n");

  //desliga a matriz LEDs 5x5
  desenho_pio(nums[10], valor_led, pio, sm, r, g, b);

  // Atualiza o conteúdo do display com animações
  ssd1306_fill(&ssd, !cor); // Limpa o display
  ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
  ssd1306_draw_string(&ssd, "UART - RP2040", 15, 5); // Desenha uma string
  ssd1306_draw_string(&ssd, "G:Off  B:Off", 15, 20); // Desenha uma string
  ssd1306_draw_string(&ssd, "Digite Algum", 15, 35); // Desenha uma string  
  ssd1306_draw_string(&ssd, "Caractere:", 15, 50); // Desenha uma string  
  ssd1306_send_data(&ssd); // Atualiza o display

  char buffer[25];
  char c;
  while (true) 
  {
    if (stdio_usb_connected()) {
      // USB conectado
      if(scanf("%c", &c) == 1){
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
          snprintf(buffer, sizeof(buffer), "Inserido: %c", c);

          ssd1306_fill(&ssd, !cor); // Limpa o display
          ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
          ssd1306_draw_string(&ssd, "UART - RP2040", 15, 5); // Desenha uma string
          if(azul == 1){
            ssd1306_draw_string(&ssd, "G:Off  B:On", 15, 20); // Desenha uma string
          }else if(verde == 1){
            ssd1306_draw_string(&ssd, "G:On  B:Off", 15, 20); // Desenha uma string
          }else{
            ssd1306_draw_string(&ssd, "G:Off  B:Off", 15, 20); // Desenha uma string
          }
          ssd1306_draw_string(&ssd, "Caractere", 30, 35); // Desenha uma string
          ssd1306_draw_string(&ssd, buffer, 20, 50); // Desenha uma string
          ssd1306_send_data(&ssd); // Atualiza o display

          //desliga a matriz LEDs 5x5
          desenho_pio(nums[10], valor_led, pio, sm, r, g, b);

          printf("Caractere foi inserido: %c\n", c);
          printf("Digite outro Caractere ou Numero:\n");
       }
       else if (c >= '0' && c <= '9'){
        snprintf(buffer, sizeof(buffer), "Inserido: %c", c);

        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        ssd1306_draw_string(&ssd, "UART - RP2040", 15, 5); // Desenha uma string
        if(azul == 1){
          ssd1306_draw_string(&ssd, "G:Off  B:On", 15, 20); // Desenha uma string
        }else if(verde == 1){
          ssd1306_draw_string(&ssd, "G:On  B:Off", 15, 20); // Desenha uma string
        }else{
          ssd1306_draw_string(&ssd, "G:Off  B:Off", 15, 20); // Desenha uma string
        }
        ssd1306_draw_string(&ssd, "Numero", 34, 35); // Desenha uma string
        ssd1306_draw_string(&ssd, buffer, 20, 50); // Desenha uma string
        ssd1306_send_data(&ssd); // Atualiza o display

        //Condição para ligar a matriz LEDs 5x5
        switch (c) {
          case '0':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[0], valor_led, pio, sm, r, g, b);
              break;
          case '1':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[1], valor_led, pio, sm, r, g, b);
              break;
          case '2':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[2], valor_led, pio, sm, r, g, b);
              break;
          case '3':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[3], valor_led, pio, sm, r, g, b);
              break;
          case '4':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[4], valor_led, pio, sm, r, g, b);
              break;
          case '5':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[5], valor_led, pio, sm, r, g, b);
              break;
          case '6':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[6], valor_led, pio, sm, r, g, b);
              break;
          case '7':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[7], valor_led, pio, sm, r, g, b);
              break;
          case '8':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[8], valor_led, pio, sm, r, g, b);
              break;
          case '9':
            printf("Número Inserido: %c\n", c);
            desenho_pio(nums[9], valor_led, pio, sm, r, g, b);
              break;
          }
          printf("Digite outro Caractere ou Numero:\n");
        }
        
      }
    }
    sleep_ms(40);
  }
}

// Rotina da interrupção dos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
  uint32_t current_time = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos

  // Implementação de debounce: verifica se passaram pelo menos 200ms desde a última interrupção
  if (current_time - last_time > 200000) {
      last_time = current_time;
      
      // Se o botão pressionado foi o btA, incrementa 'a' até 9
      if (gpio == btA) {
        gpio_put(led_g, true);
        gpio_put(led_b, false);

        printf("LED Verde Ligado\n");
        printf("Digite outro Caractere ou Numero:\n");

        azul = 0;
        verde = 1;

        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        ssd1306_draw_string(&ssd, "UART - RP2040", 15, 5); // Desenha uma string
        ssd1306_draw_string(&ssd, "G:On  B:Off", 15, 20); // Desenha uma string
        ssd1306_draw_string(&ssd, "Digite Algum", 15, 35); // Desenha uma string  
        ssd1306_draw_string(&ssd, "Caractere:", 15, 50); // Desenha uma string  
        ssd1306_send_data(&ssd); // Atualiza o display

        desenho_pio(nums[11], valor_led, pio, sm, r, g, b);

      }
      // Se o botão pressionado foi o btB, decrementa 'a' até 0
      else if (gpio == btB) {
        gpio_put(led_b, true);
        gpio_put(led_g, false);

        azul = 1;
        verde = 0;

        printf("LED Azul Ligado\n");
        printf("Digite outro Caractere ou Numero:\n");

        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        ssd1306_draw_string(&ssd, "UART - RP2040", 15, 5); // Desenha uma string
        ssd1306_draw_string(&ssd, "G:Off  B:On", 15, 20); // Desenha uma string
        ssd1306_draw_string(&ssd, "Digite Algum", 15, 35); // Desenha uma string  
        ssd1306_draw_string(&ssd, "Caractere:", 15, 50); // Desenha uma string  
        ssd1306_send_data(&ssd); // Atualiza o display

        desenho_pio(nums[12], valor_led, pio, sm, r, g, b);
      }
  }
}