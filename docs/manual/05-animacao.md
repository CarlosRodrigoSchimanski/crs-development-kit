# Animação

> Capítulo em construção. Por enquanto ele cobre as animações automáticas do personagem.

## Animações do personagem

O visual do personagem (o modelo, filho da cápsula de física) troca de animação sozinho no **Play**, conforme o que o corpo está fazendo:

| Situação | Animação | Quando |
|---|---|---|
| Parado | **idle** | no chão, quase sem velocidade |
| Andando | **walk** | no chão, na velocidade de andar |
| Correndo | **run** | no chão, mais rápido que o andar (Shift) |
| No ar | **fall** | pulou, ou está caindo há um tempo (veja abaixo). Repete até pousar |

A troca entre uma e outra é suave: as duas se misturam durante o **fadeTime** (padrão **0,2 s**).

### Quando a queda começa

- **Pulou:** a queda começa **na hora**.
- **Saiu do chão sem pular** (passou da beirada, desceu um degrau, encostou numa parede correndo): a queda só começa depois de **fallDelay** segundos no ar (padrão **0,5 s**). Antes disso, continua a animação de andar ou correr.

A espera existe porque, correndo contra uma parede ou passando por um degrau, o personagem "sai do chão" por alguns frames, um tropeço que nem aparece na tela. Sem a espera, a animação ficava trocando sem parar entre correr e cair.

- **fallDelay maior** (por exemplo 1 s): só quedas longas mostram a animação de cair. Em quedas curtas ele pousa ainda "correndo".
- **fallDelay menor** (por exemplo 0,1 s): a queda aparece logo, mas tropeços mais longos podem voltar a piscar.

### Onde configurar

Por enquanto não há card no editor para isso. Os valores ficam na cena (`assets/scenes/<cena>.json`), no objeto do visual:

```json
"characterAnimation": {
    "idle": "idle_01",
    "walk": "walking_01",
    "run": "running_01",
    "fall": "fall_01",
    "fadeTime": 0.2,
    "fallDelay": 0.5
}
```

Os nomes são os das animações dentro do modelo (`.glb`). Nome vazio usa a primeira animação do modelo. Uma cena salva antes do **fallDelay** usa o padrão de 0,5 s, e o valor passa a ser gravado no próximo **Ctrl + S**.
