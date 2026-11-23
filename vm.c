
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t enderecos[] = {
        1, 256, 32768, 32769, 128, 65534, 33153, 65536
    };

    int quantidade = sizeof(enderecos) / sizeof(enderecos[0]);

    printf("Endereco Logico | Pagina | Offset\n");
    printf("-----------------------------------\n");

    for (int i = 0; i < quantidade; i++) {
        uint32_t endereco_16 = enderecos[i] & 0xFFFF;

        uint32_t pagina = (endereco_16 >> 8) & 0xFF;
        uint32_t offset = endereco_16 & 0xFF;

        printf("%15u | %6u | %6u\n",
               enderecos[i], pagina, offset);
    }

    return 0;
}
