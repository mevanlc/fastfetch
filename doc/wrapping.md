# Column wrapping

Fastfetch wraps the information column to the terminal width by default. Wrapped
continuations are indented four spaces, and each physical row advances the logo.
Line breaks prefer spaces and existing hyphens; long identifiers are split without
adding hyphens. Colors, hyperlinks, and UTF-8 characters are preserved.

```sh
fastfetch                  # automatic width, when stdout is a terminal
fastfetch --wrap            # explicitly enable automatic width
fastfetch --wrap=off        # use the previous layout
fastfetch --wrap=80         # lay out within 80 columns, including logo and padding
```

Space-separated arguments, such as `--wrap 80`, also work. In JSONC:

```jsonc
{
    "display": {
        "wrap": "auto" // "auto", "off", or an integer from 1 through 4294967295
    }
}
```

Automatic wrapping is disabled for redirected stdout or when the terminal width
cannot be measured. A numeric width works in pipes and files, and is used exactly
even when it exceeds the physical terminal width. `--pipe` controls colors, not
width detection; colorless output on a terminal still wraps.

For compatibility, `--disable-linewrap true` (or `display.disableLinewrap: true`)
also opts out of automatic column wrapping unless `wrap` is explicitly supplied.
An explicit `wrap` setting takes precedence for column layout. The existing
`disableLinewrap` setting still controls the terminal's own autowrap mode.

Side logos move above the information if fewer than 20 information columns remain.
Live refresh rechecks width each frame and restores the requested side position
when there is room again. Top and absent logos use the full width for information.
Artwork itself is not wrapped or resized; a logo wider than the terminal can still
overflow. Image backend and terminal support requirements still apply.

Explicit newlines retain their supplied indentation; only inserted line breaks
receive the four-space indent. Extremely narrow widths reduce that indent to leave
room for text. Characters wider than the entire column are emitted intact. The
width calculation uses Fastfetch's existing Unicode cell-width tables, so complex
emoji sequences can vary with the terminal's rendering.

Separators are clipped to the available width. Color samples wrap in whole blocks
when a block fits the column. Timing statistics occupy separate right-aligned rows
when wrapping is active, so they cannot overwrite values or the logo.

Custom output containing cursor-control sequences is emitted without reflowing
that logical line. SGR colors and OSC 8 hyperlinks are supported by the wrapper;
arbitrary terminal-control programs are not interpreted. JSON result output is
unchanged.
