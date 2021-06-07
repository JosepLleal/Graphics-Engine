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

//------------------------------------------------------------------------
//-------------------- FORWARD RENDERING ---------------------------------
//------------------------------------------------------------------------

#ifdef FORWARD_RENDERING

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
    vViewDir = normalize(uCameraPosition - vPosition);

    vec3 T = normalize(vec3(uWorldMatrix * vec4(aTangent, 0.0)));
    vec3 N = normalize(vec3(uWorldMatrix * vec4(aNormal, 0.0)));

    //T = normalize(T - dot(T, N) * N); //re-orthogonalize

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

uniform float hasNormalMap;
uniform float hasReliefMap;
uniform float Relief;
uniform float Bumpiness;
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

vec2 parallaxMapping(vec2 T, vec3 V)
{
   
   float numLayers = 30;
   float bumpiness = Bumpiness;

   V = transpose(vTBN) * V;

   // height of each layer
   float layerHeight = 1.0 / numLayers;
   // depth of current layer
   float currentLayerHeight = 0;
   // shift of texture coordinates for each iteration
   vec2 dtex = bumpiness * V.xy / V.z / numLayers;

   // current texture coordinates
   vec2 currentTextureCoords = T;

   // get first depth from heightmap
   float heightFromTexture = texture(uBumpTex, currentTextureCoords).r;

   // while point is above surface
   for(int i = 0; i < numLayers && heightFromTexture > currentLayerHeight; ++i)
   //while(heightFromTexture > currentLayerHeight)
   {
      // to the next layer
      currentLayerHeight += layerHeight;
      // shift texture coordinates along vector V
      currentTextureCoords -= dtex;
      // get new depth from heightmap
      heightFromTexture = texture(uBumpTex, currentTextureCoords).r;
   }
    
   // Start of Relief Mapping
      // decrease shift and height of layer by half
   vec2 deltaTexCoord = dtex / 2;
   float deltaHeight = layerHeight / 2;

   // return to the mid point of previous layer
   currentTextureCoords += deltaTexCoord;
   currentLayerHeight -= deltaHeight;

   // binary search to increase precision of Steep Paralax Mapping
   const int numSearches = 5;
   for(int i=0; i<numSearches; i++)
   {
      // decrease shift and height of layer by half
      deltaTexCoord /= 2;
      deltaHeight /= 2;

      // new depth from heightmap
      heightFromTexture = texture(uBumpTex, currentTextureCoords).r;

      
      if(heightFromTexture > currentLayerHeight) // below the surface
      {
         currentTextureCoords -= deltaTexCoord;
         currentLayerHeight += deltaHeight;
      }
      else // above the surface
      {
         currentTextureCoords += deltaTexCoord;
         currentLayerHeight -= deltaHeight;
      }
   }

   // return results
   return currentTextureCoords;

}

void main()
{
    vec2 texCoords = vec2(0.0, 0.0);

    if(Relief == 1.0)
        texCoords = hasReliefMap == 0.0 ? vTexCoord : parallaxMapping(vTexCoord, vViewDir);
    else
        texCoords = vTexCoord;

    vec4 albedo = texture(uTexture, texCoords);

    vec3 N = vec3(0.0);

    if(hasNormalMap == 0.0)
    {
        N = vNormal;
    }
    else
    {
        //normal mapping 
        vec3 normal = texture(uNormalMap, texCoords).xyz;
        normal = normal * 2.0 - 1.0;
        normal = normalize(vTBN * normal);
        N = normal;
    }

    float ambientFactor = 0.2;

    for(unsigned int i=0; i < uLightCount; i++)
    {
        vec3 L = normalize(uLight[i].position - vPosition);
    
        float diffuseFactor = max(0.0, dot(L,N));
        oColor += ambientFactor * albedo + diffuseFactor*albedo*vec4(uLight[i].color, 1.0); 
    }
    oColor /= uLightCount;      
    oColor.w = 1.0;
}

#endif
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
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
    vViewDir = normalize(uCameraPosition - vPosition);

    vec3 T = normalize(vec3(uWorldMatrix * vec4(aTangent, 0.0)));
    vec3 N = normalize(vec3(uWorldMatrix * vec4(aNormal, 0.0)));

    //T = normalize(T - dot(T, N) * N); //re-orthogonalize

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

uniform float hasNormalMap;
uniform float hasReliefMap;
uniform float Relief;
uniform float Bumpiness;
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

vec2 parallaxMapping(vec2 T, vec3 V)
{
   
   float numLayers = 30;
   float bumpiness = Bumpiness;

   V = transpose(vTBN) * V;

   // height of each layer
   float layerHeight = 1.0 / numLayers;
   // depth of current layer
   float currentLayerHeight = 0;
   // shift of texture coordinates for each iteration
   vec2 dtex = bumpiness * V.xy / V.z / numLayers;

   // current texture coordinates
   vec2 currentTextureCoords = T;

   // get first depth from heightmap
   float heightFromTexture = texture(uBumpTex, currentTextureCoords).r;

   // while point is above surface
   for(int i = 0; i < numLayers && heightFromTexture > currentLayerHeight; ++i)
   //while(heightFromTexture > currentLayerHeight)
   {
      // to the next layer
      currentLayerHeight += layerHeight;
      // shift texture coordinates along vector V
      currentTextureCoords -= dtex;
      // get new depth from heightmap
      heightFromTexture = texture(uBumpTex, currentTextureCoords).r;
   }
    
   // Start of Relief Mapping
      // decrease shift and height of layer by half
   vec2 deltaTexCoord = dtex / 2;
   float deltaHeight = layerHeight / 2;

   // return to the mid point of previous layer
   currentTextureCoords += deltaTexCoord;
   currentLayerHeight -= deltaHeight;

   // binary search to increase precision of Steep Paralax Mapping
   const int numSearches = 5;
   for(int i=0; i<numSearches; i++)
   {
      // decrease shift and height of layer by half
      deltaTexCoord /= 2;
      deltaHeight /= 2;

      // new depth from heightmap
      heightFromTexture = texture(uBumpTex, currentTextureCoords).r;

      
      if(heightFromTexture > currentLayerHeight) // below the surface
      {
         currentTextureCoords -= deltaTexCoord;
         currentLayerHeight += deltaHeight;
      }
      else // above the surface
      {
         currentTextureCoords += deltaTexCoord;
         currentLayerHeight -= deltaHeight;
      }
   }

   // return results
   return currentTextureCoords;

}


void main()
{
    //relief mapping
    vec2 texCoords = vec2(0.0, 0.0);
    
    if(Relief == 1.0)
        texCoords = hasReliefMap == 0.0 ? vTexCoord : parallaxMapping(vTexCoord, vViewDir);
    else
        texCoords = vTexCoord;

    //oAlbedo = texture(uTexture, vTexCoord);
    oAlbedo = texture(uTexture, texCoords);

    if(hasNormalMap == 0.0)
    {
        oNormal = vec4(vec3(vNormal), 1.0);
    }
    else
    {
        //normal mapping 
        vec3 normal = texture(uNormalMap, texCoords).xyz;
        normal = normal * 2.0 - 1.0;
        normal = normalize(vTBN * normal);
        oNormal = vec4(normal, 1.0);
    }
    
    oPosition = vec4(vec3(vPosition), 1.0);

	float depth = LinearizeDepth(gl_FragCoord.z) / far; // divide by far for demonstration
	oDepth = vec4(vec3(depth), 1.0);
	
}

#endif
#endif

//--------------------------------------------------------------------------
//-------------- SCREEN SPACE AMBIENT OCCLUSION ----------------------------
//--------------------------------------------------------------------------
#ifdef SSAO_PASS

#if defined(VERTEX)

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
	gl_Position =  vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) 

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform float SSAO;
uniform float Radius;
uniform float Bias;

uniform vec3 samples[64];
uniform mat4 projection;

in vec2 vTexCoord;

layout(location = 5) out vec4 oOcclusion;

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
int kernelSize = 64;
float radius = Radius;
float bias = Bias;

// tile noise texture over screen, based on screen dimensions divided by noise size
const vec2 noiseScale = vec2(1600.0/4.0, 1200.0/4.0); // screen = 1600x1200

void main()
{
    
    vec3 fragPos   = texture(gPosition, vTexCoord).rgb;
    vec3 normal    = texture(gNormal, vTexCoord).rgb;
    vec3 randomVec = texture(texNoise, vTexCoord * noiseScale).rgb; 

    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);  

    float occlusion = 0;

    for(int i = 0; i < kernelSize; ++i)
    {
        vec3 samplePos = TBN * samples[i];
        samplePos = fragPos + samplePos * radius; 

        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset      = projection * offset;    // from view to clip-space
        offset.xyz /= offset.w;               // perspective divide
        offset.xyz  = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0  

        // get sample depth
        float sampleDepth = texture(gPosition, offset.xy).z; // get depth value of kernel sample

        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / kernelSize);  
    oOcclusion = SSAO == 1.0 ? vec4(vec3(occlusion*occlusion), 1.0) : vec4(1.0);
}

#endif
#endif

#ifdef SSAO_BLUR_PASS

#if defined(VERTEX)

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
	gl_Position =  vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) 

uniform sampler2D ssaoInput;

in vec2 vTexCoord;

layout(location = 5) out vec4 oOcclusion;

void main()
{
    float Occlusion = texture(oOcclusion, vTexCoord).r;
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) 
    {
        for (int y = -2; y < 2; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, TexCoords + offset).r;
        }
    }
    result /= 16.0;
    oOcclusion = vec4(vec3(result), 1.0);
}

#endif
#endif

//------------------------------------------------------
//------------------ SHADING PASS ----------------------
//------------------------------------------------------

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
uniform sampler2D oOcclusion;

layout(location = 0) out vec4 oColor;

void main()
{
    // Retrieve information from the G-buffer
    vec3 iAlbedo = texture(oAlbedo, vTexCoord).rgb;
	vec3 iNormal = texture(oNormal, vTexCoord).rgb;
	vec3 iPosition = texture(oPosition, vTexCoord).rgb;
    float Occlusion = texture(oOcclusion, vTexCoord).r;
	
    vec3 Normal = normalize(iNormal);
    float ambientColor = 0.5;
    vec3 lighting = iAlbedo * ambientColor * Occlusion;
    vec3 ViewDir = normalize(vViewDir - iPosition);

    for(int i = 0; i < uLightCount; ++i)
    {
       
        // diffuse
        vec3 lightDir = normalize(uLight[i].position - iPosition);
        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * iAlbedo * uLight[i].color * 1.0;
    
        // specular
        vec3 halfwayDir = normalize(lightDir + ViewDir);  
        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 10.0);
        vec3 specular = uLight[i].color * spec * vec3(0.5);

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
