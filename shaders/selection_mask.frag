#version 450

//na máscara, todo pixel coberto pelo objeto vale 1 (o resto ficou 0 na limpeza)
layout(location = 0) out float outMask;

void main() {
    outMask = 1.0;
}
