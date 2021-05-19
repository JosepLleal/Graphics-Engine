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
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;

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
out mat3 vTBN;

void main()
{
	vTexCoord = aTexCoord;
    vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0)); // 1.0 because its a point
    vNormal = vec3(uWorldMatrix * vec4(aNormal, 0.0)); // 0.0 because its a vector
    vViewDir = uCameraPosition - vPosition;

    vec3 T = normalize(vec3(uWorldMatrix * vec4(aTangent, 0.0)));
    vec3 N = normalize(vec3(uWorldMatrix * vec4(aNormal, 0.0)));

    T = normalize(T - dot(T, N) * N); //re-orthogonalize

    vec3 B = cross(N, T);

    vTBN = mat3(T, B, N);

    gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT)

in vec2 vTexCoord;
in vec3 vPosition; 
in vec3 vNormal; 
in vec3 vViewDir;
in mat3 vTBN;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
uniform sampler2D uBumpTex;

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

vec2 reliefMapping(vec2 texCoords)
{
    int numSteps = 30;

    //Compute the view ray in texture space
    vec3 rayTexspace = inverse(vTBN) * vViewDir;

    ivec2 textureSize2d = textureSize(uBumpTex,0);

    float bumpiness = 1.0;

    // Increment
    vec3 rayIncrementTexspace;
    rayIncrementTexspace.xy = bumpiness * rayTexspace.xy / abs(rayTexspace.z * textureSize2d.x);
    rayIncrementTexspace.z = 1.0/numSteps;

    //Sampling state
    vec3 samplePositionTexspace = vec3(texCoords, 0.0);
    float sampleDepth = 1.0 - texture(uBumpTex, samplePositionTexspace.xy).r;

    //Linear search
    for(int i = 0; i < numSteps && samplePositionTexspace.z < sampleDepth; ++i)
    {
        samplePositionTexspace += rayIncrementTexspace;
        sampleDepth = 1.0 - texture(uBumpTex, samplePositionTexspace.xy).r;
    }

    return samplePositionTexspace.xy;

}

void main()
{

    //relief mapping
    vec2 texCoords = reliefMapping(vTexCoord);

    //oAlbedo = texture(uTexture, vTexCoord);
    oAlbedo = texture(uTexture, texCoords);

    //normal mapping
    //vec3 normal = texture(uNormalMap, vTexCoord).xyz;
    vec3 normal = texture(uNormalMap, texCoords).xyz;
    normal = normal * 2.0 - 1.0;
    normal = normalize(vTBN * normal);
    oNormal = vec4(normal, 1.0);
    
    oPosition = vec4(vec3(vPosition), 1.0);

	float depth = LinearizeDepth(gl_FragCoord.z) / far; // divide by far for demonstration
	oDepth = vec4(vec3(depth), 1.0);

	
}

#endif
#endif

//------------------------------------------------------------------------------------

#ifdef SHADING_PASS

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
layout(location=1) in vec2 aTexCoord;

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
out vec3 vViewDir; 

void main()
{
	vTexCoord = aTexCoord;
	vViewDir = vec3(uWorldViewProjectionMatrix * vec4(uCameraPosition, 1.0));
	gl_Position =  vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) 

in vec2 vTexCoord;
in vec3 vViewDir;

layout(binding = 0, std140) uniform GlobalParams
{
    vec3            uCameraPosition;
    unsigned int    uLightCount;
    Light           uLight[16];
};

uniform sampler2D oAlbedo;
uniform sampler2D oNormal;
uniform sampler2D oPosition;
uniform sampler2D oDepth;

layout(location = 0) out vec4 oColor;

void main()
{
    // Retrieve information from the G-buffer
    vec3 iAlbedo = texture(oAlbedo, vTexCoord).rgb;
	vec3 iNormal = texture(oNormal, vTexCoord).rgb;
	vec3 iPosition = texture(oPosition, vTexCoord).rgb;
	
    
    vec3 Normal = normalize(iNormal);
    float ambientColor = 0.4;
    vec3 lighting = iAlbedo * ambientColor;
    vec3 ViewDir = normalize(vViewDir - iPosition);

    for(int i = 0; i < uLightCount; ++i)
    {
       
        // diffuse
        vec3 lightDir = normalize(uLight[i].position - iPosition);
        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * iAlbedo * uLight[i].color * 2.0;
    
        // specular
        vec3 halfwayDir = normalize(lightDir + ViewDir);  
        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 10.0);
        vec3 specular = uLight[i].color * spec * vec3(0.3);

        // attenuation
        float attenuation = 1.0;
        float dist = length(uLight[i].position - iPosition);
        attenuation = 1.0 / (1.0 + 0.1 * dist + 0.02 * pow(dist, 2.0));
        
        diffuse *= attenuation;
        specular *= attenuation;
        lighting += diffuse + specular;   
        
    }

    // Final color output
        oColor = vec4(lighting, 1.0);
}

#endif
#endif
