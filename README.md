# Qmmp fork with media library

Fork of [Qmmp](https://qmmp.ylsoftware.com/), a Qt-based multimedia player that supports Winamp-like skins, that reworks the **Media Library** plugin to behave more like the Media Library in Winamp 2, specifically to support Artist / Album filtering.

<img width="1582" height="1048" alt="Qmmp with media library" src="https://github.com/user-attachments/assets/dff220e6-3577-448c-945b-5800e4c48b45" />


> **Status:** work in progress. This fork tracks upstream Qmmp and should only change the code as needed to bring the Media Library plugin in line with the features I'd like. Everything else (decoders, output plugins, UI, effects) is untouched.

---

## Features

Goals for this fork (check the box when implemented):

- [x] **Artist pane**: list of all artists in the library, with an "All (N Artists)" entry at the top
- [x] **Album pane**: shows albums for the selected artist(s), with an "All (N Albums)" entry
- [x] **Cascading filters**: selecting an artist narrows the album list; selecting an album narrows the track list
- [ ] **Multi-selection** in the Artist and Album panes (Ctrl/Shift-click)
- [x] **Track list** filtered by the current Artist/Album selection
- [x] **Quick search box** that filters all panes as you type
- [x] **Play / Enqueue / Add to playlist** actions on any artist, album, or track selection (double-click and context menu)
- [x] **Sortable columns** in the track list (title, artist, album, track number, year, genre, length)
- [ ] **Persistent** pane layout and splitter positions between sessions

## Download

Visit the [Releases](releases) tab to download pre-built binaries for Linux (binary or AppImage), Windows, or MacOS.

## Building

This fork builds the same way as upstream Qmmp. You need a C++ toolchain, CMake, and Qt development packages (including the Qt SQL module, which the Media Library uses). Refer to the [upstream build documentation](https://sourceforge.net/p/qmmp-dev/wiki/WinBuildQt6/) for the full list of dependencies and CMake options, including how to enable or disable individual plugins. Make sure the Media Library plugin is enabled in your build configuration.

For inspiration, refer to to the automated build script in the [Github Action](.github/workflows/release.yml).

## Upstream

- Qmmp homepage: [https://qmmp.ylsoftware.com/](https://qmmp.ylsoftware.com/)
- This fork periodically merges upstream changes; conflicts are resolved in favor of upstream outside the Media Library plugin.

## License

Qmmp is licensed under the **GNU General Public License v2 (or later)**.

## Disclaimer

This project is not affiliated with or endorsed by the Qmmp developers, Winamp, or any of its owners. "Winamp" is a trademark of its respective owners and is mentioned only to describe the interaction style.
