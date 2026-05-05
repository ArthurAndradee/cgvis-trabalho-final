#version 330 core

in vec4 position_world;
in vec4 normal;
in vec4 position_model;
in vec2 texcoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

#define WALL  1
#define ALIEN 2
#define BOX   3
#define GUN   4
#define FLOOR 5
uniform int object_id;

// AGORA O SHADER SÓ PRECISA DE UM SAMPLER PARA RENDERIZAR TUDO!
uniform sampler2D TextureImage; 

out vec4 color;

void main()
{
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;
    vec4 p = position_world;
    vec4 n = normalize(normal);
    
    vec4 l = normalize(vec4(1.0, 1.0, 0.0, 0.0)); 
    vec4 v = normalize(camera_position - p);

    float U = texcoords.x;
    float V = texcoords.y;

    // Extração automática da textura enviada pelo C++
    vec3 Kd0 = texture(TextureImage, vec2(U, V)).rgb;

    // Parâmetros Especulares (Ajustes Físicos)
    vec3 K_s = vec3(0.1); 
    float q = 10.0;

    if (object_id == ALIEN) {
        K_s = vec3(0.5); 
        q = 32.0;
    } else if (object_id == GUN) {
        K_s = vec3(0.8); 
        q = 64.0;
    }

    vec3 I_a = vec3(0.3, 0.3, 0.3); 
    vec3 I_d = vec3(0.8, 0.8, 0.8); 
    vec3 I_s = vec3(1.0, 1.0, 1.0); 

    vec3 K_a = Kd0;
    vec3 K_d = Kd0;

    vec4 r = -l + 2.0 * n * dot(n, l);

    vec3 ambient_term  = K_a * I_a;
    vec3 diffuse_term  = K_d * I_d * max(0.0, dot(n, l));
    vec3 specular_term = K_s * I_s * pow(max(0.0, dot(v, r)), q);

    color.rgb = ambient_term + diffuse_term + specular_term;
    color.a = 1.0;

    color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
}