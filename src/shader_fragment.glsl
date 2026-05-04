#version 330 core

in vec4 position_world;
in vec4 normal;
in vec4 position_model;
in vec2 texcoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Nossos IDs
#define WALL  1
#define ALIEN 2
#define BOX   3
#define GUN   4
#define FLOOR 5
uniform int object_id;

uniform sampler2D TextureImage0; // Parede/Chão
uniform sampler2D TextureImage1; // Alien
uniform sampler2D TextureImage2; // Caixa
uniform sampler2D TextureImage3; // Arma

out vec4 color;

void main()
{
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;
    vec4 p = position_world;
    vec4 n = normalize(normal);
    
    // Luz fixa em cima do mapa
    vec4 l = normalize(vec4(1.0, 1.0, 0.0, 0.0)); 
    vec4 v = normalize(camera_position - p);

    float U = texcoords.x;
    float V = texcoords.y;
    vec3 Kd0 = vec3(1.0, 0.0, 1.0); // Roxo berrante se algo der errado

    vec3 K_s = vec3(0.5, 0.5, 0.5); 
    float q = 32.0;

    // --- SELEÇÃO DE TEXTURA ---
    if (object_id == WALL) {
        // Como escalamos a caixa no Y para virar parede, o V precisa ser multiplicado para não esticar verticalmente.
        U = U * 1.0;
        V = V * 2.0;
        Kd0 = texture(TextureImage0, vec2(U, V)).rgb;
        K_s = vec3(0.1); 
        q = 10.0;
    } 
    else if (object_id == FLOOR) {
        U = U * 10.0; // Multiplica para repetir pelo chão todo
        V = V * 10.0;
        Kd0 = texture(TextureImage0, vec2(U, V)).rgb;
        K_s = vec3(0.1);
        q = 10.0;
    }
    else if (object_id == ALIEN) {
        Kd0 = texture(TextureImage1, vec2(U, V)).rgb;
    }
    else if (object_id == BOX) {
        Kd0 = texture(TextureImage2, vec2(U, V)).rgb;
    }
    else if (object_id == GUN) {
        Kd0 = texture(TextureImage3, vec2(U, V)).rgb;
        K_s = vec3(0.8); 
        q = 64.0;
    }

    // --- ILUMINAÇÃO DE PHONG ---
    vec3 I_a = vec3(0.3, 0.3, 0.3); // Luz ambiente um pouco mais clara
    vec3 I_d = vec3(0.8, 0.8, 0.8); // Luz difusa
    vec3 I_s = vec3(1.0, 1.0, 1.0); // Luz especular

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