# Kit da taverna: lista de itens

Origem: "Medieval Arcade" de dark_igorek (CC-BY, ver [CREDITS](../../CREDITS.md)). Original em `C:\assets-ref\taverna\`.

**Duas etapas por item:**
1. **Blender:** separado, textura recortada, origem no lugar, exportado em `.glb`. (Claude)
2. **Resolução:** imagem de cor aumentada no ComfyUI até o **tamanho final** da tabela. (você)

**Alvo: 256 pixels por metro.** Limite de 2048 por imagem.

## Como fazer a etapa de resolução

1. Abra o ComfyUI e o workflow **upscale-textura**.
2. Em **Carregar Imagem**, escolha `C:\assets-ref\taverna\texturas_cor\<item>_cor.png`.
3. Em **Tamanho final**, digite a largura e a altura da coluna **Tamanho final**.
4. **Executar.** A imagem sai em `ComfyUI/output/textura/`.
5. Marque ⬜ → ✅ aqui e me avise: eu coloco a imagem nova no `.glb`. O normal map e a rugosidade eu aumento junto, no mesmo tamanho (sem IA, que estraga esses mapas).

## Estrutura

| Item | Medida (m) | Imagem atual | px/m | Upscale | Tamanho final | Blender | Resolução |
|---|---|---|---|---|---|---|---|
| wall_01 | 2.24 × 1.72 | 301 × 392 | 173 | ×1.5 | **446 × 580** | ✅ | ✅ |
| wall_02 | 3.23 × 2.23 | 391 × 563 | 173 | ×1.5 | **579 × 834** | ✅ | ✅ |
| wall_03 | 2.23 × 1.32 | 232 × 392 | 173 | ×1.5 | **344 × 580** | ✅ | ✅ |
| wall_04 | 3.20 × 2.22 | 558 × 388 | 173 | ×1.5 | **826 × 575** | ✅ | ✅ |
| wall_05 | 3.10 × 1.62 × 3.52 | bake 1224 × 1268 | 173 | ×1.48 (IA) | **1812 × 1876** | ✅ | ✅ |
| wall_06 | 3.52 × 1.64 | 705 × 288 | 173 | ×1.5 | **1044 × 426** | ✅ | ✅ |
| wall_07 | 3.54 × 1.84 | 1490 × 942 | 173 | ×1.4 *(limite 2048)* | **2048 × 1295** | ✅ | ✅ |
| wall_08 | 3.38 × 1.52 | 1025 × 1604 | 173 | ×1.3 *(limite 2048)* | **1309 × 2048** | ✅ | ✅ |
| wall_09 | 4.83 × 2.31 | 405 × 423 | 120 | ×2.1 | **861 × 899** | ✅ | ✅ |
| wall_10 | 1.76 × 0.98 | 187 × 309 | 173 | ×1.5 | **277 × 458** | ✅ | ✅ |
| column_01 | 0.69 × 0.69 × 1.87 | bake 1344 × 1344 | 333 (= original) | cor: IA ×4 → 1344 (nitidez) | **1344 × 1344** | ✅ | ✅ |

| stairs_4steps_01 | 1.22 × 1.29 × 1.28 | bake | 256 | cor: IA ×4 → 836 × 832 (nitidez) | **836 × 832** | ✅ | ✅ |
| stairs_2steps_01 | 1.44 × 0.58 × 0.55 | bake | 257 | cor: IA ×4 → 588 × 584 (nitidez) | **588 × 584** | ✅ | ✅ |

| window_01 | 1.47 × 0.00 × 2.55 | recorte 1150 × 1988 | 777 → 512 | **reduzida** para 756 × 1308 (sem IA) | **756 × 1308** | ✅ | ✅ |

| window_border_01 | 1.66 × 0.20 × 2.73 | bake | 256 | cor: IA ×4 → 344 × 1524 (nitidez) | **344 × 1524** | ✅ | ✅ |

| Pilhas de pedra (canto) | Medida (m) | Pedras | Tris (antes → depois) | Imagem | Blender | Resolução |
|---|---|---|---|---|---|---|
| main_bricks_01 | 0.27 × 0.27 × 4.62 | 8 | 1728 → 352 | 836 × 848 | ✅ | ✅ |
| main_bricks_02 | 0.28 × 0.28 × 3.43 | 6 | ~1290 → 264 | 852 × 832 | ✅ | ✅ |
| main_bricks_03 | 0.25 × 0.26 × 2.38 | 4 | ~860 → 176 | 696 × 740 | ✅ | ✅ |
| main_bricks_04 | 0.25 × 0.25 × 0.87 | 2 | ~400 → 88 | 336 × 376 | ✅ | ✅ |

*Pilhas de pedra:* as 4 pilhas do `main_bricks.01` (cantos das paredes), uma por .glb. Mesmo método da column_01: cada pedra virou caixa com chanfro de 1 segmento (giro escolhido pela menor medida, senão a caixa sai girada e larga), cada pedra com sua área na UV, bake da original na densidade dela (332 px/m) + IA (nitidez). Sem metálico. Origem no **centro da base**.

| Corrimão | Medida (m) | Tris | Imagem | Blender | Resolução |
|---|---|---|---|---|---|
| railing_01 | 0.21 × 0.92 × 0.80 | 1658 | 332 × 316 (256 px/m, IA nitidez) | ✅ | ✅ |

*railing_01:* módulo do corrimão (fonte: `railing.01`, separado pelo usuário): 2 balaústres + 1 trecho do corrimão. Os 2 balaústres têm textura diferente na original (não empilhados). **Decimate do usuário** (3096 → 1658 tris, −46%); bake do corrimão ORIGINAL pro reduzido. Origem no **canto de baixo** (mínimo da caixa), pra encaixar módulo com módulo. Sem metálico.

| Lajes de borda | Medida (m) | Pedras | Tris (antes → depois) | Imagem | Blender | Resolução |
|---|---|---|---|---|---|---|
| big_border_01 | 0.68 × 0.31 × 0.10 | 1 (chão) | 188 → 44 | 272 × 272 | ✅ | ✅ |
| big_border_02 | 0.97 × 0.30 × 0.10 | 2 (alto, ao longo de X) | 320 → 88 | 388 × 376 | ✅ | ✅ |
| big_border_03 | 0.23 × 1.26 × 0.11 | 1 (alto, comprida em Y) | 216 → 44 | 244 × 436 | ✅ | ✅ |
| big_border_04 | 3.15 × 0.25 × 0.11 | 5 (beira do palco) | 884 → 220 | 644 × 644 | ✅ | ✅ |

*Lajes de borda:* os 4 grupos do `big_border.01`, mesmo método das pilhas de pedra (caixas com chanfro, bake da original a 336 px/m, IA nitidez, sem metálico, origem no centro da base). A big_border_03 é um pouco torta na original: a caixa fica até 3 cm longe (o normal map compensa).

| Madeira (estrutura) | Medida (m) | Bake → final | px/m | Blender | Resolução |
|---|---|---|---|---|---|
| wood_window_01 | 2.01 × 2.21 × 1.31 | 408 → **560 × 560** | 187 → 256 | ✅ | ✅ |
| wood_connectors_01 | 0.21 × 2.17 × 1.14 | 268 × 280 → **368 × 384** | 187 → 256 | ✅ | ✅ |
| wood_connectors_02 | 0.26 × 4.09 × 0.25 | 172 × 776 → **236 × 1064** | 187 → 256 | ✅ | ✅ |
| wood_connectors_03 | 1.64 × 0.19 × 1.02 | 172 × 364 → **236 × 500** | 187 → 256 | ✅ | ✅ |
| wood_connectors_04 | 0.09 × 0.18 × 1.03 | 104 × 196 → **144 × 268** | 187 → 256 | ✅ | ✅ |
| wood_connectors_05 | 2.53 × 0.21 × 0.18 | 180 × 484 → **248 × 664** | 187 → 256 | ✅ | ✅ |
| wood_connectors_06 | 0.14 × 0.17 × 0.13 | 28 × 120 → **40 × 164** | 187 → 256 | ✅ | ✅ |
| wood_connectors_07 | 0.14 × 0.17 × 0.69 | 120 × 132 → **164 × 180** | 187 → 256 | ✅ | ✅ |

*Madeira:* vigas e conexões de madeira do teto/janela (fonte: `wood_window01`, `wood_connectors01`, `.02`…`.07`, separadas pelo usuário; todas formas diferentes). Bake na densidade do original (187 px/m) → IA até 256. Metálico só onde tem prego (04, 06, 07 sem). Orientação como na cena, origem no **canto de baixo** (mínimo da caixa). JPEG no .glb.

*wall_05 (nicho da janela, refeita 2026-11-06):* agora com as **duas placas** (de fora e de dentro) numa peça só: por ser curva, uma cópia da placa na frente da outra não fechava. As placas usam pedaços diferentes do atlas → bake (não recorte). Mesma posição/orientação da wall_05 antiga (placa de fora no mesmo lugar). Sem metálico. .glb 3,7 → 0,7 MB (JPEG).

*Janela + moldura:* as duas têm a **mesma origem** (o canto de baixo da moldura): no motor, as duas na mesma posição se encaixam. Moldura: os arcos abriam como anéis (85% da imagem vazia); cortados em dois, endireitados e postos em pé lado a lado → 4× menos imagem (344 × 1524 em vez de 1452 × 1468), mesma nitidez. Sem metálico (pedra).

*window_01:* vitral "ARCADE" **semitransparente** (40% do alpha é intermediário). Recorte do retângulo do atlas (exato), cor + alpha numa imagem RGBA, exportada como **BLEND**. ⚠️ O motor hoje só faz recorte (corte em 50%): o vitral aparece recortado até existir transparência com mistura (ver roteiro).

*Escadas:* o usuário separou o `stairs` original em duas (4 e 2 degraus). Bake na mesma densidade do original (~261 px/m), sem metálico (pedra). Origem no **canto de baixo** (encaixe na grade, como as paredes); orientação como estão na cena.

*column_01:* **malha refeita** (2026-11-06): cada um dos 48 tijolos virou uma caixa com chanfro de 1 segmento + núcleo em caixa fechada (em cima e embaixo, dá pra girar 180°): **9.280 → 2.124 triângulos**. 1ª tentativa com todos os tijolos empilhados numa UV só ficou estranha; versão final: **cada tijolo com sua área na UV** (bake do seu tijolo original), faces escondidas (coladas no núcleo/vizinho) com UV num ponto só (não gastam imagem); faces nas frestas que saíram tortas com normal lisa; tampas do núcleo pintadas com a pedra da lateral (a original era aberta). Sem mapa metálico (era todo preto). Imagens em JPEG dentro do .glb: 4,5 → 1,0 MB. Origem no **centro da base** (fica na emenda entre paredes). A `Column.001` da cena é a mesma coluna girada: não precisa fazer.

## Props

Props com UV refeita: a textura vem por **bake** do original (uma imagem só do item), não por recorte.

*Placa da taverna:* 5 peças separadas (o usuário tirou as repetidas): tavern_label_01, mast_01, hook_01, chain_two_01, chain_three_01. **Origem no topo** de cada peça (centro, ponto de pendurar); a placa é montada à mão no motor.

| Item | Medida (m) | Imagem | px/m | Upscale | Blender | Resolução | Obs. |
|---|---|---|---|---|---|---|---|
| torch_01 | 0.44 × 0.44 × 1.49 | 512 × 512 | 508 | cor: IA ×4 → 512 (nitidez) | ✅ | ✅ | UV nova com peças repetidas empilhadas; bake sem IA guardado no .blend (`torch_01_BaseColor_bake`) |
| duck_hunt_table_01 | 3.15 × 0.50 × 0.68 | 776 × 1024 | 326 | cor: IA ×4 → 776 × 1024 (nitidez) | ✅ | ✅ | balcão do tiro ao alvo; imagem cortada na largura (sem espaço vazio); 4 peças empilhadas |
| bow_01 | 1.39 × 0.27 × 0.04 | 124 × 256 | 377 | cor: IA ×4 → 124 × 256 (nitidez) | ✅ | ✅ | arco; a corda (lisa) ficou com menos pixels para o corpo ganhar espaço |
| arrow_01 | 0.45 × 0.61 × 0.12 | 116 × 128 | 750 (haste 263) | cor: IA ×4 → 116 × 128 (nitidez) | ✅ | ✅ | flecha (fonte: `arrows_modify`, separada do `arrows`); exportada inclinada como está na cena; haste encolhida |
| mug_01 | 0.15 × 0.11 × 0.13 | 128 × 128 | 342 | cor: IA ×4 → 128 (nitidez) | ✅ | ✅ | caneca de metal (fonte: `mug22`); é igual às 7 do `mug2` → `mug2` resolvido; costuras tiradas da UV original (Seams from Islands) |
| bottle_01 | 0.12 × 0.12 × 0.36 | 256 × 256 | 662 | cor: IA ×4 → 256 (nitidez) | ✅ | ✅ | garrafa de vinho (fonte: `bottle11`, 1 das 2 iguais do `bottle1`; as outras 3 do conjunto são outro modelo); 256 por causa do **texto do rótulo** (em 128 ficava ilegível) |
| duck_hunt_label_01 | 2.59 × 0.07 × 0.42 | 316 × 1536 | ~590 | cor: IA ×4 → 316 × 1536 (nitidez) | ✅ | ✅ | faixa "DUCK HUNT" (fonte: `duck_hunt_label.001`); alvo de texto (512 px/m); imagem alta e estreita porque a faixa é longa |
| shooting_range_01 | 2.21 × 0.78 × 1.72 | 1508 × 1536 | ~530 | cor: IA ×4 → 1508 × 1536 (nitidez) | ✅ | ✅ | suporte do tiro ao alvo com os patos; alvo de texto (números dos patos) |
| record_board_01 | 0.89 × 0.47 × 1.80 | 1136 × 1136 | ~525 | bake em 768 (~347, o que o original tem) → **IA ×4 → 1136** (upscale de verdade) | ✅ | ✅ | quadro "HIGH SCORES"; texto mais nítido que o original |
| flag_01 | 0.71 × 0.11 × 2.06 | 372 × 1084 | ~512 | bake em 216 × 632 (~298, o do original) → **IA ×4 → 372 × 1084** | ✅ | ✅ | estandarte "ORDEN ARCADE" com cruz; 2º andar |
| table_01 | 0.59 × 4.22 × 0.84 | 1092 × 1124 | 256 | cor: IA ×4 → 1092 × 1124 (nitidez) | ✅ | ✅ | mesa comprida; **decimate do usuário** (6332 → 2532 tris, −60%); bake da mesa ORIGINAL (detalhada) pra reduzida: o normal map recupera o relevo perdido |
| barrel_01 | 1.58 × 1.39 × 1.44 | 1400 × 1400 | 256 | cor: IA ×4 → 1400 (nitidez) | ✅ | ✅ | barril deitado no suporte, com torneira, rótulo "Classic Light" (fonte: `barrels`, o da frente); 3 pares de peças iguais empilhados; com metálico (aros e torneira); imagens JPEG no .glb |
| barrel_02 | 1.58 × 1.39 × 1.44 | 1416 × 1416 | 256 | cor: IA ×4 → 1416 (nitidez) | ✅ | ✅ | mesma malha do barrel_01, **textura diferente** (rótulo "ORIGIN DARK", madeira em outro pedaço do atlas) |
| bench_01 | 1.56 × 0.41 × 0.39 | 460 × 432 | 256 | cor: IA ×4 → 460 × 432 (nitidez) | ✅ | ✅ | banco; **decimate do usuário** (1064 → 424 tris, −60%); bake do banco ORIGINAL (trazido do .blend recuperado) pro reduzido; sem metálico |
| tavern_label_01 | 0.09 × 1.36 × 0.76 | 916 × 916 | ~512 | bake em 632 (352, o do original) → **IA ×4 → 916** | ✅ | ✅ | placa "The Strong Mead" (texto só de um lado, como no original); peça da placa |
| mast_01 | 0.13 × 1.43 × 0.13 | 172 × 344 | 256 | cor: IA ×4 → 172 × 344 (nitidez) | ✅ | ✅ | haste da placa; peças repetidas empilhadas |
| hook_01 | 0.19 × 0.05 × 0.19 | 68 × 68 | 256 | cor: IA ×4 → 68 (nitidez) | ✅ | ✅ | gancho da placa (metal) |
| chain_two_01 | 0.08 × 0.08 × 0.21 | 76 × 76 | 256 | cor: IA ×4 → 76 (nitidez) | ✅ | ✅ | corrente de 2 elos (metal) |
| chain_three_01 | 0.08 × 0.08 × 0.36 | 76 × 76 | 256 | cor: IA ×4 → 76 (nitidez) | ✅ | ✅ | corrente de 3 elos; elos iguais empilhados |
| shield_01 | 0.94 × 0.10 × 1.31 | 404 × 404 | 256 | cor: IA ×4 → 404 (nitidez) | ✅ | ✅ | escudo com espadas cruzadas; brasão sem texto (256 bastou; original tinha 352); 4 peças empilhadas |
