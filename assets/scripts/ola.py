# teste do passo 2: o Python enxerga a cena da engine pelo módulo crsdk
import sys

import crsdk

print("Olá do Python", sys.version.split()[0], "dentro da engine!", flush=True)
print(f"janela {crsdk.window.width}x{crsdk.window.height}, objetos na cena:", flush=True)
for obj in crsdk.scene.objects:
    print("  ", obj.id, obj.name, obj.worldPosition, flush=True)
