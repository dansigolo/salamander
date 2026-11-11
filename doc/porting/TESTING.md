# Como testar o Salamander Linux

## Compilar e executar

Feche a janela de uma versão anterior antes de abrir o executável novo. No terminal:

```sh
cd /home/danilo/nvme/workspace/git/salamander
cmake -S . -B build/qt -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/qt
./build/qt/salamander-qt
```

Para testar com pastas e arquivos de exemplo, execute em vez do último comando:

```sh
python3 tools/qt_smoke_fixture.py --launch
```

Esse comando cria uma pasta exclusiva em `/tmp/salamander-linux-test-*`, com painéis esquerdo/direito, subpastas, arquivos ocultos, nomes com acentos e espaços, permissões de execução e um link simbólico. Nenhum arquivo existente é alterado. O caminho aparece no terminal e os exemplos permanecem após fechar a janela. O arquivo `archive.tar.gz` é apenas um nome de exemplo, sem conteúdo compactado.

## Roteiro manual

| Teste | Ação | Resultado esperado |
| --- | --- | --- |
| Estrutura | Abra a janela e redimensione | Oito menus na ordem Left, Files, Edit, Commands, Plugins, Options, Right, Help; barra superior, locais/volumes e F1–F12 |
| Colunas | Observe um arquivo com extensão | Name, Ext, Size, Date, Time, Attr; nome sem a última extensão e extensão em coluna própria |
| Nomes especiais | Use Ctrl+H | `.hidden` e `.config.json` aparecem; nomes com acentos e espaços permanecem íntegros |
| Atributos Linux | Observe `run-me.sh` e `alpha-link.txt` | Permissões no formato `-rwx…`; o link começa com `l`; a pasta com `d` |
| Painel ativo | Clique em cada painel, pressione Tab e Shift+Tab | O painel ativo muda; barras e menus Commands/Edit atuam nele |
| Navegação | Entre em `folder-a` com Enter; volte com Backspace | Retorna à pasta anterior com foco em `folder-a`, sem marcações por padrão |
| Linha `..` | Dê Enter em `..`; tente marcá-la com Insert | Sobe um diretório; `..` nunca entra nos arquivos marcados e permanece primeiro, mesmo com ordenação invertida |
| Personalização | Options > Preserve selection when changing directories; marque itens, entre em uma pasta e volte | Com a opção ligada, restaura as marcas; desligada, limpa ao mudar de pasta. A preferência permanece após reiniciar, mas as marcas não são salvas entre execuções |
| Histórico | Navegue entre pastas e use voltar/avançar ou Alt+Left/Alt+Right | Caminhos anteriores são reabertos; histórico de um painel não altera o outro |
| Caminho inválido | Digite um caminho inexistente na barra do painel e Enter | A pasta atual é mantida e o erro aparece na linha de status |
| Marcação | Use Insert/Espaço sobre arquivos, depois mova com setas | Alterna a marcação e avança; setas movem o foco sem apagar marcações |
| Intervalo | Use Shift+setas/Home/End | Marca ou desmarca conforme o estado do item inicial; não inclui `..` |
| Todos/nenhum | Ctrl+A e Ctrl+D, com a lista em foco | Marca os itens elegíveis e limpa as marcações |
| Busca rápida | Digite `al`, use Up/Down, Right, Backspace e Esc | Foca nomes compatíveis; edita o prefixo; Esc encerra a busca sem limpar marcas |
| Ordenação | Ctrl+F3/F4/F5/F6; repita; clique nos cabeçalhos | Ordena por nome/extensão/data/tamanho, invertendo ao repetir; diretórios ficam antes dos arquivos |
| Ir para | Ctrl+L; digite uma pasta e Enter | Navega o painel ativo; esse campo aceita caminhos, não comandos de shell |
| Locais | Use Home, Desktop/Downloads quando disponíveis e o seletor de volumes | Navega o painel ativo; o seletor não monta nem desmonta volumes |
| Favorito | Add current directory; navegue e use o novo botão | Retorna ao caminho salvo; favoritos adicionados nesta etapa duram somente a sessão |
| Espaço livre | Navegue entre sistemas de arquivos | A barra mostra o espaço disponível, atualizado em segundo plano |
| Status | Marque arquivos e pastas | Mostra quantidade, pastas e soma dos bytes dos arquivos; tamanhos recursivos de pastas ainda não entram na soma |
| Barras visíveis | Options > nomes das barras | Mostra/oculta as barras durante a sessão |
| Ajuda e menus | F1 e F10 | Abre a ajuda de atalhos e ativa os menus |
| Monitoramento | Com um arquivo marcado, crie outro arquivo nessa pasta usando um editor/terminal | A lista é atualizada e o arquivo original permanece marcado |

Desligar a opção de preservar marcações não desmarca imediatamente os itens atuais; a política vale na próxima mudança de pasta e descarta marcações guardadas anteriormente. Navegação inválida ou para a mesma pasta não apaga marcas. Atualizações da lista na pasta atual continuam preservando marcas.

Backspace edita o prefixo quando a busca rápida está ativa. Pressione Esc antes de usá-lo para subir uma pasta. Ctrl+A em um campo de texto mantém a seleção de texto normal.

## Criar e renomear — F7 e F2

Use o lançador de exemplos acima. Todas as mudanças desta seção ficam na pasta temporária criada por ele.

1. No painel esquerdo, pressione **F7**, digite `Nova pasta á` e confirme. A pasta deve aparecer e receber o foco; o painel direito mantém seu caminho.
2. Com essa pasta em foco e sem itens marcados, pressione **F2**, escolha `Pasta renomeada` e confirme. O conteúdo é mantido e o foco acompanha o novo nome.
3. Tente criar novamente uma pasta ou renomear um arquivo para um nome existente. Um erro deve aparecer; o destino e o arquivo original permanecem intactos.
4. Marque dois arquivos com Insert e pressione F2. A aplicação pede um novo nome para cada item. Cancele o segundo diálogo: a primeira renomeação permanece e o segundo arquivo fica com o nome original.
5. Abra F7 e cancele: nada deve ser criado. Tente nomes vazios, `..` e `pasta/filha`: o diálogo deve pedir correção sem alterar o disco. Este F7 cria apenas um nível por vez.
6. Renomeie `alpha-link.txt`: o nome do link muda, e o destino `alpha.txt` continua intacto. Também é possível renomear links quebrados.
7. Crie `.pasta-oculta`: a mensagem confirma a criação; use Ctrl+H se o filtro estiver ocultando o resultado.
8. Verifique a barra inferior e os menus Files: os mesmos comandos F2/F7 devem funcionar por clique e teclado, mantendo os rótulos ao habilitar/desabilitar ações.

As operações são executadas em segundo plano. Os diálogos podem ser cancelados antes do envio; uma chamada atômica já iniciada não é interrompida. Se uma sequência falhar, as renomeações anteriores permanecem e os itens seguintes não são processados. Não há sobrescrita automática. Consulte `OPERATIONS.md` para a política completa.

## Copiar entre painéis — F5

1. Marque `alpha.txt`, `beta.md` e `folder-a` no painel esquerdo. Pressione **F5**: o destino deve ser a pasta do painel direito. Confirme e confira os arquivos e a árvore de pastas no destino. As origens devem continuar no painel esquerdo.
2. Sem marcas, foque um arquivo e use F5: somente esse item deve ser copiado.
3. Copie o mesmo arquivo novamente. Em **Skip**, o destino não muda; em **Replace**, ele só é substituído depois da cópia completa. **Skip all** e **Replace all files** valem para os próximos conflitos aplicáveis.
4. Copie uma pasta cujo nome já exista no destino: ela não será mesclada nesta entrega. Escolha pular ou cancelar, ou use outro destino.
5. Copie `alpha-link.txt` e um link quebrado, se disponível: os links devem ser preservados como links. O destino do link não deve ser alterado.
6. Digite um diretório inexistente no diálogo de destino: deve aparecer um erro sem apagar a origem nem criar um destino implícito.
7. Tente copiar um arquivo para a própria pasta com o mesmo nome, ou uma pasta para dentro dela mesma: a operação deve ser recusada.
8. Para observar progresso/cancelamento, opcionalmente use:

```sh
python3 tools/qt_smoke_fixture.py --large-mib 512 --launch
```

O exemplo acrescenta um arquivo esparso de 512 MiB lógicos. A cópia pode ocupar 512 MiB reais se concluída. Copie-o com F5 e pressione **Cancel** durante a operação. Aguarde o diálogo terminar a limpeza: a origem permanece e o item incompleto não deve aparecer com o nome final. Itens concluídos antes do cancelamento permanecem. Em máquinas rápidas, a cópia pode terminar antes do clique; uma cópia já concluída não é desfeita.

A barra mostra bytes planejados processados, incluindo itens pulados; o resumo final distingue itens copiados e pulados. A atualização é feita por um worker e a interface continua atendendo ao cancelamento. Veja COPYING.md para metadados e limitações.

## Limitações esperadas nesta etapa

F2 (renomear), F5 (copiar), F6 (mover) e F7 (criar pasta) estão ativos. F3–F4, F8–F9 e F11–F12 permanecem desabilitados: visualizar/editar, excluir, menu de comandos do usuário e conexões ainda precisam ser implementados. O campo **Go to** é navegação por caminho; a linha de comandos do original permanece pendente. Os ícones são do Qt/tema atual e a configuração de barras/favoritos ainda não é persistente. Não há cálculo recursivo de tamanho de pastas, paridade completa de plugins, máscaras avançadas de busca rápida ou todos os modos de visualização.

## Testes automatizados

```sh
ctest --test-dir build/qt --output-on-failure
python3 -m unittest discover -s tools/tests -p test_upstream_sync.py -v
```

O primeiro comando executa cinco suítes: algoritmo original de máscaras, operações locais, cópia, painéis e integração da janela. Elas cobrem histórico, nomes/colunas, `..`, seleção, atalhos, painel ativo, favoritos e atualizações da lista. Os testes gráficos usam o backend Qt `offscreen`; não substituem a conferência no desktop Cinnamon/X11/Wayland.

O segundo comando testa sincronização upstream em repositórios temporários, incluindo isolamento de conflitos. Não acessa a rede nem altera o clone do projeto.

## Conferir upstream e histórico Git

```sh
python3 tools/upstream_sync.py check --fetch
git log -5 --oneline
git status --short
```

A consulta upstream usa a rede para atualizar as referências do remote oficial, sem mesclar código. Se não houver novidades, o relatório informa isso. A integração efetiva deve seguir `UPSTREAM.md`.

## Como relatar um problema

Registre o commit (`git rev-parse --short HEAD`), o ambiente gráfico (X11/Wayland), os passos, o resultado esperado e o observado. Diga em qual painel ocorreu e se a busca rápida estava ativa. Uma captura da janela ajuda a comparar o layout com o Altap 4.0; confira o conteúdo visível antes de compartilhar.

## Mover entre painéis — F6

Use somente a fixture descartável (`python3 tools/qt_smoke_fixture.py --launch`).

1. Marque um arquivo e uma pasta na esquerda; pressione **F6**. Confira o destino da direita e confirme. Os itens devem desaparecer da origem e aparecer completos no destino; itens não marcados permanecem.
2. Sem marcas, foque um item e repita. Cancele o diálogo inicial: nenhuma mudança deve acontecer.
3. Crie arquivos com o mesmo nome nos dois painéis. Confira **Skip** (ambos preservados), **Replace** (destino substituído e origem removida) e **Cancel move**. Pastas existentes não oferecem substituição nem mesclagem.
4. Para EXDEV real, escolha uma pasta de testes em outro sistema de arquivos (por exemplo, `/dev/shm`, se houver espaço). Confira conteúdo, links e remoção da origem. Um caminho diferente na mesma partição não testa EXDEV.
5. Cancele um arquivo grande durante a cópia/verificação: a origem deve permanecer; uma cópia já publicada pode permanecer também, indicada no resultado. Durante a remoção verificada, o cancelamento aguarda o item atual terminar.
6. Leia os avisos de erro: eles distinguem origem mantida, origem parcial e caminho privado de recuperação. Não apague caminhos de recuperação antes de inspecioná-los. Restrições de ACL/xattrs, hardlinks e metadados estão em `MOVING.md`.

A suíte `file_move` usa dois sistemas de arquivos reais. Por padrão o destino é `/dev/shm/salamander-move-tests`; se necessário, rode `SALAMANDER_MOVE_TEST_DESTINATION=/pasta/em/outro/filesystem ctest --test-dir build/qt --output-on-failure`. A suíte falha quando não consegue exercitar EXDEV; não ignora silenciosamente esse cenário.
