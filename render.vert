#version 460
#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require

layout(location = 0) out vec3 fragColor;

struct Vertex {
    float vx, vy, vz;
    float nx, ny, nz;
    float u, v;
};
// "buffer_reference" means we are defining a pointer type
// "scalar" means we are aligning everything based on its scalar components
layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};
layout(push_constant, scalar) uniform PushConstants {
    VertexBuffer vertexBuffer;
};

layout(set = 0, binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

void main() {
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(
        vertexBuffer.vertices[gl_VertexIndex].vx, 
        vertexBuffer.vertices[gl_VertexIndex].vy, 
        vertexBuffer.vertices[gl_VertexIndex].vz, 1.0);
    fragColor = vec3(
        vertexBuffer.vertices[gl_VertexIndex].nx, 
        vertexBuffer.vertices[gl_VertexIndex].ny, 
        vertexBuffer.vertices[gl_VertexIndex].nz);
}