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

    //Fill vertex shader layout automatically
    GLint size;
    GLenum type;
    const GLsizei bufSize = 64;
    GLchar name[bufSize];
    GLsizei length;

    GLint count = 0;
    glGetProgramiv(program.handle, GL_ACTIVE_ATTRIBUTES, &count);

    for (GLint i = 0; i < count; i++)
    {
        VertexShaderAttribute attribute = {};
        GLsizei length = 0;
        glGetActiveAttrib(program.handle, (GLuint)i, bufSize,
            &length, &size, &type, name);

        attribute.componentCount = size;

        attribute.location = glGetAttribLocation(program.handle, name);

        program.vertexShaderLayout.attributes.push_back(attribute);
    }


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

void CreateFBTexture(App* app, GLuint& handle)
{
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
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

    app->mode = Mode::Mode_FinalColor;

    // FrameBuffer
    //color
    CreateFBTexture(app, app->colorAttachmentHandle);

    //albedo
    CreateFBTexture(app, app->albedoAttachmentHandle);

    //normals
    CreateFBTexture(app, app->normalAttachmentHandle);

    //position
    CreateFBTexture(app, app->positionAttachmentHandle);

    //depth texture
    CreateFBTexture(app, app->depthTextureHandle);

    // Depth
    glGenTextures(1, &app->depthAttachmentHandle);
    glBindTexture(GL_TEXTURE_2D, app->depthAttachmentHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, app->displaySize.x, app->displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Binding frame buffer
    glGenFramebuffers(1, &app->framebufferHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, app->framebufferHandle);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, app->colorAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, app->albedoAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, app->normalAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, app->positionAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, app->depthTextureHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, app->depthAttachmentHandle, 0);

    GLenum frameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (frameBufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        switch (frameBufferStatus)
        {
        case GL_FRAMEBUFFER_UNDEFINED:                      ELOG("GL_FRAMEBUFFER_UNDEFINED");                       break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:          ELOG("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");           break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:  ELOG("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");   break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:         ELOG("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");          break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:         ELOG("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");          break;
        case GL_FRAMEBUFFER_UNSUPPORTED:                    ELOG("GL_FRAMEBUFFER_UNSUPPORTED");                     break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:         ELOG("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");          break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:       ELOG("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS");        break;
        default: ELOG("Unknown framebuffer status error");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Geometry ---

    const VertexV3V2 vertices[] =
    {
        { glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // bottom-left vertex
        { glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // bottom-right vertex
        { glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)}, // top-right vertex
        { glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}, // top-left vertex
    };

    const u16 indices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

    glGenBuffers(1, &app->embeddedVertices);
    glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &app->embeddedElements);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Attribute state
    glGenVertexArrays(1, &app->vao);
    glBindVertexArray(app->vao);
    glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
    glBindVertexArray(0);

    //Creating uniform buffers
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAlignment);

    app->cbuffer = CreateConstantBuffer(app->maxUniformBufferSize);

    //Load programs
    app->texturedGeometryProgramIdx = LoadProgram(app, "shaders.glsl", "TEXTURED_GEOMETRY");
    Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];
    app->programUniformTexture = glGetUniformLocation(texturedGeometryProgram.handle, "uTexture");

    app->GeometryPassProgramIdx = LoadProgram(app, "shaders.glsl", "GEOMETRY_PASS");

    app->ShadingPassProgramIdx = LoadProgram(app, "shaders.glsl", "SHADING_PASS");

    //Texture initialization
    app->diceTexIdx = LoadTexture2D(app, "dice.png");
    app->whiteTexIdx = LoadTexture2D(app, "color_white.png");
    app->blackTexIdx = LoadTexture2D(app, "color_black.png");
    app->normalTexIdx = LoadTexture2D(app, "color_normal.png");
    app->magentaTexIdx = LoadTexture2D(app, "color_magenta.png");

    Material Default = {};
    Default.albedoTextureIdx = app->diceTexIdx;
    app->materials.push_back(Default);

    //Camera initialization
    app->camera.CameraInit(vec3(0.f, 0.f, 5.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), (float)(app->displaySize.x / app->displaySize.y));
    app->camera.position = vec3(0.f, 3.5f, 15.f);

    //  -------------- ENTITIES -------------------------

    //Lights
    Light FirstLight = {};
    FirstLight.position = vec3(5.f, 2.f, 5.f);
    FirstLight.type = LightType::Point;
    FirstLight.color = vec3(1.f, 1.f, 1.f);
    FirstLight.range = 30.f;
    app->lights.push_back(FirstLight);

    Light SecondLight = {};
    SecondLight.position = vec3(-20.f, 2.f, 5.f);
    SecondLight.type = LightType::Point;
    SecondLight.color = vec3(1.f, 0.f, 0.f);
    SecondLight.range = 30.f;
    app->lights.push_back(SecondLight);

    /*Light ThirdLight = {};
    ThirdLight.position = vec3(0.f, 2.f, -10.f);
    ThirdLight.type = LightType::Point;
    ThirdLight.color = vec3(1.f, 1.f, 1.f);
    ThirdLight.range = 30.f;
    app->lights.push_back(ThirdLight);*/

    //Entity plane = CreatePlane(app, 20.f);

    /*u32 JapanFloor = LoadModel(app, "Box/JapanFloor.fbx");

    Entity Floor = { mat4(1.0f), JapanFloor, 0, 0 };
    Floor.TransformPosition(vec3(-10.f, 0.f, -10.f));
    Floor.TransformScale(vec3(0.2f, 0.2f, 0.2f));

    Model& model = app->models[Floor.modelIndex];

    u32 submeshMaterialIdx = model.materialIdx[0];
    Material& submeshMaterial = app->materials[submeshMaterialIdx];
    submeshMaterial.bumpTextureIdx = LoadTexture2D(app, "Box/ve0icftdb_2K_Displacement.jpg");
    app->entities.push_back(Floor);*/

    //------------------------------------------

    u32 cube_model = LoadModel(app, "Box/Cube.fbx");

    Entity Cube = { mat4(1.0f), cube_model, 0, 0 };
    Cube.TransformPosition(vec3(10.f, 11.f, 0.f));
    Cube.TransformScale(vec3(0.05f, 0.05f, 0.05f));

    Model& model1 = app->models[Cube.modelIndex];

    u32 submeshMaterialIdx1 = model1.materialIdx[0];
    Material& submeshMaterial1 = app->materials[submeshMaterialIdx1]; 
    submeshMaterial1.albedoTextureIdx = LoadTexture2D(app, "Box/tile1.jpg");
    submeshMaterial1.normalsTextureIdx = LoadTexture2D(app, "Box/toy_box_normal.png");
    submeshMaterial1.bumpTextureIdx = LoadTexture2D(app, "Box/toy_box_disp.png");

    app->entities.push_back(Cube);

    //---------------------------------

    u32 cube_model2 = LoadModel(app, "Box/Cube.fbx");

    Entity Cube2 = { mat4(1.0f), cube_model2, 0, 0 };
    Cube2.TransformPosition(vec3(-10.f, 11.f, 0.f));
    Cube2.TransformScale(vec3(0.05f, 0.05f, 0.05f));

    Model& model2 = app->models[Cube2.modelIndex];

    u32 submeshMaterialIdx2 = model2.materialIdx[0];
    Material& submeshMaterial2 = app->materials[submeshMaterialIdx2];
    submeshMaterial2.albedoTextureIdx = LoadTexture2D(app, "Box/basecolor.jpg");
    submeshMaterial2.normalsTextureIdx = LoadTexture2D(app, "Box/normal.jpg");
    submeshMaterial2.bumpTextureIdx = LoadTexture2D(app, "Box/height.jpg");

    app->entities.push_back(Cube2);

    //--------------------------------------

    u32 cube_model3 = LoadModel(app, "Box/Cube.fbx");

    Entity Cube3 = { mat4(1.0f), cube_model3, 0, 0 };
    Cube3.TransformPosition(vec3(0.f, 11.f, 0.f));
    Cube3.TransformScale(vec3(0.05f, 0.05f, 0.05f));

    Model& model3 = app->models[Cube3.modelIndex];

    u32 submeshMaterialIdx3 = model3.materialIdx[0];
    Material& submeshMaterial3 = app->materials[submeshMaterialIdx3];
    submeshMaterial3.albedoTextureIdx = LoadTexture2D(app, "Box/basecolor1.jpg");
    submeshMaterial3.normalsTextureIdx = LoadTexture2D(app, "Box/normal1.jpg");
    submeshMaterial3.bumpTextureIdx = LoadTexture2D(app, "Box/height1.jpg");

    app->entities.push_back(Cube3);

    //u32 modelIdx2 = LoadModel(app, "Sphere/sphere.fbx");
    //app->models[modelIdx2].materialIdx[0] = 4;

    //Entity entity3 = { mat4(1.0f), modelIdx2, 0, 0 };
    //entity3.TransformPosition(vec3(5.f, 2.f, 5.f));
    //entity3.TransformScale(vec3(0.01f, 0.01f, 0.01f));
    //app->entities.push_back(entity3);

    //Entity entity4 = { mat4(1.0f), modelIdx2, 0, 0 };
    //entity4.TransformPosition(vec3(-5.f, 2.f, 5.f));
    //entity4.TransformScale(vec3(0.01f, 0.01f, 0.01f));
    //app->entities.push_back(entity4);

    //Entity entity5 = { mat4(1.0f), modelIdx2, 0, 0 };
    //entity5.TransformPosition(vec3(0.f, 2.f, -10.f));
    //entity5.TransformScale(vec3(0.01f, 0.01f, 0.01f));
    //app->entities.push_back(entity5);

    ///*Entity sphere = CreateSphere(app);
    //sphere.worldMatrix = translate(sphere.worldMatrix, vec3(10.0f, 20.f, 0.0f));*/

    //u32 modelIdx = LoadModel(app, "Patrick/Patrick.obj");

    //Entity entity = { mat4(1.0f), modelIdx, 0, 0 };
    //entity.TransformPosition(vec3(0.0f, 3.5f, 1.0f));
    //app->entities.push_back(entity);

    //Entity entity1 = { mat4(1.0f), modelIdx, 0, 0 };
    //entity1.TransformPosition(vec3(5.0f, 3.5f, -4.0f));
    //app->entities.push_back(entity1);

    //Entity entity2 = { mat4(1.0f), modelIdx, 0, 0 };
    //entity2.TransformPosition(vec3(-5.0f, 3.5f, -4.0f));
    //app->entities.push_back(entity2);

    //----------

   

    
}

void Gui(App* app)
{
    ImGui::Begin("Info");
    ImGui::Text("FPS: %f", 1.0f / app->deltaTime);
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

    ImGui::Separator();
    ImGui::NewLine();
    ImGui::Text("Select Render Texture");

    const char* items[] = { "Final Color", "Albedo", "Normals", "Position", "Depth" };
    static const char* current_item = items[0];

    ImGui::PushItemWidth(150);
    if (ImGui::BeginCombo("##Render Mode", current_item)) // The second parameter is the label previewed before opening the combo.
    {
        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
        {
            bool is_selected = (current_item == items[n]); // You can store your selection however you want, outside or inside your objects
            if (ImGui::Selectable(items[n], is_selected))
                current_item = items[n];
                if (is_selected)
                    ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
        }
        ImGui::EndCombo();
    }

    if (current_item == items[0])
        app->mode = Mode::Mode_FinalColor;
    else if (current_item == items[1])
        app->mode = Mode::Mode_TexturedAlbedo;
    else if (current_item == items[2])
        app->mode = Mode::Mode_TexturedNormals;
    else if (current_item == items[3])
        app->mode = Mode::Mode_TexturedPositions;
    else if (current_item == items[4])
        app->mode = Mode::Mode_TexturedDepth;

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

    //-------------------------------------- WASD position movement and QE yaw rotation -------------------------------------
    static float speed = 20.0f * app->deltaTime;

    if (app->input.keys[K_W] == BUTTON_PRESSED) app->camera.position += app->camera.front * speed;
    if (app->input.keys[K_S] == BUTTON_PRESSED) app->camera.position -= app->camera.front * speed;
    if (app->input.keys[K_D] == BUTTON_PRESSED) app->camera.position += app->camera.right * speed;
    if (app->input.keys[K_A] == BUTTON_PRESSED) app->camera.position -= app->camera.right * speed;

    if (app->input.keys[K_E] == BUTTON_PRESSED) app->camera.position += app->camera.up * speed;
    if (app->input.keys[K_Q] == BUTTON_PRESSED) app->camera.position -= app->camera.up * speed;



    if (app->input.keys[K_V] == BUTTON_PRESSED) app->camera.yaw += speed * 2;
    if (app->input.keys[K_C] == BUTTON_PRESSED) app->camera.yaw -= speed * 2;
    if (app->input.keys[K_R] == BUTTON_PRESSED) app->camera.pitch += speed * 2;
    if (app->input.keys[K_F] == BUTTON_PRESSED) app->camera.pitch -= speed * 2;


    //app->entities[0].TransformRotation(1.0 * app->deltaTime, vec3(0.0, 1.0, 0.0));

    //--------------------------------------------------------------------------------------------------------------------------
   
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
        
}

void Render(App* app)
{

    // --- Screen ---
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render on this framebuffer render targets
    glBindFramebuffer(GL_FRAMEBUFFER, app->framebufferHandle);

    // Select on which render targets to draw
    GLuint drawBuffers[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
    };

    glDrawBuffers(ARRAY_COUNT(drawBuffers), drawBuffers);

    // - clear the framebuffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    // - set the viewport
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);

    // ------- GEOMETRY PASS -------------

    Program& ProgramGeometryPass = app->programs[app->GeometryPassProgramIdx];
    glUseProgram(ProgramGeometryPass.handle);

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
            GLuint vao = FindVAO(mesh, i, ProgramGeometryPass);
            glBindVertexArray(vao);

            u32 submeshMaterialIdx = model.materialIdx[i];
            Material& submeshMaterial = app->materials[submeshMaterialIdx];

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);
            glUniform1i(app->programUniformTexture, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.normalsTextureIdx].handle);
            glUniform1i(glGetUniformLocation(ProgramGeometryPass.handle, "uNormalMap"), 1); 

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.bumpTextureIdx].handle);
            glUniform1i(glGetUniformLocation(ProgramGeometryPass.handle, "uBumpTex"), 2);

            Submesh& submesh = mesh.submeshes[i];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
        }
    }

    // -------- SHADING PASS ---------------

    Program& shadingPass = app->programs[app->ShadingPassProgramIdx];
    glUseProgram(shadingPass.handle);

    glUniform1i(glGetUniformLocation(shadingPass.handle, "oAlbedo"), 0);
    glUniform1i(glGetUniformLocation(shadingPass.handle, "oNormal"), 1);
    glUniform1i(glGetUniformLocation(shadingPass.handle, "oPosition"), 2);
    glUniform1i(glGetUniformLocation(shadingPass.handle, "oDepth"), 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->albedoAttachmentHandle);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, app->normalAttachmentHandle);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, app->positionAttachmentHandle);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, app->depthTextureHandle);

    // We only need to draw 1 buffer so it would be unnecessary to use an array of buffers
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glDepthMask(false);

    //Binding buffer ranges to uniform blocks (GLOBAL PARAMETERS)
    glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->cbuffer.handle, blockOffset, blockSize);
    renderQuad();
    glDepthMask(true);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    switch (app->mode)
    {
        case Mode::Mode_FinalColor:
        {
            app->DisplayedTexture = app->colorAttachmentHandle;
            break;
        }
        case Mode::Mode_TexturedAlbedo:
        {
            app->DisplayedTexture = app->albedoAttachmentHandle;
            break;
        }
        case Mode::Mode_TexturedNormals:
        {
            app->DisplayedTexture = app->normalAttachmentHandle;
            break;
        }
        case Mode::Mode_TexturedPositions:
        {
            app->DisplayedTexture = app->positionAttachmentHandle;
            break;
        }
        case Mode::Mode_TexturedDepth:
        {
            app->DisplayedTexture = app->depthTextureHandle;
            break;
        }
    }
        


    // --- Draw framebuffer texture -------------------------------------------------
    Program& programTexturedGeometry = app->programs[app->texturedGeometryProgramIdx];
    glUseProgram(programTexturedGeometry.handle);
    glBindVertexArray(app->vao);

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_ONE, GL_ONE);

    glUniform1i(app->programUniformTexture, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->DisplayedTexture);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    //Clear vertex array and program
    glBindVertexArray(0);
    glUseProgram(0);

}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
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
        int i = 0;
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