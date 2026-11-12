# Appearance and panel selection

**Options → Appearance…** offers System, Light, Dark and Dracula. System uses the palette Qt receives from the desktop. Accent color presets tint selection and the active panel indicator. The path, controls and free-space label use their usual theme colors. A narrow accent stripe identifies the active panel.

Changes preview in the running application. **Save** remembers the theme, accent and custom theme library. **Cancel** restores the previously saved appearance. Desktop window decorations and native dialogs follow the desktop settings.

Dracula uses the [official Dracula color palette](https://draculatheme.com/contribute#color-palette), adapted to the Salamander widgets.

## Custom themes

Select a starting theme and click **New…** to edit the five colors. **Edit…** modifies a saved custom theme. **Export…** writes the current palette to a portable JSON file, including the accent override if selected. **Import…** installs a compatible file in your theme library when you save. Select **Theme default** to use an imported theme's own accent and selection colors.

Theme files use this format:

```json
{
  "version": 1,
  "name": "My dark theme",
  "colors": {
    "window": "#282a36",
    "base": "#21222c",
    "text": "#f8f8f2",
    "selection": "#44475a",
    "accent": "#bd93f9"
  }
}
```

Use opaque hexadecimal RGB colors. Text must have at least 4.5:1 contrast against the window and file list backgrounds. The app computes a readable text color for the selection. Import accepts the Salamander JSON format. To adapt a palette from another app, enter its colors in the editor or convert them to these five fields.

## Full Row Select

By default the icon and name accept clicks, and the name area carries the focus and mark highlights. Clicking metadata leaves focus and marks alone. **Options → Full Row Select** expands both the click target and the highlights to the whole row. The setting applies to both panels and is saved across restarts. Plain clicks focus an item, and Ctrl-click, Insert and Space retain their marking behavior.
