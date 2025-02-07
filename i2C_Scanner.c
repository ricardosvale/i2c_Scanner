#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306_i2c.h"

// Declaração de funções
void ssd1306_init(void);
void calculate_render_area_buffer_length(struct render_area *area);
void ssd1306_draw_string(uint8_t *buffer, int x, int y, const char *str);
void render_on_display(uint8_t *buffer, struct render_area *area);
bool ler_i2c(char *mensagem);

// Definição dos pinos
#define I2C_PORT i2c0
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
const uint BUTTON_PIN = 6;
#define LT_CLK 9
#define LT_SDA 8

// Função para inicializar o I2C
void init_i2c() {
    i2c_init(I2C_PORT, 100 * 1000);  // Inicializa I2C a 100 kHz
    gpio_set_function(LT_SDA, GPIO_FUNC_I2C);  // Configura o pino SDA
    gpio_set_function(LT_CLK, GPIO_FUNC_I2C);  // Configura o pino SCL
    gpio_pull_up(LT_SDA);  // Ativa pull-up no pino SDA
    gpio_pull_up(LT_CLK);  // Ativa pull-up no pino SCL
}

// Função para buscar dispositivos I2C no barramento
void scan_i2c_bus() {
    printf("Procurando dispositivos I2C...\n");

    // Testa os endereços I2C de 0x08 até 0x7F (endereço válido para I2C)
    for (uint8_t address = 0x08; address <= 0x7F; address++) {
        uint8_t dummy_data = 0;
        int result = i2c_write_blocking(I2C_PORT, address, &dummy_data, 1, true);  // true: flag stop

        if (result == PICO_ERROR_GENERIC) {
            // Se falhar (sem resposta), o dispositivo não está presente nesse endereço
            continue;
        }

        // Se a escrita for bem-sucedida, o dispositivo está presente no endereço
        printf("Dispositivo encontrado no endereço 0x%02X\n", address);
    }
}

bool verificar_dispositivos_i2c() {
    // Realiza a varredura do barramento I2C
    for (uint8_t address = 0x08; address <= 0x7F; address++) {
        uint8_t dummy_data = 0;
        int result = i2c_write_blocking(I2C_PORT, address, &dummy_data, 1, true);
        
        if (result != PICO_ERROR_GENERIC) {
            // Se houver um dispositivo respondendo, exibe o endereço
            printf("Dispositivo encontrado no endereço 0x%02X\n", address);
            return true;  // Retorna true assim que um dispositivo for encontrado
        }
    }
    // Nenhum dispositivo respondeu, retorna false
    printf("Nenhum dispositivo encontrado\n");
    return false;
}


// Função para ler os pinos de CLK e SDA
bool ler_i2c(char *mensagem) {
    // Verifica se há comunicação I2C funcionando
    bool i2c_conectado = verificar_dispositivos_i2c();

    // Se não houver dispositivos I2C, retornamos "Resultado inválido"
    if (!i2c_conectado) {
        snprintf(mensagem, 52, "I2C ERROR");
        return false;  // Retorna falso para indicar erro
    }

    // Lê os pinos de CLK e SDA
    bool clk_status = gpio_get(LT_CLK);
    bool sda_status = gpio_get(LT_SDA);

    // Formata a string para mostrar no OLED
    snprintf(mensagem, 32, "CLK: %d SDA: %d", clk_status, sda_status);

    // Retorna true se a leitura for válida (qualquer um dos pinos estiver ativo)
    return clk_status || sda_status;
}

int main() {
    stdio_init_all();

    // Inicializa o i2c oled 
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o OLED
    ssd1306_init();

    // Configuração do botão
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Inicializa o I2C
    init_i2c();

    // Definir área de renderização
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // Buffer do display
    uint8_t ssd[ssd1306_buffer_length];

    // Realiza a varredura inicial do barramento I2C
    scan_i2c_bus();

    while (true) {
        // Exibir mensagem inicial
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string(ssd, 18, 10, "Scanner RFFE:");
        ssd1306_draw_string(ssd, 8, 25, "Aperte o botao");
        ssd1306_draw_string(ssd, 15, 45, "para iniciar");
        render_on_display(ssd, &frame_area);

        // Aguarda o botão ser pressionado para iniciar a leitura
        while (gpio_get(BUTTON_PIN)) {
            sleep_ms(10);
        }

        // Pequeno delay para debounce
        sleep_ms(50);

        // Exibir mensagem "Iniciando leitura..."
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string(ssd, 25, 20, "Iniciando");
        ssd1306_draw_string(ssd, 35, 35, "leitura!");
        render_on_display(ssd, &frame_area);
        sleep_ms(600); // Pequeno atraso para visualização

        // Realiza a leitura do RFFE (obtenção de status dos pinos)
        char resultado[32];
        bool sucesso = ler_i2c(resultado);

        // Atualiza a tela com o resultado
        memset(ssd, 0, ssd1306_buffer_length);
        if (sucesso) {
            ssd1306_draw_string(ssd, 20, 10, "Leitura bem");
            ssd1306_draw_string(ssd, 30, 25, "sucedida");
            ssd1306_draw_string(ssd, 10, 45, resultado);
        } else {
            ssd1306_draw_string(ssd, 30, 10, "Resultado");
            ssd1306_draw_string(ssd, 32, 25, "invalido");
            ssd1306_draw_string(ssd, 30, 45, resultado);
        }
        render_on_display(ssd, &frame_area);

        // Aguarda o botão ser pressionado para retornar à tela inicial
        while (gpio_get(BUTTON_PIN)) {
            sleep_ms(100);
        }

        // Pequeno delay para debounce antes de voltar ao loop
        sleep_ms(500);
    }

    return 0;
}
