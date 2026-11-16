# Build

Voraussetzungen: `make`, ein C17-Compiler (`clang`, alternativ `gcc`), GLFW sowie
OpenGL.

```
lib3d/   Bibliothek (glcore, lib3d, lib3d_util)
app/     Anwendung (main.c, grid.c, snake.c)
```

Es gibt genau einen Build: das `Makefile` baut die Objekte nach `obj/` und das
Binary `bin/snake`.

## Release-Build (Standard)

```sh
make              # baut bin/snake
make -j$(nproc)   # parallel
make run          # baut bei Bedarf und startet bin/snake
make clean        # entfernt bin/ und obj/
```

Die Flags sind `-Wall -Wextra -O2 -std=c17 -Ilib3d`, gelinkt wird gegen
`-lglfw -lGL -lm`. Als Compiler ist `clang` voreingestellt; die Objekte landen
pfadgetreu unter `obj/lib3d/...` und `obj/app/...`.

Nach Code-Änderungen genügt ein erneutes `make` — gebaut wird nur, was sich
geändert hat.

## Debug-Build

```sh
make debug
```

Das baut mit `-O0 -g3` statt `-O2`. Release und Debug nutzen dieselben
Verzeichnisse (`obj/`, `bin/`), deshalb räumt `make debug` vorher auf. Ein
Wechsel in die andere Richtung geht ebenfalls nur über `make clean`:

```sh
make clean && make
```

## Compiler wechseln

Der Compiler lässt sich per `make`-Variable überschreiben:

```sh
make clean && make CC=gcc
```

Auch hier gilt: `make clean` vorher, weil die Objektdateien aus `obj/` sonst
nicht neu gebaut werden.

## Starten

```sh
./bin/snake
```

Das Programm öffnet ein GLFW/OpenGL-Fenster und braucht daher eine grafische
Umgebung.

## compile_commands.json (clangd / IntelliSense)

Die Datei ist nicht eingecheckt (siehe `.gitignore`) und wird lokal erzeugt.
Zwei Wege führen zum Ziel:

```sh
make init     # nutzt bear + das Makefile (benötigt bear)
./init.sh     # reines Python3-Skript, kein bear nötig
```

`make init` räumt vorher auf und schreibt `compile_commands.json` ins
Projektwurzelverzeichnis. clangd findet die Datei dort automatisch, ein
zusätzlicher Eintrag in einer `.clangd` ist nicht nötig.

## Editor

Die Tasks in [.vscode/tasks.json](.vscode/tasks.json) und
[.zed/tasks.json](.zed/tasks.json) rufen direkt die Make-Ziele auf
(`make`, `make debug`, `make run`, `make clean`, `make init`). Die
VS-Code-Launch-Konfigurationen debuggen `bin/snake` mit gdb.
