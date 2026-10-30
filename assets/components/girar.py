import math

import crsdk


class Girar(crsdk.Component):
    """Exemplo: gira o objeto em volta do eixo Y."""

    args = {"velocidade": 90.0}  # graus por segundo

    def start(self, args):
        self.velocidade = math.radians(args["velocidade"])

    def update(self):
        self.object.transform.rotation.y += self.velocidade * crsdk.deltaTime
