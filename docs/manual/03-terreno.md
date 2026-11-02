# Terreno

O chão do jogo: um relevo que você esculpe (morros, vales, platôs) e pinta com materiais (grama, terra, rocha, pedra). No Play, o personagem anda nele de verdade.

- [Criar um terreno](#criar-um-terreno)
- [Mudar o tamanho](#mudar-o-tamanho)
- [Esculpir](#esculpir)
- [Curvas de nível](#curvas-de-nível)
- [Pintar materiais](#pintar-materiais)
- [A regra dos 4 materiais por trecho](#a-regra-dos-4-materiais-por-trecho)
- [Usar um material novo](#usar-um-material-novo)
- [Salvar](#salvar)
- [No Play e no Python](#no-play-e-no-python)
- [Limites de hoje](#limites-de-hoje)

---

## Criar um terreno

1. No menu **Adicionar** (ou **Shift + A**), clique em **Terreno**.
2. Ele aparece no meio da cena com **256 × 256 m**, plano, já selecionado e coberto com o material padrão (o xadrez).

O objeto do terreno fica **no meio** dele. Para mudar o lugar, mova o objeto. Girar e escalar não têm efeito no terreno.

> **Dica:** o xadrez serve para você ter noção de tamanho e ver onde ainda não pintou. Cada quadrado mede uns 50 cm.

## Mudar o tamanho

No card **Terreno**, a linha **Tamanho** tem **−** e **+**.

- O terreno é feito de **pedaços de 256 m**, e cada clique tira ou põe um pedaço por lado: 256, 512, 768, 1024 m... até **2048 m**.
- **Ao aumentar:** o que você já esculpiu e pintou **fica no mesmo lugar**, e os pedaços novos entram planos para os lados **+X e +Z**.
- **Ao diminuir:** os pedaços das bordas +X e +Z saem com o que tinha neles. Mudou de ideia? **Ctrl + Z**.

## Esculpir

No card **Terreno**, escolha uma ferramenta e segure o **botão esquerdo** em cima do terreno:

| Ferramenta | O que faz |
|---|---|
| **Subir** | levanta o terreno. Com **Shift**, abaixa |
| **Suavizar** | arredonda o relevo e tira os degraus |
| **Nivelar** | leva tudo para a altura onde o clique começou (bom para platôs e estradas) |
| **Nenhum** | desliga o pincel: o clique volta a selecionar objetos |

- **Raio** (1 a 64 m): o tamanho do pincel. O aro branco no terreno mostra onde ele vai mexer.
- **Força** (0,05 a 1): a rapidez da mudança. Com força 1, o centro do pincel sobe 5 m por segundo.
- O efeito é mais forte no centro do pincel e vai sumindo até a borda, sem degrau.

> **Cuidado:** se você esculpir um morro por cima de um objeto, ele fica enterrado e no Play cai pelo chão. Esculpa em volta dos objetos ou mova-os para cima depois.

## Curvas de nível

As linhas azuis desenhadas por cima do terreno mostram a altura, como num mapa topográfico.

- Uma linha a cada **1 m** de altura, e uma mais grossa a cada **5 m**.
- Aparecem **só no editor**. No Play e no jogo exportado nunca aparecem.
- No card **Terreno**: **Curvas de nível** liga e desliga, e o campo ao lado muda o intervalo (de 0,25 m a 50 m).
- Em chão plano não aparece linha.

## Pintar materiais

Tudo de pintura fica no card **Material**, abaixo do card Terreno.

### Carregar um material no pincel

1. Clique em **Carregar material** (ou no espaço **Pincel**).
2. Escolha um da lista. São os materiais da pasta `assets/materials`.
3. Ele vai para o pincel, e o **Pintar** já liga sozinho.
4. O card mostra uma **esfera** com o material aplicado, para você reconhecer qual é sem depender do nome. Clicar na esfera também abre a lista.

Carregar outro material tira o anterior do pincel. **O que você já pintou continua no chão.**

### Pintar e apagar

Com **Pintar** ligado (fica azul):

| Mouse | O que faz |
|---|---|
| **Botão esquerdo** | pinta o material do pincel por cima do que já tem |
| **Botão direito** | apaga o material do pincel, e aparece o que estava embaixo |

**Raio** e **Força** são os mesmos do card Terreno.

**Espalhar** (0 a 1) controla a borda do pincel:
- **0:** borda suave, com o material sumindo liso até a beirada.
- **1:** borda salpicada, como uma lata de spray. O centro pinta cheio e, indo para a borda, só alguns pontos recebem o material, cada vez mais raros.
- O padrão é **0,6**, um meio-termo. Os pontos são sorteados uma vez por pincelada, então segurar o pincel reforça os mesmos pontos, e cada pincelada nova sai com um desenho diferente. Várias pinceladas por cima umas das outras formam uma mistura natural, como terra salpicada na grama.

A pintura tem **4 pontos por metro** (um a cada 25 cm), mais fina que a malha do relevo (1 por metro). Por isso o salpicado sai em grãos pequenos, e não em manchas de 1 m. Cenas pintadas antes dessa mudança são convertidas sozinhas ao abrir, com a mesma cara. Ao salvar, elas passam para o formato novo.

Nas encostas e paredes a textura não estica: ela é projetada de cima e dos lados ao mesmo tempo (projeção **triplanar**), e cada lado do morro usa a direção que está de frente para ele.

Onde dois materiais se encontram, a engine mistura pela **altura** de cada um. Na borda entre terra e pedra, a terra entra nas frestas e as pedras aparecem por cima, em vez de uma faixa borrada.

### Escala da textura (Repetir)

O campo **Repetir** diz de quantos em quantos metros a textura do material se repete no chão. É o tamanho de "uma foto" dela.

- **Número menor**, textura menor. Se a rocha está grande demais, diminua de 4 m para 2 m.
- **Número maior**, textura maior.
- Cada material tem o seu valor, e ele é salvo com a cena.
- O campo aparece quando o material do pincel **já foi usado** no terreno.

### Chão base

O **chão base** é o material que aparece onde nada foi pintado. Para trocar, carregue o material no pincel e clique em **Usar como base**. Se ele já estava pintado em algum lugar, aquela pintura passa a ser chão base também.

## A regra dos 4 materiais por trecho

O terreno é desenhado em **trechos de 32 × 32 m**. Para o jogo ficar leve, cada trecho deve ter **no máximo 4 materiais**, como no World of Warcraft.

- **Enquanto você pinta**, a engine não tira nada sozinha.
- **Trecho com mais de 4** ganha uma **aura laranja-avermelhada**, com a borda marcando os limites dele. Ela aparece só no editor.
- **Para corrigir:** use o **Checar** para ver o que tem ali, carregue o material que quer tirar e apague com o **botão direito**. A aura some quando o trecho volta a ter 4 ou menos.
- **Se você esquecer**, no Play e no jogo exportado o trecho desenha só os 4 materiais que mais cobrem, e o resto é ignorado.

### Checar

1. No card **Material**, ligue **Checar**.
2. Clique no terreno, por exemplo num trecho com aura.
3. No painel de cima, junto do FPS, aparecem os materiais **dentro do círculo do pincel**, com quanto cada um cobre, do maior para o menor.

O de menor porcentagem costuma ser o mais fácil de tirar.

## Usar um material novo

Os materiais vêm do [Poly Haven](https://polyhaven.com/textures), que é gratuito e permite usar em jogo sem dar crédito.

1. Na página da textura, escolha **2K**.
2. Baixe em **JPG** os mapas:

   | Mapa | Para quê |
   |---|---|
   | **Diffuse** | a cor |
   | **Normal (GL)** | o relevo fino. **Não** use o Normal (DX): no Vulkan o relevo sai invertido |
   | **Rough** | onde é liso (brilha) e onde é áspero |
   | **AO** | as frestas mais escuras |
   | **Displacement** | a altura, usada na mistura entre materiais |

3. Crie uma pasta com o nome do material em `assets/materials/` (por exemplo `assets/materials/sand_01/`) e ponha os arquivos nela.
4. Pronto: ele aparece no **Carregar material**.

A engine reconhece cada mapa pelo nome do arquivo (`_diff`, `_nor_gl`, `_rough`, `_ao`, `_disp`), então pode ser de outro site que siga o mesmo padrão. Só a cor é obrigatória.

> **Memória:** um material só é carregado na memória de vídeo quando aparece em algum trecho, e sai quando para de aparecer. O painel de cima mostra **"Materiais do terreno na memória"**.

## Salvar

**Ctrl + S** salva o terreno junto com a cena:

- as alturas e a pintura vão para `assets/terrains/<nome da cena>_<número>/`, com um arquivo por pedaço;
- materiais que você pintou e depois cobriu por inteiro saem da lista do terreno sozinhos.

## No Play e no Python

- **Física:** o terreno é chão de verdade. O personagem anda, sobe rampa e para em ladeira íngreme, e objetos caem e rolam nele.
- **Python:**

```python
import crsdk

# altura do chão num ponto do mundo (None fora do terreno)
altura = crsdk.scene.terrainHeight(10.0, 25.0)

# um raio também acerta o terreno
acerto = crsdk.scene.raycast(crsdk.Vec3(0, 50, 0), crsdk.Vec3(0, -1, 0), 100.0)
if acerto:
    print(acerto.point)
```

## Desfazer

**Ctrl + Z** desfaz a última pincelada (esculpir, pintar ou apagar), a mudança de tamanho e o "Usar como base". **Ctrl + Y** refaz. Cada pincelada, do apertar ao soltar o botão, é um passo. O painel de cima mostra o que foi desfeito.

## Limites de hoje

- **Sombra:** os morros recebem a sombra dos objetos, mas ainda não fazem sombra.
- **Estrada com padrão** (pedras alinhadas, tijolo): numa curva as pedras não acompanham o caminho. Para isso, use peças de estrada modeladas. No terreno, prefira materiais sem direção (terra, grama, areia, rocha, trilha).
