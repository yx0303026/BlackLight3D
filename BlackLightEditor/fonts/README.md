# Editor fonts

Vendored icon font used by the editor UI.

## codicon.ttf

- Source: [@vscode/codicons](https://github.com/microsoft/vscode-codicons) `0.0.46-24`,
  published through the `@vscode/codicons` npm package (`package/dist/codicon.ttf`).
- Files: `codicon.ttf` (font), `LICENSE-codicons.txt` (CC-BY-4.0 for the icons),
  `LICENSE-CODE-codicons.txt` (MIT for the build code).
- The font is embedded into `blacklight_editor` at build time by
  `blacklight_embed_binary` (see `BlackLight/cmake/BlackLightEmbedBinary.cmake`) and
  registered by `EditorApplication` under the family `BlackLightIcons`.
- Glyph codepoints are declared in `include/editor/ui/EditorIcons.h`.

Redistribution notice: the Codicons font and icons are licensed by Microsoft under
CC-BY-4.0; see `LICENSE-codicons.txt` for the full text.
