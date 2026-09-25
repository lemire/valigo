// Valigo: a small Mario-style platformer written in C++20 with raylib.
// Builds natively and for the browser (Emscripten/WebAssembly).
// All graphics are procedural, so there are no asset files to ship.

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

// ---------------------------------------------------------------------------
// Tuning
// ---------------------------------------------------------------------------
constexpr int kTile = 32;
constexpr int kRows = 14;
constexpr int kGroundRow = 12;
constexpr int kScreenW = 800;
constexpr int kScreenH = kRows * kTile;  // 448

constexpr float kGravityHold = 1500.0f;  // while rising with jump held
constexpr float kGravity = 3400.0f;
constexpr float kMaxFall = 900.0f;
constexpr float kJumpSpeed = 720.0f;
constexpr float kWalkSpeed = 200.0f;
constexpr float kRunSpeed = 330.0f;
constexpr float kGoombaSpeed = 60.0f;
constexpr float kStep = 1.0f / 120.0f;  // physics sub-step
constexpr float kLevelTime = 300.0f;

constexpr Color kSky{92, 148, 252, 255};

// Tiles: '#' ground, 'B' brick, '?' question block, 'X' used block,
// 'S' hard block, '<' '>' pipe top, '(' ')' pipe body.
constexpr bool isSolid(char c) { return std::string_view("#BSX?<>()").find(c) != std::string_view::npos; }

// ---------------------------------------------------------------------------
// Sprites (pixel art as strings)
// ---------------------------------------------------------------------------
template <std::size_t N>
consteval bool sameWidth(const std::array<std::string_view, N>& rows, std::size_t w) {
  for (auto r : rows)
    if (r.size() != w) return false;
  return true;
}

// Valigo: bearded, glasses, green sweater, jeans, boots. Faces right.
// 14x24 pixels: head + torso (17 rows) shared by all frames, legs (7 rows) animated.
constexpr int kHeroW = 14;
constexpr std::array<std::string_view, 17> kHeroTop{
    ".....HHHHh....",  //
    "...HHHHhHHH...",  //
    "..HHHHHHHHHH..",  //
    "..HHHHSSSSSS..",  //
    "..HHHSSSEEEE..",  // glasses: top rim
    "..HHsEEEEKSE..",  // ear, temple arm, lens with eye
    "..HHSSSSEEEES.",  // bottom rim, nose
    "..HHSDDDDDSS..",  // beard
    "...HDDDDDDKD..",  //
    "....DDDDDDDD..",  //
    ".....DDDDDD...",  //
    "......SSGG....",  // neck, collar
    "....GGGGGGGG..",  // sweater
    "...gGGGGGGGGG.",  //
    "...gGGGGgGGGG.",  //
    "..SgGGGGgGGGGS",  // hands
    "...GGGGGGGGG..",  //
};
constexpr std::array<std::string_view, 7> kLegsStand{
    "....JJJJJJJ...", "....JJJJJJJ...", "....JJJ.JJJ...", "....JJJ.JJJ...",
    "....ccc.ccc...", "....BBBBBBBB..", "....bbbbbbbbb.",
};
constexpr std::array<std::string_view, 7> kLegsStride{
    "....JJJJJJJ...", "...JJJJ.JJJJ..", "...JJJ...JJJ..", "..JJJ.....JJJ.",
    "..ccc.....ccc.", ".BBBB.....BBBB", ".bbb......bbbb",
};
constexpr std::array<std::string_view, 7> kLegsPass{
    "....JJJJJJJ...", "....JJJJJJJ...", "....JJJ.JJJ...", "...JJJ..JJJ...",
    "...ccc...ccc..", "..BBBB...BBBB.", "..bbb....bbbb.",
};
constexpr std::array<std::string_view, 7> kLegsJump{
    "....JJJJJJJJ..", "....JJJJJJJJJ.", "...JJJ....JJJ.", "..JJJ.....ccc.",
    "..ccc....BBBB.", ".BBBB....bbbb.", ".bbb..........",
};

constexpr std::array<std::string_view, 16> kGoomba{
    "......MMMM......", ".....MMMMMM.....", "....MMMMMMMM....", "...MKKMMMMKKM...",
    "..MMWKMMMMKWMM..", "..MMWKKMMKKWMM..", ".MMMWWKMMKWWMMM.", ".MMMMMMMMMMMMMM.",
    "MMMMMMMMMMMMMMMM", "MMMMMTTTTTTMMMMM", ".MMMTTTTTTTTMMM.", "....TTTTTTTT....",
    "...KKTTTTTTKK...", "..KKKKTTTTKKKK..", "..KKKKK..KKKKK..", "...KKKK..KKKK...",
};

static_assert(sameWidth(kHeroTop, kHeroW) && sameWidth(kLegsStand, kHeroW) && sameWidth(kLegsStride, kHeroW) &&
              sameWidth(kLegsPass, kHeroW) && sameWidth(kLegsJump, kHeroW));
static_assert(sameWidth(kGoomba, 16));

constexpr Color paletteColor(char c) {
  switch (c) {
    case 'H': return {110, 56, 20, 255};    // hair
    case 'h': return {165, 95, 40, 255};    // hair highlight
    case 'D': return {96, 48, 16, 255};     // beard
    case 'S': return {248, 196, 150, 255};  // skin
    case 's': return {215, 150, 105, 255};  // skin shadow
    case 'E': return {236, 186, 24, 255};   // glasses frame
    case 'G': return {70, 145, 62, 255};    // sweater
    case 'g': return {40, 100, 40, 255};    // sweater shadow
    case 'J': return {38, 72, 135, 255};    // jeans
    case 'c': return {72, 112, 172, 255};   // rolled cuffs
    case 'B': return {105, 62, 30, 255};    // boots
    case 'b': return {62, 36, 16, 255};     // boot soles
    case 'M': return {172, 76, 20, 255};    // goomba body
    case 'T': return {240, 196, 150, 255};  // goomba face
    case 'K': return {30, 16, 8, 255};      // dark
    case 'W': return {255, 255, 255, 255};
    default: return BLANK;
  }
}

void drawPixels(std::span<const std::string_view> rows, float x, float y, float scale, bool flip,
                float squash = 1.0f) {
  const float h = scale * squash;
  for (std::size_t r = 0; r < rows.size(); ++r) {
    const auto row = rows[r];
    for (std::size_t c = 0; c < row.size(); ++c) {
      if (row[c] == '.') continue;
      const std::size_t col = flip ? row.size() - 1 - c : c;
      DrawRectangleRec({x + col * scale, y + r * h, scale, h}, paletteColor(row[c]));
    }
  }
}

// ---------------------------------------------------------------------------
// Level
// ---------------------------------------------------------------------------
struct Level {
  int width = 0;
  std::vector<std::string> rows;
  int computerX = 0;  // goal: a desk with a computer

  char at(int tx, int ty) const {
    if (tx < 0 || tx >= width) return '#';  // invisible side walls
    if (ty < 0 || ty >= kRows) return '.';
    return rows[ty][tx];
  }
  bool solidAt(int tx, int ty) const { return isSolid(at(tx, ty)); }
  void set(int tx, int ty, char c) {
    if (tx >= 0 && tx < width && ty >= 0 && ty < kRows) rows[ty][tx] = c;
  }
};

struct TilePos {
  int x, y;
};

struct LevelData {
  Level level;
  TilePos start{};
  std::vector<TilePos> goombas;
  std::vector<TilePos> coins;
};

// Loosely modelled on World 1-1.
LevelData buildLevel() {
  LevelData d;
  Level& lv = d.level;
  lv.width = 212;
  lv.rows.assign(kRows, std::string(lv.width, '.'));

  auto ground = [&](int x0, int x1) {
    for (int x = x0; x < x1; ++x)
      for (int y = kGroundRow; y < kRows; ++y) lv.set(x, y, '#');
  };
  auto blocks = [&](int x, int y, std::string_view s) {
    for (std::size_t i = 0; i < s.size(); ++i) lv.set(x + int(i), y, s[i]);
  };
  auto pipe = [&](int x, int h) {
    const int top = kGroundRow - h;
    lv.set(x, top, '<');
    lv.set(x + 1, top, '>');
    for (int y = top + 1; y < kGroundRow; ++y) {
      lv.set(x, y, '(');
      lv.set(x + 1, y, ')');
    }
  };
  auto column = [&](int x, int h) {
    for (int i = 0; i < h; ++i) lv.set(x, kGroundRow - 1 - i, 'S');
  };
  auto stairsUp = [&](int x, int n) {
    for (int i = 0; i < n; ++i) column(x + i, i + 1);
  };
  auto stairsDown = [&](int x, int n) {
    for (int i = 0; i < n; ++i) column(x + i, n - i);
  };

  ground(0, 69);
  ground(71, 86);
  ground(89, 153);
  ground(155, 212);

  blocks(16, 8, "?");
  blocks(20, 8, "B?B?B");
  blocks(22, 4, "?");
  pipe(28, 2);
  pipe(38, 3);
  pipe(46, 4);
  pipe(57, 4);
  blocks(77, 8, "B?B");
  blocks(80, 4, "BBBBBBBB");
  blocks(91, 4, "BBB?");
  blocks(94, 8, "B");
  blocks(100, 8, "BB");
  blocks(106, 8, "?");
  blocks(109, 8, "?");
  blocks(109, 4, "?");
  blocks(112, 8, "?");
  blocks(118, 8, "B");
  blocks(121, 4, "BBB");
  blocks(128, 4, "B??B");
  blocks(129, 8, "BB");
  stairsUp(134, 4);
  stairsDown(140, 4);
  stairsUp(148, 4);
  column(152, 4);
  stairsDown(155, 4);
  pipe(163, 2);
  blocks(168, 8, "BB?B");
  pipe(179, 2);
  stairsUp(181, 8);
  column(189, 8);

  lv.computerX = 199;

  d.start = {3, 11};
  d.goombas = {{22, 11},  {40, 11},  {51, 11},  {53, 11},  {80, 3},   {82, 3},   {97, 11},  {99, 11},
               {114, 11}, {116, 11}, {124, 11}, {126, 11}, {128, 11}, {130, 11}, {174, 11}, {176, 11}};
  d.coins = {{68, 8},  {69, 7},   {70, 7},   {71, 8},   {86, 8},   {87, 7},  {88, 8},
             {100, 7}, {101, 7},  {144, 10}, {145, 10}, {146, 10}, {147, 10}};
  return d;
}

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------
struct Hits {
  bool ground = false;
  bool ceiling = false;
  bool wall = false;
  int headX = -1, headY = -1;  // tile bumped from below
};

// Splits a frame into equal physics steps of at most kStep.
int substepCount(float dt) { return std::max(1, int(std::ceil(dt / kStep - 1e-3f))); }

std::pair<int, int> tileSpan(float lo, float hi) {
  return {int(std::floor(lo / kTile)), int(std::floor((hi - 0.01f) / kTile))};
}

// Moves a box through the tile map, one axis at a time.
Hits moveAndCollide(const Level& lv, Rectangle& b, Vector2& v, float dt) {
  Hits h;

  b.x += v.x * dt;
  if (v.x != 0.0f) {
    auto [x0, x1] = tileSpan(b.x, b.x + b.width);
    auto [y0, y1] = tileSpan(b.y, b.y + b.height);
    for (int ty = y0; ty <= y1; ++ty)
      for (int tx = x0; tx <= x1; ++tx) {
        if (!lv.solidAt(tx, ty)) continue;
        if (v.x > 0)
          b.x = std::min(b.x, float(tx * kTile) - b.width);
        else
          b.x = std::max(b.x, float((tx + 1) * kTile));
        h.wall = true;
      }
    if (h.wall) v.x = 0;
  }

  b.y += v.y * dt;
  if (v.y != 0.0f) {
    auto [x0, x1] = tileSpan(b.x, b.x + b.width);
    auto [y0, y1] = tileSpan(b.y, b.y + b.height);
    const float centerX = b.x + b.width / 2;
    float bestDist = 1e9f;
    for (int ty = y0; ty <= y1; ++ty)
      for (int tx = x0; tx <= x1; ++tx) {
        if (!lv.solidAt(tx, ty)) continue;
        if (v.y > 0) {
          b.y = std::min(b.y, float(ty * kTile) - b.height);
          h.ground = true;
        } else {
          b.y = std::max(b.y, float((ty + 1) * kTile));
          h.ceiling = true;
          const float dist = std::fabs((tx + 0.5f) * kTile - centerX);
          if (dist < bestDist) {
            bestDist = dist;
            h.headX = tx;
            h.headY = ty;
          }
        }
      }
    if (h.ground || h.ceiling) v.y = 0;
  }
  // Standing counts as grounded even when this step moved too little to touch the floor.
  if (v.y >= 0 && !h.ground) {
    auto [x0, x1] = tileSpan(b.x, b.x + b.width);
    const int below = int(std::floor((b.y + b.height + 1.0f) / kTile));
    for (int tx = x0; tx <= x1 && !h.ground; ++tx)
      h.ground = lv.solidAt(tx, below) && b.y + b.height >= below * kTile - 0.5f;
  }
  return h;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------
struct Player {
  Rectangle box{};
  Vector2 vel{};
  bool onGround = false;
  bool facingRight = true;
  bool visible = true;
  float coyote = 0;
  float jumpBuffer = 0;
  float walkAnim = 0;
  int stompCombo = 0;
};

enum class EnemyState { Walking, Squashed, Knocked, Gone };

struct Enemy {
  Rectangle box{};
  Vector2 vel{};
  int dir = -1;
  EnemyState state = EnemyState::Walking;
  bool active = false;
  float timer = 0;
};

struct Coin {
  Vector2 center;
  bool taken = false;
};
struct CoinPop {
  Vector2 pos;
  float vy;
  float t = 0;
};
struct Debris {
  Vector2 pos, vel;
};
struct FloatText {
  Vector2 pos;
  int value;
  float t = 0;
};
struct Bump {
  int tx, ty;
  float t = 0;
};

struct Input {
  bool left, right, run, jumpHeld, jumpPressed;
};

bool anyKey(bool (*fn)(int), std::initializer_list<int> keys) {
  return std::ranges::any_of(keys, fn);
}

Input readInput() {
  return {
      .left = anyKey(IsKeyDown, {KEY_LEFT, KEY_A}),
      .right = anyKey(IsKeyDown, {KEY_RIGHT, KEY_D}),
      .run = anyKey(IsKeyDown, {KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_X, KEY_J}),
      .jumpHeld = anyKey(IsKeyDown, {KEY_SPACE, KEY_UP, KEY_W, KEY_Z, KEY_K}),
      .jumpPressed = anyKey(IsKeyPressed, {KEY_SPACE, KEY_UP, KEY_W, KEY_Z, KEY_K}),
  };
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------
void textShadow(const char* s, int x, int y, int size, Color c) {
  DrawText(s, x + 2, y + 2, size, {0, 0, 0, 150});
  DrawText(s, x, y, size, c);
}

void textCentered(const char* s, int y, int size, Color c) {
  textShadow(s, (kScreenW - MeasureText(s, size)) / 2, y, size, c);
}

void drawBrick(int x, int y) {
  constexpr Color base{184, 72, 16, 255}, mortar{60, 20, 0, 255};
  DrawRectangle(x, y, kTile, kTile, base);
  for (int r = 0; r < 4; ++r) {
    const int ry = y + r * 8;
    DrawRectangle(x, ry, kTile, 1, mortar);
    const int off = (r % 2) * 8;
    DrawRectangle(x + off, ry, 1, 8, mortar);
    DrawRectangle(x + off + 16, ry, 1, 8, mortar);
  }
}

void drawGround(int x, int y) {
  DrawRectangle(x, y, kTile, kTile, {200, 76, 12, 255});
  DrawRectangle(x, y, kTile, 2, {252, 188, 140, 255});
  DrawRectangleLines(x, y, kTile, kTile, {100, 36, 0, 255});
  DrawRectangle(x + 12, y + 2, 1, 14, {100, 36, 0, 255});
  DrawRectangle(x + 1, y + 16, 22, 1, {100, 36, 0, 255});
}

void drawRivets(int x, int y, Color c) {
  for (auto [dx, dy] : {std::pair{3, 3}, {kTile - 6, 3}, {3, kTile - 6}, {kTile - 6, kTile - 6}})
    DrawRectangle(x + dx, y + dy, 3, 3, c);
}

void drawQuestion(int x, int y, float clock) {
  const float pulse = 0.5f + 0.5f * std::sin(clock * 5.0f);
  const Color fill{252, (unsigned char)(160 + 40 * pulse), (unsigned char)(40 + 30 * pulse), 255};
  DrawRectangle(x, y, kTile, kTile, fill);
  DrawRectangleLines(x, y, kTile, kTile, {136, 60, 0, 255});
  drawRivets(x, y, {136, 60, 0, 255});
  // "C++" in chunky pixels (11x5, drawn at 2x).
  static constexpr std::array<std::string_view, 5> kLabel{
      "###........", "#....#...#.", "#...###.###", "#....#...#.", "###........",
  };
  static_assert(sameWidth(kLabel, 11));
  for (std::size_t r = 0; r < kLabel.size(); ++r)
    for (std::size_t c = 0; c < kLabel[r].size(); ++c)
      if (kLabel[r][c] == '#') DrawRectangle(x + 5 + int(c) * 2, y + 11 + int(r) * 2, 2, 2, {136, 60, 0, 255});
}

void drawUsed(int x, int y) {
  DrawRectangle(x, y, kTile, kTile, {136, 84, 40, 255});
  DrawRectangleLines(x, y, kTile, kTile, {60, 30, 0, 255});
  drawRivets(x, y, {60, 30, 0, 255});
}

void drawHard(int x, int y) {
  DrawRectangle(x, y, kTile, kTile, {200, 110, 60, 255});
  DrawRectangle(x, y, kTile, 3, {252, 188, 140, 255});
  DrawRectangle(x, y, 3, kTile, {252, 188, 140, 255});
  DrawRectangle(x, y + kTile - 3, kTile, 3, {100, 36, 0, 255});
  DrawRectangle(x + kTile - 3, y, 3, kTile, {100, 36, 0, 255});
}

void drawPipeSection(int x, int y, bool top) {
  constexpr Color body{0, 168, 0, 255}, light{140, 220, 80, 255}, dark{0, 80, 0, 255};
  const int inset = top ? -3 : 2;
  const int w = 2 * kTile - 2 * inset;
  DrawRectangle(x + inset, y, w, kTile, body);
  DrawRectangle(x + inset + 6, y, 6, kTile, light);
  DrawRectangle(x + inset + w - 14, y, 4, kTile, dark);
  DrawRectangleLines(x + inset, y, w, kTile, dark);
}

void drawCoinShape(Vector2 c, float clock) {
  const float squeeze = std::fabs(std::sin(clock * 4.0f));
  DrawEllipse(int(c.x), int(c.y), 3 + 6 * squeeze, 12, {252, 216, 0, 255});
  DrawEllipse(int(c.x), int(c.y), 1 + 3 * squeeze, 8, {252, 240, 140, 255});
}

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Music: a tiny chiptune synth (square lead, triangle bass, noise drums).
// Scores use one token per 16th note: a note ("C5", "F#4", "Bb3"),
// "." to hold the previous note, "-" for silence. '|' separates bars.
// ---------------------------------------------------------------------------
constexpr int kHold = -2;
constexpr int kRest = -1;

std::vector<int> parseScore(std::string_view score) {
  constexpr std::array<int, 7> kSemitone{9, 11, 0, 2, 4, 5, 7};  // A..G
  auto isSep = [](char c) { return c == ' ' || c == '|' || c == '\n'; };
  std::vector<int> steps;
  for (std::size_t i = 0; i < score.size();) {
    if (isSep(score[i])) {
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j < score.size() && !isSep(score[j])) ++j;
    const std::string_view tok = score.substr(i, j - i);
    i = j;
    if (tok == ".") {
      steps.push_back(kHold);
    } else if (tok == "-") {
      steps.push_back(kRest);
    } else {
      int note = kSemitone[tok[0] - 'A'];
      std::size_t k = 1;
      if (tok[k] == '#') ++note, ++k;
      else if (tok[k] == 'b') --note, ++k;
      steps.push_back(12 * (tok[k] - '0' + 1) + note);
    }
  }
  return steps;
}

// Bass line bouncing between each root and its octave, two roots per bar.
std::vector<int> bassFromRoots(std::string_view roots) {
  std::vector<int> steps;
  for (int r : parseScore(roots))
    for (int k = 0; k < 2; ++k) steps.insert(steps.end(), {r, kHold, r + 12, kHold});
  return steps;
}

struct Song {
  std::vector<int> lead, bass;
  bool drums = true;
  bool loop = true;
};

Song themeSong() {
  return {
      .lead = parseScore("E5 . G5 . C6 . G5 . A5 . G5 . E5 . D5 . |"
                         "C5 . D5 . E5 . . . - - G4 . C5 . - - |"
                         "F5 . A5 . C6 . A5 . G5 . F5 . E5 . D5 . |"
                         "E5 . . . C5 . . . D5 . . . - - - - |"
                         "E5 . G5 . C6 . G5 . A5 . B5 . C6 . D6 . |"
                         "E6 . D6 . C6 . A5 . G5 . . . - - - - |"
                         "F5 . E5 . D5 . F5 . E5 . D5 . C5 . B4 . |"
                         "C5 . . . . . . . - - - - - - - -"),
      .bass = bassFromRoots("C3 C3 | A2 G2 | F2 F2 | G2 G2 | C3 C3 | A2 F2 | F2 G2 | C3 C3"),
  };
}

Song victorySong() {
  return {
      .lead = parseScore("C5 E5 G5 C6 . . . - B4 D5 G5 B5 . . . - | C5 E5 G5 C6 E6 . . . . . . . - - - -"),
      .bass = parseScore("C3 . . . . . . - G2 . . . . . . - | C3 . . . . . . . . . . . - - - -"),
      .drums = false,
      .loop = false,
  };
}

class Chiptune {
 public:
  static constexpr int kSampleRate = 44100;
  static constexpr int kChunk = 4096;
  static constexpr float kStepSeconds = 0.1f;  // one 16th note at 150 BPM

  Chiptune() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    SetAudioStreamBufferSizeDefault(kChunk);
    stream_ = LoadAudioStream(kSampleRate, 32, 1);
    PlayAudioStream(stream_);
    ready_ = true;
  }
  ~Chiptune() {
    if (ready_) UnloadAudioStream(stream_);
    CloseAudioDevice();
  }
  Chiptune(const Chiptune&) = delete;
  Chiptune& operator=(const Chiptune&) = delete;

  void play(Song song) {
    song_ = std::move(song);
    playing_ = true;
    step_ = 0;
    sampleInStep_ = kSamplesPerStep;  // trigger step 0 on the next sample
  }
  void stop() {
    playing_ = false;
    lead_.gate = bass_.gate = false;
  }
  void toggleMute() { muted_ = !muted_; }
  bool muted() const { return muted_; }

  // Call once per frame: refills whichever stream buffers have been consumed.
  void update() {
    if (!ready_) return;
    while (IsAudioStreamProcessed(stream_)) {
      for (float& sample : buffer_) sample = nextSample();
      UpdateAudioStream(stream_, buffer_.data(), kChunk);
    }
  }

 private:
  static constexpr int kSamplesPerStep = int(kSampleRate * kStepSeconds);
  static constexpr float kDt = 1.0f / kSampleRate;

  struct Voice {
    float freq = 0, phase = 0, time = 0, level = 0;
    bool gate = false;

    void apply(int token) {
      if (token >= 0) {
        freq = 440.0f * std::pow(2.0f, (token - 69) / 12.0f);
        time = 0;
        gate = true;
      } else if (token == kRest) {
        gate = false;
      }
    }
    // Returns the envelope level; the smoothing avoids clicks.
    float envelope(float decayRate, float sustain) {
      time += kDt;
      const float target = gate ? sustain + (1 - sustain) * std::exp(-time * decayRate) : 0.0f;
      level += (target - level) * 0.01f;
      return level;
    }
    float advance() {
      phase += freq * kDt;
      if (phase >= 1.0f) phase -= 1.0f;
      return phase;
    }
  };

  void nextStep() {
    const std::size_t length = std::max(song_.lead.size(), song_.bass.size());
    if (step_ >= length) {
      if (!song_.loop) {
        stop();
        return;
      }
      step_ = 0;
    }
    if (step_ < song_.lead.size()) lead_.apply(song_.lead[step_]);
    if (step_ < song_.bass.size()) bass_.apply(song_.bass[step_]);
    if (song_.drums) {
      const std::size_t beat = step_ % 16;
      if (beat % 2 == 0) hat_ = 1.0f;
      if (beat == 4 || beat == 12) snare_ = 1.0f;
      if (beat == 0 || beat == 8 || beat == 10) kick_ = 1.0f, kickPhase_ = 0;
    }
    ++step_;
  }

  float nextSample() {
    if (playing_ && ++sampleInStep_ >= kSamplesPerStep) {
      sampleInStep_ = 0;
      nextStep();
    }
    const float square = (lead_.advance() < 0.25f ? 1.0f : -1.0f) + 0.5f;  // 25% duty, DC removed
    const float triangle = 4.0f * std::fabs(bass_.advance() - 0.5f) - 1.0f;
    noise_ = noise_ * 1664525u + 1013904223u;
    const float white = float(noise_ >> 8) / float(1u << 24) * 2.0f - 1.0f;

    hat_ *= 0.9989f;    // ~20 ms decay
    snare_ *= 0.9997f;  // ~80 ms
    kick_ *= 0.9998f;   // ~120 ms
    kickPhase_ += (45.0f + 110.0f * kick_) * kDt;

    float mix = 0.16f * square * lead_.envelope(6.0f, 0.55f) +
                0.24f * triangle * bass_.envelope(4.0f, 0.7f) +
                0.05f * white * hat_ + 0.12f * white * snare_ +
                0.35f * std::sin(2.0f * PI * kickPhase_) * kick_;
    if (muted_) mix = 0;
    return std::clamp(mix, -1.0f, 1.0f);
  }

  AudioStream stream_{};
  bool ready_ = false, playing_ = false, muted_ = false;
  Song song_;
  std::size_t step_ = 0;
  int sampleInStep_ = 0;
  Voice lead_, bass_;
  float hat_ = 0, snare_ = 0, kick_ = 0, kickPhase_ = 0;
  std::uint32_t noise_ = 12345;
  std::array<float, kChunk> buffer_{};
};

enum class State { Title, Playing, Dying, Clear, GameOver };

class Game {
 public:
  Game() {
    newGame();
    state_ = State::Title;
  }

  void frame() {
    const float dt = std::min(GetFrameTime(), 1.0f / 20.0f);
    if (IsKeyPressed(KEY_M)) music_.toggleMute();
    update(dt);
    music_.update();
    BeginDrawing();
    draw();
    EndDrawing();
  }

 private:
  Chiptune music_;
  Level level_;
  Player player_;
  std::vector<Enemy> enemies_;
  std::vector<Coin> coins_;
  std::vector<CoinPop> pops_;
  std::vector<Debris> debris_;
  std::vector<FloatText> texts_;
  std::vector<Bump> bumps_;

  State state_ = State::Title;
  int score_ = 0, hiScore_ = 0, coinCount_ = 0, lives_ = 3;
  float timeLeft_ = kLevelTime;
  float camX_ = 0;
  float clock_ = 0, stateTimer_ = 0, tallyAcc_ = 0;
  int clearPhase_ = 0;

  float groundTop() const { return float(kGroundRow * kTile); }
  float deskX() const { return float(level_.computerX * kTile); }
  float keyboardX() const { return deskX() + 48; }

  // --- setup -------------------------------------------------------------
  void newGame() {
    score_ = 0;
    coinCount_ = 0;
    lives_ = 3;
    loadLevel();
  }

  void loadLevel() {
    LevelData d = buildLevel();
    level_ = std::move(d.level);
    player_ = Player{};
    player_.onGround = true;
    player_.box = {d.start.x * float(kTile) + 6, (d.start.y + 1) * float(kTile) - 44, 20, 44};
    enemies_.clear();
    for (auto [x, y] : d.goombas) {
      Enemy e;
      e.box = {x * float(kTile) + 2, y * float(kTile) + 4, 28, 28};
      enemies_.push_back(e);
    }
    coins_.clear();
    for (auto [x, y] : d.coins) coins_.push_back({{(x + 0.5f) * kTile, (y + 0.5f) * kTile}});
    pops_.clear();
    debris_.clear();
    texts_.clear();
    bumps_.clear();
    timeLeft_ = kLevelTime;
    camX_ = 0;
    clearPhase_ = 0;
    stateTimer_ = 0;
    state_ = State::Playing;
    music_.play(themeSong());
  }

  // --- scoring -----------------------------------------------------------
  void addScore(int points, Vector2 at) {
    score_ += points;
    hiScore_ = std::max(hiScore_, score_);
    texts_.push_back({at, points});
  }

  void addCoin(Vector2 at) {
    if (++coinCount_ >= 100) {
      coinCount_ -= 100;
      ++lives_;
    }
    addScore(200, at);
  }

  // --- update ------------------------------------------------------------
  void update(float dt) {
    clock_ += dt;
    updateEffects(dt);
    switch (state_) {
      case State::Title:
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) newGame();
        break;
      case State::Playing: updatePlaying(dt); break;
      case State::Dying: updateDying(dt); break;
      case State::Clear: updateClear(dt); break;
      case State::GameOver:
        if (IsKeyPressed(KEY_ENTER)) {
          state_ = State::Title;
          music_.play(themeSong());
        }
        break;
    }
  }

  void updateEffects(float dt) {
    for (auto& p : pops_) {
      p.t += dt;
      p.vy += 1800.0f * dt;
      p.pos.y += p.vy * dt;
    }
    std::erase_if(pops_, [](const CoinPop& p) { return p.t > 0.55f; });
    for (auto& d : debris_) {
      d.vel.y += 1800.0f * dt;
      d.pos.x += d.vel.x * dt;
      d.pos.y += d.vel.y * dt;
    }
    std::erase_if(debris_, [](const Debris& d) { return d.pos.y > kScreenH + 32; });
    for (auto& t : texts_) {
      t.t += dt;
      t.pos.y -= 50.0f * dt;
    }
    std::erase_if(texts_, [](const FloatText& t) { return t.t > 0.8f; });
    for (auto& b : bumps_) b.t += dt;
    std::erase_if(bumps_, [](const Bump& b) { return b.t > 0.18f; });
    for (auto& e : enemies_) {
      if (e.state == EnemyState::Squashed && (e.timer -= dt) <= 0) e.state = EnemyState::Gone;
      if (e.state == EnemyState::Knocked) {
        e.vel.y += kGravity * 0.5f * dt;
        e.box.x += e.vel.x * dt;
        e.box.y += e.vel.y * dt;
        if (e.box.y > kScreenH + 32) e.state = EnemyState::Gone;
      }
    }
  }

  void updatePlaying(float dt) {
    timeLeft_ -= dt;
    if (timeLeft_ <= 0) {
      timeLeft_ = 0;
      killPlayer();
      return;
    }

    const Input in = readInput();
    if (in.jumpPressed) player_.jumpBuffer = 0.12f;

    for (int i = 0, n = substepCount(dt); i < n; ++i) {
      const float step = dt / n;
      stepPlayer(in, step);
      stepEnemies(step);
      if (!checkEnemyContacts(in)) return;
    }

    // Camera only scrolls forward, like the original.
    const float target = player_.box.x + player_.box.width / 2 - kScreenW * 0.4f;
    camX_ = std::clamp(std::max(camX_, target), 0.0f, float(level_.width * kTile - kScreenW));

    for (auto& c : coins_) {
      if (c.taken) continue;
      if (CheckCollisionRecs(player_.box, {c.center.x - 8, c.center.y - 12, 16, 24})) {
        c.taken = true;
        addCoin(c.center);
      }
    }

    if (player_.box.y > kScreenH) {
      killPlayer(/*fell=*/true);
      return;
    }

    if (player_.box.x + player_.box.width >= deskX() - 8) startClear();
  }

  void stepPlayer(const Input& in, float dt) {
    Player& p = player_;
    const int dir = int(in.right) - int(in.left);
    const float prevSpeed = std::fabs(p.vel.x);
    if (dir != 0) {
      float accel = p.onGround ? 900.0f : 600.0f;
      if (p.vel.x * dir < 0) accel = p.onGround ? 2200.0f : 1200.0f;  // skid
      p.vel.x += dir * accel * dt;
      if (p.onGround) p.facingRight = dir > 0;
    } else if (p.onGround) {
      const float dec = 1100.0f * dt;
      p.vel.x = std::fabs(p.vel.x) <= dec ? 0.0f : p.vel.x - std::copysign(dec, p.vel.x);
    }
    const float limit = in.run ? kRunSpeed : kWalkSpeed;
    if (std::fabs(p.vel.x) > limit) p.vel.x = std::copysign(std::max(limit, prevSpeed - 600.0f * dt), p.vel.x);

    p.coyote = p.onGround ? 0.08f : p.coyote - dt;
    p.jumpBuffer -= dt;
    if (p.jumpBuffer > 0 && p.coyote > 0) {
      p.vel.y = -(kJumpSpeed + 0.12f * std::fabs(p.vel.x));
      p.jumpBuffer = 0;
      p.coyote = 0;
    }
    const float g = (p.vel.y < 0 && in.jumpHeld) ? kGravityHold : kGravity;
    p.vel.y = std::min(p.vel.y + g * dt, kMaxFall);

    const Hits h = moveAndCollide(level_, p.box, p.vel, dt);
    p.onGround = h.ground;
    if (p.onGround) p.stompCombo = 0;
    if (h.ceiling && h.headX >= 0) hitBlock(h.headX, h.headY);

    if (p.box.x < camX_) {
      p.box.x = camX_;
      p.vel.x = std::max(p.vel.x, 0.0f);
    }
    p.walkAnim += std::fabs(p.vel.x) * dt;
  }

  void hitBlock(int tx, int ty) {
    const Vector2 top{(tx + 0.5f) * kTile, float(ty * kTile)};
    const char c = level_.at(tx, ty);
    if (c == '?') {
      level_.set(tx, ty, 'X');
      bumps_.push_back({tx, ty});
      pops_.push_back({top, -560.0f});
      addCoin({top.x, top.y - 40});
    } else if (c == 'B') {
      level_.set(tx, ty, '.');
      for (auto [vx, vy] : {std::pair{-120.f, -520.f}, {120.f, -520.f}, {-90.f, -380.f}, {90.f, -380.f}})
        debris_.push_back({{top.x, top.y + 12}, {vx, vy}});
      addScore(50, top);
    } else {
      return;
    }
    // Knock out enemies standing on the block.
    const Rectangle above{float(tx * kTile), float(ty * kTile) - 4, float(kTile), 4};
    for (auto& e : enemies_)
      if (e.state == EnemyState::Walking && CheckCollisionRecs(e.box, above)) knockEnemy(e, top);
  }

  void stepEnemies(float dt) {
    for (auto& e : enemies_) {
      if (e.state != EnemyState::Walking) continue;
      if (!e.active) {
        if (e.box.x < camX_ + kScreenW + 32) e.active = true;
        else continue;
      }
      e.vel.x = e.dir * kGoombaSpeed;
      e.vel.y = std::min(e.vel.y + kGravity * dt, kMaxFall);
      if (moveAndCollide(level_, e.box, e.vel, dt).wall) e.dir = -e.dir;
      if (e.box.y > kScreenH || e.box.x + e.box.width < camX_ - 64) e.state = EnemyState::Gone;
    }
    // Goombas bounce off each other.
    for (std::size_t i = 0; i < enemies_.size(); ++i)
      for (std::size_t j = i + 1; j < enemies_.size(); ++j) {
        Enemy& a = enemies_[i];
        Enemy& b = enemies_[j];
        if (a.state != EnemyState::Walking || b.state != EnemyState::Walking) continue;
        if (!CheckCollisionRecs(a.box, b.box)) continue;
        a.dir = a.box.x < b.box.x ? -1 : 1;
        b.dir = -a.dir;
      }
  }

  void knockEnemy(Enemy& e, Vector2 at) {
    e.state = EnemyState::Knocked;
    e.vel = {e.box.x < at.x ? -80.0f : 80.0f, -500.0f};
    addScore(100, {e.box.x, e.box.y});
  }

  // Returns false if the player died.
  bool checkEnemyContacts(const Input& in) {
    Player& p = player_;
    for (auto& e : enemies_) {
      if (e.state != EnemyState::Walking || !e.active) continue;
      if (!CheckCollisionRecs(p.box, e.box)) continue;
      const float feet = p.box.y + p.box.height;
      if (p.vel.y > 0 && feet - e.box.y < 16) {
        e.state = EnemyState::Squashed;
        e.timer = 0.5f;
        p.box.y = e.box.y - p.box.height;
        p.vel.y = in.jumpHeld ? -640.0f : -380.0f;
        static constexpr std::array kComboPoints{100, 200, 400, 500, 800, 1000, 2000, 4000, 5000, 8000};
        addScore(kComboPoints[std::min<std::size_t>(p.stompCombo++, kComboPoints.size() - 1)],
                 {e.box.x, e.box.y});
      } else {
        killPlayer();
        return false;
      }
    }
    return true;
  }

  void killPlayer(bool fell = false) {
    if (state_ != State::Playing) return;
    state_ = State::Dying;
    music_.stop();
    stateTimer_ = 0;
    player_.vel = {0, fell ? 0.0f : -720.0f};
  }

  void updateDying(float dt) {
    stateTimer_ += dt;
    if (stateTimer_ > 0.5f) {
      player_.vel.y += kGravity * 0.5f * dt;
      player_.box.y += player_.vel.y * dt;
    }
    if (stateTimer_ > 3.0f) {
      if (--lives_ <= 0) state_ = State::GameOver;
      else loadLevel();
    }
  }

  void startClear() {
    state_ = State::Clear;
    music_.play(victorySong());
    clearPhase_ = 0;
    stateTimer_ = 0;
    player_.facingRight = true;
  }

  static constexpr float kBootTime = 2.4f;

  void updateClear(float dt) {
    stateTimer_ += dt;
    Player& p = player_;
    switch (clearPhase_) {
      case 0: {  // walk up to the keyboard
        const bool arrived = p.box.x + p.box.width / 2 >= keyboardX();
        for (int i = 0, n = substepCount(dt); i < n; ++i) {
          const float step = dt / n;
          p.vel.x = arrived ? 0.0f : 140.0f;
          p.vel.y = std::min(p.vel.y + kGravity * step, kMaxFall);
          p.onGround = moveAndCollide(level_, p.box, p.vel, step).ground;
          p.walkAnim += std::fabs(p.vel.x) * step;
        }
        if (arrived && p.onGround) {
          clearPhase_ = 1;
          stateTimer_ = 0;
        }
        break;
      }
      case 1:  // the computer boots and runs the build
        if (stateTimer_ >= kBootTime) {
          addScore(1000, {keyboardX() + 40, groundTop() - 170});
          timeLeft_ = std::ceil(timeLeft_);
          tallyAcc_ = 0;
          clearPhase_ = 2;
        }
        break;
      case 2: {  // convert remaining time to points
        tallyAcc_ += dt * 120.0f;
        while (tallyAcc_ >= 1.0f && timeLeft_ > 0) {
          tallyAcc_ -= 1.0f;
          timeLeft_ -= 1.0f;
          score_ += 50;
        }
        hiScore_ = std::max(hiScore_, score_);
        if (timeLeft_ <= 0) clearPhase_ = 3;
        break;
      }
      default:
        if (IsKeyPressed(KEY_ENTER)) newGame();
        break;
    }
  }

  // --- draw --------------------------------------------------------------
  void draw() const {
    ClearBackground(kSky);
    drawBackdrop();

    const Camera2D cam{.offset = {0, 0}, .target = {std::floor(camX_), 0}, .rotation = 0, .zoom = 1};
    BeginMode2D(cam);
    drawTiles();
    drawComputer();
    for (const auto& c : coins_)
      if (!c.taken) drawCoinShape(c.center, clock_);
    for (const auto& p : pops_) drawCoinShape(p.pos, clock_ * 4);
    drawEnemies();
    drawPlayer();
    for (const auto& d : debris_) DrawRectangle(int(d.pos.x) - 6, int(d.pos.y) - 6, 12, 12, {184, 72, 16, 255});
    for (const auto& t : texts_) textShadow(TextFormat("%d", t.value), int(t.pos.x), int(t.pos.y), 16, WHITE);
    EndMode2D();

    drawHud();
    drawOverlay();
  }

  void drawBackdrop() const {
    // Parallax hills.
    constexpr float hillPeriod = 768.0f;
    const float hillScroll = camX_ * 0.5f;
    const float gy = groundTop();
    for (int i = -1; i < 3; ++i) {
      const float sx = std::floor(hillScroll / hillPeriod) * hillPeriod + i * hillPeriod - hillScroll;
      DrawCircleV({sx + 120, gy + 40}, 130, {56, 150, 40, 255});
      DrawCircleV({sx + 120, gy + 40}, 126, {80, 180, 60, 255});
      DrawEllipse(int(sx + 90), int(gy - 50), 5, 10, {40, 110, 30, 255});
      DrawEllipse(int(sx + 150), int(gy - 60), 5, 10, {40, 110, 30, 255});
      DrawCircleV({sx + 480, gy + 20}, 70, {80, 180, 60, 255});
      DrawEllipse(int(sx + 470), int(gy - 20), 4, 8, {40, 110, 30, 255});
    }
    // Parallax clouds.
    constexpr float cloudPeriod = 620.0f;
    const float cloudScroll = camX_ * 0.25f;
    for (int i = -1; i < 3; ++i) {
      const float sx = std::floor(cloudScroll / cloudPeriod) * cloudPeriod + i * cloudPeriod - cloudScroll;
      for (auto [cx, cy] : {std::pair{140.f, 80.f}, {430.f, 140.f}}) {
        DrawCircleV({sx + cx - 26, cy + 6}, 20, WHITE);
        DrawCircleV({sx + cx, cy - 6}, 26, WHITE);
        DrawCircleV({sx + cx + 28, cy + 6}, 20, WHITE);
        DrawRectangle(int(sx + cx - 26), int(cy + 6), 54, 20, WHITE);
      }
    }
  }

  float bumpOffset(int tx, int ty) const {
    for (const auto& b : bumps_)
      if (b.tx == tx && b.ty == ty) return -std::sin(b.t / 0.18f * PI) * 8.0f;
    return 0.0f;
  }

  void drawTiles() const {
    const int x0 = std::max(0, int(camX_ / kTile) - 2);
    const int x1 = std::min(level_.width - 1, int((camX_ + kScreenW) / kTile) + 1);
    for (int ty = 0; ty < kRows; ++ty)
      for (int tx = x0; tx <= x1; ++tx) {
        const int x = tx * kTile;
        const int y = ty * kTile + int(bumpOffset(tx, ty));
        switch (level_.at(tx, ty)) {
          case '#': drawGround(x, y); break;
          case 'B': drawBrick(x, y); break;
          case '?': drawQuestion(x, y, clock_); break;
          case 'X': drawUsed(x, y); break;
          case 'S': drawHard(x, y); break;
          case '<': drawPipeSection(x, y, true); break;
          case '(': drawPipeSection(x, y, false); break;
          default: break;
        }
      }
  }

  void drawComputer() const {
    const int x = int(deskX());
    const int gy = kGroundRow * kTile;
    constexpr Color wood{150, 100, 55, 255}, woodDark{100, 62, 30, 255};
    constexpr Color casing{210, 208, 196, 255}, casingDark{120, 118, 110, 255};

    // Desk.
    DrawRectangle(x - 8, gy - 40, 5 * kTile, 10, wood);
    DrawRectangle(x - 8, gy - 32, 5 * kTile, 2, woodDark);
    DrawRectangle(x - 2, gy - 30, 8, 30, woodDark);
    DrawRectangle(x + 5 * kTile - 22, gy - 30, 8, 30, woodDark);

    // Tower under the desk.
    DrawRectangle(x + 100, gy - 28, 22, 28, {70, 72, 78, 255});
    DrawRectangleLines(x + 100, gy - 28, 22, 28, {30, 30, 34, 255});
    DrawCircle(x + 111, gy - 20, 2, clearPhase_ >= 1 ? GREEN : Color{80, 20, 20, 255});

    // Monitor.
    DrawRectangle(x + 58, gy - 58, 24, 18, casingDark);
    DrawRectangle(x + 42, gy - 42, 56, 3, casingDark);
    DrawRectangle(x + 6, gy - 156, 128, 100, casing);
    DrawRectangleLines(x + 6, gy - 156, 128, 100, casingDark);
    const Rectangle screen{float(x + 14), float(gy - 148), 112, 80};
    const bool on = clearPhase_ >= 1;
    DrawRectangleRec(screen, on ? Color{8, 36, 18, 255} : Color{24, 30, 28, 255});
    DrawCircle(x + 124, gy - 62, 3, on ? GREEN : Color{140, 30, 30, 255});

    if (on) {
      // Type out a little build log.
      static constexpr std::array<std::string_view, 4> kLog{"$ cmake --build", "[100%] valigo", "tests: PASS", "LEVEL COMPLETE"};
      const float bootT = clearPhase_ == 1 ? stateTimer_ : kBootTime;
      int budget = int(bootT * 26.0f);
      for (std::size_t i = 0; i < kLog.size() && budget > 0; ++i) {
        const std::string line(kLog[i].substr(0, std::min<std::size_t>(budget, kLog[i].size())));
        budget -= int(kLog[i].size()) + 4;  // small pause between lines
        const Color c = i + 1 == kLog.size() ? Color{252, 216, 0, 255} : Color{80, 255, 120, 255};
        DrawText(line.c_str(), int(screen.x) + 6, int(screen.y) + 6 + int(i) * 17, 10, c);
      }
      if (int(clock_ * 3) % 2 == 0) DrawRectangle(int(screen.x) + 6, int(screen.y) + 70, 6, 2, {80, 255, 120, 255});
    }

    // Keyboard in front of the monitor.
    DrawRectangle(x + 20, gy - 46, 88, 6, casing);
    DrawRectangleLines(x + 20, gy - 46, 88, 6, casingDark);
    for (int k = 0; k < 10; ++k) DrawRectangle(x + 24 + k * 8, gy - 44, 5, 2, casingDark);
  }

  void drawEnemies() const {
    for (const auto& e : enemies_) {
      const float x = std::floor(e.box.x) - 2;
      switch (e.state) {
        case EnemyState::Walking: {
          const bool flip = int(clock_ * 5.0f) % 2 == 0;
          drawPixels(kGoomba, x, std::floor(e.box.y) - 4, 2, flip);
          break;
        }
        case EnemyState::Squashed:
          drawPixels(std::span(kGoomba).subspan(8), x, e.box.y + e.box.height - 8, 2, false, 0.5f);
          break;
        case EnemyState::Knocked:
          // Upside down: draw rows in reverse by mirroring vertically.
          for (std::size_t r = 0; r < kGoomba.size(); ++r)
            drawPixels(std::span(kGoomba).subspan(kGoomba.size() - 1 - r, 1), x, e.box.y - 4 + r * 2, 2, false);
          break;
        case EnemyState::Gone: break;
      }
    }
  }

  void drawPlayer() const {
    const Player& p = player_;
    if (!p.visible) return;
    std::span<const std::string_view> legs = kLegsStand;
    const bool airborne = state_ == State::Dying || !p.onGround;
    if (airborne) {
      legs = kLegsJump;
    } else if (std::fabs(p.vel.x) > 10.0f) {
      static constexpr std::array<std::span<const std::string_view>, 4> kCycle{kLegsStride, kLegsPass, kLegsStand,
                                                                              kLegsPass};
      // One pose per 40 px travelled: about 5 poses/s when walking, 8 when running.
      legs = kCycle[int(p.walkAnim / 40.0f) % kCycle.size()];
    }
    // Sprite is 28x48 on screen; the hitbox is 20x44 centred at the feet.
    const float x = std::floor(p.box.x) - 4;
    const float y = std::floor(p.box.y) - 4;
    drawPixels(kHeroTop, x, y, 2, !p.facingRight);
    drawPixels(legs, x, y + kHeroTop.size() * 2, 2, !p.facingRight);
  }

  void drawHud() const {
    const int t = int(std::ceil(timeLeft_));
    textShadow("VALIGO", 40, 10, 20, WHITE);
    textShadow(TextFormat("%06d", score_), 40, 32, 20, WHITE);
    drawCoinShape({262, 42}, 0.4f);
    textShadow(TextFormat("x%02d", coinCount_), 274, 32, 20, WHITE);
    textShadow("WORLD", 420, 10, 20, WHITE);
    textShadow("1-1", 432, 32, 20, WHITE);
    textShadow("TIME", 600, 10, 20, WHITE);
    textShadow(TextFormat("%03d", t), 606, 32, 20, WHITE);
    textShadow(TextFormat("LIVES %d", lives_), 690, 32, 16, WHITE);
  }

  void drawOverlay() const {
    auto panel = [](int h) { DrawRectangle(150, (kScreenH - h) / 2, kScreenW - 300, h, {0, 0, 0, 170}); };
    switch (state_) {
      case State::Title:
        panel(220);
        textCentered("VALIGO", 140, 44, {252, 216, 0, 255});
        textCentered("Arrows/WASD move  -  Space/Up jump  -  Shift run  -  M music", 210, 16, WHITE);
        textCentered(TextFormat("TOP %06d", hiScore_), 240, 20, WHITE);
        if (int(clock_ * 2) % 2 == 0) textCentered("PRESS ENTER", 280, 24, WHITE);
        break;
      case State::GameOver:
        panel(140);
        textCentered("GAME OVER", 180, 44, WHITE);
        textCentered("Press ENTER", 240, 20, WHITE);
        break;
      case State::Clear:
        if (clearPhase_ >= 2) {
          // Kept high on screen so the computer stays visible.
          DrawRectangle(150, 70, kScreenW - 300, 100, {0, 0, 0, 170});
          textCentered("COURSE CLEAR!", 84, 40, {252, 216, 0, 255});
          if (clearPhase_ == 3) textCentered("Press ENTER to play again", 136, 20, WHITE);
        }
        break;
      default: break;
    }
  }
};

std::unique_ptr<Game> g_game;

void runFrame() { g_game->frame(); }

}  // namespace

int main() {
  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(kScreenW, kScreenH, "Valigo");
  SetExitKey(KEY_NULL);  // Esc should not quit (especially in the browser)
  g_game = std::make_unique<Game>();

#if defined(__EMSCRIPTEN__)
  emscripten_set_main_loop(runFrame, 0, 1);
#else
  SetTargetFPS(60);
  while (!WindowShouldClose()) runFrame();
  g_game.reset();
  CloseWindow();
#endif
  return 0;
}
