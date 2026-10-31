#version 450

//um triângulo só que cobre a tela inteira (sem buffer de vértices: sai do número do vértice)

void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
