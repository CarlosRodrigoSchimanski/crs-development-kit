//push constant da prévia de material: tem que bater com o PreviewPushConstants do material_preview.cpp
layout(push_constant) uniform Push {
    mat4 viewProjection;
    vec4 cameraPosition;  //xyz = de onde a câmera de estúdio olha
} push;
