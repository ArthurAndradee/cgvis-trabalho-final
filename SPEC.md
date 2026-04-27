# Especificação da Implementação

> [!CAUTION]
> - Você <ins>**não pode utilizar ferramentas de IA para escrever esta
>   especificação**</ins>

## Integrantes da dupla

- **Aluno 1 - Nome**: <mark>`Arthur Andrade da Silva`</mark>
- **Aluno 1 - Cartão UFRGS**: <mark>`00588758`</mark>

- **Aluno 2 - Nome**: <mark>`Eduarda Carvalho Waechter`</mark>
- **Aluno 2 - Cartão UFRGS**: <mark>`00304585`</mark>

## Detalhes do que será implementado

- **Título do trabalho**: <mark>`Quack`</mark>
- **Parágrafo curto descrevendo o que será implementado**: <mark>`Jogo de tiro em primeira pessoa baseado em Quake, no qual o objetivo é chegar ao final da fase lutando contra inimigos.`</mark>

## Especificação visual

### Vídeo - Link

> [!IMPORTANT]
> - Coloque aqui um link para um vídeo que mostre a aplicação gráfica
>   de referência que você vai implementar. **Sua implementação deverá
>   ser o mais parecido possível com o que é mostrado no vídeo (mais
>   detalhes abaixo).**
> - **Você não pode escolher como referência: (1) algum trabalho realizado
>   por outros alunos desta disciplina, em semestres anteriores. (2) Minecraft.**
> - Por exemplo, você pode colocar um vídeo de um jogo que você gosta,
>   e seu trabalho final será uma re-implementação do jogo.
> - O vídeo pode ser um link para YouTube, Google Drive, ou arquivo mp4 dentro
>   do próprio repositório. Mas, garanta que qualquer um tenha
>   permissão de acesso ao vídeo através deste link.

<mark>`https://www.youtube.com/watch?v=4-3lF5BCnzA`</mark>

### Vídeo - Timestamp

> [!IMPORTANT]
> - Coloque aqui um **intervalo de ~30 segundos** do vídeo acima, que
>   será a base de comparação para avaliar se o seu trabalho final
>   conseguiu ou não reproduzir a referência.

- **Timestamp inicial**: <mark>`00:30`</mark>
- **Timestamp final**: <mark>`01:00`</mark>

### Imagens

> [!IMPORTANT]
> - Coloque aqui **três imagens** capturadas do vídeo acima, que você
>   irá usar como ilustração para as explicações que vêm abaixo.

<mark>`<preencher>`</mark>

## Especificação textual

Para cada um dos requisitos abaixo (detalhados no [Enunciado do Trabalho final - Moodle](https://moodle.ufrgs.br/mod/assign/view.php?id=6018620)), escreva um parágrafo **curto** explicando como este requisito será atendido, apontando itens específicos do vídeo/imagens que você incluiu acima que atendem estes requisitos.
1. <img width="1060" height="590" alt="image" src="https://github.com/user-attachments/assets/3c5cf758-9918-4bd3-aeec-506f431fe0dd" />
2. <img width="1578" height="899" alt="image" src="https://github.com/user-attachments/assets/b9a8b497-825c-4972-b8d2-be1be7094190" />
3. <img width="1599" height="899" alt="image" src="https://github.com/user-attachments/assets/bbec0cfd-10d1-4052-8041-43d1a50cb34f" />


### Malhas poligonais complexas
<mark>`Os inimigos, armas e itens serão formados por malhas pologionais complexas, como visto na primeira imagem, conforme o print 1`</mark>

### Transformações geométricas controladas pelo usuário
<mark>`O recúo da arma do personagem que o usuário controla ao atirar. Conforme a segunda imagem`</mark>

### Diferentes tipos de câmeras
<mark>`Conforme o print 1, haverá câmera em primeira pessoa e, futuramente implementado, uma câmera em terceira pessoa que será acionada quando o personagem morrer.`</mark>

### Instâncias de objetos
<mark>`Conforme o print 2, terão caixas de munição, projéteis que sairão da arma do jogador, etc.`</mark>

### Testes de intersecção
<mark>`Conforme o print 3, o usuário não pode atravessar paredes do cenário.`</mark>

### Modelos de Iluminação em todos os objetos
<mark>`Conforme o print 2, no qual a iluminação vem do teto, ela está sombreando a porta e a parede do lado dela.`</mark>

### Mapeamento de texturas em todos os objetos
<mark>`Conforme todos os prints, cada inimigo/arma/parte do cenário tem sua própria textura.`</mark>

### Movimentação com curva Bézier cúbica
<mark>`Será futuramente implementado na trajetória que o projétil disparado por armas faz.`</mark>

### Animações baseadas no tempo ($\Delta t$)
<mark>`Conforme o print 2, haverá a movimentação do personagem, dos inimigos e dos projéteis.`</mark>

## Limitações esperadas

> [!IMPORTANT]
> - Coloque aqui uma lista de detalhes visuais ou de interação que
>   aparecem no vídeo e/ou imagens acima, mas que você **não pretende
>   implementar** ou que você **irá implementar parcialmente**.
> - Para cada item, **explique por que** não será implementado ou por
>   que será implementado parcialmente.

<mark>`Haverão 3 limitações:
1. Sangue ao um projétil acertar o inimigo: Consideramos desnecessário dentro do escopo do trabalho, uma vez que é apenas uma particula instanciada que não interage com os outros elementos.
2. Número de armas/munição presentes: Apenas uma arma será implementada, visando aproveitar melhor a curta duração da fase e concentrando o esforço em uma arma funcional.
3. Água no cenário: Não será implementada a parte de baixo do cenário, apresentada nos segundos finais do vídeo, por limitações de escopo.
`</mark>
