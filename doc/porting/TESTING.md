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

## Limitações esperadas nesta etapa

As ações F2–F9 e F11–F12 aparecem desabilitadas: renomear, visualizar/editar, copiar, mover, criar/excluir pastas, menu de comandos do usuário e conexões ainda precisam ser implementados. O campo **Go to** é navegação por caminho; a linha de comandos do original permanece pendente. Os ícones são do Qt/tema atual e a configuração de barras/favoritos ainda não é persistente. Não há cálculo recursivo de tamanho de pastas, paridade completa de plugins, máscaras avançadas de busca rápida ou todos os modos de visualização.

## Testes automatizados

```sh
ctest --test-dir build/qt --output-on-failure
python3 -m unittest discover -s tools/tests -p test_upstream_sync.py -v
```

O primeiro comando executa três suítes: algoritmo original de máscaras, painéis e integração da janela. Elas cobrem histórico, nomes/colunas, `..`, seleção, atalhos, painel ativo, favoritos e atualizações da lista. Os testes gráficos usam o backend Qt `offscreen`; não substituem a conferência no desktop Cinnamon/X11/Wayland.

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
