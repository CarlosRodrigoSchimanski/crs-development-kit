"""crsdk: a API da CRS Development Kit para scripts Python.

O módulo existe só dentro da engine (não dá pra instalar com pip). Este arquivo é a
referência: o VS Code lê ele para autocompletar e mostrar a ajuda de cada função.
Mantido à mão: mudou src/engine/scripting/crs_python_api.cpp, atualizar aqui junto
(as teclas ficam em keys.pyi, os botões do mouse em mouse.pyi).

Exemplo::

    import crsdk

    player = crsdk.scene.find("Player")
    if crsdk.input.isDown(crsdk.keys.SPACE):
        player.transform.translation.y += 0.1

Os objetos da engine (GameObject, Transform, Animator...) são entregues por referência:
mudar um campo muda o objeto de verdade na cena. Um GameObject guardado num script
vale enquanto a cena dele existir (a cena do Play some ao parar o Play).
"""

from typing import List, Optional

from . import keys as keys
from . import mouse as mouse

scene: Scene
"""A cena ativa: a do editor, ou a cópia do Play enquanto o jogo roda."""

input: Input
"""Teclado e mouse neste frame."""

window: Window
"""A janela do jogo."""

deltaTime: float
"""Segundos desde o frame anterior. Multiplique a velocidade por ele: o movimento fica
igual em qualquer FPS (ex: ``x += 2 * crsdk.deltaTime`` anda 2 unidades por segundo)."""


class Component:
    """Base dos componentes: um comportamento preso a um objeto, rodando durante o Play.

    Crie um arquivo em ``assets/components/``, herde desta classe e escreva ``start`` e
    ``update``. O arquivo pode importar outros da mesma pasta (ou de assets/scripts)::

        import crsdk
        from comum.matematica import lerp   # assets/components/comum/matematica.py

        class Girar(crsdk.Component):
            args = {"velocidade": 90.0}

            def start(self, args):
                self.velocidade = args["velocidade"]

            def update(self):
                self.object.transform.rotation.y += self.velocidade * crsdk.deltaTime

    Um erro dentro do componente aparece na tela e tira só ele do Play; o jogo continua.
    """

    args: dict
    """Os parâmetros do componente e o valor padrão de cada um. O ``start`` recebe uma
    cópia: mudar o dicionário recebido não afeta os outros objetos."""
    object: GameObject
    """O objeto em que o componente está preso."""

    def __init__(self, obj: GameObject) -> None: ...
    def start(self, args: dict) -> None:
        """Chamado uma vez ao dar Play, antes do primeiro update."""
    def update(self) -> None:
        """Chamado todo frame durante o Play."""


class Vec2:
    """Vetor 2D (posição e movimento do mouse)."""

    x: float
    y: float

    def __init__(self, x: float = 0.0, y: float = 0.0) -> None: ...
    def __add__(self, other: Vec2) -> Vec2: ...
    def __sub__(self, other: Vec2) -> Vec2: ...
    def __mul__(self, scalar: float) -> Vec2: ...
    def __rmul__(self, scalar: float) -> Vec2: ...
    def __truediv__(self, scalar: float) -> Vec2: ...
    def __neg__(self) -> Vec2: ...
    def __eq__(self, other: object) -> bool: ...
    def copy(self) -> Vec2:
        """Uma cópia independente (mudar a cópia não muda o original)."""
    def length(self) -> float:
        """O tamanho do vetor."""


class Vec3:
    """Vetor 3D: posição, rotação (radianos), escala, direção.

    Vindo de um objeto (ex: ``obj.transform.translation``), o vetor É o do objeto:
    ``t = obj.transform.translation; t.y += 1`` move o objeto. Para guardar o valor
    de agora sem acompanhar mudanças, use ``copy()``.
    """

    x: float
    y: float
    z: float

    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None: ...
    def __add__(self, other: Vec3) -> Vec3: ...
    def __sub__(self, other: Vec3) -> Vec3: ...
    def __mul__(self, scalar: float) -> Vec3: ...
    def __rmul__(self, scalar: float) -> Vec3: ...
    def __truediv__(self, scalar: float) -> Vec3: ...
    def __neg__(self) -> Vec3: ...
    def __eq__(self, other: object) -> bool: ...
    def copy(self) -> Vec3:
        """Uma cópia independente (mudar a cópia não muda o original)."""
    def length(self) -> float:
        """O tamanho do vetor."""
    def normalized(self) -> Vec3:
        """O mesmo sentido com tamanho 1. O vetor zero continua zero."""
    def dot(self, other: Vec3) -> float:
        """Produto escalar."""
    def cross(self, other: Vec3) -> Vec3:
        """Produto vetorial (perpendicular aos dois)."""


class Transform:
    """Posição, rotação e escala do objeto, relativas ao pai (ou ao mundo, sem pai)."""

    translation: Vec3
    rotation: Vec3
    """Rotação em radianos em cada eixo (use math.radians para converter de graus)."""
    scale: Vec3


class Animator:
    """Toca as animações de um modelo com esqueleto."""

    clip: str
    """Nome da animação tocando agora (só leitura: troque com play())."""
    animations: List[str]
    """Arquivos de animação do objeto (só leitura)."""
    duration: float
    """Duração da animação atual em segundos (só leitura; 0 = a engine ainda não carregou)."""
    time: float
    """Instante atual da animação, em segundos."""
    speed: float
    """Velocidade: 1 = normal, 2 = o dobro, 0 = parada."""
    loop: bool
    """Recomeça ao chegar no fim."""

    def play(self, name: str, fade: float = 0.2, loop: bool = True) -> None:
        """Troca para a animação ``name`` misturando com a atual por ``fade`` segundos.
        Pedir a mesma que já toca não recomeça (pode chamar todo frame)."""
    def isBlending(self) -> bool:
        """Está no meio de uma transição entre duas animações."""
    def blendWeight(self) -> float:
        """Quanto vale a animação nova na mistura: 0 = só a anterior, 1 = só a nova."""
    def clipTime(self) -> float:
        """O instante dentro da animação atual, em segundos (no loop, volta a 0 a cada volta)."""
    def progress(self) -> float:
        """Quanto da animação já tocou, de 0 a 1 (não depende da velocidade)."""
    def isFinished(self) -> bool:
        """A animação sem loop chegou ao fim."""
    def frame(self, fps: float = 30.0) -> int:
        """O quadro atual, contando ``fps`` quadros por segundo (o Mixamo usa 30)."""


class CharacterInput:
    """O que quem controla o personagem quer fazer neste frame. Um componente (o do jogador, uma
    IA) preenche; o controlador da engine executa (velocidade, pulo só no chão, gravidade)."""

    moveDirection: Vec3
    """Pra onde andar, no mundo (só x e z contam). Tamanho 1 = velocidade cheia; maior que 1 é cortado."""
    jump: bool
    """Pular neste frame. A engine apaga depois de usar: peça de novo a cada aperto."""
    run: bool
    """Correr (runSpeed) em vez de andar (moveSpeed)."""


class Character:
    """Personagem controlável. Sem ninguém preenchendo o ``input``, ele fica parado."""

    moveSpeed: float
    """Velocidade andando (unidades por segundo)."""
    runSpeed: float
    """Velocidade correndo."""
    jumpSpeed: float
    """Velocidade inicial do pulo."""
    input: CharacterInput
    """A intenção deste frame."""


class Shape:
    """Forma de colisão de um RigidBody."""
    Box: "Shape"
    Sphere: "Shape"
    Capsule: "Shape"


class RigidBody:
    """Corpo físico: forma de colisão, massa e velocidade."""

    isStatic: bool
    """True = parado (chão, parede); False = a gravidade age nele."""
    shape: Shape
    """Box, Sphere ou Capsule (mudar refaz o corpo na física)."""
    halfExtents: Vec3
    """Caixa: metade do tamanho em cada eixo (antes da escala do objeto)."""
    radius: float
    """Esfera e cápsula: o raio."""
    height: float
    """Cápsula: altura total, de ponta a ponta (de pé, no eixo Y)."""
    center: Vec3
    """Centro da forma em relação à origem do objeto."""
    mass: float
    """Massa em kg (quanto mais pesado, mais difícil de empurrar)."""
    velocity: Vec3
    """Velocidade atual; mudar dá um impulso (ex: velocity.y = 5 pula)."""
    onGround: bool
    """Está apoiado em algo (só leitura: quem decide é a física)."""


class Camera:
    fovY: float
    """Ângulo de visão vertical, em radianos."""
    nearPlane: float
    farPlane: float


class PointLight:
    intensity: float
    """Força da luz (a cor é o ``color`` do objeto)."""


class GameObject:
    """Um objeto da cena. Não se cria direto: use ``crsdk.scene.createObject()``."""

    id: int
    """Identificador único na cena (só leitura)."""
    name: str
    """Nome mostrado no editor."""
    transform: Transform
    color: Vec3
    parentId: Optional[int]
    """Id do pai, ou None se o objeto é raiz."""
    parent: Optional[GameObject]
    """O pai (None = raiz). Trocar o pai não mexe no transform: ele passa a valer relativo
    ao novo pai. Não aceita o próprio objeto nem um filho dele (ValueError)."""
    children: List[GameObject]
    """Os filhos diretos, em ordem de id (só leitura)."""
    worldPosition: Vec3
    """Posição no mundo, somando os pais (só leitura)."""
    animator: Optional[Animator]
    """O tocador de animações, ou None se o objeto não tem."""
    character: Optional[Character]
    """O personagem controlável, ou None."""
    rigidBody: Optional[RigidBody]
    """O corpo físico, ou None."""
    camera: Optional[Camera]
    """A lente, se o objeto é uma câmera, ou None."""
    pointLight: Optional[PointLight]
    """A luz pontual, ou None."""

    def worldDirection(self, local: Vec3) -> Vec3:
        """Uma direção do objeto vista no mundo, com tamanho 1. Ex: numa câmera,
        ``worldDirection(Vec3(0, 0, -1))`` é pra onde ela olha."""
    def applyRotation(self, radians: Vec3, local: bool = True) -> None:
        """Gira mais um tanto (radianos em cada eixo). ``local=True`` gira em volta dos eixos do
        próprio objeto; ``False``, dos eixos do mundo (num filho também)."""


class RayHit:
    """O que um raycast acertou."""

    object: GameObject
    distance: float
    """Distância da origem do raio até o impacto, em unidades do mundo."""
    point: Vec3
    """Ponto do impacto no mundo."""


class Scene:
    objects: List[GameObject]
    """Todos os objetos, em ordem de id (só leitura)."""
    activeCamera: Optional[GameObject]
    """A câmera usada no Play."""

    def find(self, name: str) -> Optional[GameObject]:
        """O primeiro objeto (menor id) com esse nome, ou None."""
    def findById(self, id: int) -> Optional[GameObject]:
        """O objeto com esse id, ou None."""
    def createObject(self, name: str = "") -> GameObject:
        """Cria um objeto vazio na cena e devolve ele para configurar."""
    def raycast(self, origin: Vec3, direction: Vec3, maxDistance: float,
                ignore: Optional[GameObject] = None) -> Optional[RayHit]:
        """Lança um raio contra as caixas de colisão (objetos com corpo físico) e devolve o
        acerto mais perto até ``maxDistance``, ou None. ``direction`` não precisa ter
        tamanho 1. ``ignore`` fica de fora (ex: o próprio personagem)."""


class Input:
    """Teclado e mouse. As teclas são as constantes de ``crsdk.keys``; os botões, de ``crsdk.mouse``."""

    mousePosition: Vec2
    """Posição do cursor na janela em pixels (origem no canto de cima, à esquerda)."""
    mouseDelta: Vec2
    """Quanto o mouse andou desde o frame anterior, em pixels (funciona com o cursor capturado)."""

    def isDown(self, key: int) -> bool:
        """A tecla está apertada agora (segurar conta em todo frame)."""
    def wasPressed(self, key: int) -> bool:
        """A tecla foi apertada neste frame (um frame por aperto)."""
    def wasReleased(self, key: int) -> bool:
        """A tecla foi solta neste frame."""
    def isMouseDown(self, button: int) -> bool:
        """O botão do mouse está apertado agora."""
    def wasMousePressed(self, button: int) -> bool:
        """O botão do mouse foi apertado neste frame."""


class Window:
    width: int
    """Largura da janela em pixels."""
    height: int
    """Altura da janela em pixels."""
