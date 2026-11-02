# Manual da CRS Development Kit

Como usar a engine, um tema por página: o passo a passo, o que cada campo faz e dicas. Serve para aprender do zero e para consultar depois.

## Temas

1. **Começando** *(em breve)*: a interface, a câmera do editor e os atalhos
2. **Cena e objetos** *(em breve)*: Outliner, adicionar, mover, girar, escalar, apagar, salvar e abrir
3. [**Terreno**](03-terreno.md): criar, esculpir, curvas de nível e pintar materiais
4. **Física** *(em breve)*: formas de colisão, estático e dinâmico, massa, o personagem
5. **Animação** *(em breve)*: clipes e transição
6. **Componentes Python** *(em breve)*: criar, args, `start` e `update`, e a referência da API com exemplos
7. **Play e câmera de jogo** *(em breve)*
8. **Exportar o jogo** *(em breve)*: o instalador

## Muitas cópias não pesam

Objetos que usam **o mesmo modelo** são desenhados juntos, numa chamada só (instancing): 100 paredes iguais custam quase o mesmo que uma. Então duplique à vontade com **Ctrl + D** para montar o cenário. O painel de cima mostra quantos objetos e quantas chamadas de desenho tem a cena.

Para renomear uma cópia (`cube.001` → `parede_norte`), selecione o objeto e mude o campo **Nome**, no topo do card **Objeto**.

> **Cuidado:** um componente Python que procura o objeto pelo nome (`crsdk.scene.find("Player")`) deixa de achar se você renomear o objeto.

## Atalhos mais usados

| Atalho | O que faz |
|---|---|
| Botão do meio + arrastar | gira a câmera do editor em volta do ponto que ela olha |
| Shift + botão do meio + arrastar | arrasta a vista para os lados |
| Rodinha | aproxima e afasta |
| Clique esquerdo | seleciona o objeto |
| G / R / S | setas de mover / girar / escalar |
| Shift + A | menu Adicionar, onde está o mouse |
| Delete | apaga o objeto selecionado (e os filhos dele) |
| Ctrl + D | duplica o objeto selecionado (e os filhos dele): a cópia nasce no mesmo lugar, já selecionada |
| Ctrl + Z | desfaz a última ação (pincelada, mover, apagar, campo do painel...) |
| Ctrl + Y (ou Ctrl + Shift + Z) | refaz o que foi desfeito |
| Ctrl + S | salva a cena |
| P | começa o Play |
| ' (tecla à esquerda do 1) | volta do Play para o editor |
