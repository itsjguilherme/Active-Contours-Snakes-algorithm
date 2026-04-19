#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define PI 3.14159265358979323846

// Estas duas linhas ativam a biblioteca que baixamos para ler e escrever PNGs
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define N_POINTS 200 // Quantidade de pontinhos no nosso elástico vermelho

// Estrutura para guardar os mapas de força da imagem
typedef struct {
    double *fx;
    double *fy;
    int width;
    int height;
} ForceMap;

/* =========================================================================
 * PASSO A: CRIAR O MAPA DE PESOS (A imagem "Weight" index_05.png)
 * ========================================================================= */
ForceMap create_edge_forces(unsigned char *image_data, int width, int height) {
    ForceMap map;
    map.width = width;
    map.height = height;
    map.fx = (double *)calloc(width * height, sizeof(double));
    map.fy = (double *)calloc(width * height, sizeof(double));
    
    double *magnitude = (double *)calloc(width * height, sizeof(double));

    // 1. Filtro de Sobel (Acha as bordas brancas da ressonância)
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            // Pega os vizinhos para ver se há mudança de cor
            int p1 = image_data[(y-1)*width + (x-1)]; int p2 = image_data[(y-1)*width + x]; int p3 = image_data[(y-1)*width + (x+1)];
            int p4 = image_data[y*width + (x-1)];     /* centro */                            int p6 = image_data[y*width + (x+1)];
            int p7 = image_data[(y+1)*width + (x-1)]; int p8 = image_data[(y+1)*width + x]; int p9 = image_data[(y+1)*width + (x+1)];

            double gx = -p1 + p3 - 2*p4 + 2*p6 - p7 + p9;
            double gy = -p1 - 2*p2 - p3 + p7 + 2*p8 + p9;
            
            // Intensidade da borda
            magnitude[y*width + x] = sqrt(gx*gx + gy*gy);
        }
    }

    // 2. Gradiente do Mapa de Bordas (Cria os vetores que puxam a Snake)
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            // fx e fy apontam para onde a borda é mais forte
            map.fx[y*width + x] = magnitude[y*width + (x+1)] - magnitude[y*width + (x-1)];
            map.fy[y*width + x] = magnitude[(y+1)*width + x] - magnitude[(y-1)*width + x];
        }
    }

    free(magnitude);
    return map;
}

/* =========================================================================
 * PASSO B: O MOTOR DA SNAKE (O elástico encolhendo, index_07.png)
 * ========================================================================= */
void iterate_snake(double *x, double *y, ForceMap map, double alpha, double beta, double gamma, double w_ext, int n_iters) {
    double new_x[N_POINTS], new_y[N_POINTS];

    for (int iter = 0; iter < n_iters; iter++) {
        for (int i = 0; i < N_POINTS; i++) {
            int p2 = (i - 2 + N_POINTS) % N_POINTS; int p1 = (i - 1 + N_POINTS) % N_POINTS;
            int n1 = (i + 1) % N_POINTS;            int n2 = (i + 2) % N_POINTS;

            // Força Interna (Mantém o elástico suave e conectado)
            double f_int_x = alpha * (x[p1] + x[n1] - 2*x[i]) - beta * (x[p2] - 4*x[p1] + 6*x[i] - 4*x[n1] + x[n2]);
            double f_int_y = alpha * (y[p1] + y[n1] - 2*y[i]) - beta * (y[p2] - 4*y[p1] + 6*y[i] - 4*y[n1] + y[n2]);

            // Força Externa (Lê o "ímã" da imagem na posição atual do nó)
            int ix = (int)x[i]; int iy = (int)y[i];
            double f_ext_x = 0, f_ext_y = 0;
            
            // Verifica se o ponto não vazou da imagem
            if (ix > 0 && ix < map.width-1 && iy > 0 && iy < map.height-1) {
                f_ext_x = map.fx[iy * map.width + ix] * w_ext;
                f_ext_y = map.fy[iy * map.width + ix] * w_ext;
            }

            // Atualiza a posição
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
    int width, height, channels;
    
    // 1. CARREGAR A IMAGEM
    // Certifique-se de que a sua ressonância original se chama "cerebro_imagem.png" na mesma pasta
    unsigned char *img = stbi_load("cerebro_imagem.png", &width, &height, &channels, 1);
    if (img == NULL) {
        printf("[!] Erro ao carregar cerebro_imagem.png. O arquivo esta na pasta?\n");
        return 1;
    }
    printf("[+] Imagem carregada: %dx%d pixels.\n", width, height);

    // 2. CRIAR MAPA DE FORÇAS (O equivalente ao index_05.png)
    printf("[+] Calculando forcas da imagem (Bordas)...\n");
    ForceMap map = create_edge_forces(img, width, height);

    // 3. INICIALIZAR A SNAKE (Formato híbrido: Oval achatado no topo)
    double x[N_POINTS], y[N_POINTS];
    
    double centro_x = width / 2.0;
    double centro_y = height / 2.0;
    
    double raio_x = width * 0.31; 
    
    // Criamos dois raios diferentes para o eixo Y
    double raio_y_baixo = height * 0.42; // Raio normal para a base do cérebro
    double raio_y_topo = height * 0.30;  // Raio encurtado para achatar o topo
    
    for (int i = 0; i < N_POINTS; i++) {
        double angulo = ((double)i / N_POINTS) * (2.0 * PI); 
        x[i] = centro_x + raio_x * cos(angulo);
        
        // Se o seno for negativo, estamos desenhando a metade de CIMA da imagem
        if (sin(angulo) < 0) {
            y[i] = centro_y + raio_y_topo * sin(angulo); // Aplica o achatamento
        } 
        // Senão, estamos desenhando a metade de BAIXO
        else {
            y[i] = centro_y + raio_y_baixo * sin(angulo); // Mantém ovalado
        }
    }
    
    // 4. RODAR O ALGORITMO
    printf("[+] Deformando a Snake para achar o cerebro...\n");
    // iterate_snake(x, y, map, alpha, beta, gamma, w_ext, iterações);
    iterate_snake(x, y, map, 0.7, 0.99, 0.1, 0.003, 100000);

    // 5. SALVAR O ARQUIVO CSV
    FILE *fp = fopen("coordenadas_cerebro.csv", "w");
    fprintf(fp, "x,y\n");
    for (int i = 0; i < N_POINTS; i++) fprintf(fp, "%f,%f\n", x[i], y[i]);
    fclose(fp);
    printf("[+] Coordenadas salvas em 'coordenadas_cerebro.csv'.\n");

    // 6. DESENHAR O RESULTADO (O equivalente ao index_07.png)
    // Recarrega a imagem em modo colorido (RGB) para podermos pintar de vermelho
    unsigned char *color_img = stbi_load("cerebro_imagem.png", &width, &height, &channels, 3);
    for (int i = 0; i < N_POINTS; i++) {
        int px = (int)x[i]; int py = (int)y[i];
        if (px >= 0 && px < width && py >= 0 && py < height) {
            // Pinta um pequeno quadrado vermelho ao redor do ponto para ficar visivel
            for(int dy=-1; dy<=1; dy++) {
                for(int dx=-1; dx<=1; dx++) {
                    int c_px = px+dx; int c_py = py+dy;
                    if(c_px >= 0 && c_px < width && c_py >= 0 && c_py < height) {
                        color_img[(c_py*width + c_px)*3 + 0] = 255; // R (Vermelho no maximo)
                        color_img[(c_py*width + c_px)*3 + 1] = 0;   // G
                        color_img[(c_py*width + c_px)*3 + 2] = 0;   // B
                    }
                }
            }
        }
    }

    // Salva a nova imagem PNG
    stbi_write_png("resultado.png", width, height, 3, color_img, width * 3);
    printf("[+] Imagem final salva como 'resultado.png'. Abra para ver a magica!\n");

    // Limpar memoria
    free(img); free(color_img); free(map.fx); free(map.fy);
    return 0;
}