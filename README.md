# Valigo

A small Mario-style platformer in C++20 and [raylib](https://www.raylib.com/).
It runs natively and in the browser through WebAssembly. All graphics are drawn
by the code (pixel art is stored as strings in `src/main.cpp`), so there are no
asset files.

## Controls

| Action  | Keys                     |
|---------|--------------------------|
| Move    | Arrow keys / A, D        |
| Jump    | Space / Up / W / Z       |
| Run     | Shift / X                |
| Start   | Enter                    |
| Music on/off | M                   |

Stomp the goombas, bump `?` blocks for coins, break bricks, and reach the
computer at the end of the level.

## Native build

```sh
cmake -B build
cmake --build build
./build/valigo
```

CMake downloads raylib 5.5 automatically with FetchContent.

## Web (WASM) build

Install and activate the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html), then:

```sh
emcmake cmake -B build-web
cmake --build build-web
cd build-web && python3 -m http.server 8080
# open http://localhost:8080/valigo.html
```

To publish the game, copy `valigo.html`, `valigo.js` and `valigo.wasm` to any
static web host. The HTML page template is in `web/shell.html`.
