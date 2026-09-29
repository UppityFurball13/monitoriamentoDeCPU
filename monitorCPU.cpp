/*
 * Monitor de Processador - foco em um único núcleo (clock)
 * Para Windows + Dev-C++
 *
 * IMPORTANTE - adicione estas bibliotecas no linker:
 *   Ferramentas > Opções do Compilador > marque
 *   "Adicionar os seguintes comandos ao chamar o linker" e escreva:
 *       -lpowrprof -lpdh
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <powrprof.h>
#include <pdh.h>
#include <tlhelp32.h>

/* O MinGW antigo não declara PdhAddEnglishCounterA, então
   carregamos a função direto da pdh.dll */
typedef PDH_STATUS (WINAPI *PFN_ADD_ENGLISH)(PDH_HQUERY, LPCSTR,
                                              DWORD_PTR, PDH_HCOUNTER *);

/* Estrutura própria (evita conflito de versões do MinGW) */
typedef struct {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} INFO_PROC;

/* Lê o nome do processador no registro do Windows */
void obter_nome_cpu(char *nome, DWORD tam)
{
    HKEY chave;
    strcpy(nome, "Desconhecido");
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &chave) == ERROR_SUCCESS) {
        RegQueryValueExA(chave, "ProcessorNameString", NULL, NULL,
                         (LPBYTE)nome, &tam);
        RegCloseKey(chave);
    }
}

/* Frequência atual e máxima (MHz) do núcleo escolhido */
int obter_frequencias(int nucleo, int total, ULONG *atual, ULONG *maxima)
{
    INFO_PROC *v = (INFO_PROC *)malloc(sizeof(INFO_PROC) * total);
    if (!v) return 0;

    /* 11 = ProcessorInformation */
    LONG r = CallNtPowerInformation((POWER_INFORMATION_LEVEL)11, NULL, 0,
                                    v, sizeof(INFO_PROC) * total);
    if (r == 0) {
        *atual  = v[nucleo].CurrentMhz;
        *maxima = v[nucleo].MaxMhz;
        free(v);
        return 1;
    }
    free(v);
    return 0;
}

/* Conta quantos processos existem no sistema */
int contar_processos(void)
{
    int n = 0;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return -1;
    if (Process32First(snap, &pe)) {
        do { n++; } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return n;
}

/* Desenha uma barra de uso: [#####-----] */
void desenhar_barra(double pct)
{
    int i, cheio = (int)(pct / 5.0);   /* 20 posições */
    if (cheio > 20) cheio = 20;
    printf("[");
    for (i = 0; i < 20; i++) printf(i < cheio ? "#" : "-");
    printf("]");
}

/* Volta o cursor ao topo (evita a tela piscar com system("cls")) */
void cursor_topo(void)
{
    COORD c = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

int main(void)
{
    SYSTEM_INFO si;
    char nome[128];
    int total, nucleo;
    ULONG atual = 0, maxima = 0;
    char caminho[64];
    PDH_HQUERY query;
    PDH_HCOUNTER contador;
    PDH_FMT_COUNTERVALUE valor;

    GetSystemInfo(&si);
    total = (int)si.dwNumberOfProcessors;
    obter_nome_cpu(nome, sizeof(nome));

    printf("=== Monitor de Processador ===\n");
    printf("Processador: %s\n", nome);
    printf("Nucleos logicos: %d\n\n", total);
    printf("Qual nucleo deseja monitorar (0 a %d)? ", total - 1);
    if (scanf("%d", &nucleo) != 1 || nucleo < 0 || nucleo >= total) {
        printf("Nucleo invalido.\n");
        return 1;
    }

    /* Contador de desempenho do Windows para o núcleo escolhido.
       "English" evita problemas com Windows em português. */
    sprintf(caminho, "\\Processor(%d)\\%% Processor Time", nucleo);

    HMODULE dll = LoadLibraryA("pdh.dll");
    PFN_ADD_ENGLISH addEnglish = dll ?
        (PFN_ADD_ENGLISH)GetProcAddress(dll, "PdhAddEnglishCounterA") : NULL;
    if (!addEnglish) {
        printf("Nao foi possivel carregar a funcao do contador.\n");
        return 1;
    }

    if (PdhOpenQueryA(NULL, 0, &query) != ERROR_SUCCESS ||
        addEnglish(query, caminho, 0, &contador) != ERROR_SUCCESS) {
        printf("Erro ao abrir o contador de desempenho.\n");
        return 1;
    }
    PdhCollectQueryData(query);   /* primeira leitura (referência) */

    system("cls");
    while (1) {
        Sleep(1000);
        PdhCollectQueryData(query);
        double uso = 0.0;
        if (PdhGetFormattedCounterValue(contador, PDH_FMT_DOUBLE, NULL,
                                        &valor) == ERROR_SUCCESS)
            uso = valor.doubleValue;

        int temfreq = obter_frequencias(nucleo, total, &atual, &maxima);

        cursor_topo();
        printf("=== Monitor de Processador ===                    \n");
        printf("Processador : %-60s\n", nome);
        printf("Nucleo      : %d de %d                            \n\n",
               nucleo, total - 1);

        if (temfreq) {
            printf("Velocidade atual  : %lu MHz (%.2f GHz)        \n",
                   atual, atual / 1000.0);
            printf("Velocidade maxima : %lu MHz (%.2f GHz)        \n",
                   maxima, maxima / 1000.0);
        } else {
            printf("Velocidade atual  : N/D                       \n");
            printf("Velocidade maxima : N/D                       \n");
        }

        printf("Uso               : %5.1f%%  ", uso);
        desenhar_barra(uso);
        printf("\n");
        printf("Processos (total) : %d                            \n\n",
               contar_processos());
        printf("Pressione 'q' para sair.                          \n");

        if (_kbhit()) {
            int tecla = _getch();
            if (tecla == 'q' || tecla == 'Q') break;
        }
    }

    PdhCloseQuery(query);
    return 0;
}
