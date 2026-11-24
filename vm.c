#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define TAM_PAGINA 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 128

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <arquivo> <algoritmo_pagina> <algoritmo_tlb>\n",
               argv[0]);
        return 1;
    }

    FILE *arquivo = fopen(argv[1], "r");
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    if (arquivo == NULL || backing_store == NULL) {
        printf("Erro ao abrir arquivo de entrada ou BACKING_STORE.bin.\n");

        if (arquivo != NULL) {
            fclose(arquivo);
        }

        if (backing_store != NULL) {
            fclose(backing_store);
        }

        return 1;
    }

    int algoritmo_lru = (strcmp(argv[2], "lru") == 0);

    int tabela_paginas[NUM_PAGINAS];
    int frame_para_pagina[NUM_FRAMES];

    unsigned char memoria_fisica[NUM_FRAMES][TAM_PAGINA];
    unsigned long tempo_uso[NUM_FRAMES];

    for (int i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_para_pagina[i] = -1;
        tempo_uso[i] = 0;
    }

    int frames_utilizados = 0;
    int proxima_vitima = 0;
    int page_faults = 0;
    int substituicoes = 0;
    int total_acessos = 0;

    unsigned long relogio = 0;
    char linha[256];

    printf("Endereco logico | Pagina | Offset | Frame | Endereco fisico | Valor\n");

    while (fgets(linha, sizeof(linha), arquivo) != NULL) {
        char *fim;
        unsigned long numero = strtoul(linha, &fim, 10);

        if (fim == linha) {
            fprintf(stderr, "Erro ao interpretar endereco: %s\n", linha);
            fclose(arquivo);
            fclose(backing_store);
            return 1;
        }

        uint32_t endereco = (uint32_t)numero;
        uint32_t endereco_16 = endereco & 0xFFFF;

        int pagina = (endereco_16 >> 8) & 0xFF;
        int offset = endereco_16 & 0xFF;

        relogio++;
        total_acessos++;

        if (tabela_paginas[pagina] == -1) {
            int frame;

            page_faults++;

            if (frames_utilizados < NUM_FRAMES) {
                frame = frames_utilizados;
                frames_utilizados++;
            } else {
                if (algoritmo_lru) {
                    frame = 0;

                    for (int i = 1; i < NUM_FRAMES; i++) {
                        if (tempo_uso[i] < tempo_uso[frame]) {
                            frame = i;
                        }
                    }
                } else {
                    frame = proxima_vitima;
                    proxima_vitima = (proxima_vitima + 1) % NUM_FRAMES;
                }

                int pagina_antiga = frame_para_pagina[frame];
                tabela_paginas[pagina_antiga] = -1;

                substituicoes++;
            }

            if (fseek(backing_store, pagina * TAM_PAGINA, SEEK_SET) != 0 ||
                fread(memoria_fisica[frame], sizeof(unsigned char),
                      TAM_PAGINA, backing_store) != TAM_PAGINA) {

                fprintf(stderr, "Erro ao carregar pagina %d.\n", pagina);

                fclose(arquivo);
                fclose(backing_store);
                return 1;
            }

            tabela_paginas[pagina] = frame;
            frame_para_pagina[frame] = pagina;
        }

        int frame = tabela_paginas[pagina];

        tempo_uso[frame] = relogio;

        int endereco_fisico = frame * TAM_PAGINA + offset;
        signed char valor = (signed char)memoria_fisica[frame][offset];

        printf("%15u | %6d | %6d | %5d | %15d | %5d\n",
               endereco, pagina, offset, frame, endereco_fisico, valor);
    }

    if (ferror(arquivo) || total_acessos == 0) {
        fprintf(stderr, "Erro de leitura ou arquivo sem enderecos.\n");
        fclose(arquivo);
        fclose(backing_store);
        return 1;
    }

    printf("\nTotal de acessos: %d\n", total_acessos);
    printf("Page faults: %d\n", page_faults);
    printf("Frames utilizados: %d\n", frames_utilizados);
    printf("Substituicoes (%s): %d\n",
           algoritmo_lru ? "LRU" : "FIFO", substituicoes);

    fclose(arquivo);
    fclose(backing_store);

    return 0;
}