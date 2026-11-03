# Manual da CRS Development Kit

Como usar a engine, um tema por página: o passo a passo, o que cada campo faz e dicas. Serve para aprender do zero e para consultar depois.

## Temas

1. **Começando** *(em breve)*: a interface, a câmera do editor e os atalhos
2. **Cena e objetos** *(em breve)*: Outliner, adicionar, mover, girar, escalar, apagar, salvar e abrir
3. [**Terreno**](03-terreno.md): criar, esculpir, curvas de nível e pintar materiais
4. [**Física**](04-fisica.md): colisão feita no Blender (`UCX_`), exportar um grupo para modelar a colisão, física dentro de grupos *(o resto em breve)*
5. [**Animação**](05-animacao.md): as animações do personagem (parado, andando, correndo, no ar) *(o resto em breve)*
6. **Componentes Python** *(em breve)*: criar, args, `start` e `update`, e a referência da API com exemplos
7. **Play e câmera de jogo** *(em breve)*
8. **Exportar o jogo** *(em breve)*: o instalador

## Salvar

**Ctrl + S** salva a cena. Enquanto houver mudança não salva, o título da janela mostra um **\*** (por exemplo `CRS Development Kit - main.json *`).

Ao fechar a engine (pelo X ou por **Arquivo > Sair**) com mudança não salva, ela pergunta **"Salvar antes de sair?"**:
- **Salvar:** salva e fecha.
- **Sair sem salvar:** fecha e perde as mudanças.
- **Cancelar:** volta para o editor.

## Muitas cópias não pesam

Objetos que usam **o mesmo modelo** são desenhados juntos, numa chamada só (instancing): 100 paredes iguais custam quase o mesmo que uma. Então duplique à vontade com **Ctrl + D** para montar o cenário. O painel de cima mostra quantos objetos e quantas chamadas de desenho tem a cena.

Para renomear uma cópia (`cube.001` → `parede_norte`), selecione o objeto e mude o campo **Nome**, no topo do card **Objeto**.

> **Cuidado:** um componente Python que procura o objeto pelo nome (`crsdk.scene.find("Player")`) deixa de achar se você renomear o objeto.

## Grupos (pai e filhos)

Um objeto pode ser **pai** de outros. Os filhos andam, giram e escalam junto com ele. É assim que se monta uma casa ou um castelo a partir de um kit de peças.

**Para pôr um objeto dentro de outro:** no **Outliner**, arraste o nome dele e solte em cima do nome do pai. **Para tirar do grupo:** arraste e solte na faixa **"solte aqui para tirar do pai"**, que aparece embaixo da lista enquanto você arrasta.

Nos dois casos o objeto **fica no mesmo lugar**: a engine recalcula a posição, a rotação e a escala dele em relação ao pai novo. O **Ctrl + Z** desfaz.

### Exemplo: uma casa com colisão

1. **Adicionar > Empty**, e no card dele **Adicionar física**: **Caixa**, tipo **Estático**. Ajuste o tamanho para envolver a casa. O Empty é invisível no Play, então essa caixa vira uma parede invisível.
2. Monte a casa com as peças do kit (paredes, telhado, portas), **sem física** nelas.
3. Arraste cada peça para dentro da caixa no Outliner.
4. Pronto: selecione a caixa e mova ou gire. A casa inteira vai junto, e a colisão também.

Com o **Ctrl + D** na caixa, você duplica a casa inteira, com as peças dentro.

### Colisão dentro do grupo

Os filhos também podem ter física própria, por exemplo a **escada** do castelo com a caixa de colisão dela. Mover ou girar o pai leva todas as colisões junto. Para a casa inteira, dá para **exportar o grupo para o Blender**, modelar a colisão por cima e trazer de volta: veja [Física](04-fisica.md#montar-a-colisão-de-um-grupo-casa-castelo).

Não dá para pôr um objeto dentro de um filho dele mesmo (seria um círculo), e o **terreno** não pode ter pai.

## Enquadrar o selecionado

Como o **"View Selected"** do Blender (na Unity e na Unreal é a tecla F).

1. Selecione o objeto, clicando nele ou no **Outliner**.
2. Aperte o **Del do teclado numérico**. Ele é o **ponto (.)** do numérico e funciona com o Num Lock ligado ou desligado. Também dá para dar **duplo clique** no nome do objeto no Outliner.

A câmera vai até o objeto numa animação curta. Ela para numa distância em que o objeto inteiro cabe na tela, e o ângulo de vista continua o mesmo. Depois disso, girar com o botão do meio e a rodinha passam a ser **em volta do objeto**.

- **Objeto com filhos** (o Player, por exemplo): enquadra ele com os filhos.
- **Terreno**: enquadra o terreno inteiro, com os morros.
- **Objeto sem tamanho** (vazio, luz, câmera): vai até ele e fica a alguns metros.
- Mexer na câmera (botão do meio ou rodinha) no meio da animação faz ela parar onde está.

> O **Delete** de cima do teclado continua apagando o objeto. Só o Del do **numérico** enquadra.

## Atalhos mais usados

| Atalho | O que faz |
|---|---|
| Botão do meio + arrastar | gira a câmera do editor em volta do ponto que ela olha |
| Shift + botão do meio + arrastar | arrasta a vista para os lados |
| Rodinha | aproxima e afasta |
| Clique esquerdo | seleciona o objeto |
| Del (ou ponto) do teclado numérico | enquadra o objeto selecionado: a câmera vai até ele, veja [Enquadrar](#enquadrar-o-selecionado) |
| Duplo clique no Outliner | seleciona e enquadra o objeto |
| Arrastar no Outliner | solto em cima de outro objeto, vira filho dele (veja [Grupos](#grupos-pai-e-filhos)) |
| G / R / S | setas de mover / girar / escalar |
| Shift + A | menu Adicionar, onde está o mouse |
| Delete | apaga o objeto selecionado (e os filhos dele) |
| Ctrl + D | duplica o objeto selecionado (e os filhos dele): a cópia nasce no mesmo lugar, já selecionada |
| Ctrl + Z | desfaz a última ação (pincelada, mover, apagar, campo do painel...) |
| Ctrl + Y (ou Ctrl + Shift + Z) | refaz o que foi desfeito |
| Ctrl + S | salva a cena |
| P | começa o Play |
| ' (tecla à esquerda do 1) | volta do Play para o editor |
