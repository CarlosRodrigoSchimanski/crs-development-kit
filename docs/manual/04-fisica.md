# Física

> Capítulo em construção. Por enquanto ele cobre a colisão feita no Blender e a física dentro de grupos.

Um objeto pode colidir de dois jeitos:

| Jeito | Para quê | Como |
|---|---|---|
| **Card Física** | formas simples: caixa, esfera, cápsula. Pode ser **estático** (parede) ou **dinâmico** (cai, rola, é empurrado) | no card do objeto, **Adicionar física** |
| **Colisão no modelo** | a forma exata do cenário: parede com escada, porta, arco, uma casa inteira. Sempre **estática** | malhas **`UCX_`** dentro do `.glb`, feitas no Blender |

Se um objeto tiver os dois, o **card Física manda** e a colisão do modelo fica de fora. Isso serve para exceções, como um caixote que precisa cair.

## Colisão feita no Blender (UCX_)

Toda malha cujo nome começa com **`UCX_`** (ou **`COL_`**) vira colisão:

- **não é desenhada no jogo**. No editor ela aparece **verde e translúcida**, com as arestas marcadas e cada face num tom (para ver o volume). A do objeto selecionado fica mais forte. Ela fica um pouquinho à frente da parede desenhada, então aparece mesmo quando coincide com ela;
- é **estática**, com **a forma exata** que você modelou. Pode ser côncava, com vão, porta, canto para dentro, escada colada na parede;
- vale para **todo objeto que usa o modelo**. As 20 escadas iguais do castelo usam a mesma colisão. Mudou no Blender e exportou de novo, mudam todas;
- **sem `UCX_` no arquivo, o modelo fica sem colisão.**

### Dicas para modelar a colisão

- **Simples:** dezenas ou centenas de triângulos, não os milhares da peça bonita. Ela não aparece, só precisa estar no lugar certo.
- **Escada vira rampa:** uma rampa por cima dos degraus faz o personagem subir liso, sem tropeçar. É o que quase todo jogo faz.
- **Pode ter várias:** `UCX_parede`, `UCX_rampa`, `UCX_chao`... Cada uma pode ser uma malha separada no Blender.
- O nome pode estar no **objeto** ou na **malha** do Blender, com maiúscula ou minúscula (`ucx_` também vale).

## Montar a colisão de um grupo (casa, castelo)

Para quando você montou uma casa com um kit de peças na engine e quer modelar a colisão dela inteira no Blender.

1. **Monte a casa** com um **Empty** como pai e as peças como filhos (veja [Grupos](README.md#grupos-pai-e-filhos)).
2. Selecione o Empty e use **Arquivo > Exportar grupo para o Blender (.glb)...**. A engine grava um `.glb` com todas as peças, já montadas, e com **o Empty na origem** (o ponto 0, 0, 0 do Blender). Cada peça sai com o nome dela no Outliner. Se alguma peça já tinha colisão `UCX_`, ela sai junto, para você ajustar.
3. **No Blender:** **File > Import > glTF 2.0** e escolha o arquivo. A casa aparece inteira, servindo de **molde**.
4. Modele as malhas **`UCX_`** por cima da casa.
5. Selecione **só as `UCX_`** e use **File > Export > glTF 2.0**:
   - **Format:** glTF Binary (`.glb`);
   - em **Include**, marque **Selected Objects** (a casa-molde não vai junto);
   - salve em `assets/models/`, por exemplo `casa_colisao.glb`.
6. **Na engine:** **Arquivo > Importar .glb...** e escolha o `casa_colisao.glb`. Ele aparece só como a colisão verde translúcida.
7. No **Outliner**, arraste o objeto importado para **dentro do Empty**.
8. No card **Objeto** dele, zere a **posição** e a **rotação** (0, 0, 0). Como agora ele é filho, zero quer dizer "na origem do grupo", exatamente onde você modelou.

Pronto: a colisão encaixa na casa. Mover, girar, escalar ou duplicar (Ctrl+D) o Empty leva a casa e a colisão juntas.

> **Mudou a casa?** Se trocar peças de lugar, exporte o molde de novo e ajuste a colisão no Blender. Para casas repetidas (a mesma casa em 10 lugares), o trabalho é feito uma vez só.

## Física dentro de grupos

Os filhos de um grupo também podem ter o **card Física** próprio. A colisão de cada um fica onde ele aparece no mundo, somada com a posição, o giro e a escala do pai:

- **Estático** (parede, escada, chão): segue o pai quando o grupo é movido.
- **Dinâmico** (um caixote dentro do grupo): cai e rola sozinho no Play, e a posição dele continua guardada em relação ao pai.
