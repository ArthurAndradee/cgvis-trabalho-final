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
#define BULLET 6 // ID para o Projétil Laser
#define PORTAL 7 // ID para o Portal
#define AMMO_BOX 8 // ID para a Caixa de Munição
#define HEALTH_BOX 9 // ID para a Caixa de Vida

uniform int object_id;
uniform float hit_flash; // 0.0 = sem efeito, 1.0 = totalmente vermelho

// Headlamp point light no jogador (muzzle flash quando atira).
uniform vec3  player_light_pos;       // posição mundo
uniform float player_light_intensity; // multiplicador (0 = desligado, 1 = normal, 3-5 = muzzle flash)

// AGORA O SHADER SÓ PRECISA DE UM SAMPLER PARA RENDERIZAR TUDO!
uniform sampler2D TextureImage;

out vec4 color;

void main()
{
    // =========================================================
    // CUSTOMIZAÇÃO DA COR DOS ITENS ESPECIAIS E LASER
    // Pinta da cor desejada e ignora as sombras (brilho puro)
    // =========================================================
    if (object_id == BULLET) {
        color = vec4(1.0, 1.0, 0.0, 1.0); // Amarelo
        color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
        return; 
    } else if (object_id == PORTAL) {
        color = vec4(0.0, 0.0, 1.0, 1.0); // Azul
        color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
        return; 
    }
    // =========================================================

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
        q = 64.0;
    } else if (object_id == GUN) {
        K_s = vec3(0.8); 
        q = 128.0;
    }

    vec3 I_a = vec3(0.3, 0.3, 0.3); 
    vec3 I_d = vec3(0.8, 0.8, 0.8); 
    vec3 I_s = vec3(1.0, 1.0, 1.0); 

    vec3 K_a = Kd0;
    vec3 K_d = Kd0;

    vec4 h = normalize(l + v);

    vec3 ambient_term  = K_a * I_a;
    vec3 diffuse_term  = K_d * I_d * max(0.0, dot(n, l));
    vec3 specular_term = K_s * I_s * pow(max(0.0, dot(n, h)), q);

    // ------ Headlamp point light no jogador ------
    // Luz pontual segue a câmera. Atenuação 1 / (1 + k * d^2).
    // Quando dispara, player_light_intensity sobe e cria um efeito de muzzle
    // flash que acende o ambiente local por uns frames.
    vec3 toLight = player_light_pos - p.xyz;
    float dist2  = dot(toLight, toLight);
    float dist   = sqrt(dist2);
    vec4 lp = vec4(toLight / max(dist, 0.0001), 0.0);
    float atten = 1.0 / (1.0 + 0.15 * dist2);
    vec4 hp = normalize(lp + v);
    vec3 I_p = vec3(1.0, 0.95, 0.85) * player_light_intensity; // ligeiramente amarelado
    vec3 diffuse_player  = K_d * I_p * max(0.0, dot(n, lp)) * atten;
    vec3 specular_player = K_s * I_p * pow(max(0.0, dot(n, hp)), q) * atten;

    color.rgb = ambient_term + diffuse_term + specular_term + diffuse_player + specular_player;
    color.a = 1.0;

    // Pisca-pisca vermelho ao levar tiro
    if (hit_flash > 0.0) {
        color.rgb = mix(color.rgb, vec3(1.2, 0.0, 0.0), clamp(hit_flash, 0.0, 1.0));
    }

    color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
}