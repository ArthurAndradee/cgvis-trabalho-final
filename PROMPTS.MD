Arthur:

1. "Monte um plano de como seguir esse trabalho. Vou recriar o Doom como trabalho final de uma disciplina de computação gráfica. Vou me basear nesse repositório github.com/ZDoom/gzdoom. A primeira entrega é apenas o essencial do jogo, segue abaixo a especificação:" (especificação do trabalho no moodle...)
2. "Sem usar gluLookAt(), gluOrtho2D(), gluPerspective(), gluPickMatrix(), gluProject(), gluUnProject(), glm::lookAt(), glm::ortho(), glm::perspective(), glm::pickMatrix(), glm::rotate(), glm::scale(), glm::translate(), dentre outras, vamos começar a fase 1 do projeto. Atualmente temos o código já deixado pelo professor, mas devemos seguir a ideia do SPEC.md: " (código atual do repositório e arquivo SPEC.md)
3. A câmera só se movimenta se eu movimento o mouse enquanto estou pressionando o botão esquerdo, ela deve acompanhar o movimento do mouse e o eixo Y (vertical) de controle do mouse deve ser invertido: 
4.  Vamos iniciar a fase 2. Como estamos fazendo um jogo parecido com Quake original, precisaremos de malhas complexas de modelos 3D (personagem, arma, inimigo, caixa de item), mapear texturas para termos um cenário 3D, fazer instancias no mapa dos modelos de malhas, e aplicar iluminação do tipo Phong. Elabore um plano para fazermos isso, sabendo que estamos replicando uma fase simples do Quake com o personagem, uma arma, varios inimigo, algumas caixa de item e uma fase (mapa) curto e geometricamente simples sem usar uma ferramenta dedicada pra fazer o mapa. Tambem informe que formato baixar os modelos 3d com textura e a propria textura que usaremos na parede/chão/teto, inicialmente usaremos a mesma textura para tudo
5. Perfeito, vamos usar o alien para inimigos, a caixa como um item, a gun como nossa arma que deve acompanhar a câmera e ser renderizada no "olho" do personagem, mas nada terá interação ainda:
mapa de cada arquivo:
PS E:\university\courses\2026-1\computacao-grafica-e-visualizacao\cgvis-trabalho-final\assets> ls -recursive
6. Como mudar a r otação/direção/altura/escala da arma?
7.  As armas estão perfeitas, apenas mais um detalhe. Faça com que, enquanto o botão esquerdo do mouse esteja sendo segurado: Uma arma, de cada vez, sofra "recúo", com o movimento de recúo sendo uma ida para trás rápida e uma volta para sua posição original lenta, uma arma só se movimenta se a outra terminou: 
8. E como eu poderia usar um mapa já pronto? como eu poderia encontrar o mapa Quake 1 Demo?
9. Faça com que o movimento do jogador seja suave, como se ele "escorregasse" bem levemente quando parar de se movimentar em uma direção: 
10. De maneira similar, faça com que a arma segurada pelo personagem balance de forma natural como em outros jogos quando ele se mexe, mas não deve atrapalhar os disparos da arma.
11. De maneira similar, faça com que a arma segurada pelo personagem balance de forma natural como em outros jogos quando ele se mexe, mas não deve atrapalhar os disparos da arma.
12. De maneira similar, forneça um contador de munição na UI acima da barra de vida, deve ser apenas um contador com o tanto de projéteis restantes/limite máximo. Forneça apenas as partes necessárias que devem ser mudadas. 
13. 

Duda: 
1. when moving, the player is getting stuck on high ledges, I want to be able to fall down regardless of height
2. can we make it so that the enemies can also go down small ledges to chase the player and also can go around obstacles instead of just walking in a straight line
3. fix enemy+enemy collision so that they don't overlap and  can we have some of them standing still and shooting slow moving projectiles (that the player is supposed to be able to doge) instead of chasing
4. make it visible when enemies are hit like blinking red
5. can we make the hp hud look somewhat like this (print da hud do exemplo)