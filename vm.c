#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define TAM_PAGINA 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 128
#define TAM_TLB 16

typedef struct {
    int pagina;
    int frame;
    unsigned long entrada;
    unsigned long ultimo_uso;
} EntradaTLB;

int buscar_tlb(EntradaTLB tlb[], int pagina,
               unsigned long relogio) {
    for (int i = 0; i < TAM_TLB; i++) {
        if (tlb[i].pagina == pagina) {
            tlb[i].ultimo_uso = relogio;
            return tlb[i].frame;
        }
    }

    return -1;
}

void inserir_tlb(EntradaTLB tlb[], int pagina, int frame,
                 unsigned long relogio, int tlb_lru) {
    int posicao = -1;

    for (int i = 0; i < TAM_TLB; i++) {
        if (tlb[i].pagina == -1) {
            posicao = i;
            break;
        }
    }

    if (posicao == -1) {
        posicao = 0;

        for (int i = 1; i < TAM_TLB; i++) {
            if (tlb_lru) {
                if (tlb[i].ultimo_uso < tlb[posicao].ultimo_uso) {
                    posicao = i;
                }
            } else {
                if (tlb[i].entrada < tlb[posicao].entrada) {
                    posicao = i;
                }
            }
        }
    }

    tlb[posicao].pagina = pagina;
    tlb[posicao].frame = frame;
    tlb[posicao].entrada = relogio;
    tlb[posicao].ultimo_uso = relogio;
}

void invalidar_tlb(EntradaTLB tlb[], int pagina) {
    for (int i = 0; i < TAM_TLB; i++) {
        if (tlb[i].pagina == pagina) {
            tlb[i].pagina = -1;
            tlb[i].frame = -1;
            tlb[i].entrada = 0;
            tlb[i].ultimo_uso = 0;
        }
    }
}

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
    int tlb_lru = (strcmp(argv[3], "lru") == 0);

    int tabela_paginas[NUM_PAGINAS];
    int frame_para_pagina[NUM_FRAMES];

    unsigned char memoria_fisica[NUM_FRAMES][TAM_PAGINA];
    unsigned long tempo_uso[NUM_FRAMES];

    EntradaTLB tlb[TAM_TLB];

    for (int i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_para_pagina[i] = -1;
        tempo_uso[i] = 0;
    }

    for (int i = 0; i < TAM_TLB; i++) {
        tlb[i].pagina = -1;
        tlb[i].frame = -1;
        tlb[i].entrada = 0;
        tlb[i].ultimo_uso = 0;
    }

    int frames_utilizados = 0;
    int proxima_vitima = 0;
    int page_faults = 0;
    int substituicoes = 0;
    int total_acessos = 0;
    int tlb_hits = 0;

    unsigned long relogio = 0;
    char linha[256];

    printf("Endereco logico | Pagina | Offset | Frame | Endereco fisico | Valor | TLB\n");

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

        int frame = buscar_tlb(tlb, pagina, relogio);
        int tlb_hit = (frame != -1);

        if (tlb_hit) {
            tlb_hits++;
        } else {
            frame = tabela_paginas[pagina];

            if (frame == -1) {
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
                    invalidar_tlb(tlb, pagina_antiga);

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

            inserir_tlb(tlb, pagina, frame, relogio, tlb_lru);
        }

        tempo_uso[frame] = relogio;

        int endereco_fisico = frame * TAM_PAGINA + offset;
        signed char valor = (signed char)memoria_fisica[frame][offset];

        printf("%15u | %6d | %6d | %5d | %15d | %5d | %s\n",
               endereco, pagina, offset, frame, endereco_fisico,
               valor, tlb_hit ? "HIT" : "MISS");
    }

    if (ferror(arquivo) || total_acessos == 0) {
        fprintf(stderr, "Erro de leitura ou arquivo sem enderecos.\n");
        fclose(arquivo);
        fclose(backing_store);
        return 1;
    }

    double taxa_page_faults = (double)page_faults / total_acessos;
    double taxa_tlb_hits = (double)tlb_hits / total_acessos;

    printf("\nTotal de acessos: %d\n", total_acessos);
    printf("Page faults: %d\n", page_faults);
    printf("Frames utilizados: %d\n", frames_utilizados);
    printf("Substituicoes de paginas (%s): %d\n",
           algoritmo_lru ? "LRU" : "FIFO", substituicoes);
    printf("Page fault rate: %.3f\n", taxa_page_faults);
    printf("Algoritmo da TLB: %s\n", tlb_lru ? "LRU" : "FIFO");
    printf("TLB hits: %d\n", tlb_hits);
    printf("TLB hit rate: %.3f\n", taxa_tlb_hits);

    fclose(arquivo);
    fclose(backing_store);

    return 0;
}