// Dev-C++ (TDM-GCC 64-bit): comandos do linker
// -mwindows -lpdh -lpsapi -lcomdlg32 -lgdi32 -ladvapi32 -luser32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define PSAPI_VERSION 1
#include <windows.h>
#include <commdlg.h>
#include <pdhmsg.h>
#include <psapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// O pdh.h do TDM-GCC 4.9.2 declara PdhValidatePathExA duas vezes.
// Estas sao apenas as declaracoes das funcoes PDH usadas pelo monitor.
typedef LONG PDH_STATUS;
typedef HANDLE PDH_HQUERY;
typedef HANDLE PDH_HCOUNTER;

struct PDH_FMT_COUNTERVALUE {
    DWORD CStatus;
    union {
        LONG longValue;
        double doubleValue;
        LONGLONG largeValue;
        LPCSTR AnsiStringValue;
        LPCWSTR WideStringValue;
    };
};

extern "C" {
    PDH_STATUS WINAPI PdhOpenQueryA(LPCSTR, DWORD_PTR, PDH_HQUERY*);
    PDH_STATUS WINAPI PdhCollectQueryData(PDH_HQUERY);
    PDH_STATUS WINAPI PdhGetFormattedCounterValue(PDH_HCOUNTER, DWORD, LPDWORD,
                                                  PDH_FMT_COUNTERVALUE*);
    PDH_STATUS WINAPI PdhCloseQuery(PDH_HQUERY);
}

namespace {
const int ID_SERIE = 101;
const int ID_GRADE = 102;
const int ID_CSV = 103;
const UINT_PTR ID_TIMER = 1;
const ULONGLONG JANELA_MS = 60000;
const DWORD FORMATO_PDH_DOUBLE = 0x00000200;
const double INDISPONIVEL = -1.0;
const COLORREF AZUL = RGB(26, 104, 187);
const COLORREF TEXTO = RGB(29, 46, 65);

struct CpuLogica {
    std::string nome;
    PDH_HCOUNTER uso;
    PDH_HCOUNTER clock;
    CpuLogica() : uso(NULL), clock(NULL) {}
};

struct Amostra {
    SYSTEMTIME hora;
    ULONGLONG instante;
    double total;
    long processos;
    std::vector<double> uso;
    std::vector<double> clock;
};

struct Painel {
    PDH_HQUERY consulta;
    PDH_HCOUNTER usoTotal;
    std::vector<CpuLogica> cpus;
    std::deque<Amostra> historico;
    HWND modelo, topologia, total, processos, clock, serie, grafico, grade, csv;
    HWND titulo, legendaGrade, rodape;
    HFONT fonteTitulo;
    int selecao;

    Painel() : consulta(NULL), usoTotal(NULL), modelo(NULL), topologia(NULL),
        total(NULL), processos(NULL), clock(NULL), serie(NULL), grafico(NULL),
        grade(NULL), csv(NULL), titulo(NULL), legendaGrade(NULL), rodape(NULL),
        fonteTitulo(NULL), selecao(-1) {}
} painel;

std::string inteiro(long numero) {
    if (numero < 0) return "N/D";
    std::ostringstream texto;
    texto << numero;
    return texto.str();
}

std::string decimal(double numero, int casas) {
    if (numero < 0) return "N/D";
    std::ostringstream texto;
    texto << std::fixed << std::setprecision(casas) << numero;
    return texto.str();
}

std::string porcentagem(double numero) {
    if (numero < 0) return "N/D";
    return decimal(numero, 1) + "%";
}

std::string decimalCsv(double numero) {
    std::string valor = decimal(numero, 1);
    std::replace(valor.begin(), valor.end(), '.', ',');
    return valor;
}

std::string modeloCpu() {
    char modelo[256] = {0};
    DWORD tamanho = sizeof(modelo);
    const LONG resultado = RegGetValueA(
        HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "ProcessorNameString", RRF_RT_REG_SZ, NULL, modelo, &tamanho);
    return resultado == ERROR_SUCCESS && modelo[0] ? modelo : "N/D";
}

long nucleosFisicos() {
    DWORD tamanho = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &tamanho);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || tamanho == 0) return -1;

    std::vector<BYTE> buffer(tamanho);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX dados =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(&buffer[0]);
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, dados, &tamanho)) {
        return -1;
    }

    long total = 0;
    for (DWORD pos = 0; pos < tamanho; ++total) {
        const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* nucleo =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(&buffer[pos]);
        if (nucleo->Size == 0 || nucleo->Size > tamanho - pos) return -1;
        pos += nucleo->Size;
    }
    return total;
}

// Os nomes em ingles funcionam mesmo no Windows em portugues.
typedef PDH_STATUS (WINAPI *AdicionarContador)(PDH_HQUERY, LPCSTR,
                                               DWORD_PTR, PDH_HCOUNTER*);
PDH_HCOUNTER adicionar(AdicionarContador api, const std::string& caminho) {
    PDH_HCOUNTER contador = NULL;
    if (!painel.consulta || !api) return NULL;
    if (api(painel.consulta, caminho.c_str(), 0, &contador) != ERROR_SUCCESS) {
        return NULL;
    }
    return contador;
}

void iniciarContadores() {
    PdhOpenQueryA(NULL, 0, &painel.consulta);
    // Resolucao dinamica para funcionar com cabecalhos antigos do MinGW.
    AdicionarContador api = NULL;
    const HMODULE pdh = GetModuleHandleA("pdh.dll");
    FARPROC endereco = pdh ? GetProcAddress(pdh, "PdhAddEnglishCounterA") : NULL;
    std::memcpy(&api, &endereco, sizeof(api));

    const std::string objeto = "\\Processor Information(";
    painel.usoTotal = adicionar(api, objeto + "_Total)\\% Processor Time");
    const WORD grupos = GetActiveProcessorGroupCount();
    if (!painel.usoTotal && grupos == 1) {
        painel.usoTotal = adicionar(api, "\\Processor(_Total)\\% Processor Time");
    }

    for (WORD grupo = 0; grupo < grupos; ++grupo) {
        const DWORD quantidade = GetActiveProcessorCount(grupo);
        for (DWORD cpu = 0; cpu < quantidade; ++cpu) {
            const std::string instancia = inteiro(grupo) + "," + inteiro(cpu);
            CpuLogica item;
            item.nome = grupos == 1 ? "CPU " + inteiro(cpu) : "CPU " + instancia;
            item.uso = adicionar(api, objeto + instancia + ")\\% Processor Time");
            if (!item.uso && grupos == 1) {
                item.uso = adicionar(api, "\\Processor(" + inteiro(cpu) + ")\\% Processor Time");
            }
            item.clock = adicionar(api, objeto + instancia + ")\\Processor Frequency");
            painel.cpus.push_back(item);
        }
    }
}

double ler(PDH_HCOUNTER contador) {
    if (!contador) return INDISPONIVEL;
    PDH_FMT_COUNTERVALUE valor;
    if (PdhGetFormattedCounterValue(contador, FORMATO_PDH_DOUBLE, NULL, &valor) != ERROR_SUCCESS
        || (valor.CStatus != PDH_CSTATUS_VALID_DATA && valor.CStatus != PDH_CSTATUS_NEW_DATA)
        || !(valor.doubleValue >= 0.0)) {
        return INDISPONIVEL;
    }
    return valor.doubleValue;
}

double lerUso(PDH_HCOUNTER contador) {
    return std::min(100.0, ler(contador));
}

void coletar(bool primeira) {
    Amostra amostra;
    GetLocalTime(&amostra.hora);
    amostra.instante = GetTickCount64();
    PERFORMANCE_INFORMATION info = {0};
    info.cb = sizeof(info);
    amostra.processos = GetPerformanceInfo(&info, sizeof(info)) ?
        static_cast<long>(info.ProcessCount) : -1;

    const bool ok = painel.consulta && PdhCollectQueryData(painel.consulta) == ERROR_SUCCESS;
    amostra.total = ok && !primeira ? lerUso(painel.usoTotal) : INDISPONIVEL;
    for (size_t i = 0; i < painel.cpus.size(); ++i) {
        const CpuLogica& cpu = painel.cpus[i];
        amostra.uso.push_back(ok && !primeira ? lerUso(cpu.uso) : INDISPONIVEL);
        const double frequencia = ok ? ler(cpu.clock) : INDISPONIVEL;
        amostra.clock.push_back(frequencia > 0.0 ? frequencia : INDISPONIVEL);
    }

    painel.historico.push_back(amostra);
    while (amostra.instante - painel.historico.front().instante > JANELA_MS) {
        painel.historico.pop_front();
    }
}

void atualizarPainel() {
    const Amostra& amostra = painel.historico.back();
    SetWindowTextA(painel.total, ("Uso total: " + porcentagem(amostra.total)).c_str());
    SetWindowTextA(painel.processos, ("Processos: " + inteiro(amostra.processos)).c_str());

    std::string clock = "Clock informado: selecione uma CPU";
    if (painel.selecao >= 0) {
        const double mhz = amostra.clock[painel.selecao];
        clock = painel.cpus[painel.selecao].nome + " | Clock informado: " + decimal(mhz, 0);
        if (mhz >= 0) clock += " MHz";
    }
    SetWindowTextA(painel.clock, clock.c_str());
    InvalidateRect(painel.grafico, NULL, FALSE);
    InvalidateRect(painel.grade, NULL, FALSE);
}

void preencher(HDC dc, const RECT& area, COLORREF cor) {
    SetDCBrushColor(dc, cor);
    FillRect(dc, &area, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void texto(HDC dc, int x, int y, const std::string& valor, COLORREF cor) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, cor);
    TextOutA(dc, x, y, valor.c_str(), static_cast<int>(valor.size()));
}

void desenharGrafico(HDC dc, const RECT& area) {
    preencher(dc, area, RGB(255, 255, 255));
    HGDIOBJ fonteAnterior = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ canetaAnterior = SelectObject(dc, GetStockObject(DC_PEN));
    const int esquerda = area.left + 55;
    const int direita = area.right - 20;
    const int topo = area.top + 17;
    const int fundo = area.bottom - 26;

    SetDCPenColor(dc, RGB(218, 228, 240));
    for (int porcento = 0; porcento <= 100; porcento += 25) {
        const int y = fundo - (fundo - topo) * porcento / 100;
        MoveToEx(dc, esquerda, y, NULL);
        LineTo(dc, direita, y);
        texto(dc, area.left + 10, y - 7, inteiro(porcento) + "%", TEXTO);
    }
    for (int segundos = 0; segundos <= 60; segundos += 10) {
        const int x = direita - (direita - esquerda) * segundos / 60;
        MoveToEx(dc, x, topo, NULL);
        LineTo(dc, x, fundo);
        texto(dc, x - 8, fundo + 5, inteiro(segundos) + "s", TEXTO);
    }

    if (!painel.historico.empty()) {
        const ULONGLONG recente = painel.historico.back().instante;
        ULONGLONG anterior = 0;
        bool ligar = false;
        SetDCPenColor(dc, AZUL);
        for (size_t i = 0; i < painel.historico.size(); ++i) {
            const Amostra& amostra = painel.historico[i];
            const double uso = painel.selecao < 0 ? amostra.total : amostra.uso[painel.selecao];
            if (uso < 0) {
                ligar = false;
                continue;
            }
            const int x = direita - static_cast<int>(
                (recente - amostra.instante) * (direita - esquerda) / JANELA_MS);
            const int y = fundo - static_cast<int>(uso * (fundo - topo) / 100.0);
            if (ligar && amostra.instante - anterior <= 2500) LineTo(dc, x, y);
            MoveToEx(dc, x, y, NULL);
            SetPixelV(dc, x, y, AZUL);
            ligar = true;
            anterior = amostra.instante;
        }
    }

    SelectObject(dc, canetaAnterior);
    SelectObject(dc, fonteAnterior);
}

void desenharCelula(const DRAWITEMSTRUCT& item) {
    if (item.itemID >= painel.cpus.size()) return;
    const double uso = painel.historico.empty() ? INDISPONIVEL :
        painel.historico.back().uso[item.itemID];
    const bool selecionado = static_cast<int>(item.itemID) == painel.selecao;
    RECT area = item.rcItem;
    preencher(item.hDC, area, RGB(255, 255, 255));
    InflateRect(&area, -3, -2);
    preencher(item.hDC, area, selecionado ? RGB(210, 229, 251) : RGB(237, 243, 249));
    texto(item.hDC, area.left + 7, area.top + 3,
          painel.cpus[item.itemID].nome + "  |  " + porcentagem(uso), TEXTO);

    RECT barra = {area.left + 7, area.bottom - 10, area.right - 7, area.bottom - 5};
    preencher(item.hDC, barra, RGB(198, 214, 232));
    if (uso >= 0.0) {
        barra.right = barra.left + static_cast<int>((barra.right - barra.left) * uso / 100.0);
        preencher(item.hDC, barra, AZUL);
    }
    if (item.itemState & ODS_FOCUS) DrawFocusRect(item.hDC, &area);
}

void exportarCsv(HWND janela) {
    wchar_t destino[MAX_PATH] = L"historico_cpu.csv";
    OPENFILENAMEW dialogo = {0};
    dialogo.lStructSize = sizeof(dialogo);
    dialogo.hwndOwner = janela;
    dialogo.lpstrFilter = L"Arquivo CSV (*.csv)\0*.csv\0";
    dialogo.lpstrFile = destino;
    dialogo.nMaxFile = MAX_PATH;
    dialogo.lpstrDefExt = L"csv";
    dialogo.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialogo)) {
        if (CommDlgExtendedError() != 0) {
            MessageBoxA(janela, "Falha ao abrir a janela de exportacao.", "Exportar CSV",
                        MB_OK | MB_ICONERROR);
        }
        return;
    }

    // O dialogo pode processar mensagens do timer. Capturamos os dados ao salvar.
    FILE* arquivo = _wfopen(destino, L"w");
    if (!arquivo) {
        MessageBoxA(janela, "Nao foi possivel abrir o arquivo para gravacao.", "Exportar CSV",
                    MB_OK | MB_ICONERROR);
        return;
    }

    std::fprintf(arquivo, "data_hora;cpu_total_pct;processos");
    for (size_t i = 0; i < painel.cpus.size(); ++i) {
        std::fprintf(arquivo, ";%s_uso_pct;%s_clock_mhz",
                     painel.cpus[i].nome.c_str(), painel.cpus[i].nome.c_str());
    }
    std::fprintf(arquivo, "\n");
    for (size_t i = 0; i < painel.historico.size(); ++i) {
        const Amostra& amostra = painel.historico[i];
        const SYSTEMTIME& hora = amostra.hora;
        std::fprintf(arquivo, "%04d-%02d-%02d %02d:%02d:%02d;%s;%s",
                     hora.wYear, hora.wMonth, hora.wDay, hora.wHour, hora.wMinute,
                     hora.wSecond, decimalCsv(amostra.total).c_str(),
                     inteiro(amostra.processos).c_str());
        for (size_t cpu = 0; cpu < painel.cpus.size(); ++cpu) {
            std::fprintf(arquivo, ";%s;%s", decimalCsv(amostra.uso[cpu]).c_str(),
                         decimal(amostra.clock[cpu], 0).c_str());
        }
        std::fprintf(arquivo, "\n");
    }

    const bool falhaGravacao = std::ferror(arquivo) != 0;
    const bool falhaFechar = std::fclose(arquivo) != 0;
    if (falhaGravacao || falhaFechar) {
        MessageBoxA(janela, "O arquivo pode estar incompleto.", "Exportar CSV",
                    MB_OK | MB_ICONERROR);
    } else {
        MessageBoxA(janela, "Historico dos ultimos 60 segundos exportado.", "Exportar CSV",
                    MB_OK | MB_ICONINFORMATION);
    }
}

HWND controle(HWND pai, const char* classe, const char* rotulo, DWORD estilo, int id = 0) {
    HWND janela = CreateWindowExA(0, classe, rotulo, WS_CHILD | WS_VISIBLE | estilo,
        0, 0, 0, 0, pai, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleA(NULL), NULL);
    SendMessageA(janela, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
    return janela;
}

void posicionar(HWND janela) {
    RECT area;
    GetClientRect(janela, &area);
    const int largura = area.right - 40;
    const int alturaGrafico = std::max(155, static_cast<int>((area.bottom - 245) * 55 / 100));
    const int topoGrade = 183 + alturaGrafico;
    MoveWindow(painel.titulo, 20, 13, largura, 33, TRUE);
    MoveWindow(painel.modelo, 20, 50, largura, 22, TRUE);
    MoveWindow(painel.topologia, 20, 74, largura, 20, TRUE);
    MoveWindow(painel.total, 20, 103, 150, 22, TRUE);
    MoveWindow(painel.processos, 180, 103, 125, 22, TRUE);
    MoveWindow(painel.clock, 315, 103, largura - 295, 22, TRUE);
    MoveWindow(painel.serie, 20, 132, 220, 300, TRUE);
    MoveWindow(painel.csv, largura - 108, 130, 128, 27, TRUE);
    MoveWindow(painel.grafico, 20, 164, largura, alturaGrafico, TRUE);
    MoveWindow(painel.legendaGrade, 20, topoGrade, largura, 20, TRUE);
    MoveWindow(painel.grade, 20, topoGrade + 22, largura, area.bottom - topoGrade - 57, TRUE);
    SendMessageA(painel.grade, LB_SETCOLUMNWIDTH, std::max(135, (largura - 4) / 4), 0);
    MoveWindow(painel.rodape, 20, area.bottom - 24, largura, 20, TRUE);
}

bool criarControles(HWND janela) {
    painel.titulo = controle(janela, "STATIC", "Monitorador de processador", SS_LEFT);
    painel.fonteTitulo = CreateFontA(-25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Segoe UI");
    if (painel.fonteTitulo) {
        SendMessageA(painel.titulo, WM_SETFONT, reinterpret_cast<WPARAM>(painel.fonteTitulo), TRUE);
    }
    painel.modelo = controle(janela, "STATIC", modeloCpu().c_str(), SS_LEFT | SS_ENDELLIPSIS);
    const DWORD logicos = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    const std::string topologia = "Nucleos fisicos: " + inteiro(nucleosFisicos()) +
        "     |     Processadores logicos: " + (logicos ? inteiro(logicos) : "N/D");
    painel.topologia = controle(janela, "STATIC", topologia.c_str(), SS_LEFT);
    painel.total = controle(janela, "STATIC", "", SS_LEFT);
    painel.processos = controle(janela, "STATIC", "", SS_LEFT);
    painel.clock = controle(janela, "STATIC", "", SS_LEFT);
    painel.serie = controle(janela, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, ID_SERIE);
    painel.csv = controle(janela, "BUTTON", "Exportar CSV", BS_PUSHBUTTON | WS_TABSTOP, ID_CSV);
    painel.grafico = controle(janela, "STATIC", "", SS_OWNERDRAW | WS_BORDER);
    painel.legendaGrade = controle(janela, "STATIC",
        "Processadores logicos | Selecione um para ver o historico", SS_LEFT);
    painel.grade = controle(janela, "LISTBOX", "", WS_BORDER | WS_TABSTOP | WS_HSCROLL |
        LBS_MULTICOLUMN | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY, ID_GRADE);
    painel.rodape = controle(janela, "STATIC",
        "Ultimos 60 segundos | Atualizacao a cada 1 s | N/D = leitura indisponivel", SS_LEFT);

    if (!painel.titulo || !painel.modelo || !painel.topologia || !painel.total ||
        !painel.processos || !painel.clock || !painel.serie || !painel.csv ||
        !painel.grafico || !painel.legendaGrade || !painel.grade || !painel.rodape) {
        return false;
    }

    SendMessageA(painel.serie, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Uso total da CPU"));
    for (size_t i = 0; i < painel.cpus.size(); ++i) {
        const LPARAM nome = reinterpret_cast<LPARAM>(painel.cpus[i].nome.c_str());
        SendMessageA(painel.serie, CB_ADDSTRING, 0, nome);
        SendMessageA(painel.grade, LB_ADDSTRING, 0, nome);
    }
    SendMessageA(painel.serie, CB_SETCURSEL, 0, 0);
    SendMessageA(painel.grade, LB_SETITEMHEIGHT, 0, 34);
    posicionar(janela);
    coletar(true);
    atualizarPainel();
    return SetTimer(janela, ID_TIMER, 1000, NULL) != 0;
}

LRESULT CALLBACK janelaProc(HWND janela, UINT mensagem, WPARAM w, LPARAM l) {
    switch (mensagem) {
        case WM_CREATE:
            return criarControles(janela) ? 0 : -1;
        case WM_SIZE:
            if (painel.rodape && w != SIZE_MINIMIZED) posicionar(janela);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* limites = reinterpret_cast<MINMAXINFO*>(l);
            limites->ptMinTrackSize.x = 690;
            limites->ptMinTrackSize.y = 575;
            return 0;
        }
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* item = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (item->CtlID == ID_GRADE) {
                item->itemHeight = 34;
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT& item = *reinterpret_cast<const DRAWITEMSTRUCT*>(l);
            if (item.CtlID == ID_GRADE) {
                desenharCelula(item);
                return TRUE;
            }
            if (item.hwndItem == painel.grafico) {
                desenharGrafico(item.hDC, item.rcItem);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(w);
            SetTextColor(dc, TEXTO);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        case WM_TIMER:
            if (w == ID_TIMER) {
                coletar(false);
                atualizarPainel();
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == ID_SERIE && HIWORD(w) == CBN_SELCHANGE) {
                const LRESULT indice = SendMessageA(painel.serie, CB_GETCURSEL, 0, 0);
                if (indice != CB_ERR) {
                    painel.selecao = static_cast<int>(indice) - 1;
                    SendMessageA(painel.grade, LB_SETCURSEL, painel.selecao, 0);
                    atualizarPainel();
                }
            } else if (LOWORD(w) == ID_GRADE && HIWORD(w) == LBN_SELCHANGE) {
                const LRESULT indice = SendMessageA(painel.grade, LB_GETCURSEL, 0, 0);
                if (indice != LB_ERR) {
                    painel.selecao = static_cast<int>(indice);
                    SendMessageA(painel.serie, CB_SETCURSEL, indice + 1, 0);
                    atualizarPainel();
                }
            } else if (LOWORD(w) == ID_CSV && HIWORD(w) == BN_CLICKED) {
                exportarCsv(janela);
            }
            return 0;
        case WM_DESTROY:
            KillTimer(janela, ID_TIMER);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(janela, mensagem, w, l);
}
} // namespace

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE, LPSTR, int mostrar) {
    const char nomeClasse[] = "MonitoradorDeProcessador";
    WNDCLASSA classe = {0};
    classe.lpfnWndProc = janelaProc;
    classe.hInstance = instancia;
    classe.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(32512));
    classe.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    classe.lpszClassName = nomeClasse;
    if (!RegisterClassA(&classe)) {
        MessageBoxA(NULL, "Falha ao registrar a janela.", "Monitorador de processador",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    iniciarContadores();
    HWND janela = CreateWindowExA(WS_EX_CONTROLPARENT, nomeClasse,
        "Monitorador de processador", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 890, 685, NULL, NULL, instancia, NULL);

    int resultado = 1;
    if (janela) {
        ShowWindow(janela, mostrar);
        UpdateWindow(janela);
        MSG mensagem;
        BOOL leitura;
        while ((leitura = GetMessageA(&mensagem, NULL, 0, 0)) > 0) {
            if (!IsDialogMessageA(janela, &mensagem)) {
                TranslateMessage(&mensagem);
                DispatchMessageA(&mensagem);
            }
        }
        if (leitura == 0) resultado = static_cast<int>(mensagem.wParam);
    } else {
        MessageBoxA(NULL, "Falha ao abrir a janela ou iniciar o temporizador.",
                    "Monitorador de processador", MB_OK | MB_ICONERROR);
    }
    if (painel.consulta) PdhCloseQuery(painel.consulta);
    if (painel.fonteTitulo) DeleteObject(painel.fonteTitulo);
    return resultado;
}