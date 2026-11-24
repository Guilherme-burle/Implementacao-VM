#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>

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

typedef struct {
    const EntradaTLB *entrada;
    int pagina;
    int encontrou;
} DadosBusca;

int algoritmo_valido(const char *algoritmo) {
    return strcmp(algoritmo, "fifo") == 0 ||
           strcmp(algoritmo, "lru") == 0;
}

int converter_endereco(const char *linha, uint32_t *endereco) {
    while (isspace((unsigned char)*linha)) {
        linha++;
    }

    if (!isdigit((unsigned char)*linha)) {
        return 0;
    }

    char *fim;

    errno = 0;
    unsigned long long numero = strtoull(linha, &fim, 10);

    if (errno == ERANGE || numero > UINT32_MAX) {
        return 0;
    }

    while (isspace((unsigned char)*fim)) {
        fim++;
    }

    if (*fim != '\0') {
        return 0;
    }

    *endereco = (uint32_t)numero;

    return 1;
}

void *buscar_entrada(void *arg) {
    DadosBusca *dados = arg;

    dados->encontrou = (dados->entrada->pagina == dados->pagina);

    return NULL;
}

int buscar_tlb(EntradaTLB tlb[], int pagina,
               unsigned long relogio) {
    pthread_t threads[TAM_TLB];
    DadosBusca dados[TAM_TLB];

    int threads_criadas = 0;

    for (int i = 0; i < TAM_TLB; i++) {
        dados[i].entrada = &tlb[i];
        dados[i].pagina = pagina;
        dados[i].encontrou = 0;

        if (pthread_create(&threads[i], NULL,
                           buscar_entrada, &dados[i]) != 0) {
            break;
        }

        threads_criadas++;
    }

    for (int i = 0; i < threads_criadas; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Erro ao aguardar thread.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (threads_criadas != TAM_TLB) {
        return -2;
    }

    for (int i = 0; i < TAM_TLB; i++) {
        if (dados[i].encontrou) {
            tlb[i].ultimo_uso = relogio;
            return i;
        }
    }

    return -1;
}

int inserir_tlb(EntradaTLB tlb[], int pagina, int frame,
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

    return posicao;
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
        fprintf(stderr,
                "Uso: %s <arquivo> <algoritmo_pagina> <algoritmo_tlb>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (!algoritmo_valido(argv[2])) {
        fprintf(stderr, "Algoritmo de paginas invalido: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    if (!algoritmo_valido(argv[3])) {
        fprintf(stderr, "Algoritmo da TLB invalido: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    FILE *arquivo = fopen(argv[1], "r");

    if (arquivo == NULL) {
        perror("Erro ao abrir arquivo de enderecos");
        return EXIT_FAILURE;
    }

    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    if (backing_store == NULL) {
        perror("Erro ao abrir BACKING_STORE.bin");
        fclose(arquivo);
        return EXIT_FAILURE;
    }

    if (fseek(backing_store, 0, SEEK_END) != 0 ||
        ftell(backing_store) != NUM_PAGINAS * TAM_PAGINA ||
        fseek(backing_store, 0, SEEK_SET) != 0) {

        fprintf(stderr, "BACKING_STORE.bin invalido.\n");

        fclose(arquivo);
        fclose(backing_store);

        return EXIT_FAILURE;
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
    int status = EXIT_SUCCESS;

    unsigned long total_acessos = 0;
    unsigned long page_faults = 0;
    unsigned long tlb_hits = 0;
    unsigned long relogio = 0;
    unsigned long numero_linha = 0;

    char *linha = NULL;
    size_t capacidade = 0;
    ssize_t tamanho;

    while ((tamanho = getline(&linha, &capacidade, arquivo)) != -1) {
        uint32_t endereco;

        numero_linha++;

        if (memchr(linha, '\0', (size_t)tamanho) != NULL ||
            !converter_endereco(linha, &endereco)) {

            fprintf(stderr, "Endereco invalido na linha %lu.\n",
                    numero_linha);

            status = EXIT_FAILURE;
            break;
        }

        uint32_t endereco_16 = endereco & 0xFFFF;

        int pagina = (endereco_16 >> 8) & 0xFF;
        int offset = endereco_16 & 0xFF;

        relogio++;
        total_acessos++;

        int posicao_tlb = buscar_tlb(tlb, pagina, relogio);
        int frame;

        if (posicao_tlb == -2) {
            fprintf(stderr, "Erro ao criar threads para busca na TLB.\n");
            status = EXIT_FAILURE;
            break;
        }

        if (posicao_tlb != -1) {
            frame = tlb[posicao_tlb].frame;
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
                }

                if (fseek(backing_store, pagina * TAM_PAGINA, SEEK_SET) != 0 ||
                    fread(memoria_fisica[frame], sizeof(unsigned char),
                          TAM_PAGINA, backing_store) != TAM_PAGINA) {

                    fprintf(stderr, "Erro ao carregar pagina %d.\n", pagina);

                    status = EXIT_FAILURE;
                    break;
                }

                tabela_paginas[pagina] = frame;
                frame_para_pagina[frame] = pagina;
            }

            posicao_tlb = inserir_tlb(tlb, pagina, frame,
                                      relogio, tlb_lru);
        }

        tempo_uso[frame] = relogio;

        int endereco_fisico = frame * TAM_PAGINA + offset;
        signed char valor = (signed char)memoria_fisica[frame][offset];

        printf("Virtual address: %" PRIu32
               " TLB: %d Physical address: %d Value: %d\n",
               endereco, posicao_tlb, endereco_fisico, valor);
    }

    if (status == EXIT_SUCCESS) {
        if (ferror(arquivo) || !feof(arquivo)) {
            fprintf(stderr, "Erro durante a leitura dos enderecos.\n");
            status = EXIT_FAILURE;
        } else if (total_acessos == 0) {
            fprintf(stderr, "Arquivo de enderecos vazio.\n");
            status = EXIT_FAILURE;
        }
    }

    if (status == EXIT_SUCCESS) {
        double taxa_page_faults = (double)page_faults / total_acessos;
        double taxa_tlb_hits = (double)tlb_hits / total_acessos;

        printf("Number of Translated Addresses = %lu\n", total_acessos);
        printf("Page Faults = %lu\n", page_faults);
        printf("Page Fault Rate = %.3f\n", taxa_page_faults);
        printf("TLB Hits = %lu\n", tlb_hits);
        printf("TLB Hit Rate = %.3f", taxa_tlb_hits);
    }

    if (fflush(stdout) == EOF || ferror(stdout)) {
        fprintf(stderr, "Erro ao escrever a saida.\n");
        status = EXIT_FAILURE;
    }

    free(linha);
    fclose(arquivo);
    fclose(backing_store);

    return status;
}