# notepad

Editor de notas rich text em QML/Quickshell inspirado no Notas do macOS, com layout em sidebar + editor e componentes compartilhados do Astrea.

## Dependencias

- Quickshell (`qs`)
- Componentes Astrea em `AstreaComponents`, apontando para `/home/agony/.local/share/Astrea/Core/components`

## Rodar

```sh
cd /home/agony/GitHub/Bench
qs -p notepad
```

## Atalhos

- `Ctrl+N`: novo documento
- `Ctrl+O`: abrir arquivo
- `Ctrl+S`: salvar
- `Ctrl+Shift+S`: salvar como
- `Ctrl+B`: negrito
- `Ctrl+I`: italico
- `Ctrl+U`: sublinhado
- `Ctrl+Q`: sair

Arquivos escolhidos no "Salvar como" sao salvos como `.md` por enquanto. Se outro sufixo for escolhido, ele e trocado para `.md`.

Os botoes de formatacao atuam apenas sobre texto selecionado; sem selecao, eles nao inserem texto automaticamente.
