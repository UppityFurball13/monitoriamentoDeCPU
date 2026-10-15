# Monitorador de Processador

Programa em C para Windows que monitora, em tempo real, um único núcleo (clock) do processador, de forma semelhante à aba de desempenho do Gerenciador de Tarefas.

> **Projeto acadêmico em grupo**, desenvolvido na disciplina de **Arquitetura de Computadores** (UFMA, 2026.2) e **avaliado para fins de obtenção de nota**.

---

## Sobre o projeto

O objetivo é aplicar na prática conceitos de Arquitetura de Computadores, como frequência de clock, núcleos de processamento, uso de CPU e escalonamento de processos. Para isso, o grupo desenvolveu um monitor que acompanha o comportamento de um núcleo escolhido pelo usuário.

### O que o programa exibe

| Informação          | Descrição                                                  |
| ------------------- | ---------------------------------------------------------- |
| Nome do processador | Modelo da CPU, lido do registro do Windows                 |
| Velocidade atual    | Frequência de operação do núcleo (MHz e GHz)               |
| Velocidade máxima   | Frequência máxima suportada pelo núcleo                    |
| Uso (%)             | Percentual de utilização do núcleo, com barra de progresso |
| Processos           | Quantidade total de processos em execução no sistema       |

### Limitações conhecidas

- O Windows não informa quantos processos estão rodando em cada núcleo, pois um processo pode migrar entre núcleos a qualquer instante. Por isso o programa mostra o total de processos do sistema.
- Em alguns processadores, a velocidade "atual" pode ser reportada de acordo com o plano de energia do Windows.
- "Núcleo" refere-se a núcleo lógico (com Hyper-Threading, um núcleo físico aparece como dois).

---

## Tecnologias

- Linguagem **C**
- **Dev-C++** (compilador MinGW)
- API do Windows: `CallNtPowerInformation`, PDH (Performance Data Helper), Toolhelp32 e Registro do Windows

---

## Como compilar e executar

1. Abra o arquivo `monitor_cpu.c` no **Dev-C++**.
2. Vá em **Ferramentas > Opções do Compilador**.
3. Marque **"Adicionar os seguintes comandos ao chamar o linker"** e escreva:
   ```
   -lpowrprof -lpdh
   ```
4. Compile e execute com **F11**.
5. Informe o número do núcleo que deseja monitorar. Para sair, pressione `q`.

---

## Acompanhamento do projeto

### Arquivos enviados ao professor

Todas as versões e atualizações do projeto enviadas ao professor durante o desenvolvimento estão na pasta:

📁 **[`Arquivos/`](./Arquivos)**

### Cronograma (Diagrama de Gantt)

O planejamento e o andamento das etapas do projeto estão no Notion:

🔗 **[Acessar o diagrama de Gantt no Notion](https://app.notion.com/p/Diagrama-de-Gantt-ea9bf99354c7832d81be012f81b9c7cf)**

---

## Equipe

Projeto desenvolvido em grupo pelos seguintes integrantes:

- João Yuri Anderson da Silva Frazão
- Silas de Sousa Silva
- Vinícius Guilhon Cunha
- Vitor dos Santos de Sousa

**Disciplina:** Arquitetura de Computadores
**Professor(a):** Luís Henrique Neves Rodrigues
**Instituição:** Universidade Federal do Maranhão (UFMA)
**Período:** 2026.2
