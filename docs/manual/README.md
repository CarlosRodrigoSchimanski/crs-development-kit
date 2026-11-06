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

## Folhas e grama: textura com recorte

Grama, folhas, cercas de arame e cabelo costumam ser **planos simples** com o desenho numa textura que tem partes **transparentes**. A engine **recorta** esses modelos: onde a textura é transparente (alpha abaixo de 50%), o pixel não é desenhado. As faces aparecem dos dois lados, então a folha também é vista de trás.

O recorte só liga quando o **material do .glb pede** (alphaMode **MASK** ou **BLEND** no glTF). Nos outros modelos, a transparência da textura é ignorada, para nenhum pedaço sumir por engano.

**No Blender, para o .glb sair com recorte:**
1. No material, ligue o **Alpha** da textura de cor na entrada **Alpha** do Principled BSDF.
2. Entre os dois, ponha um **Math > Less Than** (valor 0.5) seguido de um **Math > Subtract** (1 − resultado). É assim que o exportador de glTF entende "recorte" (MASK).
3. Exporte em **.glb**. Os tufos prontos ficam em `assets/models/grass/`.

A **sombra** também sai recortada, no formato das folhas (veja [Sombra recortada](#sombra-recortada)).

> **Ainda falta:** o **contorno de seleção** desses modelos ainda sai retangular, e personagens animados com recorte (cabelo em planos) ainda fazem sombra inteira.

### Por dentro (para entender o código)

1. **Ler o pedido do arquivo:** o `CrsModel::Builder::loadModel` ([crs_model.cpp](../../src/engine/rendering/crs_model.cpp)) olha o `alpha_mode` do material que tem a textura de cor e liga `alphaCutout` quando não é OPAQUE.
2. **Textura opaca fica opaca:** sem recorte, a `CrsTexture` é criada com `forceOpaque`, que troca o alpha de todo pixel por 255 logo depois de decodificar o PNG. Por isso o shader pode recortar sempre, sem precisar de um aviso por objeto.
3. **O recorte em si:** no [simple_shader.frag](../../shaders/simple_shader.frag), `if (texel.a < 0.5) discard;`. O `discard` manda a placa de vídeo esquecer aquele pixel: nem cor, nem profundidade.
4. **Os dois lados:** a pipeline já desenha com `VK_CULL_MODE_NONE` (sem descartar as faces de costas).

O teste está em [test_model_material.cpp](../../tests/test_model_material.cpp).

## Faz sombra (ligar e desligar a sombra de um objeto)

No card **Objeto** de qualquer objeto com modelo, há a caixa **Faz sombra**, ligada por padrão.

- **Ligada:** o objeto projeta sombra no chão e nos outros objetos.
- **Desligada:** o objeto **não projeta** sombra, mas **continua recebendo** a sombra dos outros. Uma grama sem sombra própria ainda fica escura debaixo de uma árvore.

É salva com a cena, e o **Ctrl + Z** desfaz.

### Quando desligar

- **Grama baixa, pedrinhas, flores, folhas no chão:** coisas pequenas e numerosas. De longe a sombra delas mal aparece, e são elas que existem aos milhares.
- **Objetos dentro de casa, longe do sol:** a sombra do sol não chega lá mesmo.
- **Muita grama com recorte:** a sombra recortada é a mais cara de todas (veja abaixo). Num campo com milhares de tufos, desligue na grama baixa e deixe ligada só na alta.

Deixe **ligada** em tudo que é grande ou importante na cena: paredes, árvores, personagens, móveis.

### Por que economiza

A sombra do sol é feita num **passe à parte**: antes de desenhar a tela, a engine desenha a cena inteira **vista do sol**, guardando só a distância de cada ponto até ele (o **mapa de sombra**). Depois, ao desenhar a tela, cada pixel consulta esse mapa: "tem algo entre mim e o sol?". Se tem, fica na sombra.

Então **cada objeto que faz sombra é desenhado duas vezes** por frame: uma para a tela e outra para o mapa de sombra. Desligar o **Faz sombra** tira o objeto só do segundo desenho. Ele continua na tela e continua consultando o mapa (por isso recebe sombra).

### Por dentro (para entender o código)

- **O campo:** `castShadow` no `CrsGameObject` ([crs_game_object.hpp](../../src/engine/scene/crs_game_object.hpp)), `true` por padrão.
- **Salvar e abrir:** o [scene_serializer.cpp](../../src/engine/scene/scene_serializer.cpp) grava `"castShadow"` no `.json` da cena. Cena antiga, sem o campo, abre com tudo fazendo sombra.
- **O passe de sombra:** o [shadow_render_system.cpp](../../src/engine/rendering/systems/shadow_render_system.cpp) monta os grupos com `batches.build(..., true)`. Esse `true` (`shadowCastersOnly`, em [crs_instance_batches.cpp](../../src/engine/rendering/crs_instance_batches.cpp)) pula quem tem `castShadow` desligado. Os animados (personagens) são pulados no laço deles, logo abaixo.
- **O card:** a caixa é desenhada no card Objeto do [crs_editor.cpp](../../src/editor/crs_editor.cpp). Ela só aparece em objeto com modelo, e não aparece no terreno.
- **Ctrl + Z sem código extra:** o histórico compara a cena salva antes e depois de cada mexida no painel. Como o campo vai para o `.json`, a mudança entra no histórico sozinha.
- **Teste:** em [test_scene_serializer.cpp](../../tests/test_scene_serializer.cpp), o campo volta igual depois de salvar e abrir, e uma cena sem o campo abre com sombra ligada.

## Sombra recortada

Os modelos com recorte (grama, folhas) fazem sombra **no formato das folhas**, não do plano inteiro. É automático: não tem opção para ligar. Basta o modelo ter recorte (veja [Folhas e grama](#folhas-e-grama-textura-com-recorte)) e o **Faz sombra** estar ligado.

Se o objeto estiver com a textura desligada (`useTexture`), ele aparece inteiro na tela, então a sombra também sai inteira.

### Por que é mais cara

Na sombra comum, o shader de pixel é **vazio**. A placa de vídeo só grava a distância de cada ponto até o sol, e ela sabe de antemão quais pixels ficam escondidos atrás de outros. Ela pula esses pixels sem nem rodar o shader (o "early-z").

Na sombra recortada, o shader **lê a textura** e decide se joga o pixel fora (`discard`). Como a placa de vídeo não sabe de antemão se o pixel vai sobreviver, ela **perde o atalho**: roda o shader em todos os pixels do plano, inclusive nos transparentes.

Por isso a engine usa **duas pipelines** no passe de sombra:
- **Sólidos** (paredes, pedras, personagens): a pipeline vazia, rápida, como antes.
- **Com recorte** (grama, folhas): a pipeline que lê a textura. Só esses pagam o custo.

### Por dentro (para entender o código)

- **O modelo avisa:** `CrsModel::hasAlphaCutout()` ([crs_model.hpp](../../src/engine/rendering/crs_model.hpp)) vem do `alphaCutout` do Builder.
- **Os shaders:**
  - [shadow_cutout.vert](../../shaders/shadow_cutout.vert): igual ao `shadow.vert`, mas passa o UV adiante.
  - [shadow_cutout.frag](../../shaders/shadow_cutout.frag): lê a textura e faz `discard` abaixo de 0.5, o mesmo corte do `simple_shader.frag`. Assim a sombra bate com o que aparece na tela.
- **O passe de sombra:** o [shadow_render_system.cpp](../../src/engine/rendering/systems/shadow_render_system.cpp) desenha primeiro todos os grupos sólidos. Depois troca **uma vez** para a `cutoutPipeline` e desenha os grupos com recorte, ligando a textura de cada um no **set 1**. O descriptor de cada textura é criado na primeira vez (`getTextureSet`) e reaproveitado nos frames seguintes.
- **Os animados** (personagens) continuam na pipeline de sombra deles, sem recorte.

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
