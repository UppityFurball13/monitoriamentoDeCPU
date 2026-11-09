# Monitorador de Processador

Aplicação gráfica em **C++ para Windows** que acompanha o uso total da CPU e de cada processador lógico. O painel é atualizado a cada segundo e mantém o histórico dos últimos **60 segundos** para visualização e exportação em CSV.

> Projeto acadêmico em grupo da disciplina de **Arquitetura de Computadores** da Universidade Federal do Maranhão (UFMA), no período 2026.2.

## O que o painel mostra

| Informação                              | Como aparece                                                                                  |
| --------------------------------------- | --------------------------------------------------------------------------------------------- |
| Modelo do processador                   | Nome da CPU consultado no Registro do Windows.                                                |
| Núcleos físicos e processadores lógicos | Quantidades exibidas separadamente no topo da janela.                                         |
| Uso total da CPU                        | Percentual atual e gráfico dos últimos 60 segundos.                                           |
| Uso por processador lógico              | Percentual na grade e histórico no gráfico ao selecionar uma CPU.                             |
| Frequência                              | Valor em MHz informado pelo Windows para o processador lógico selecionado, quando disponível. |
| Processos                               | Total de processos em execução no sistema.                                                    |

## Como compilar e executar

Você precisa do **Windows** e do **MinGW-w64 de 64 bits**. O código usa **C++98** e pode ser compilado no **Dev-C++ com TDM-GCC 4.9.2 64-bit Release**.

### No Dev-C++

1. Abra [`monitorCPU.cpp`](./monitorCPU.cpp) diretamente no Dev-C++.
2. Em **Ferramentas → Opções do Compilador → Geral**, selecione o perfil **TDM-GCC 4.9.2 64-bit Release**.
3. Marque a opção para adicionar comandos ao **linker** e inclua:

   ```text
   -mwindows -lpdh -lpsapi -lcomdlg32 -lgdi32 -ladvapi32 -luser32
   ```

4. Pressione **F11** para compilar e executar.

### Pelo PowerShell

Com o `g++` do MinGW-w64 disponível no `PATH`, abra o PowerShell na pasta do código e execute:

```powershell
g++ -std=c++98 -mwindows monitorCPU.cpp -o monitorCPU.exe -lpdh -lpsapi -lcomdlg32 -lgdi32 -ladvapi32 -luser32
.\monitorCPU.exe
```

Se aparecer `undefined reference` para funções do Windows, confira se as bibliotecas foram adicionadas ao **linker**. No terminal, deixe as opções `-l...` depois de `monitorCPU.cpp`.

## Como usar

1. Ao abrir o programa, acompanhe o gráfico de **uso total da CPU**.
2. Clique em uma CPU na grade ou escolha um processador lógico no seletor para ver seu histórico. Para voltar, selecione **Uso total da CPU**.
3. Clique em **Exportar CSV** e escolha onde salvar o histórico disponível naquele momento. Cada linha contém o horário, o uso total, a quantidade de processos e os valores de uso e frequência por processador lógico. O arquivo usa ponto e vírgula como separador de colunas.

O percentual de uso começa a aparecer após a segunda amostra. **N/D** indica que uma leitura está indisponível.

## Como o código funciona

A interface usa a **API Win32** e as medições de desempenho são consultadas pelo **PDH** (_Performance Data Helper_). A função `iniciarContadores()` prepara as consultas, `coletar()` registra as amostras e `atualizarPainel()` atualiza os valores e solicita o redesenho da tela. A função `exportarCsv()` grava o histórico quando o usuário aciona o botão.

## Acompanhamento do projeto

- [Arquivos enviados ao professor](./Arquivos)
- [Cronograma do projeto no Notion](https://app.notion.com/p/Diagrama-de-Gantt-ea9bf99354c7832d81be012f81b9c7cf)

## Equipe

- João Yuri Anderson da Silva Frazão
- Silas de Sousa Silva
- Vinícius Guilhon Cunha
- Vitor dos Santos de Sousa

**Disciplina** — Arquitetura de Computadores  
**Professor** — Luís Henrique Neves Rodrigues  
**Instituição** — Universidade Federal do Maranhão (UFMA)  
**Período** — 2026.2
