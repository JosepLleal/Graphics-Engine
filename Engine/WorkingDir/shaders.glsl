///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef TEXTURED_GEOMETRY

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
        vTexCoord = aTexCoord;
        gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;

uniform sampler2D uTexture;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = texture(uTexture, vTexCoord);
}


#endif
#endif

//------------------------------------------------------------------------------------------------------------

#ifdef SHOW_TEXTURED_MESH

struct Light
{
    unsigned int type;
    float range;
    vec3 color;
    vec3 direction;
    vec3 position;
};

#if defined(VERTEX)
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(binding = 1, std140) uniform LocalParams
{
    mat4 uWorldMatrix;
    mat4 uWorldViewProjectionMatrix;
};

layout(binding = 0, std140) uniform GlobalParams
{
    vec3            uCameraPosition;
    unsigned int    uLightCount;
    Light           uLight[16];
};

out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;

void main()
{
    vTexCoord = aTexCoord;
    vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0)); // 1.0 because its a point
    vNormal = vec3(uWorldMatrix * vec4(aNormal, 0.0)); // 0.0 because its a vector
    vViewDir = uCameraPosition - vPosition;
    gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}


#elif defined(FRAGMENT)

in vec2 vTexCoord;
in vec3 vPosition;
in vec3 vNormal;
in vec3 vViewDir;

uniform sampler2D uTexture;

layout(binding = 0, std140) uniform GlobalParams
{
    vec3            uCameraPosition;
    unsigned int    uLightCount;
    Light           uLight[16];
};

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oDepth;

float near = 0.1; 
float far  = 100.0; 

float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}

void main()
{
    vec4 albedo = texture(uTexture, vTexCoord);
    vec3 N = normalize(vNormal);
    float ambientFactor = 0.2;

    for(unsigned int i=0; i < uLightCount; i++)
    {
        vec3 L = normalize(uLight[i].position - vPosition);
    
        float diffuseFactor = max(0.0, dot(L,N));
        oColor += ambientFactor * albedo + diffuseFactor*albedo*vec4(uLight[i].color, 1.0); 
    }

    oDepth =  vec4(vec3(LinearizeDepth(gl_FragCoord.z) / far), 1.0); // divide by far for demonstration

    oNormal = vec4(N, 1.0);

    oPosition = vec4(vec3(vPosition), 1.0);

    oColor /= uLightCount;      
    oColor.w = 1.;
}

#endif
#endif

//------------------------- DEFERRED LIGHTNING -----------------------------------

#ifdef GEOMETRY_PASS

struct Light
{
    unsigned int type;
    float range;
    vec3 color;
    vec3 direction;
    vec3 position;
};

#if defined(VERTEX) 

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;


layout(binding = 1, std140) uniform LocalParams
{
    mat4 uWorldMatrix;
    mat4 uWorldViewProjectionMatrix;
};

layout(binding = 0, std140) uniform GlobalParams
{
    vec3            uCameraPosition;
    unsigned int    uLightCount;
    Light           uLight[16];
};


out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;

void main()
{
	vTexCoord = aTexCoord;
    vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0)); // 1.0 because its a point
    vNormal = vec3(uWorldMatrix * vec4(aNormal, 0.0)); // 0.0 because its a vector
    vViewDir = uCameraPosition - vPosition;
    gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT)

in vec2 vTexCoord;
in vec3 vPosition; 
in vec3 vNormal; 
in vec3 vViewDir;

uniform sampler2D uTexture;

layout(binding = 0, std140) uniform GlobalParams
{
    vec3            uCameraPosition;
    unsigned int    uLightCount;
    Light           uLight[16];
};

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAlbedo;
layout(location = 2) out vec4 oNormal;
layout(location = 3) out vec4 oPosition;
layout(location = 4) out vec4 oDepth;

float near = 0.1; 
float far  = 100.0; 
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}

void main()
{
    oAlbedo = texture(uTexture, vTexCoord);
    oNormal = vec4(normalize(vNormal), 1.0);
    oPosition = vec4(vec3(vPosition), 1.0);

	float depth = LinearizeDepth(gl_FragCoord.z) / far; // divide by far for demonstration
	oDepth = vec4(vec3(depth), 1.0);

	
}

#endif
#endif
