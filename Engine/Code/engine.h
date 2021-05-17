//
// engine.h: This file contains the types and functions relative to the engine.
//

#pragma once

#include "platform.h"
#include <glad/glad.h>

typedef glm::vec2  vec2;
typedef glm::vec3  vec3;
typedef glm::vec4  vec4;
typedef glm::ivec2 ivec2;
typedef glm::ivec3 ivec3;
typedef glm::ivec4 ivec4;

struct VertexShaderAttribute;
struct VertexBufferAttribute;

using namespace glm;

struct VertexV3V2
{
    vec3 pos;
    vec2 uv;
};

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
};

//Relate VertexBuffers and Shaders
struct Vao
{
    GLuint handle;
    GLuint programHandle;
};


// MODELS AND MATERIALS

struct Submesh
{
    VertexBufferLayout vertexBufferLayout;
    std::vector<float> vertices;
    std::vector<u32>   indices;
    u32                vertexOffset;
    u32                indexOffset;

    std::vector<Vao> vaos;
};

struct Mesh
{
    std::vector<Submesh> submeshes;
    GLuint               vertexBufferHandle;
    GLuint               indexBufferHandle;
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


struct Entity
{
    mat4        worldMatrix;
    u32         modelIndex;
    u32         localParamsOffset;
    u32         localParamsSize;

    void TransformPosition(const vec3& pos)
    {
        this->worldMatrix = translate(this->worldMatrix, pos);
    }

    void TransformScale(const vec3& scaleFactors)
    {
        this->worldMatrix = scale(this->worldMatrix, scaleFactors);
    }

    void TransformPositionScale(const vec3& pos, const vec3& scaleFactors)
    {
        this->worldMatrix = translate(this->worldMatrix, pos);
        this->worldMatrix = scale(this->worldMatrix, scaleFactors);
    }
};
// ---------------------------------------

enum LightType
{
    Directional,
    Point
};

struct Light //efficient alignment
{    
    vec3 position;
    vec3 direction;
    LightType type;
    vec3 color;
    float range;    
};

struct Image
{
    void* pixels;
    ivec2 size;
    i32   nchannels;
    i32   stride;
};

struct Texture
{
    GLuint      handle;
    std::string filepath;
};

struct Program
{
    GLuint             handle;
    std::string        filepath;
    std::string        programName;
    u64                lastWriteTimestamp; 

    VertexShaderLayout vertexShaderLayout;
};

struct Buffer 
{
    GLuint handle;
    GLenum type;
    u32 size;
    u32 head;
    void* data; //mapped data
};

struct OpenGLInfo
{
    std::string OpenGLversion;
    std::string GPU;
    std::string Vendor;
    std::string GLSLverison;
};

enum class Mode
{
    Mode_TexturedQuad,
    Mode_TexturedMesh,
    Mode_TexturedNormals,
    Mode_TexturedDepth,
    Mode_TexturedAlbedo,
    Mode_Count
};


class OpenGLErrorGuard
{
public:
    OpenGLErrorGuard(const char* message) : msg(message) {
        checkGLError("BEGIN", msg);
    }

    ~OpenGLErrorGuard() {
        checkGLError("END", msg);
    }

    static void checkGLError(const char* around, const char* message);
    const char* msg;
};

struct Camera
{

    mat4 ViewMatrix;
    mat4 ProjectionMatrix;

    vec3 worldUp;
    vec3 position;
    vec3 front;
    vec3 right;
    vec3 up;

    float pitch;
    float yaw;
    float roll;

    float fov;
    float aspect;
    float near_plane;
    float far_plane;

    void CameraInit(vec3 position, vec3 direction, vec3  worldup, float aspect)
    {
        this->ViewMatrix = mat4(1.f);
        this->ViewMatrix = mat4(1.f);

        this->position = position;
        this->worldUp = worldup;
        this->up = worldup;
        this->right = vec3(0.f);

        this->pitch = 0.f;
        this->yaw = -90.f;
        this->roll = 0.f;

        this->fov = 60.f;
        this->aspect = aspect;
        this->near_plane = 0.1f;
        this->far_plane = 1000.f;

        this->UpdateCameraVectors();
    }

    void UpdateCameraVectors()
    {
        this->front.x = cos(radians(this->yaw)) * cos(radians(this->pitch));
        this->front.y = sin(radians(this->pitch));
        this->front.z = sin(radians(this->yaw)) * cos(radians(this->pitch));

        this->front = normalize(this->front);
        this->right = normalize(cross(this->front, this->worldUp));
        this->up = normalize(cross(this->right, this->front));
    }

    const mat4 GetViewMatrix()
    {
        this->UpdateCameraVectors();

        this->ViewMatrix = lookAt(this->position, this->position + this->front, this->up);

        return this->ViewMatrix;
    }

    const mat4 GetProjectionMatrix()
    {
        this->ProjectionMatrix = perspective(radians(this->fov), this->aspect, this->near_plane, this->far_plane);

        return this->ProjectionMatrix;
    }
};

struct App
{
    // Loop
    f32  deltaTime;
    bool isRunning;

    // Input
    Input input;

    // Graphics
    OpenGLInfo Info;
    //char gpuName[64];
    //char openGlVersion[64];

    ivec2 displaySize;

    Camera camera;

    std::vector<Entity>     entities;

    std::vector<Texture>    textures;
    std::vector<Material>   materials;
    std::vector<Mesh>       meshes;
    std::vector<Model>      models;
    std::vector<Light>      lights;
    std::vector<Program>    programs;

    // program indices
    u32 texturedGeometryProgramIdx;
    u32 GeometryPassProgramIdx;
    u32 ShadingPassProgramIdx;

    //Uniform buffers info
    GLint maxUniformBufferSize;
    GLint uniformBlockAlignment;

    u32 globalParamOffset;
    u32 globalParamSize;
    Buffer cbuffer;
    
    // texture indices
    u32 diceTexIdx;
    u32 whiteTexIdx;
    u32 blackTexIdx;
    u32 normalTexIdx;
    u32 magentaTexIdx;

    // Mode
    Mode mode;

    // Embedded geometry (in-editor simple meshes such as
    // a screen filling quad, a cube, a sphere...)
    GLuint embeddedVertices;
    GLuint embeddedElements;

    // Location of the texture uniform in the textured quad shader
    GLuint programUniformTexture;
  

    // VAO object to link our screen filling quad with our textured quad shader
    GLuint vao;

    // Frame buffer variables
    GLuint colorAttachmentHandle;
    GLuint albedoAttachmentHandle;
    GLuint normalAttachmentHandle;
    GLuint positionAttachmentHandle;
    GLuint depthTextureHandle;
    GLuint depthAttachmentHandle;

    GLuint framebufferHandle;

};


u32 LoadTexture2D(App* app, const char* filepath);

void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

void OnGlError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program);

u32 LoadModel(App* app, const char* filename);

Entity CreatePlane(App* app, float size);
Entity CreateSphere(App* app);


bool IsPowerOf2(u32 value);
u32 Align(u32 value, u32 alignment);
Buffer CreateBuffer(u32 size, GLenum type, GLenum usage);
void BindBuffer(const Buffer& buffer);
void MapBuffer(Buffer& buffer, GLenum access);
void UnmapBuffer(Buffer& buffer);
void AlignHead(Buffer& buffer, u32 alignment);
void PushAlignedData(Buffer& buffer, const void* data, u32 size, u32 alignment);