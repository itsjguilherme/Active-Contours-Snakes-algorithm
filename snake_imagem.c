#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

// CUMPRINDO O REQUISITO DA ESTRUTURA DE DADOS:
// 500 elementos double = 4000 bytes para X + 4000 bytes para Y = 8000 bytes (~8 KBytes)
#define N_POINTS 500 

// CUMPRINDO O REQUISITO "SEM ALOCAÇÃO DINÂMICA":
// Como não podemos usar malloc, definimos um tamanho máximo estático para a foto
#define MAX_WIDTH 512
#define MAX_HEIGHT 512

// Variáveis Globais (Alocação Estática, zero malloc)
int img_width, img_height, max_color_val;
unsigned char image_data[MAX_HEIGHT][MAX_WIDTH];
double map_fx[MAX_HEIGHT][MAX_WIDTH];
double map_fy[MAX_HEIGHT][MAX_WIDTH];
double magnitude[MAX_HEIGHT][MAX_WIDTH];

/* =========================================================================
 * LEITOR E ESCRITOR DE ARQUIVOS PGM P2 (CUMPRINDO O REQUISITO DE IMAGEM)
 * ========================================================================= */
int load_pgm(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char magic[3];
    fscanf(fp, "%2s", magic);
    if (magic[0] != 'P' || magic[1] != '2') {
        fclose(fp);
        return 0; // Não é um arquivo PGM P2 válido
    }

    // Pula possíveis comentários (linhas começando com #)
    int c = getc(fp);
    while (c == '#' || c == '\n' || c == '\r' || c == ' ') {
        if (c == '#') { while (getc(fp) != '\n'); }
        c = getc(fp);
    }
    ungetc(c, fp);

    // Lê as dimensões e o valor máximo de cor
    fscanf(fp, "%d %d %d", &img_width, &img_height, &max_color_val);

    if (img_width > MAX_WIDTH || img_height > MAX_HEIGHT) {
        printf("[!] Erro: A imagem eh maior que o limite estatico (%dx%d).\n", MAX_WIDTH, MAX_HEIGHT);
        fclose(fp);
        return 0;
    }

    // Lê os pixels (Valores em texto, já que é P2)
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
    
    // Cabeçalho PGM P2
    fprintf(fp, "P2\n");
    fprintf(fp, "%d %d\n", img_width, img_height);
    fprintf(fp, "255\n");

    // Cria uma cópia da imagem na memória para podermos "pintar" o contorno
    unsigned char saida[MAX_HEIGHT][MAX_WIDTH];
    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            saida[y][x] = image_data[y][x];
        }
    }

    // Desenha a Snake (Pintamos os pixels de branco brilhante = 255)
    for (int i = 0; i < N_POINTS; i++) {
        int px = (int)snake_x[i];
        int py = (int)snake_y[i];
        if (px >= 0 && px < img_width && py >= 0 && py < img_height) {
            saida[py][px] = 255;
            // Pinta um pequeno + para ficar visível na imagem PGM
            if(px+1 < img_width) saida[py][px+1] = 255;
            if(px-1 >= 0) saida[py][px-1] = 255;
            if(py+1 < img_height) saida[py+1][px] = 255;
            if(py-1 >= 0) saida[py-1][px] = 255;
        }
    }

    // Grava os pixels no arquivo de texto
    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            fprintf(fp, "%d ", saida[y][x]);
        }
        fprintf(fp, "\n");
    }
    
    fclose(fp);
}

/* =========================================================================
 * PASSO A: CRIAR O MAPA DE PESOS (Reescrito para matriz estática)
 * ========================================================================= */
void create_edge_forces() {
    // 1. Filtro de Sobel (CUMPRINDO REQUISITO: Laços aninhados)
    for (int y = 1; y < img_height - 1; y++) {
        for (int x = 1; x < img_width - 1; x++) {
            int p1 = image_data[y-1][x-1]; int p2 = image_data[y-1][x]; int p3 = image_data[y-1][x+1];
            int p4 = image_data[y][x-1];   /* centro */                 int p6 = image_data[y][x+1];
            int p7 = image_data[y+1][x-1]; int p8 = image_data[y+1][x]; int p9 = image_data[y+1][x+1];

            double gx = -p1 + p3 - 2*p4 + 2*p6 - p7 + p9;
            double gy = -p1 - 2*p2 - p3 + p7 + 2*p8 + p9;
            
            magnitude[y][x] = sqrt(gx*gx + gy*gy);
        }
    }

    // 2. Gradiente do Mapa de Bordas
    for (int y = 1; y < img_height - 1; y++) {
        for (int x = 1; x < img_width - 1; x++) {
            map_fx[y][x] = magnitude[y][x+1] - magnitude[y][x-1];
            map_fy[y][x] = magnitude[y+1][x] - magnitude[y-1][x];
        }
    }
}

/* =========================================================================
 * PASSO B: O MOTOR DA SNAKE
 * ========================================================================= */
void iterate_snake(double *x, double *y, double alpha, double beta, double gamma, double w_ext, int n_iters) {
    double new_x[N_POINTS], new_y[N_POINTS];

    for (int iter = 0; iter < n_iters; iter++) {
        for (int i = 0; i < N_POINTS; i++) {
            int p2 = (i - 2 + N_POINTS) % N_POINTS; int p1 = (i - 1 + N_POINTS) % N_POINTS;
            int n1 = (i + 1) % N_POINTS;            int n2 = (i + 2) % N_POINTS;

            double f_int_x = alpha * (x[p1] + x[n1] - 2*x[i]) - beta * (x[p2] - 4*x[p1] + 6*x[i] - 4*x[n1] + x[n2]);
            double f_int_y = alpha * (y[p1] + y[n1] - 2*y[i]) - beta * (y[p2] - 4*y[p1] + 6*y[i] - 4*y[n1] + y[n2]);

            int ix = (int)x[i]; int iy = (int)y[i];
            double f_ext_x = 0, f_ext_y = 0;
            
            if (ix > 0 && ix < img_width-1 && iy > 0 && iy < img_height-1) {
                f_ext_x = map_fx[iy][ix] * w_ext;
                f_ext_y = map_fy[iy][ix] * w_ext;
            }

            new_x[i] = x[i] + gamma * (f_int_x + f_ext_x);
            new_y[i] = y[i] + gamma * (f_int_y + f_ext_y);
        }

        for (int i = 0; i < N_POINTS; i++) { x[i] = new_x[i]; y[i] = new_y[i]; }
    }
}

/* =========================================================================
 * FUNÇÃO PRINCIPAL
 * ========================================================================= */
int main() {
    // 1. CARREGAR A IMAGEM (Agora lendo o formato acadêmico PGM)
    if (!load_pgm("Imagens_pgm/cerebro_imagem.pgm")) {
        printf("[!] Erro: Nao foi possivel carregar 'cerebro_imagem.pgm'.\n");
        printf("    Certifique-se de que a imagem foi convertida para PGM P2 (ASCII).\n");
        return 1;
    }
    printf("[+] Imagem PGM carregada com sucesso: %dx%d pixels.\n", img_width, img_height);

    // 2. CRIAR MAPA DE FORÇAS
    printf("[+] Calculando forcas da imagem...\n");
    create_edge_forces();

    // 3. INICIALIZAR A SNAKE (Estrutura de ~8 KBytes)
    double x[N_POINTS], y[N_POINTS];
    
    double centro_x = img_width / 2.0;
    double centro_y = img_height / 2.0;
    double raio_x = img_width * 0.31; 
    
    double raio_y_baixo = img_height * 0.42; 
    double raio_y_topo = img_height * 0.30;  
    
    for (int i = 0; i < N_POINTS; i++) {
        double angulo = ((double)i / N_POINTS) * (2.0 * PI); 
        x[i] = centro_x + raio_x * cos(angulo);
        
        if (sin(angulo) < 0) {
            y[i] = centro_y + raio_y_topo * sin(angulo); 
        } else {
            y[i] = centro_y + raio_y_baixo * sin(angulo); 
        }
    }
    
    // 4. RODAR O ALGORITMO
    printf("[+] Deformando a Snake para achar o cerebro...\n");
    iterate_snake(x, y, 0.7, 0.99, 0.1, 0.003, 10000);

    // 5. SALVAR ARQUIVO PGM DE SAÍDA E CSV
    save_pgm_result("resultado.pgm", x, y);
    printf("[+] Imagem final salva como 'resultado.pgm'.\n");

    FILE *fp = fopen("coordenadas_cerebro.csv", "w");
    fprintf(fp, "x,y\n");
    for (int i = 0; i < N_POINTS; i++) fprintf(fp, "%f,%f\n", x[i], y[i]);
    fclose(fp);
    printf("[+] Coordenadas salvas em 'coordenadas_cerebro.csv'.\n");

    return 0; // Fim do programa, sem memory leaks pois não há alocação dinâmica!
}