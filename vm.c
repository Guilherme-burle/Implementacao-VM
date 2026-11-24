#include <stdio.h>
#include <stdint.h>

#define TAM_PAGINA 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 128

int main(int argc, char *argv[]) {
    (void)argc;

    FILE *arquivo = fopen(argv[1], "r");
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    if (arquivo == NULL || backing_store == NULL) {
        printf("Erro ao abrir arquivo de entrada ou BACKING_STORE.bin.\n");

        if (arquivo != NULL)
            fclose(arquivo);

        if (backing_store != NULL)
            fclose(backing_store);

        return 1;
    }

    int tabela_paginas[NUM_PAGINAS];
    int frame_para_pagina[NUM_FRAMES];

    unsigned char memoria_fisica[NUM_FRAMES][TAM_PAGINA];

    for (int i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_para_pagina[i] = -1;
    }

    int frames_utilizados = 0;
    int proxima_vitima = 0;
    int page_faults = 0;
    int substituicoes = 0;

    uint32_t endereco;

    printf("Endereco Logico | Pagina | Offset | Frame | Endereco Fisico | Valor\n");
    printf("---------------------------------------------------------------------\n");

    while (fscanf(arquivo, "%u", &endereco) == 1) {
        uint32_t endereco_16 = endereco & 0xFFFF;

        int pagina = (endereco_16 >> 8) & 0xFF;
        int offset = endereco_16 & 0xFF;

        if (tabela_paginas[pagina] == -1) {
            int frame;

            page_faults++;

            if (frames_utilizados < NUM_FRAMES) {
                frame = frames_utilizados;
                frames_utilizados++;
            } else {
                frame = proxima_vitima;

                int pagina_antiga = frame_para_pagina[frame];

                tabela_paginas[pagina_antiga] = -1;

                substituicoes++;

                proxima_vitima = (proxima_vitima + 1) % NUM_FRAMES;
            }

            fseek(backing_store, pagina * TAM_PAGINA, SEEK_SET);

            fread(memoria_fisica[frame], sizeof(unsigned char),
                  TAM_PAGINA, backing_store);

            tabela_paginas[pagina] = frame;
            frame_para_pagina[frame] = pagina;
        }

        int frame = tabela_paginas[pagina];

        int endereco_fisico = frame * TAM_PAGINA + offset;

        signed char valor = (signed char)memoria_fisica[frame][offset];

        printf("%15u | %6d | %6d | %5d | %15d | %5d\n",
               endereco, pagina, offset, frame,
               endereco_fisico, valor);
    }

    printf("\nEstatisticas:\n");
    printf("Page faults: %d\n", page_faults);
    printf("Frames utilizados: %d\n", frames_utilizados);
    printf("Substituicoes FIFO: %d\n", substituicoes);

    fclose(arquivo);
    fclose(backing_store);

    return 0;
}