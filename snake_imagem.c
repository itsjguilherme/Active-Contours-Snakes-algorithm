/******************************************************************************
 *  Aplicação: Segmentação de contornos com Snake (Active Contour Model)
 *
 *  Descrição geral:
 *  Este programa carrega uma imagem PGM em tons de cinza, calcula um campo de
 *  forças baseado em bordas e evolui uma curva fechada (snake) até que ela se
 *  ajuste aos contornos da imagem. Ao final, o programa salva:
 *    1) uma imagem PGM com o contorno sobreposto;
 *    2) um arquivo TXT com as coordenadas finais da snake.
 *
 *  Contexto de desenvolvimento:
 *  Trabalho acadêmico da disciplina: Sistemas Embarcados
 *
 *  Autores / estudantes:
 *  - Daniel Leandro Campos Silva
 *  - João Guilherme Santos de Sousa
 *
 *  Data:
 *  30/04/2026
 *
 *  Plataforma-alvo: ???? Daniel oq a gnt coloca?
 *  - Sistema operacional: [PREENCHER: Windows / Linux / macOS] ????
 *  - Compilador: GCC / Clang / equivalente compatível com C99 ???
 *
 *  Entrada:
 *  - Arquivo PGM (formato ASCII P2) com imagem em tons de cinza.
 *
 *  Saída:
 *  - Arquivo PGM com a snake desenhada sobre a imagem original.
 *  - Arquivo TXT com as coordenadas (x, y) dos pontos da snake.
 *
 *  Como usar:
 *    Compilação:
 *      gcc snake.c -o snake -lm
 *
 *    Execução:
 *      ./snake
 *
 *    Ajuda:
 *      ./snake --help
 *      ./snake -h
 *
 *  Permissões de uso / Copyright:
 *  Copyright (c) 2026 por Daniel Leandro Campos Silva e João Guilherme Santos de Sousa,
 *  Instituto Federal de Educação, Ciência e Tecnologia do Ceará - IFCE
 *  All rights reserved.
 *  Uso acadêmico permitido para fins de estudo, avaliação e demonstração.
 *  Qualquer redistribuição ou uso fora do contexto acadêmico deve respeitar
 *  as regras de copyright definidas pela instituição e pelos autores.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* Constante matemática PI usada na inicialização da snake circular. */
#define PI 3.14159265358979323846

/* Número de pontos que compõem a snake. Quanto maior, mais detalhada a curva. */
#define N_POINTS 250

/* Limites máximos suportados para a imagem de entrada. */
#define MAX_WIDTH 90
#define MAX_HEIGHT 90

/* Nome fixo do arquivo de entrada e dos arquivos de saída. */
#define INPUT_PGM "imagem_olho.pgm"
#define OUTPUT_PGM "resultado.pgm"
#define OUTPUT_TXT "coordenadas_cerebro.txt"

/* ---------------------------------------------------------------------------
 * Variáveis globais
 * ---------------------------------------------------------------------------
 * Essas variáveis armazenam o estado da imagem e dos mapas de força usados
 * pelo algoritmo.
 *
 * img_width:
 *   Largura efetiva da imagem carregada.
 *
 * img_height:
 *   Altura efetiva da imagem carregada.
 *
 * max_color_val:
 *   Valor máximo de intensidade declarado no PGM (normalmente 255).
 *
 * image_data:
 *   Matriz com os pixels da imagem original em tons de cinza.
 *
 * map_fx:
 *   Componente horizontal do campo de forças externas.
 *
 * map_fy:
 *   Componente vertical do campo de forças externas.
 *
 * magnitude:
 *   Magnitude do gradiente em cada ponto da imagem.
 * ------------------------------------------------------------------------- */
int img_width, img_height, max_color_val;
unsigned char image_data[MAX_HEIGHT][MAX_WIDTH];
double map_fx[MAX_HEIGHT][MAX_WIDTH];
double map_fy[MAX_HEIGHT][MAX_WIDTH];
double magnitude[MAX_HEIGHT][MAX_WIDTH];

/**
 * @brief Mostra instruções de uso da aplicação.
 *
 * Esta função imprime na saída padrão informações sobre como compilar,
 * executar e interpretar o programa.
 *
 * @param program_name Nome executável do programa, geralmente argv[0].
 *
 * @return Nenhum retorno.
 *
 * Variáveis globais afetadas:
 *   Nenhuma.
 */
void print_help(const char *program_name) {
    printf("Uso: %s [opções]\n", program_name);
    printf("\n");
    printf("Opções:\n");
    printf("  -h, --help    Mostra esta mensagem de ajuda e encerra.\n");
    printf("\n");
    printf("Entrada:\n");
    printf("  %s (imagem PGM ASCII no formato P2)\n", INPUT_PGM);
    printf("\n");
    printf("Saídas:\n");
    printf("  %s   -> imagem com a snake desenhada\n", OUTPUT_PGM);
    printf("  %s -> coordenadas finais da snake\n", OUTPUT_TXT);
    printf("\n");
    printf("Compilação:\n");
    printf("  gcc snake.c -o snake -lm\n");
}

/**
 * @brief Carrega uma imagem PGM ASCII (P2) para a memória.
 *
 * A função lê o cabeçalho do arquivo PGM, valida o formato e copia os pixels
 * para a matriz global image_data. Também atualiza as variáveis globais de
 * dimensões da imagem.
 *
 * @param filename Caminho do arquivo PGM a ser carregado.
 *
 * @return 1 se a leitura for bem-sucedida; 0 em caso de erro.
 *
 * Variáveis globais afetadas:
 *   img_width, img_height, max_color_val, image_data.
 */
int load_pgm(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char magic[3];
    fscanf(fp, "%2s", magic);

    /* Validação do formato: o programa aceita apenas P2 (PGM ASCII). */
    if (magic[0] != 'P' || magic[1] != '2') {
        fclose(fp);
        return 0;
    }

    /*
     * O bloco abaixo ignora comentários e espaços em branco após o cabeçalho.
     * O formato PGM permite linhas iniciadas por '#' como comentário.
     */
    int c = getc(fp);
    while (c == '#' || c == '\n' || c == '\r' || c == ' ' || c == '\t') {
        if (c == '#') {
            while (getc(fp) != '\n'); /* pula a linha inteira do comentário */
        }
        c = getc(fp);
    }
    ungetc(c, fp);

    /* Leitura das dimensões e do valor máximo de intensidade. */
    fscanf(fp, "%d %d %d", &img_width, &img_height, &max_color_val);

    /* Verifica se a imagem ultrapassa o limite suportado pelo programa. */
    if (img_width > MAX_WIDTH || img_height > MAX_HEIGHT) {
        printf("[!] Erro: imagem maior que o limite suportado (%dx%d).\n",
               MAX_WIDTH, MAX_HEIGHT);
        fclose(fp);
        return 0;
    }

    /*
     * Leitura dos pixels.
     * Cada valor lido é convertido para unsigned char e armazenado na matriz.
     */
    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            int pixel_val;
            fscanf(fp, "%d", &pixel_val);
            image_data[y][x] = (unsigned char)pixel_val;
        }
    }

    fclose(fp);
    return 1;
}

/**
 * @brief Salva a imagem original com a snake desenhada sobre ela.
 *
 * A função cria uma cópia da imagem carregada, desenha os pontos da snake em
 * branco e grava o resultado em um novo arquivo PGM ASCII.
 *
 * @param filename Caminho do arquivo de saída PGM.
 * @param snake_x Vetor com as coordenadas X dos pontos da snake.
 * @param snake_y Vetor com as coordenadas Y dos pontos da snake.
 *
 * @return Nenhum retorno.
 *
 * Variáveis globais afetadas:
 *   Nenhuma escrita direta; lê img_width, img_height e image_data.
 */
void save_pgm_result(const char *filename, double *snake_x, double *snake_y) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[!] Erro: não foi possível criar '%s'.\n", filename);
        return;
    }

    /* O arquivo de saída também será ASCII (P2). */
    fprintf(fp, "P2\n%d %d\n255\n", img_width, img_height);

    /* Cópia da imagem original para uma matriz auxiliar de saída. */
    unsigned char saida[MAX_HEIGHT][MAX_WIDTH];

    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            saida[y][x] = image_data[y][x];
        }
    }

    /*
     * Desenha os pontos da snake em branco (255).
     * Também pinta os vizinhos imediatos para aumentar a visibilidade.
     */
    for (int i = 0; i < N_POINTS; i++) {
        int px = (int)snake_x[i];
        int py = (int)snake_y[i];

        if (px >= 0 && px < img_width && py >= 0 && py < img_height) {
            saida[py][px] = 255;
            if (px + 1 < img_width)  saida[py][px + 1] = 255;
            if (px - 1 >= 0)         saida[py][px - 1] = 255;
            if (py + 1 < img_height) saida[py + 1][px] = 255;
            if (py - 1 >= 0)         saida[py - 1][px] = 255;
        }
    }

    /* Grava a matriz final no arquivo de saída. */
    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            fprintf(fp, "%d ", saida[y][x]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}

/**
 * @brief Calcula o campo de forças externas baseado nas bordas da imagem.
 *
 * A função estima o gradiente da imagem com uma aproximação tipo Sobel e, em
 * seguida, calcula a magnitude do gradiente. Depois disso, estima o vetor de
 * força externa como o gradiente da magnitude.
 *
 * @return Nenhum retorno.
 *
 * Variáveis globais afetadas:
 *   magnitude, map_fx, map_fy.
 *
 * Variáveis globais lidas:
 *   image_data, img_width, img_height.
 */
void create_edge_forces(void) {
    /* Calcula a magnitude do gradiente para cada pixel interno da imagem. */
    for (int y = 1; y < img_height - 1; y++) {
        for (int x = 1; x < img_width - 1; x++) {
            int p1 = image_data[y - 1][x - 1];
            int p2 = image_data[y - 1][x];
            int p3 = image_data[y - 1][x + 1];
            int p4 = image_data[y][x - 1];
            int p6 = image_data[y][x + 1];
            int p7 = image_data[y + 1][x - 1];
            int p8 = image_data[y + 1][x];
            int p9 = image_data[y + 1][x + 1];

            /* Aproximação do gradiente em X e Y. */
            double gx = -p1 + p3 - 2 * p4 + 2 * p6 - p7 + p9;
            double gy = -p1 - 2 * p2 - p3 + p7 + 2 * p8 + p9;

            magnitude[y][x] = sqrt(gx * gx + gy * gy);
        }
    }

    /*
     * A força externa é obtida a partir da variação da magnitude do gradiente.
     * Isso direciona a snake para regiões de borda.
     */
    for (int y = 1; y < img_height - 1; y++) {
        for (int x = 1; x < img_width - 1; x++) {
            map_fx[y][x] = magnitude[y][x + 1] - magnitude[y][x - 1];
            map_fy[y][x] = magnitude[y + 1][x] - magnitude[y - 1][x];
        }
    }
}

/**
 * @brief Executa a evolução da snake por um número fixo de iterações.
 *
 * A curva é atualizada combinando:
 *   - forças internas: elasticidade e rigidez;
 *   - forças externas: atração às bordas da imagem.
 *
 * @param x Vetor de coordenadas X dos pontos da snake.
 * @param y Vetor de coordenadas Y dos pontos da snake.
 * @param alpha Peso da elasticidade da snake.
 * @param beta Peso da rigidez/curvatura da snake.
 * @param gamma Fator de passo da atualização iterativa.
 * @param w_ext Peso aplicado às forças externas.
 * @param n_iters Número de iterações da otimização.
 *
 * @return Nenhum retorno.
 *
 * Variáveis globais afetadas:
 *   Nenhuma diretamente.
 *
 * Variáveis globais lidas:
 *   img_width, img_height, map_fx, map_fy.
 *
 * Variáveis modificadas indiretamente:
 *   Os vetores x e y são alterados in-place.
 */
void iterate_snake(double *x, double *y, double alpha, double beta, double gamma,
                   double w_ext, int n_iters) {
    double new_x[N_POINTS], new_y[N_POINTS];

    for (int iter = 0; iter < n_iters; iter++) {
        for (int i = 0; i < N_POINTS; i++) {
            /*
             * Índices vizinhos com comportamento circular:
             * a snake é fechada, então o primeiro ponto é vizinho do último.
             */
            int p2 = (i - 2 + N_POINTS) % N_POINTS;
            int p1 = (i - 1 + N_POINTS) % N_POINTS;
            int n1 = (i + 1) % N_POINTS;
            int n2 = (i + 2) % N_POINTS;

            /*
             * Força interna:
             * - termo de primeira ordem (elasticidade)
             * - termo de segunda ordem (rigidez)
             */
            double f_int_x = alpha * (x[p1] + x[n1] - 2 * x[i]) -
                             beta * (x[p2] - 4 * x[p1] + 6 * x[i] - 4 * x[n1] + x[n2]);

            double f_int_y = alpha * (y[p1] + y[n1] - 2 * y[i]) -
                             beta * (y[p2] - 4 * y[p1] + 6 * y[i] - 4 * y[n1] + y[n2]);

            /* Converte a posição atual para índices inteiros da imagem. */
            int ix = (int)x[i];
            int iy = (int)y[i];

            double f_ext_x = 0.0;
            double f_ext_y = 0.0;

            /*
             * Apenas aplica força externa se o ponto estiver dentro dos limites
             * válidos da imagem.
             */
            if (ix > 0 && ix < img_width - 1 && iy > 0 && iy < img_height - 1) {
                f_ext_x = map_fx[iy][ix] * w_ext;
                f_ext_y = map_fy[iy][ix] * w_ext;
            }

            /* Atualização da posição do ponto na snake. */
            new_x[i] = x[i] + gamma * (f_int_x + f_ext_x);
            new_y[i] = y[i] + gamma * (f_int_y + f_ext_y);
        }

        /* Copia o novo estado para continuar a iteração. */
        for (int i = 0; i < N_POINTS; i++) {
            x[i] = new_x[i];
            y[i] = new_y[i];
        }
    }
}

/**
 * @brief Função principal do programa.
 *
 * Fluxo executado:
 *   1) verifica opção de ajuda;
 *   2) carrega a imagem PGM;
 *   3) calcula o campo de forças;
 *   4) inicializa a snake em círculo;
 *   5) executa as iterações;
 *   6) salva o resultado em PGM e TXT.
 *
 * @param argc Quantidade de argumentos recebidos na linha de comando.
 * @param argv Vetor com os argumentos da linha de comando.
 *
 * @return 0 em caso de sucesso; valor diferente de zero em caso de erro.
 *
 * Variáveis globais afetadas:
 *   img_width, img_height, max_color_val, image_data,
 *   map_fx, map_fy, magnitude.
 */
int main(int argc, char *argv[]) {
    /* Exibe ajuda caso o usuário peça. */
    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help(argv[0]);
        return 0;
    }

    /* Carrega a imagem de entrada. */
    if (!load_pgm(INPUT_PGM)) {
        printf("[!] Erro ao carregar a imagem '%s'.\n", INPUT_PGM);
        return 1;
    }

    printf("[+] Imagem carregada: %dx%d\n", img_width, img_height);

    /* Gera os mapas de força baseados nas bordas da imagem. */
    create_edge_forces();

    /* Vetores que guardam as coordenadas da snake. */
    double x[N_POINTS], y[N_POINTS];

    /*
     * Inicialização da snake:
     * uma circunferência centralizada na imagem, com raio proporcional
     * ao menor lado.
     */
    double centro_x = img_width / 2.0;
    double centro_y = img_height / 2.0;
    double raio = (img_width < img_height ? img_width : img_height) * 0.25;

    for (int i = 0; i < N_POINTS; i++) {
        double angulo = ((double)i / N_POINTS) * (2.0 * PI);
        x[i] = centro_x + raio * cos(angulo);
        y[i] = centro_y + raio * sin(angulo);
    }

    printf("[+] Executando snake...\n");

    /*
     * Parâmetros do modelo:
     * alpha = elasticidade
     * beta  = rigidez
     * gamma = passo de atualização
     * w_ext = peso da força externa
     */
    iterate_snake(x, y, 0.7, 0.9, 0.1, 0.001, 100000);

    /* Salva a imagem final com o contorno desenhado. */
    save_pgm_result(OUTPUT_PGM, x, y);

    /* Salva as coordenadas finais em arquivo texto. */
    FILE *fp = fopen(OUTPUT_TXT, "w");
    if (!fp) {
        printf("[!] Erro: não foi possível criar '%s'.\n", OUTPUT_TXT);
        return 1;
    }

    for (int i = 0; i < N_POINTS; i++) {
        fprintf(fp, "%f, %f\n", x[i], y[i]);
    }

    fclose(fp);

    printf("[+] Coordenadas salvas em '%s'.\n", OUTPUT_TXT);
    printf("[+] Finalizado com sucesso.\n");

    return 0;
}