import crsdk
from crsdk import Vec3, keys


class ControleJogador(crsdk.Component):
    """Move o personagem com WASD em relação à câmera; Espaço pula, Shift corre."""

    args = {
        "correrComShift": True,
    }

    def start(self, args):
        self.correrComShift = args["correrComShift"]

    def update(self):
        personagem = self.object.character
        if personagem is None:
            return
        teclas = crsdk.input

        # frente e direita da câmera, achatadas no chão (a câmera olha pro -Z dela)
        frente = Vec3(0, 0, -1)
        direita = Vec3(1, 0, 0)
        camera = crsdk.scene.activeCamera
        if camera is not None:
            frenteCamera = camera.worldDirection(Vec3(0, 0, -1))
            direitaCamera = camera.worldDirection(Vec3(1, 0, 0))
            frenteCamera.y = 0
            direitaCamera.y = 0
            # câmera olhando reto pra baixo: a frente achatada some, fica a do mundo
            if frenteCamera.length() > 0.001:
                frente = frenteCamera.normalized()
            if direitaCamera.length() > 0.001:
                direita = direitaCamera.normalized()

        direcao = Vec3()
        if teclas.isDown(keys.W):
            direcao += frente
        if teclas.isDown(keys.S):
            direcao -= frente
        if teclas.isDown(keys.D):
            direcao += direita
        if teclas.isDown(keys.A):
            direcao -= direita

        # o controlador do C++ faz o resto: velocidade, pulo só no chão, gravidade
        personagem.input.moveDirection = direcao
        personagem.input.jump = teclas.wasPressed(keys.SPACE)
        personagem.input.run = self.correrComShift and teclas.isDown(keys.LEFT_SHIFT)
