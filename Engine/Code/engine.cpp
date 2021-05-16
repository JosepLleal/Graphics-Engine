//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"

#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define BINDING(b) b

#define CreateConstantBuffer(size) CreateBuffer(size, GL_UNIFORM_BUFFER, GL_STREAM_DRAW)
#define CreateStaticVertexBuffer(size) CreateBuffer(size, GL_ARRAY_BUFFER, GL_STATIC_DRAW)
#define CreateStaticIndexBuffer(size) CreateBuffer(size, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW)

#define PushData(buffer, data, size) PushAlignedData(buffer, data, size, 1)
#define PushUInt(buffer, value) { u32 v = value; PushAlignedData(buffer, &v, sizeof(v), 4); }
#define PushVec3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushVec4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))

GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
    GLchar  infoLogBuffer[1024] = {};
    GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
    GLsizei infoLogSize;
    GLint   success;

    char versionString[] = "#version 430\n";
    char shaderNameDefine[128];
    sprintf(shaderNameDefine, "#define %s\n", shaderName);
    char vertexShaderDefine[] = "#define VERTEX\n";
    char fragmentShaderDefine[] = "#define FRAGMENT\n";

    const GLchar* vertexShaderSource[] = {
        versionString,
        shaderNameDefine,
        vertexShaderDefine,
        programSource.str
    };
    const GLint vertexShaderLengths[] = {
        (GLint) strlen(versionString),
        (GLint) strlen(shaderNameDefine),
        (GLint) strlen(vertexShaderDefine),
        (GLint) programSource.len
    };
    const GLchar* fragmentShaderSource[] = {
        versionString,
        shaderNameDefine,
        fragmentShaderDefine,
        programSource.str
    };
    const GLint fragmentShaderLengths[] = {
        (GLint) strlen(versionString),
        (GLint) strlen(shaderNameDefine),
        (GLint) strlen(fragmentShaderDefine),
        (GLint) programSource.len
    };

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint programHandle = glCreateProgram();
    glAttachShader(programHandle, vshader);
    glAttachShader(programHandle, fshader);
    glLinkProgram(programHandle);
    glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    glUseProgram(0);

    glDetachShader(programHandle, vshader);
    glDetachShader(programHandle, fshader);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
    String programSource = ReadTextFile(filepath);

    Program program = {};
    program.handle = CreateProgramFromSource(programSource, programName);
    program.filepath = filepath;
    program.programName = programName;
    program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);
    app->programs.push_back(program);

    return app->programs.size() - 1;
}

Image LoadImage(const char* filename)
{
    Image img = {};
    stbi_set_flip_vertically_on_load(true);
    img.pixels = stbi_load(filename, &img.size.x, &img.size.y, &img.nchannels, 0);
    if (img.pixels)
    {
        img.stride = img.size.x * img.nchannels;
    }
    else
    {
        ELOG("Could not open file %s", filename);
    }
    return img;
}

void FreeImage(Image image)
{
    stbi_image_free(image.pixels);
}

GLuint CreateTexture2DFromImage(Image image)
{
    GLenum internalFormat = GL_RGB8;
    GLenum dataFormat     = GL_RGB;
    GLenum dataType       = GL_UNSIGNED_BYTE;

    switch (image.nchannels)
    {
        case 3: dataFormat = GL_RGB; internalFormat = GL_RGB8; break;
        case 4: dataFormat = GL_RGBA; internalFormat = GL_RGBA8; break;
        default: ELOG("LoadTexture2D() - Unsupported number of channels");
    }

    GLuint texHandle;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_2D, texHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.size.x, image.size.y, 0, dataFormat, dataType, image.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texHandle;
}

u32 LoadTexture2D(App* app, const char* filepath)
{
    for (u32 texIdx = 0; texIdx < app->textures.size(); ++texIdx)
        if (app->textures[texIdx].filepath == filepath)
            return texIdx;

    Image image = LoadImage(filepath);

    if (image.pixels)
    {
        Texture tex = {};
        tex.handle = CreateTexture2DFromImage(image);
        tex.filepath = filepath;

        u32 texIdx = app->textures.size();
        app->textures.push_back(tex);

        FreeImage(image);
        return texIdx;
    }
    else
    {
        return UINT32_MAX;
    }
}

void Init(App* app)
{
    glEnable(GL_DEBUG_OUTPUT);
    
    glDebugMessageCallback(OnGlError, app);

    glEnable(GL_DEPTH_TEST);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        ELOG("OpenGL error: %s", glGetString(err));
    }

    // Retrieve OpenGL information
    app->Info.OpenGLversion = (const char*)glGetString(GL_VERSION);
    app->Info.GPU = (const char*)glGetString(GL_RENDERER);
    app->Info.Vendor = (const char*)glGetString(GL_VENDOR);
    app->Info.GLSLverison = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

    app->mode = Mode::Mode_TexturedQuad;

    //Texture initialization
    app->diceTexIdx = LoadTexture2D(app, "dice.png");
    app->whiteTexIdx = LoadTexture2D(app, "color_white.png");
    app->blackTexIdx = LoadTexture2D(app, "color_black.png");
    app->normalTexIdx = LoadTexture2D(app, "color_normal.png");
    app->magentaTexIdx = LoadTexture2D(app, "color_magenta.png");

    Material Default = {};
    Default.albedoTextureIdx = app->whiteTexIdx;
    app->materials.push_back(Default);

    //Camera initialization
    app->camera.CameraInit(vec3(0.f, 0.f, 5.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), (float)(app->displaySize.x/ app->displaySize.y));
    app->camera.position = vec3( 0.f, 3.5f, 15.f);

    //Creating uniform buffers
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAlignment);
        
    app->cbuffer = CreateConstantBuffer(app->maxUniformBufferSize);


    //Lights
    Light FirstLight = {};
    FirstLight.position = vec3(5.f, 2.f, 5.f);
    FirstLight.type = LightType::Point;
    FirstLight.color = vec3(0.f, 1.f, 0.f);
    FirstLight.range = 30.f;
    app->lights.push_back(FirstLight);

    Light SecondLight = {};
    SecondLight.position = vec3(-5.f, 2.f, 5.f);
    SecondLight.type = LightType::Point;
    SecondLight.color = vec3(1.f, 0.f, 0.f);
    SecondLight.range = 30.f;
    app->lights.push_back(SecondLight);

    Light ThirdLight = {};
    ThirdLight.position = vec3(0.f, 2.f, -10.f);
    ThirdLight.type = LightType::Point;
    ThirdLight.color = vec3(0.f, 0.f, 1.f);
    ThirdLight.range = 30.f;
    app->lights.push_back(ThirdLight);

    Entity plane = CreatePlane(app, 10.f);

    /*Entity sphere = CreateSphere(app);
    sphere.worldMatrix = translate(sphere.worldMatrix, vec3(10.0f, 20.f, 0.0f));*/

    u32 modelIdx = LoadModel(app, "Patrick/Patrick.obj");

    Entity entity = { mat4(1.0f), modelIdx, 0, 0 };
    entity.TransformPosition(vec3(0.0f, 3.5f, 1.0f));
    app->entities.push_back(entity);

    Entity entity1 = { mat4(1.0f), modelIdx, 0, 0 };
    entity1.TransformPosition(vec3(5.0f, 3.5f, -4.0f));
    app->entities.push_back(entity1);
    
    Entity entity2 = { mat4(1.0f), modelIdx, 0, 0 };
    entity2.TransformPosition(vec3(-5.0f, 3.5f, -4.0f));
    app->entities.push_back(entity2);
    
     
    // program initialization
    app->texturedGeometryProgramIdx = LoadProgram(app, "shaders.glsl", "SHOW_TEXTURED_MESH");
    Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];
    app->programUniformTexture = glGetUniformLocation(texturedGeometryProgram.handle, "uTexture");

    //Fill vertex shader layout automatically
    GLint size;
    GLenum type;
    const GLsizei bufSize = 64;
    GLchar name[bufSize];
    GLsizei length;

    GLint count = 0;
    glGetProgramiv(texturedGeometryProgram.handle, GL_ACTIVE_ATTRIBUTES, &count);

    for (GLint i = 0; i < count; i++)
    {
        VertexShaderAttribute attribute = {};
        GLsizei length = 0;
        glGetActiveAttrib(texturedGeometryProgram.handle, (GLuint)i, bufSize,
            &length, &size, &type, name);

        attribute.componentCount = size;

        attribute.location = glGetAttribLocation(texturedGeometryProgram.handle, name);

        texturedGeometryProgram.vertexShaderLayout.attributes.push_back(attribute);
    }
    
}

void Gui(App* app)
{
    ImGui::Begin("Info");
    ImGui::Text("FPS: %f", 1.0f/app->deltaTime);
    ImGui::Separator();
    
    //APP INFO
    ImGui::Text("OpenGL version: %s", app->Info.OpenGLversion.c_str());
    ImGui::Text("OpenGL renderer: %s", app->Info.GPU.c_str());
    ImGui::Text("OpenGL vendor: %s", app->Info.Vendor.c_str());
    ImGui::Text("OpenGL GLSL verison: %s", app->Info.GLSLverison.c_str());
    ImGui::Separator();

    //Camera Movement UI
    ImGui::Text("Camera");
    ImGui::NewLine();
    ImGui::SameLine; ImGui::Text("Posiion X Y Z");

    ImGui::PushItemWidth(50); ImGui::DragFloat("##X1", &app->camera.position.x, 0.1f, -INFINITY, INFINITY);
    ImGui::SameLine(); ImGui::PushItemWidth(50); ImGui::DragFloat("##Y1", &app->camera.position.y, 0.1f, -INFINITY, INFINITY);
    ImGui::SameLine(); ImGui::PushItemWidth(50); ImGui::DragFloat("##Z1", &app->camera.position.z, 0.1f, -INFINITY, INFINITY);
    ImGui::SameLine; ImGui::Text("Pitch/Yaw/Roll");

    ImGui::PushItemWidth(50); ImGui::DragFloat("##X2", &app->camera.pitch, 0.1f, -INFINITY, INFINITY);
    ImGui::SameLine(); ImGui::PushItemWidth(50); ImGui::DragFloat("##Y2", &app->camera.yaw, 0.1f, -INFINITY, INFINITY);
    ImGui::SameLine(); ImGui::PushItemWidth(50); ImGui::DragFloat("##Z2", &app->camera.roll, 0.1f, -INFINITY, INFINITY);
    

    ImGui::End();
}

void Update(App* app)
{
    // You can handle app->input keyboard/mouse here

    app->camera.UpdateCameraVectors();

    // Shader hot reload
    for (u64 i = 0; i < app->programs.size(); i++)
    {
        Program& program = app->programs[i];
        u64 currentTimestamp = GetFileLastWriteTimestamp(program.filepath.c_str());
        if (currentTimestamp > program.lastWriteTimestamp)
        {
            glDeleteProgram(program.handle);
            String programSource = ReadTextFile(program.filepath.c_str());
            const char* programName = program.programName.c_str();
            program.handle = CreateProgramFromSource(programSource, programName);
            program.lastWriteTimestamp = currentTimestamp;
        }
    }


   
    MapBuffer(app->cbuffer, GL_WRITE_ONLY);

    //Global params
    app->globalParamOffset = app->cbuffer.head;
    PushVec3(app->cbuffer, app->camera.position);
    PushUInt(app->cbuffer, app->lights.size());

    for (auto& light : app->lights)
    {
        AlignHead(app->cbuffer, sizeof(vec4));

        PushUInt(app->cbuffer, light.type);
        PushUInt(app->cbuffer, (u32)light.range);
        PushVec3(app->cbuffer, light.color);
        PushVec3(app->cbuffer, light.direction);
        PushVec3(app->cbuffer, light.position);
    }
    app->globalParamSize = app->cbuffer.head - app->globalParamOffset;

    //Local params
    for (auto& entity : app->entities)
    {
        AlignHead(app->cbuffer, app->uniformBlockAlignment);

        mat4 WorldViewProjectionMatrix = app->camera.GetProjectionMatrix() * app->camera.GetViewMatrix() * entity.worldMatrix;

        entity.localParamsOffset = app->cbuffer.head;
        PushMat4(app->cbuffer, entity.worldMatrix);
        PushMat4(app->cbuffer, WorldViewProjectionMatrix);
        entity.localParamsSize = app->cbuffer.head - entity.localParamsOffset;
    }

    UnmapBuffer(app->cbuffer);

    ////Filling Uniform buffers
    //glBindBuffer(GL_UNIFORM_BUFFER, app->bufferHandle);
    //u8* bufferData = (u8*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
    //u32 bufferHead = 0;
    //for (auto& entity : app->entities)
    //{
    //    //align each shader block
    //    bufferHead = Align(bufferHead, app->uniformBlockAlignment);

    //    entity.localParamsOffset = bufferHead;

    //    memcpy(bufferData + bufferHead, value_ptr(entity.worldMatrix), sizeof(mat4));
    //    bufferHead += sizeof(mat4);

    //    mat4 WorldViewProjectionMatrix = app->camera.GetProjectionMatrix() * app->camera.GetViewMatrix() * entity.worldMatrix;

    //    memcpy(bufferData + bufferHead, value_ptr(WorldViewProjectionMatrix), sizeof(mat4));
    //    bufferHead += sizeof(mat4);

    //    entity.localParamsSize = bufferHead - entity.localParamsOffset;

    //}

    //glUnmapBuffer(GL_UNIFORM_BUFFER);
    //glBindBuffer(GL_UNIFORM_BUFFER, 0);

    
}

void Render(App* app)
{
    
    // - clear the framebuffer
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // - set the viewport
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

     // - bind the program 
    Program& texturedMeshProgram = app->programs[app->texturedGeometryProgramIdx];
    glUseProgram(texturedMeshProgram.handle);

    //Binding buffer ranges to uniform blocks (GLOBAL PARAMETERS)
    u32 blockOffset = app->globalParamOffset;
    u32 blockSize = app->globalParamSize;
    glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->cbuffer.handle, blockOffset, blockSize);

    for (auto& entity : app->entities)
    {
        Model& model = app->models[entity.modelIndex];
        Mesh& mesh = app->meshes[model.meshIdx];

        //Binding buffer ranges to uniform blocks (LOCAL PARAMETERS)
        u32 blockOffset = entity.localParamsOffset;
        u32 blockSize = sizeof(mat4) * 2;
        glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), app->cbuffer.handle, blockOffset, blockSize);

        for (u32 i = 0; i < mesh.submeshes.size(); ++i)
        {
            GLuint vao = FindVAO(mesh, i, texturedMeshProgram);
            glBindVertexArray(vao);

            u32 submeshMaterialIdx = model.materialIdx[i];
            Material& submeshMaterial = app->materials[submeshMaterialIdx];

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);
            glUniform1i(app->programUniformTexture, 0);

            Submesh& submesh = mesh.submeshes[i];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
        }
    }
    //Clear vertex array and program
    glBindVertexArray(0);
    glUseProgram(0);

}

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program)
{
    Submesh& submesh = mesh.submeshes[submeshIndex];

    // Try finding a vao for this submesh/program
    for (u32 i = 0; i < (u32)submesh.vaos.size(); ++i)
        if (submesh.vaos[i].programHandle == program.handle)
            return submesh.vaos[i].handle;

    //Create a new vao for this submesh/program
    GLuint vaoHandle = 0;
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

    for (u32 i = 0; i < program.vertexShaderLayout.attributes.size(); ++i)
    {
        bool attributeWasLinked = false;

        for (u32 j = 0; j < submesh.vertexBufferLayout.attributes.size(); ++j)
        {
            if (program.vertexShaderLayout.attributes[i].location == submesh.vertexBufferLayout.attributes[j].location)
            {
                const u32 index = submesh.vertexBufferLayout.attributes[j].location;
                const u32 ncomp = submesh.vertexBufferLayout.attributes[j].componentCount;
                const u32 offset = submesh.vertexBufferLayout.attributes[j].offset+submesh.vertexOffset;
                const u32 stride = submesh.vertexBufferLayout.stride;
                glVertexAttribPointer(index, ncomp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)offset);
                glEnableVertexAttribArray(index);

                attributeWasLinked = true;
                break;
            }
        }
        assert(attributeWasLinked); //The submesh should provide an attribute for each vertex inputs
    }

    glBindVertexArray(0);

    //Store it in the list of vaos of this submesh
    Vao vao = { vaoHandle, program.handle };
    submesh.vaos.push_back(vao);
    return vaoHandle;

}

// ---------------------------------------------------
// ---------- ASSIMP LOADING FUNCTIONS ---------------
//----------------------------------------------------

void ProcessAssimpMesh(const aiScene* scene, aiMesh* mesh, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices)
{
    std::vector<float> vertices;
    std::vector<u32> indices;

    bool hasTexCoords = false;
    bool hasTangentSpace = false;

    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        vertices.push_back(mesh->mVertices[i].x);
        vertices.push_back(mesh->mVertices[i].y);
        vertices.push_back(mesh->mVertices[i].z);
        vertices.push_back(mesh->mNormals[i].x);
        vertices.push_back(mesh->mNormals[i].y);
        vertices.push_back(mesh->mNormals[i].z);

        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            hasTexCoords = true;
            vertices.push_back(mesh->mTextureCoords[0][i].x);
            vertices.push_back(mesh->mTextureCoords[0][i].y);
        }

        if (mesh->mTangents != nullptr && mesh->mBitangents)
        {
            hasTangentSpace = true;
            vertices.push_back(mesh->mTangents[i].x);
            vertices.push_back(mesh->mTangents[i].y);
            vertices.push_back(mesh->mTangents[i].z);

            // For some reason ASSIMP gives me the bitangents flipped.
            // Maybe it's my fault, but when I generate my own geometry
            // in other files (see the generation of standard assets)
            // and all the bitangents have the orientation I expect,
            // everything works ok.
            // I think that (even if the documentation says the opposite)
            // it returns a left-handed tangent space matrix.
            // SOLUTION: I invert the components of the bitangent here.
            vertices.push_back(-mesh->mBitangents[i].x);
            vertices.push_back(-mesh->mBitangents[i].y);
            vertices.push_back(-mesh->mBitangents[i].z);
        }
    }

    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // store the proper (previously proceessed) material for this mesh
    submeshMaterialIndices.push_back(baseMeshMaterialIndex + mesh->mMaterialIndex);

    // create the vertex format
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 });
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) });
    vertexBufferLayout.stride = 6 * sizeof(float);
    if (hasTexCoords)
    {
        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 2 * sizeof(float);
    }
    if (hasTangentSpace)
    {
        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 3, 3, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 3 * sizeof(float);

        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 4, 3, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 3 * sizeof(float);
    }

    // add the submesh into the mesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices.swap(vertices);
    submesh.indices.swap(indices);
    myMesh->submeshes.push_back(submesh);
}

void ProcessAssimpMaterial(App* app, aiMaterial* material, Material& myMaterial, String directory)
{
    aiString name;
    aiColor3D diffuseColor;
    aiColor3D emissiveColor;
    aiColor3D specularColor;
    ai_real shininess;
    material->Get(AI_MATKEY_NAME, name);
    material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
    material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
    material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
    material->Get(AI_MATKEY_SHININESS, shininess);

    myMaterial.name = name.C_Str();
    myMaterial.albedo = vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
    myMaterial.emissive = vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
    myMaterial.smoothness = shininess / 256.0f;

    aiString aiFilename;
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
    {
        material->GetTexture(aiTextureType_DIFFUSE, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.albedoTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_EMISSIVE) > 0)
    {
        material->GetTexture(aiTextureType_EMISSIVE, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.emissiveTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_SPECULAR) > 0)
    {
        material->GetTexture(aiTextureType_SPECULAR, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.specularTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
    {
        material->GetTexture(aiTextureType_NORMALS, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.normalsTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_HEIGHT) > 0)
    {
        material->GetTexture(aiTextureType_HEIGHT, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.bumpTextureIdx = LoadTexture2D(app, filepath.str);
    }

    //myMaterial.createNormalFromBump();
}

void ProcessAssimpNode(const aiScene* scene, aiNode* node, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices)
{
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessAssimpMesh(scene, mesh, myMesh, baseMeshMaterialIndex, submeshMaterialIndices);
    }

    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessAssimpNode(scene, node->mChildren[i], myMesh, baseMeshMaterialIndex, submeshMaterialIndices);
    }
}

u32 LoadModel(App* app, const char* filename)
{
    const aiScene* scene = aiImportFile(filename,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_SortByPType);

    if (!scene)
    {
        ELOG("Error loading mesh %s: %s", filename, aiGetErrorString());
        return UINT32_MAX;
    }

    app->meshes.push_back(Mesh{});
    Mesh& mesh = app->meshes.back();
    u32 meshIdx = (u32)app->meshes.size() - 1u;

    app->models.push_back(Model{});
    Model& model = app->models.back();
    model.meshIdx = meshIdx;
    u32 modelIdx = (u32)app->models.size() - 1u;

    String directory = GetDirectoryPart(MakeString(filename));

    // Create a list of materials
    u32 baseMeshMaterialIndex = (u32)app->materials.size();
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        app->materials.push_back(Material{});
        Material& material = app->materials.back();
        ProcessAssimpMaterial(app, scene->mMaterials[i], material, directory);
    }

    ProcessAssimpNode(scene, scene->mRootNode, &mesh, baseMeshMaterialIndex, model.materialIdx);

    aiReleaseImport(scene);

    u32 vertexBufferSize = 0;
    u32 indexBufferSize = 0;

    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
    {
        vertexBufferSize += mesh.submeshes.at(i).vertices.size() * sizeof(float);
        indexBufferSize += mesh.submeshes.at(i).indices.size() * sizeof(u32);
    }

    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
    {
        const void* verticesData = mesh.submeshes.at(i).vertices.data();
        u32   verticesSize = mesh.submeshes.at(i).vertices.size() * sizeof(float);
        glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
        mesh.submeshes.at(i).vertexOffset = verticesOffset;
        verticesOffset += verticesSize;

        const void* indicesData = mesh.submeshes.at(i).indices.data();
        const u32   indicesSize = mesh.submeshes.at(i).indices.size() * sizeof(u32);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
        mesh.submeshes.at(i).indexOffset = indicesOffset;
        indicesOffset += indicesSize;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return modelIdx;
}

// ---------------------------------------------
// ---------- CREATE PRIMITIVES ----------------
// ---------------------------------------------

Entity CreatePlane(App* app, float size)
{

    const float vertices_array[] = {
         -size, 0.f, -size, 0.f, 1.0f, 0.f, 0.f, 0.f,// bottom - left vertex
         size, 0.f, -size, 0.f, 1.0f, 0.f, 1.f, 0.f, // bottom-right vertices
         size, 0.f, size, 0.f, 1.0f, 0.f, 1.f, 1.f,  // top-right vertex
         -size, 0.f, size, 0.f, 1.0f, 0.f, 0.f, 1.f  // top-left vertex
    };
    std::vector<float> Vertices(vertices_array, vertices_array + (sizeof(vertices_array) / sizeof(vertices_array[0])));


    // index buffers
    const u32 indices[] = {
        0, 2, 1,
        0, 3, 2
    };
    std::vector<u32> Indices(indices, indices + (sizeof(indices) / sizeof(indices[0])));

    //create vertex format
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 }); //vertex
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) }); //normals
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, 6 * sizeof(float) }); //tex coords
    vertexBufferLayout.stride = 8 * sizeof(float);

    app->meshes.push_back(Mesh{});
    Mesh& mesh = app->meshes.back();
    u32 meshIdx = (u32)app->meshes.size() - 1u;

    app->models.push_back(Model{});
    Model& model = app->models.back();
    model.meshIdx = meshIdx;
    u32 modelIdx = (u32)app->models.size() - 1u;
    model.materialIdx.push_back(0); //default material created at initialization

    Entity entity = { mat4(1.0f), modelIdx, 0, 0 };
    app->entities.push_back(entity);

    // add the submesh into the mesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices.swap(Vertices);
    submesh.indices.swap(Indices);
    mesh.submeshes.push_back(submesh);

    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, submesh.vertices.size() * sizeof(float), NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indices.size() * sizeof(u32), NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    const void* verticesData = submesh.vertices.data();
    u32   verticesSize = submesh.vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    submesh.vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = submesh.indices.data();
    const u32   indicesSize = submesh.indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    submesh.indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return entity;
}

Entity CreateSphere(App* app)
{
    const float radius = 10.f;
    const unsigned int sectorCount = 50;
    const unsigned int stackCount = 50;
    
    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;

    std::vector<float> vertices;

    for (int i = 0; i <= stackCount; ++i)
    {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectorCount;
            t = (float)i / stackCount;
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    std::vector<u32> indices;
    int k1, k2;
    for (int i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if (i != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    //create vertex format
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 }); //vertex
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) }); //normals
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, 6 * sizeof(float) }); //tex coords
    vertexBufferLayout.stride = 8 * sizeof(float);

    app->meshes.push_back(Mesh{});
    Mesh& mesh = app->meshes.back();
    u32 meshIdx = (u32)app->meshes.size() - 1u;

    app->models.push_back(Model{});
    Model& model = app->models.back();
    model.meshIdx = meshIdx;
    u32 modelIdx = (u32)app->models.size() - 1u;
    model.materialIdx.push_back(0); //default material created at initialization

    Entity entity = { mat4(1.0f), modelIdx, 0, 0 };
    app->entities.push_back(entity);

    // add the submesh into the mesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices.swap(vertices);
    submesh.indices.swap(indices);
    mesh.submeshes.push_back(submesh);

    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, submesh.vertices.size() * sizeof(float), NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indices.size() * sizeof(u32), NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    const void* verticesData = submesh.vertices.data();
    u32   verticesSize = submesh.vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    submesh.vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = submesh.indices.data();
    const u32   indicesSize = submesh.indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    submesh.indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return entity;
}

// ----------------------------------------------
// ---------- BUFFER MANAGMENT ------------------
// ----------------------------------------------

bool IsPowerOf2(u32 value)
{
    return value && !(value & (value - 1));
}

u32 Align(u32 value, u32 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

Buffer CreateBuffer(u32 size, GLenum type, GLenum usage)
{
    Buffer buffer = {};
    buffer.size = size;
    buffer.type = type;

    glGenBuffers(1, &buffer.handle);
    glBindBuffer(type, buffer.handle);
    glBufferData(type, buffer.size, NULL, usage);
    glBindBuffer(type, 0);

    return buffer;
}

void BindBuffer(const Buffer& buffer)
{
    glBindBuffer(buffer.type, buffer.handle);
}

void MapBuffer(Buffer& buffer, GLenum access)
{
    glBindBuffer(buffer.type, buffer.handle);
    buffer.data = (u8*)glMapBuffer(buffer.type, access);
    buffer.head = 0;
}

void UnmapBuffer(Buffer& buffer)
{
    glUnmapBuffer(buffer.type);
    glBindBuffer(buffer.type, 0);
}

void AlignHead(Buffer& buffer, u32 alignment)
{
    ASSERT(IsPowerOf2(alignment), "The alignment must be a power of 2");
    buffer.head = Align(buffer.head, alignment);
}

void PushAlignedData(Buffer& buffer, const void* data, u32 size, u32 alignment)
{
    ASSERT(buffer.data != NULL, "The buffer must be mapped first");
    AlignHead(buffer, alignment);
    memcpy((u8*)buffer.data + buffer.head, data, size);
    buffer.head += size;
}

// ----------------------------------------------
// ---------- MANAGE OPEN GL ERRORS -------------
// ----------------------------------------------

void OnGlError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    ELOG("OpenGL debug message %s", message);

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:               ELOG(" - source: GL_DEBUG_SOURCE_API"); break; //Calls the OpenGL API
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:     ELOG(" - source: GL_DEBUG_SOURCE_WINDOW_SYSTEM"); break; //Calls to a window-system API
    case GL_DEBUG_SOURCE_SHADER_COMPILER:   ELOG(" - source: GL_DEBUG_SOURCE_SHADER_COMPILE"); break; // a compiler for a shading language
    case GL_DEBUG_SOURCE_THIRD_PARTY:       ELOG(" - source: GL_DEBUG_SOURCE_THIRD_PARTY"); break; //An application associated with OpenGL
    case GL_DEBUG_SOURCE_APPLICATION:       ELOG(" - source: GL_DEBUG_SOURCE_APPLICATION"); break; // Generated by the user of this application
    case GL_DEBUG_SOURCE_OTHER:             ELOG(" - source: GL_DEBUG_SOURCE_OTHER"); break; //Some source that isn't one of these
    }

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:               ELOG(" - type: GL_DEBUG_TYPE_ERROR"); break; //An error, typically from the api
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: ELOG(" - type: GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR"); break; //Some behaviour naked deprecated
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  ELOG(" - type: GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR"); break; //something has invoked undefined behaviour
    case GL_DEBUG_TYPE_PORTABILITY:         ELOG(" - type: GL_DEBUG_TYPE_PORTABILITY"); break; // some functionality the user relies upon
    case GL_DEBUG_TYPE_PERFORMANCE:         ELOG(" - type: GL_DEBUG_TYPE_PERFORMANCE"); break; //Code has triggered possible performance
    case GL_DEBUG_TYPE_MARKER:              ELOG(" - type: GL_DEBUG_TYPE_MARKER"); break; //command stream annotation
    case GL_DEBUG_TYPE_PUSH_GROUP:          ELOG(" - type: GL_DEBUG_TYPE_PUSH_GROUP"); break; //Group pushing
    case GL_DEBUG_TYPE_POP_GROUP:           ELOG(" - type: GL_DEBUG_TYPE_POP_GROUP"); break; //Foo
    case GL_DEBUG_TYPE_OTHER:               ELOG(" - type: GL_DEBUG_TYPE_OTHER"); break; //Some type that isn't one of these
    }

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:            ELOG(" - severity: GL_DEBUG_SEVERITY_HIGH"); break; //All OpenGL errors, shader compilation/link
    case GL_DEBUG_SEVERITY_MEDIUM:          ELOG(" - severity: GL_DEBUG_SEVERITY_MEDIUM"); break; // Major performance warnings, shader compilation
    case GL_DEBUG_SEVERITY_LOW:             ELOG(" - severity: GL_DEBUG_SEVERITY_LOW"); break; //Redundant state change performance warning
    case GL_DEBUG_SEVERITY_NOTIFICATION:    ELOG(" - severity: GL_DEBUG_SEVERITY_NOTIFICATION"); break; // anything that isn't an error 
    }

}