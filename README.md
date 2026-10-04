# Need for Madness — port nativo para PS Vita e Linux

![icon](data/icon.png)

Port não oficial de **Need for Madness** (Radicalplay, 2015) para **PS Vita**
e **Linux**, escrito em C. O código-fonte original nunca foi publicado: o
jogo foi descompilado e reescrito linha a linha, a física primeiro em
JavaScript/WebGL (`web/`) e depois em C (`native/`). O objetivo é ser fiel
ao original, 1:1. O `Game.jar` original, corrigido para rodar em Java
moderno, fica no repositório e serve de referência para comparar.

Os assets do jogo (`data/`, `stages/`, `mycars/`, `mystages/`, `music/`) são
os originais, byte a byte, e o port os lê como estão.

---

## Build para PS Vita

### 1. Pré-requisitos

- **VitaSDK** instalado (https://vitasdk.org), com as variáveis de ambiente:
  ```sh
  export VITASDK=/usr/local/vitasdk
  export PATH=$VITASDK/bin:$PATH
  ```
- **vitaGL** e **zlib** pelo gerenciador de pacotes do VitaSDK:
  ```sh
  vdpm vitaGL
  vdpm zlib
  ```
- Opcional: para tirar a splash screen do vitaGL antes do jogo, recompile o
  vitaGL sem ela (o build avisa com um WARNING se ela ainda estiver lá):
  ```sh
  git clone https://github.com/Rinnegatamante/vitaGL
  cd vitaGL && make clean && make NO_SPLASHSCREEN=1 install
  ```

### 2. Compilar

Na raiz do repositório:

```sh
cmake -S native -B native/build-vita -DNFM_PLATFORM=vita \
      -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build native/build-vita -j
```

### 3. Onde fica o VPK

```
native/build-vita/platform/vita/nfm_vita.vpk
```

O VPK já traz todos os assets do jogo (`data/`, `stages/`, `music/`,
`mycars/`), a bolinha e o LiveArea. Não precisa copiar nada à parte para o
cartão de memória.

### 4. Instalar

Copie o `nfm_vita.vpk` para o Vita (por FTP ou USB no VitaShell) e instale
pelo VitaShell. O app aparece como **Need for Madness**, Title ID
`NFMD00001`. O progresso fica salvo em `ux0:data/NFMD00001/`.

### Controles no Vita

| Botão | Ação |
|---|---|
| R | acelerar |
| L | frear / ré |
| Analógico esquerdo | virar |
| X | freio de mão (e confirmar nos menus) |
| X + analógico esquerdo, no ar | manobras: frente/trás = loops, lados = giros. Só empurrões firmes e retos contam; diagonais e toques pequenos não fazem nada |
| Analógico direito | girar a câmera |
| Direcional ▲ | seta guia: pista ↔ carros |
| Direcional ▼ | radar (minimapa + velocímetro) |
| △ | trocar câmera |
| □ | mutar música |
| Select | mutar efeitos |
| Start | pausar |
| O | voltar nos menus |

Lista completa, com os do Linux, em [`CONTROLES.txt`](CONTROLES.txt).

---

## Build para Linux

O alvo Linux é o ambiente de desenvolvimento: usa o mesmo código do Vita,
com SDL2 e OpenGL no lugar de sceCtrl/vitaGL.

```sh
# dependências (Debian/Ubuntu)
sudo apt install build-essential cmake libsdl2-dev libgl-dev zlib1g-dev

cmake -S native -B native/build-linux -DNFM_PLATFORM=linux -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-linux -j

# rode a partir da RAIZ do repositório (os assets são lidos de lá)
native/build-linux/platform/linux/nfm_linux
```

Opção `-DNFM_SHOW_FPS=ON` desenha FPS e o pior frame na tela.

Testes do núcleo (rodam no host, sem janela):

```sh
cmake -S native/tests -B native/tests/build && cmake --build native/tests/build -j
cd native/tests/build && ctest
```

---

## O que foi feito no port nativo

O jogo inteiro roda: boot, menus, escolha de carro e de pista, corrida
contra a IA, replays, pausa, telas de vitória/derrota e progresso salvo nas
campanhas NFM 1 e NFM 2 e no Free Play.

**Fidelidade ao original**
- Física, colisão, IA, checkpoints, dano e manobras traduzidos do Java com a
  mesma aritmética (inteiros de 32 bits com overflow, arredondamento float32),
  conferidos contra o jar original com testes diferenciais.
- Renderização sem depth buffer, por ordem de submissão, como no original:
  céu, chão, neblina, sombras que acompanham rampas, poeira, faíscas, chamas,
  o anel de reparo.
- Motion blur e tremor de tela pelo mesmo mecanismo do `paint()` original
  (blend de frames com alpha).
- Sequência de boot original: tela de loading com a barra azul,
  "Click/Press to Start", intro da Radicalplay.
- Menus 1:1: principal, modos de jogo, instruções (todas as páginas, com arte
  de botões do Vita), créditos, escolha de carro com o carro girando, escolha
  de pista com o voo da câmera sobre a pista, pista bloqueada.
- **Tela de apresentação da pista** antes de cada corrida: dica do Coach
  Insano, "Loading complete! Press Start to begin..." e o botão START piscando.
- Voo de câmera ao redor dos carros antes da contagem 3-2-1-GO.
- Menu de pausa original (continuar, replay instantâneo, instruções, sair) e
  replay de destaque no fim da corrida.
- HUD original: dano, power, posição, voltas, wasted, velocímetro, radar,
  seta guia, mensagens de manobra.

**Áudio**
- **Música idêntica ao original, byte a byte**: o renderizador de MOD do jogo
  (`ModSlayer` + `SuperClip`) foi traduzido para C (`native/core/radical_mod.c`).
  As 34 faixas saem idênticas ao que o jar gera, inclusive os pontos de loop
  e o "som granulado" característico do original. Isso é verificado por
  teste.
- Música do menu da escolha de carro até a confirmação da pista, música de
  cada pista com o BPM, a velocidade e o volume originais, e `party.zip` na
  pista 27 do NFM 2.
- Todos os efeitos: motor (5 tipos × 5 rotações), ar, batidas, derrapagem,
  raspadas, contagem, checkpoint, reparo, wasted.

**Extras deste port**
- Tela de **Settings** no menu de pausa: intensidade do motion blur, de 0 a
  100 em passos de 20, salva entre sessões.
- Esquema de controle pensado para o Vita (tabela acima).
- Bolinha e LiveArea feitos com a arte do próprio menu principal
  (`native/platform/vita/make_livearea.py` regera).

**Desempenho**
- A corrida desenha uma vez por tick de física (53 ms, como o original) e os
  menus a 40 ms, reapresentando o último quadro no meio, o que corta 2/3 do
  trabalho de desenho sem mudar o que aparece na tela.
- Instruções por quadro na corrida de 13,25M para 8,68M (ordenação, buffers
  na pilha, funções inline, menos passes de render).
- Vazamentos de memória corrigidos (cópias de carros no ring de replay,
  texturas do HUD por corrida).

**Ainda diferente do original**
- O texto usa uma fonte vetorial 5×7 só com maiúsculas; o original usa Arial
  negrito. É a maior diferença visual que sobra.
- O multiplayer online existe no port web, mas não no nativo.
- O alvo Vita compila e roda no aparelho, mas cada mudança é testada primeiro
  no Linux; as mais recentes (controles, LiveArea) ainda precisam ser
  conferidas no hardware.

---

## Estrutura do repositório

| Pasta | Conteúdo |
|---|---|
| `native/` | **O port nativo.** `core/` é o jogo independente de plataforma (física, render, áudio, decoders); `platform/common/game.c` é o loop do jogo e os menus; `platform/linux/` e `platform/vita/` são as camadas finas de cada alvo; `tests/` são os testes do núcleo. |
| `web/` | O port JavaScript/WebGL, de onde o C foi traduzido. Abra `index.html` com `python3 -m http.server 8123`. |
| `decompilation/` | O Java descompilado (`java-src/`), referência de leitura, não entrada do build. |
| `java/` | O `Game.jar` original corrigido para Java moderno (`./start.sh`) e o original intacto (`Game.jar.bak`). |
| `data/`, `stages/`, `mycars/`, `mystages/`, `music/` | Assets originais, **não modificar**. `data/vita/` tem a arte de botões do Vita (gerada por `tools/gen_vita_assets.py`). |
| `tools/` | Scripts auxiliares (arte do Vita, música). |

## Documentação para quem for mexer

- [`native/PORT_SPEC.md`](native/PORT_SPEC.md) — regras do port nativo.
- [`native/TASKS_NATIVE.md`](native/TASKS_NATIVE.md) — o que foi feito, como foi verificado e o que falta.
- [`WORK.md`](WORK.md) — descobertas e armadilhas, uma por linha (por exemplo: o construtor do `RadicalMod` está errado no descompilado; o bug de `intToBytes16` que dá o som da música).
- [`AGENTS.md`](AGENTS.md) — como rodar, medir e verificar; as invariantes que não podem quebrar (sem depth buffer, uma única chamada de desenho em ordem de submissão).
- [`web/TRANSPILE_SPEC.md`](web/TRANSPILE_SPEC.md) — o contrato de tradução Java → código (overflow de inteiros, float32).

Comparar com o original: `./start.sh` roda o jar. Para capturas automáticas
de qualquer tela do jar, dá para dirigi-lo com `java.awt.Robot` sob `xvfb-run`
(ver `WORK.md`).

---

*Need for Madness © Radicalplay. Projeto de fã, sem fins comerciais.*
