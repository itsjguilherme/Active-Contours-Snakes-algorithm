/******************************************************************************
 *  Aplicação: Segmentação de contornos com Snake (Active Contour Model)
 *
 *  Descrição geral:
 *  Este programa carrega uma imagem PGM em tons de cinza, calcula um campo de
 *  forças baseado em bordas e evolui uma curva fechada (snake) até que ela se
 *  ajuste aos contornos da imagem.
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
 *  Plataforma-alvo: Nucleo STM32F030
 * 
 * gcc snake_imagem.c -o snake_imagem -lm
 * ./snake_imagem imagem_bola.pgm
 * ./snake_imagem imagem_maca.pgm 45 45 50  
 * 
 * posição X=120, Y=80 e um raio estimado de 50 pixels
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846
#define N_POINTS 250
#define MAX_WIDTH 90
#define MAX_HEIGHT 90

/* Removido o INPUT_PGM fixo. Os outputs ainda podem ser fixos ou dinâmicos. */
#define OUTPUT_PGM "resultado.pgm"
#define OUTPUT_TXT "coordenadas.txt"

int img_width, img_height, max_color_val;
unsigned char image_data[MAX_HEIGHT][MAX_WIDTH];
double map_fx[MAX_HEIGHT][MAX_WIDTH];
double map_fy[MAX_HEIGHT][MAX_WIDTH];
double magnitude[MAX_HEIGHT][MAX_WIDTH];

void print_help(const char *program_name) {
    printf("Uso: %s <imagem.pgm> [centro_x] [centro_y] [raio]\n", program_name);
    printf("\n");
    printf("Opções:\n");
    printf("  -h, --help    Mostra esta mensagem de ajuda e encerra.\n");
    printf("\n");
    printf("Argumentos:\n");
    printf("  <imagem.pgm>  Obrigatório. Caminho para a imagem PGM ASCII (P2).\n");
    printf("  [centro_x]    Opcional. Coordenada X inicial do círculo.\n");
    printf("  [centro_y]    Opcional. Coordenada Y inicial do círculo.\n");
    printf("  [raio]        Opcional. Raio inicial do círculo.\n");
    printf("  * Se os opcionais não forem passados, o centro da imagem será usado.\n");
    printf("\n");
    printf("Exemplo de execução para um objeto no centro:\n");
    printf("  %s imagem_olho.pgm\n", program_name);
    printf("\n");
    printf("Exemplo de execução para um objeto no canto (X=100, Y=150, Raio=40):\n");
    printf("  %s imagem_olho.pgm 100 150 40\n", program_name);
}

int load_pgm(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char magic[3];
    fscanf(fp, "%2s", magic);

    if (magic[0] != 'P' || magic[1] != '2') {
        fclose(fp);
        return 0;
    }

    int c = getc(fp);
    while (c == '#' || c == '\n' || c == '\r' || c == ' ' || c == '\t') {
        if (c == '#') {
            while (getc(fp) != '\n'); 
        }
        c = getc(fp);
    }
    ungetc(c, fp);

    fscanf(fp, "%d %d %d", &img_width, &img_height, &max_color_val);

    if (img_width > MAX_WIDTH || img_height > MAX_HEIGHT) {
        printf("[!] Erro: imagem maior que o limite suportado (%dx%d).\n",
               MAX_WIDTH, MAX_HEIGHT);
        fclose(fp);
        return 0;
    }

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

void save_pgm_result(const char *filename, double *snake_x, double *snake_y) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[!] Erro: não foi possível criar '%s'.\n", filename);
        return;
    }

    fprintf(fp, "P2\n%d %d\n255\n", img_width, img_height);
    unsigned char saida[MAX_HEIGHT][MAX_WIDTH];

    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            saida[y][x] = image_data[y][x];
        }
    }

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

    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            fprintf(fp, "%d ", saida[y][x]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}

void create_edge_forces(void) {
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

            double gx = -p1 + p3 - 2 * p4 + 2 * p6 - p7 + p9;
            double gy = -p1 - 2 * p2 - p3 + p7 + 2 * p8 + p9;

            magnitude[y][x] = sqrt(gx * gx + gy * gy);
        }
    }

    for (int y = 1; y < img_height - 1; y++) {
        for (int x = 1; x < img_width - 1; x++) {
            map_fx[y][x] = magnitude[y][x + 1] - magnitude[y][x - 1];
            map_fy[y][x] = magnitude[y + 1][x] - magnitude[y - 1][x];
        }
    }
}

void iterate_snake(double *x, double *y, double alpha, double beta, double gamma,
                   double w_ext, int n_iters) {
    double new_x[N_POINTS], new_y[N_POINTS];

    for (int iter = 0; iter < n_iters; iter++) {
        for (int i = 0; i < N_POINTS; i++) {
            int p2 = (i - 2 + N_POINTS) % N_POINTS;
            int p1 = (i - 1 + N_POINTS) % N_POINTS;
            int n1 = (i + 1) % N_POINTS;
            int n2 = (i + 2) % N_POINTS;

            double f_int_x = alpha * (x[p1] + x[n1] - 2 * x[i]) -
                             beta * (x[p2] - 4 * x[p1] + 6 * x[i] - 4 * x[n1] + x[n2]);

            double f_int_y = alpha * (y[p1] + y[n1] - 2 * y[i]) -
                             beta * (y[p2] - 4 * y[p1] + 6 * y[i] - 4 * y[n1] + y[n2]);

            int ix = (int)x[i];
            int iy = (int)y[i];

            double f_ext_x = 0.0;
            double f_ext_y = 0.0;

            if (ix > 0 && ix < img_width - 1 && iy > 0 && iy < img_height - 1) {
                f_ext_x = map_fx[iy][ix] * w_ext;
                f_ext_y = map_fy[iy][ix] * w_ext;
            }

            new_x[i] = x[i] + gamma * (f_int_x + f_ext_x);
            new_y[i] = y[i] + gamma * (f_int_y + f_ext_y);
        }

        for (int i = 0; i < N_POINTS; i++) {
            x[i] = new_x[i];
            y[i] = new_y[i];
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1 || (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))) {
        print_help(argv[0]);
        return 0;
    }

    const char *input_file = argv[1];

    if (!load_pgm(input_file)) {
        printf("[!] Erro ao carregar a imagem '%s'. Verifique se o arquivo existe e é um PGM ASCII (P2).\n", input_file);
        return 1;
    }

    printf("[+] Imagem carregada: %dx%d\n", img_width, img_height);

    create_edge_forces();

    double x[N_POINTS], y[N_POINTS];

    /* Define os valores padrões (centro da imagem) */
    double centro_x = img_width / 2.0;
    double centro_y = img_height / 2.0;
    double raio = (img_width < img_height ? img_width : img_height) * 0.25;

    /* Sobrescreve com os argumentos do usuário, se fornecidos */
    if (argc >= 5) {
        centro_x = atof(argv[2]);
        centro_y = atof(argv[3]);
        raio = atof(argv[4]);
        printf("[+] Usando inicializacao customizada: Centro(%.1f, %.1f), Raio: %.1f\n", centro_x, centro_y, raio);
    } else {
        printf("[+] Usando inicializacao padrao: Centro(%.1f, %.1f), Raio: %.1f\n", centro_x, centro_y, raio);
    }

    for (int i = 0; i < N_POINTS; i++) {
        double angulo = ((double)i / N_POINTS) * (2.0 * PI);
        x[i] = centro_x + raio * cos(angulo);
        y[i] = centro_y + raio * sin(angulo);
    }

    printf("[+] Executando snake...\n");

    iterate_snake(x, y, 0.7, 0.9, 0.1, 0.001, 100000);

    save_pgm_result(OUTPUT_PGM, x, y);

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
    printf("[+] Imagem final salva em '%s'.\n", OUTPUT_PGM);
    printf("[+] Finalizado com sucesso.\n");

    return 0;
}