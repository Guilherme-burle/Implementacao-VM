# Gerenciador de Memória Virtual

Implementação de um gerenciador de memória virtual em C, utilizando tabela de páginas, memória física e TLB. O programa implementa os algoritmos de substituição FIFO e LRU, além de utilizar Pthreads para realizar a busca paralela na TLB.

## Características

- 256 páginas virtuais de 256 bytes.
- Memória física com 128 frames.
- TLB com 16 entradas.
- Substituição de páginas e entradas da TLB utilizando FIFO ou LRU.
- Busca na TLB utilizando 16 threads.
- Tratamento de erros de argumentos e arquivos.
- Cálculo das taxas de page faults e TLB hits.

## Requisitos

- Sistema Linux ou ambiente compatível, como WSL.
- Compilador GCC.
- Make.
- Biblioteca Pthreads.

## Compilação

Para compilar o programa:

```bash
make
```

Para remover o executável e o arquivo de saída dos testes:

```bash
make clean
```

## Execução

```bash
./vm <arquivo_enderecos> <algoritmo_pagina> <algoritmo_tlb>
```

Os algoritmos disponíveis são `fifo` e `lru`.

Exemplos:

```bash
./vm addresses.txt fifo fifo
./vm addresses.txt fifo lru
./vm addresses.txt lru fifo
./vm addresses.txt lru lru
```

O arquivo `BACKING_STORE.bin` deve estar no diretório em que o programa é executado.

O programa apresenta os endereços virtuais, as posições na TLB, os endereços físicos, os valores recuperados da memória e as estatísticas finais.

## Arquivos

- `vm.c`: código-fonte do gerenciador de memória virtual.
- `Makefile`: responsável pela compilação e limpeza do projeto.
- `README.md`: documentação e instruções de utilização.
- `BACKING_STORE.bin`: arquivo binário que armazena o conteúdo das páginas virtuais.
- `addresses.txt`: arquivo de endereços fornecido para testes.
- `correct.txt`: saída de referência fornecida pelo professor.
- `evidencias.log`: registro dos testes realizados durante o desenvolvimento.