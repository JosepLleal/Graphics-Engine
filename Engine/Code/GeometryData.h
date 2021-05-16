#pragma once

#include "platform.h"
#include "engine.h"
#include <glad/glad.h>

// Structs to organize VBO EBO SHADER VAO

struct VertexBufferLayout
{
    std::vector<VertexBufferAttribute> attributes;
    u8 stride;
};

struct VertexBufferAttribute
{
    u8 location; //like attribute_id
    u8 componentCount;
    u8 offset;
};

struct VertexShaderLayout
{
    std::vector<VertexShaderAttribute> attributes;
};

struct VertexShaderAttribute
{
    u8 location;
    u8 componentCount;
    std::string name;
    GLenum type;
};

//Relate VertexBuffers and Shaders
struct Vao
{
    GLuint handle;
    GLuint programHandle;
};


// MODELS AND MATERIALS

struct Mesh
{
    std::vector<Submesh> submeshes;
    GLuint               vertexBufferHandle;
    GLuint               indexBufferHandle;
};

struct Submesh
{
    VertexBufferLayout vertexBufferLayout;
    std::vector<float> vertices;
    std::vector<u32>   indices;
    u32                vertexOffset;
    u32                indexOffset;

    std::vector<Vao> vaos;
};

struct Material
{
    std::string     name;
    vec3            albedo;
    vec3            emissive;
    f32             smoothness;

    u32             albedoTextureIdx;
    u32             emissiveTextureIdx;
    u32             specularTextureIdx;
    u32             normalsTextureIdx;
    u32             bumpTextureIdx;
};


struct Model
{
    u32              meshIdx;
    std::vector<u32> materialIdx;
};


