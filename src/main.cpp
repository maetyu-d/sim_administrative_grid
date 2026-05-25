#include <SDL.h>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace civic {

constexpr int ScreenW = 1280;
constexpr int ScreenH = 800;
constexpr int MapW = 96;
constexpr int MapH = 96;
constexpr float TileSize = 24.0f;
constexpr float Pi = 3.1415926535f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Mat4 {
    float m[16]{};
};

static Mat4 ortho(float left, float right, float bottom, float top) {
    Mat4 out{};
    out.m[0] = 2.0f / (right - left);
    out.m[5] = 2.0f / (top - bottom);
    out.m[10] = -1.0f;
    out.m[12] = -(right + left) / (right - left);
    out.m[13] = -(top + bottom) / (top - bottom);
    out.m[15] = 1.0f;
    return out;
}

static Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return Color{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

enum class Tile : int {
    Empty = 0,
    Road = 1,
    Conveyor = 2,
    Rail = 3,
    Block = 4,
    Holding = 5,
    Mine = 6,
    Factory = 7,
    Refinery = 8,
    Port = 9
};

enum class Tool : int {
    Road = 1,
    Conveyor = 2,
    Rail = 3,
    Block = 4,
    Holding = 5,
    Mine = 6,
    Factory = 7,
    Refinery = 8,
    Port = 9,
    Erase = 0
};

enum class Direction : int {
    East = 0,
    South = 1,
    West = 2,
    North = 3
};

enum class BuildingStatus : int {
    Idle = 0,
    Running = 1,
    Starved = 2,
    OutputBlocked = 3,
    NoRail = 4,
    Full = 5,
    Suspended = 6
};

enum class OverlayMode : int {
    Normal = 0,
    Resources = 1,
    Flow = 2,
    Stress = 3
};

enum class GameMode : int {
    Title = 0,
    Playing = 1,
    Paused = 2,
    End = 3
};

enum class EndState : int {
    None = 0,
    Verified = 1,
    Collapse = 2,
    Insolvency = 3,
    PurityBreach = 4
};

struct Cell {
    Tile tile = Tile::Empty;
    Tile zone = Tile::Empty;
    int level = 0;
    float growth = 0.0f;
    float pollution = 0.0f;
    float desirability = 0.5f;
    uint8_t variant = 0;
    Direction dir = Direction::East;
    BuildingStatus status = BuildingStatus::Idle;
    float pink = 0.0f;
    float fuel = 0.0f;
    float gold = 0.0f;
    float refined = 0.0f;
    float recentFlow = 0.0f;
    float blockedFlow = 0.0f;
    float packetFlow = 0.0f;
    int packetKind = 0;
    int shutdownTimer = 0;
};

struct Vertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
    float kind;
    float u;
    float v;
};

struct Glyph {
    std::array<const char*, 7> rows;
};

struct Alert {
    std::string text;
    float ttl = 0.0f;
    Color color;
};

static const std::map<char, Glyph>& glyphs() {
    static const std::map<char, Glyph> g = {
        {'A', {{{"01110","10001","10001","11111","10001","10001","10001"}}}},
        {'B', {{{"11110","10001","10001","11110","10001","10001","11110"}}}},
        {'C', {{{"01111","10000","10000","10000","10000","10000","01111"}}}},
        {'D', {{{"11110","10001","10001","10001","10001","10001","11110"}}}},
        {'E', {{{"11111","10000","10000","11110","10000","10000","11111"}}}},
        {'F', {{{"11111","10000","10000","11110","10000","10000","10000"}}}},
        {'G', {{{"01111","10000","10000","10111","10001","10001","01111"}}}},
        {'H', {{{"10001","10001","10001","11111","10001","10001","10001"}}}},
        {'I', {{{"11111","00100","00100","00100","00100","00100","11111"}}}},
        {'J', {{{"00111","00010","00010","00010","10010","10010","01100"}}}},
        {'K', {{{"10001","10010","10100","11000","10100","10010","10001"}}}},
        {'L', {{{"10000","10000","10000","10000","10000","10000","11111"}}}},
        {'M', {{{"10001","11011","10101","10101","10001","10001","10001"}}}},
        {'N', {{{"10001","11001","10101","10011","10001","10001","10001"}}}},
        {'O', {{{"01110","10001","10001","10001","10001","10001","01110"}}}},
        {'P', {{{"11110","10001","10001","11110","10000","10000","10000"}}}},
        {'Q', {{{"01110","10001","10001","10001","10101","10010","01101"}}}},
        {'R', {{{"11110","10001","10001","11110","10100","10010","10001"}}}},
        {'S', {{{"01111","10000","10000","01110","00001","00001","11110"}}}},
        {'T', {{{"11111","00100","00100","00100","00100","00100","00100"}}}},
        {'U', {{{"10001","10001","10001","10001","10001","10001","01110"}}}},
        {'V', {{{"10001","10001","10001","10001","10001","01010","00100"}}}},
        {'W', {{{"10001","10001","10001","10101","10101","10101","01010"}}}},
        {'X', {{{"10001","10001","01010","00100","01010","10001","10001"}}}},
        {'Y', {{{"10001","10001","01010","00100","00100","00100","00100"}}}},
        {'Z', {{{"11111","00001","00010","00100","01000","10000","11111"}}}},
        {'0', {{{"01110","10001","10011","10101","11001","10001","01110"}}}},
        {'1', {{{"00100","01100","00100","00100","00100","00100","01110"}}}},
        {'2', {{{"01110","10001","00001","00010","00100","01000","11111"}}}},
        {'3', {{{"11110","00001","00001","01110","00001","00001","11110"}}}},
        {'4', {{{"00010","00110","01010","10010","11111","00010","00010"}}}},
        {'5', {{{"11111","10000","10000","11110","00001","00001","11110"}}}},
        {'6', {{{"01110","10000","10000","11110","10001","10001","01110"}}}},
        {'7', {{{"11111","00001","00010","00100","01000","01000","01000"}}}},
        {'8', {{{"01110","10001","10001","01110","10001","10001","01110"}}}},
        {'9', {{{"01110","10001","10001","01111","00001","00001","01110"}}}},
        {':', {{{"00000","01100","01100","00000","01100","01100","00000"}}}},
        {'-', {{{"00000","00000","00000","11111","00000","00000","00000"}}}},
        {'/', {{{"00001","00010","00010","00100","01000","01000","10000"}}}},
        {'%', {{{"11001","11010","00100","01000","10110","00110","00000"}}}},
        {'$', {{{"00100","01111","10100","01110","00101","11110","00100"}}}},
        {'+', {{{"00000","00100","00100","11111","00100","00100","00000"}}}},
        {' ', {{{"00000","00000","00000","00000","00000","00000","00000"}}}}
    };
    return g;
}

class Game {
public:
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            SDL_Log("SDL init failed: %s", SDL_GetError());
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        window_ = SDL_CreateWindow("Administrative Grid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   ScreenW, ScreenH, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window_) return false;

        gl_ = SDL_GL_CreateContext(window_);
        if (!gl_) return false;
        SDL_GL_SetSwapInterval(1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (!createShaders() || !createTextureAtlas() || !createBuffers() || !createFramebuffer(ScreenW, ScreenH)) return false;
        rng_.seed(static_cast<unsigned int>(std::time(nullptr)));
        generateMap();
        mode_ = GameMode::Title;
        lastTicks_ = SDL_GetTicks();
        return true;
    }

    void run() {
        while (running_) {
            uint32_t now = SDL_GetTicks();
            float dt = std::min(0.05f, (now - lastTicks_) / 1000.0f);
            lastTicks_ = now;
            handleEvents();
            update(dt);
            render();
        }
    }

    void shutdown() {
        if (tileVbo_) glDeleteBuffers(1, &tileVbo_);
        if (tileVao_) glDeleteVertexArrays(1, &tileVao_);
        if (screenVbo_) glDeleteBuffers(1, &screenVbo_);
        if (screenVao_) glDeleteVertexArrays(1, &screenVao_);
        if (tileProgram_) glDeleteProgram(tileProgram_);
        if (postProgram_) glDeleteProgram(postProgram_);
        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        if (fboTex_) glDeleteTextures(1, &fboTex_);
        if (atlasTex_) glDeleteTextures(1, &atlasTex_);
        if (gl_) SDL_GL_DeleteContext(gl_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    static GLuint compile(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048]{};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            SDL_Log("Shader compile error: %s", log);
        }
        return shader;
    }

    static GLuint link(GLuint vs, GLuint fs) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048]{};
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            SDL_Log("Program link error: %s", log);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    bool createShaders() {
        const char* tileVs = R"GLSL(
            #version 150
            in vec2 aPos;
            in vec4 aColor;
            in float aKind;
            in vec2 aUv;
            uniform mat4 uProj;
            out vec4 vColor;
            out float vKind;
            out vec2 vWorld;
            out vec2 vUv;
            void main() {
                vColor = aColor;
                vKind = aKind;
                vWorld = aPos;
                vUv = aUv;
                gl_Position = uProj * vec4(aPos, 0.0, 1.0);
            }
        )GLSL";

        const char* tileFs = R"GLSL(
            #version 150
            in vec4 vColor;
            in float vKind;
            in vec2 vWorld;
            in vec2 vUv;
            uniform sampler2D uAtlas;
            uniform float uTime;
            uniform float uNight;
            uniform float uFog;
            uniform float uUgly;
            out vec4 frag;
            void main() {
                vec4 c = vColor;
                vec3 civic = vec3(c.r * 0.90 + 0.08, c.g * 1.02 + 0.09, c.b * 0.92 + 0.075);
                c.rgb = mix(c.rgb, civic, (1.0 - uUgly) * 0.22);
                vec4 texel = texture(uAtlas, vUv);
                c.rgb = mix(c.rgb, texel.rgb * (0.55 + c.rgb * 0.75), texel.a * 0.72);
                if (vKind > 5.5 && vKind < 6.5) {
                    float wave = sin(vWorld.x * 0.05 + uTime * 0.7) * 0.012 + sin(vWorld.y * 0.07 - uTime * 0.5) * 0.008;
                    c.rgb += vec3(wave * 0.4, wave * 0.7, wave);
                }
                if (vKind > 6.5 && vKind < 7.5) {
                    float lit = 0.14 + uNight * 0.30;
                    c.rgb += vec3(lit * 0.035, lit * 0.042, lit * 0.05);
                }
                float bruise = sin(vWorld.x * 0.023 + vWorld.y * 0.017 + uTime * 0.18) * 0.5 + 0.5;
                vec3 sick = vec3(c.r * 0.68 + 0.095, c.g * 0.58 + 0.055, c.b * 0.52 + 0.045);
                vec3 rust = vec3(0.34, 0.08, 0.06) * bruise;
                c.rgb = mix(c.rgb, sick + rust * 0.18, uUgly * 0.58);
                c.rgb *= mix(0.94, 0.55, uNight);
                c.rgb = mix(c.rgb, vec3(0.62, 0.64, 0.61), uFog * 0.09);
                frag = c;
            }
        )GLSL";

        const char* postVs = R"GLSL(
            #version 150
            in vec2 aPos;
            in vec2 aUv;
            out vec2 vUv;
            void main() {
                vUv = aUv;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )GLSL";

        const char* postFs = R"GLSL(
            #version 150
            in vec2 vUv;
            uniform sampler2D uScene;
            uniform float uTime;
            uniform float uNight;
            uniform float uPollution;
            uniform float uUgly;
            out vec4 frag;
            void main() {
                vec3 col = texture(uScene, vUv).rgb;
                float vignette = smoothstep(0.86, 0.18, distance(vUv, vec2(0.5)));
                float paper = fract(sin(dot(floor(vUv * 900.0), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
                float scan = sin((vUv.y + uTime * 0.01) * 700.0) * 0.004;
                vec3 civicWash = vec3(0.026, 0.046, 0.034) * (1.0 - uUgly);
                vec3 night = vec3(0.025, 0.035, 0.050) * uNight;
                vec3 smog = vec3(0.12, 0.10, 0.07) * uPollution * 0.11;
                float warpNoise = sin(vUv.y * 31.0 + uTime * 0.7) * sin(vUv.x * 19.0 - uTime * 0.23);
                vec3 stain = vec3(0.20, 0.08, 0.055) * max(0.0, warpNoise) * uUgly * 0.18;
                col = col * (0.96 + vignette * 0.06) + civicWash + night + smog + stain + scan + paper * (0.010 + uUgly * 0.032);
                float grey = dot(col, vec3(0.32, 0.42, 0.26));
                col = mix(vec3(grey), col, 0.92 - uUgly * 0.40);
                col.r += uUgly * 0.018;
                col.b *= 1.0 - uUgly * 0.09;
                frag = vec4(col, 1.0);
            }
        )GLSL";

        tileProgram_ = link(compile(GL_VERTEX_SHADER, tileVs), compile(GL_FRAGMENT_SHADER, tileFs));
        postProgram_ = link(compile(GL_VERTEX_SHADER, postVs), compile(GL_FRAGMENT_SHADER, postFs));
        return tileProgram_ && postProgram_;
    }

    static uint32_t hash2(int x, int y, int seed) {
        uint32_t h = static_cast<uint32_t>(x * 374761393u + y * 668265263u + seed * 2246822519u);
        h = (h ^ (h >> 13u)) * 1274126177u;
        return h ^ (h >> 16u);
    }

    bool createTextureAtlas() {
        constexpr int tile = 64;
        constexpr int cols = 4;
        constexpr int rows = 4;
        constexpr int w = tile * cols;
        constexpr int h = tile * rows;
        std::vector<uint8_t> pixels(w * h * 4, 255);

        auto put = [&](int material, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            int ox = (material % cols) * tile;
            int oy = (material / cols) * tile;
            int i = ((oy + y) * w + (ox + x)) * 4;
            pixels[i + 0] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = a;
        };

        auto noiseAt = [](int x, int y, int seed) {
            return static_cast<float>(hash2(x, y, seed) & 255u) / 255.0f;
        };

        auto putColor = [&](int material, int x, int y, Color c, float alpha = 1.0f) {
            put(material, x, y,
                static_cast<uint8_t>(clamp01(c.r) * 255.0f),
                static_cast<uint8_t>(clamp01(c.g) * 255.0f),
                static_cast<uint8_t>(clamp01(c.b) * 255.0f),
                static_cast<uint8_t>(clamp01(alpha) * 255.0f));
        };

        auto drawLine = [&](int material, int x0, int y0, int x1, int y1, Color color, int thickness) {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while (true) {
                for (int oy = -thickness; oy <= thickness; ++oy) {
                    for (int ox = -thickness; ox <= thickness; ++ox) {
                        int px = x0 + ox;
                        int py = y0 + oy;
                        if (px >= 0 && py >= 0 && px < tile && py < tile) putColor(material, px, py, color, 0.92f);
                    }
                }
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        };

        auto drawRect = [&](int material, int x0, int y0, int rw, int rh, Color color, float alpha = 1.0f) {
            for (int y = std::max(0, y0); y < std::min(tile, y0 + rh); ++y) {
                for (int x = std::max(0, x0); x < std::min(tile, x0 + rw); ++x) putColor(material, x, y, color, alpha);
            }
        };

        auto fillMaterial = [&](int material, Color base, int seed, int mode) {
            for (int y = 0; y < tile; ++y) {
                for (int x = 0; x < tile; ++x) {
                    float n0 = noiseAt(x, y, seed);
                    float n1 = noiseAt(x / 3, y / 3, seed + 19);
                    float noise = (n0 - 0.5f) * 0.18f + (n1 - 0.5f) * 0.16f;
                    float grain = std::sin((x + seed) * 0.41f) * 0.028f + std::sin((y - seed) * 0.29f) * 0.020f;
                    float scale = 1.0f + noise + grain;
                    if (mode == 1 && (x % 16 == 0 || y % 16 == 0)) scale *= 0.68f;
                    if (mode == 2 && ((x + y + seed) % 13 < 2)) scale *= 0.72f;
                    if (mode == 3 && (std::abs((x % 18) - 9) < 2 || std::abs((y % 18) - 9) < 2)) scale *= 1.16f;
                    if (mode == 4 && ((x / 7 + y / 5) % 2 == 0)) scale *= 0.80f;
                    put(material, x, y,
                        static_cast<uint8_t>(clamp01(base.r * scale) * 255.0f),
                        static_cast<uint8_t>(clamp01(base.g * scale) * 255.0f),
                        static_cast<uint8_t>(clamp01(base.b * scale) * 255.0f),
                        material == 0 ? 0 : 230);
                }
            }
        };

        fillMaterial(0, rgba(230, 230, 230), 1, 0);    // transparent fallback/text
        fillMaterial(1, rgba(118, 128, 116), 2, 1);    // civic concrete grid
        fillMaterial(2, rgba(55, 62, 58), 3, 2);       // softened asphalt
        fillMaterial(3, rgba(72, 78, 72), 4, 4);       // conveyor rubber
        fillMaterial(4, rgba(68, 72, 66), 5, 3);       // rail ballast
        fillMaterial(5, rgba(138, 83, 116), 6, 0);     // pink block material
        fillMaterial(6, rgba(54, 62, 58), 7, 1);       // holding/metal
        fillMaterial(7, rgba(15, 15, 14), 8, 2);       // mine carbon
        fillMaterial(8, rgba(128, 122, 76), 9, 1);     // factory plate
        fillMaterial(9, rgba(166, 148, 72), 10, 3);    // refinery brass
        fillMaterial(10, rgba(92, 104, 100), 11, 1);   // port concrete
        fillMaterial(11, rgba(17, 22, 20), 12, 2);     // dark civic UI panel
        fillMaterial(12, rgba(42, 50, 46), 13, 1);     // button metal
        fillMaterial(13, rgba(150, 116, 44), 14, 3);   // gold
        fillMaterial(14, rgba(218, 204, 143), 15, 0);  // refined
        fillMaterial(15, rgba(58, 47, 32), 16, 2);     // stain

        // Concrete slabs: expansion joints, hairline cracks, aggregate flecks.
        for (int p = 15; p < tile; p += 16) {
            drawLine(1, p, 0, p + ((p / 16) % 2), tile - 1, rgba(42, 44, 42), 0);
            drawLine(1, 0, p, tile - 1, p - ((p / 16) % 2), rgba(42, 44, 42), 0);
        }
        drawLine(1, 7, 42, 23, 34, rgba(34, 36, 34), 0);
        drawLine(1, 23, 34, 41, 39, rgba(34, 36, 34), 0);
        drawLine(1, 44, 12, 55, 24, rgba(118, 121, 113), 0);
        for (int i = 0; i < 50; ++i) {
            int x = static_cast<int>(hash2(i, 1, 21) % tile);
            int y = static_cast<int>(hash2(i, 2, 21) % tile);
            putColor(1, x, y, rgba(130, 132, 124), 0.85f);
        }

        // Asphalt: tar repairs, patched rectangles, worn lane dust.
        drawRect(2, 8, 10, 22, 9, rgba(30, 31, 30), 0.95f);
        drawRect(2, 38, 40, 18, 12, rgba(28, 29, 29), 0.95f);
        drawLine(2, 4, 33, 59, 30, rgba(70, 69, 62), 1);
        drawLine(2, 12, 4, 5, 57, rgba(23, 24, 24), 0);
        for (int y = 6; y < tile; y += 12) drawRect(2, 2, y, 9, 1, rgba(96, 92, 76), 0.75f);

        // Conveyor rubber: ribs, arrows, scuffed belt edges.
        for (int x = -12; x < tile; x += 10) drawLine(3, x, 0, x + 24, tile - 1, rgba(34, 31, 36), 1);
        drawRect(3, 0, 2, tile, 3, rgba(83, 74, 85), 0.9f);
        drawRect(3, 0, tile - 5, tile, 3, rgba(32, 29, 35), 0.9f);
        drawLine(3, 22, 28, 32, 20, rgba(156, 75, 118), 1);
        drawLine(3, 32, 20, 42, 28, rgba(156, 75, 118), 1);

        // Rail ballast: sleepers, twin rails, crushed stone.
        for (int y = 4; y < tile; y += 8) drawRect(4, 0, y, tile, 3, rgba(71, 65, 55), 0.9f);
        drawRect(4, 17, 0, 3, tile, rgba(126, 119, 99), 0.95f);
        drawRect(4, 43, 0, 3, tile, rgba(126, 119, 99), 0.95f);
        for (int i = 0; i < 90; ++i) {
            int x = static_cast<int>(hash2(i, 9, 22) % tile);
            int y = static_cast<int>(hash2(i, 3, 22) % tile);
            putColor(4, x, y, rgba(94, 90, 78), 0.9f);
        }

        // Pink material: packed cells with wrinkles and pale inventory bands.
        for (int y = 5; y < tile; y += 10) drawRect(5, 3, y, tile - 6, 4, rgba(170, 89, 132), 0.85f);
        drawLine(5, 9, 12, 50, 17, rgba(232, 145, 187), 0);
        drawLine(5, 12, 46, 53, 39, rgba(93, 47, 76), 0);
        drawRect(5, 6, 6, 12, 12, rgba(213, 108, 164), 0.9f);
        drawRect(5, 38, 34, 14, 13, rgba(112, 61, 91), 0.9f);

        // Holding metal: rivets, grates, numbered rack feel.
        for (int p = 7; p < tile; p += 14) {
            drawRect(6, p, 0, 2, tile, rgba(24, 25, 26), 0.95f);
            drawRect(6, 0, p, tile, 2, rgba(61, 63, 63), 0.8f);
        }
        for (int y = 8; y < tile; y += 16) for (int x = 8; x < tile; x += 16) drawRect(6, x, y, 3, 3, rgba(94, 95, 91), 0.9f);

        // Mine carbon: fractured chunks and glossy coal seams.
        drawLine(7, 3, 14, 34, 5, rgba(63, 61, 56), 1);
        drawLine(7, 13, 58, 58, 22, rgba(4, 4, 4), 2);
        drawLine(7, 42, 0, 27, 63, rgba(48, 47, 44), 1);
        for (int i = 0; i < 70; ++i) {
            int x = static_cast<int>(hash2(i, 5, 31) % tile);
            int y = static_cast<int>(hash2(i, 6, 31) % tile);
            putColor(7, x, y, noiseAt(x, y, 77) > 0.75f ? rgba(96, 94, 84) : rgba(2, 2, 2), 0.85f);
        }

        // Factory plate: riveted sheet metal, oil streaks.
        for (int y = 12; y < tile; y += 16) drawRect(8, 0, y, tile, 2, rgba(68, 57, 35), 0.95f);
        for (int x = 10; x < tile; x += 18) drawRect(8, x, 0, 2, tile, rgba(151, 121, 58), 0.6f);
        for (int y = 8; y < tile; y += 16) for (int x = 8; x < tile; x += 16) drawRect(8, x, y, 3, 3, rgba(185, 152, 76), 0.85f);
        drawLine(8, 4, 47, 55, 56, rgba(44, 38, 28), 1);

        // Refinery brass: pipes, oxidized scratches, hot seams.
        for (int x = 8; x < tile; x += 14) drawRect(9, x, 0, 4, tile, rgba(117, 86, 31), 0.75f);
        drawLine(9, 0, 18, 63, 25, rgba(223, 176, 61), 1);
        drawLine(9, 7, 48, 63, 43, rgba(82, 63, 28), 1);
        for (int i = 0; i < 34; ++i) {
            int x = static_cast<int>(hash2(i, 11, 37) % tile);
            int y = static_cast<int>(hash2(i, 12, 37) % tile);
            drawRect(9, x, y, 2, 1, rgba(70, 93, 72), 0.8f);
        }

        // Port concrete: large slabs, tyre marks, cargo scuffs.
        for (int p = 20; p < tile; p += 21) {
            drawLine(10, p, 0, p, tile - 1, rgba(43, 49, 51), 0);
            drawLine(10, 0, p, tile - 1, p, rgba(43, 49, 51), 0);
        }
        drawRect(10, 7, 9, 20, 10, rgba(93, 101, 102), 0.75f);
        drawRect(10, 35, 37, 18, 12, rgba(55, 62, 64), 0.75f);
        drawLine(10, 2, 55, 62, 47, rgba(28, 31, 32), 1);

        // UI panels and buttons: brushed dark material.
        for (int y = 4; y < tile; y += 6) drawLine(11, 0, y, tile - 1, y + (y % 3), rgba(29, 32, 32), 0);
        drawRect(11, 0, 0, tile, 1, rgba(75, 82, 78), 0.7f);
        for (int y = 6; y < tile; y += 10) drawLine(12, 3, y, tile - 4, y - 2, rgba(56, 64, 59), 0);
        drawRect(12, 4, 4, tile - 8, tile - 8, rgba(43, 48, 46), 0.65f);

        // Gold/refined: ingot lines and pressed foil scratches.
        for (int y = 8; y < tile; y += 12) drawRect(13, 5, y, tile - 10, 5, rgba(190, 145, 48), 0.9f);
        drawLine(13, 6, 44, 58, 30, rgba(233, 189, 75), 0);
        for (int x = 7; x < tile; x += 12) drawLine(14, x, 4, x + 5, 60, rgba(243, 231, 174), 0);
        drawRect(14, 9, 10, 46, 9, rgba(190, 177, 119), 0.72f);
        drawRect(14, 16, 37, 36, 8, rgba(245, 234, 170), 0.55f);

        // Stain: irregular brown pools, runoff lines.
        for (int i = 0; i < 8; ++i) {
            int cx = static_cast<int>(hash2(i, 17, 45) % tile);
            int cy = static_cast<int>(hash2(i, 18, 45) % tile);
            int radius = 4 + static_cast<int>(hash2(i, 19, 45) % 9);
            for (int y = std::max(0, cy - radius); y < std::min(tile, cy + radius); ++y) {
                for (int x = std::max(0, cx - radius); x < std::min(tile, cx + radius); ++x) {
                    float dist = std::sqrt(static_cast<float>((x - cx) * (x - cx) + (y - cy) * (y - cy)));
                    if (dist < radius * (0.7f + noiseAt(x, y, 62) * 0.55f)) putColor(15, x, y, rgba(76, 52, 31), 0.88f);
                }
            }
        }
        drawLine(15, 8, 0, 29, 63, rgba(38, 29, 20), 1);
        drawLine(15, 42, 2, 51, 63, rgba(90, 61, 34), 0);

        glGenTextures(1, &atlasTex_);
        glBindTexture(GL_TEXTURE_2D, atlasTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        return true;
    }

    bool createBuffers() {
        glGenVertexArrays(1, &tileVao_);
        glGenBuffers(1, &tileVbo_);
        glBindVertexArray(tileVao_);
        glBindBuffer(GL_ARRAY_BUFFER, tileVbo_);
        glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
        GLint pos = glGetAttribLocation(tileProgram_, "aPos");
        GLint col = glGetAttribLocation(tileProgram_, "aColor");
        GLint kind = glGetAttribLocation(tileProgram_, "aKind");
        GLint uvLoc = glGetAttribLocation(tileProgram_, "aUv");
        glEnableVertexAttribArray(pos);
        glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(col);
        glVertexAttribPointer(col, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(float) * 2));
        glEnableVertexAttribArray(kind);
        glVertexAttribPointer(kind, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(float) * 6));
        glEnableVertexAttribArray(uvLoc);
        glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(float) * 7));

        float quad[] = {
            -1, -1, 0, 0,  1, -1, 1, 0,  1, 1, 1, 1,
            -1, -1, 0, 0,  1, 1, 1, 1,  -1, 1, 0, 1
        };
        glGenVertexArrays(1, &screenVao_);
        glGenBuffers(1, &screenVbo_);
        glBindVertexArray(screenVao_);
        glBindBuffer(GL_ARRAY_BUFFER, screenVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        GLint p = glGetAttribLocation(postProgram_, "aPos");
        GLint uv = glGetAttribLocation(postProgram_, "aUv");
        glEnableVertexAttribArray(p);
        glVertexAttribPointer(p, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(uv);
        glVertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
        return true;
    }

    bool createFramebuffer(int w, int h) {
        viewW_ = w;
        viewH_ = h;
        if (fboTex_) glDeleteTextures(1, &fboTex_);
        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        glGenTextures(1, &fboTex_);
        glBindTexture(GL_TEXTURE_2D, fboTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex_, 0);
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }

    int index(int x, int y) const { return y * MapW + x; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < MapW && y < MapH; }

    Vec2 dirVector(Direction dir) const {
        switch (dir) {
            case Direction::East: return {1.0f, 0.0f};
            case Direction::South: return {0.0f, 1.0f};
            case Direction::West: return {-1.0f, 0.0f};
            case Direction::North: return {0.0f, -1.0f};
        }
        return {1.0f, 0.0f};
    }

    std::string directionName(Direction dir) const {
        switch (dir) {
            case Direction::East: return "EAST";
            case Direction::South: return "SOUTH";
            case Direction::West: return "WEST";
            case Direction::North: return "NORTH";
        }
        return "EAST";
    }

    Direction rotateDirection(Direction dir, int delta) const {
        int v = (static_cast<int>(dir) + delta) % 4;
        if (v < 0) v += 4;
        return static_cast<Direction>(v);
    }

    void rotatePlacementDirection(int delta) {
        placementDir_ = rotateDirection(placementDir_, delta);
    }

    void cycleOverlayMode() {
        int next = (static_cast<int>(overlayMode_) + 1) % 4;
        overlayMode_ = static_cast<OverlayMode>(next);
        addAlert("VIEW: " + overlayName(), rgba(205, 214, 202), 1.8f);
    }

    void resetEconomy() {
        profit_ = 0.0f;
        cash_ = 1000.0f;
        upkeep_ = 0.0f;
        budget_ = cash_;
        stability_ = 0.78f;
        purity_ = 0.98f;
        variance_ = 0.04f;
        auditPressure_ = 0.0f;
        routeEfficiency_ = 1.0f;
        exportPenalty_ = 0.0f;
        containmentCost_ = 0.0f;
        dehumanisation_ = 0.0f;
        exported_ = 0.0f;
        goalComplete_ = false;
        auditFailed_ = false;
        endState_ = EndState::None;
        auditReportClock_ = 0.0f;
        auditReportTtl_ = 0.0f;
        lastAuditReport_.clear();
        for (bool& seen : milestoneSeen_) seen = false;
        alerts_.clear();
    }

    void setTile(int x, int y, Tile tile, Direction dir = Direction::East) {
        if (!inside(x, y)) return;
        Cell& c = cells_[index(x, y)];
        c.tile = tile;
        c.zone = tile;
        c.dir = dir;
        c.variant = static_cast<uint8_t>((x * 37 + y * 19 + static_cast<int>(tile) * 13) & 15);
    }

    void addRoadLine(int x0, int y0, int x1, int y1) {
        int dx = (x1 > x0) ? 1 : (x1 < x0 ? -1 : 0);
        int dy = (y1 > y0) ? 1 : (y1 < y0 ? -1 : 0);
        Direction dir = std::abs(x1 - x0) >= std::abs(y1 - y0) ? (dx >= 0 ? Direction::East : Direction::West) : (dy >= 0 ? Direction::South : Direction::North);
        int x = x0, y = y0;
        while (inside(x, y)) {
            setTile(x, y, Tile::Road, dir);
            if (x == x1 && y == y1) break;
            x += dx;
            y += dy;
        }
    }

    void addRailLine(int x0, int y0, int x1, int y1) {
        int dx = (x1 > x0) ? 1 : (x1 < x0 ? -1 : 0);
        int dy = (y1 > y0) ? 1 : (y1 < y0 ? -1 : 0);
        Direction dir = std::abs(x1 - x0) >= std::abs(y1 - y0) ? (dx >= 0 ? Direction::East : Direction::West) : (dy >= 0 ? Direction::South : Direction::North);
        int x = x0, y = y0;
        while (inside(x, y)) {
            setTile(x, y, Tile::Rail, dir);
            if (x == x1 && y == y1) break;
            x += dx;
            y += dy;
        }
    }

    std::string presetName() const {
        switch (presetIndex_) {
            case 1: return "RAIL SIDING";
            case 2: return "DIAGNOSTIC CHAIN";
            default: return "BLANK GRID";
        }
    }

    void cyclePreset(int delta) {
        presetIndex_ = (presetIndex_ + delta) % 3;
        if (presetIndex_ < 0) presetIndex_ += 3;
        generateMap();
        mode_ = GameMode::Title;
    }

    void generateMap() {
        cells_.assign(MapW * MapH, Cell{});
        std::uniform_real_distribution<float> d(0.0f, 1.0f);

        for (int y = 0; y < MapH; ++y) {
            for (int x = 0; x < MapW; ++x) {
                Cell& c = cells_[index(x, y)];
                c.variant = static_cast<uint8_t>((x * 37 + y * 23) & 15);
                c.desirability = d(rng_);
            }
        }

        addRoadLine(8, MapH / 2, MapW - 16, MapH / 2);
        addRoadLine(MapW / 2, 12, MapW / 2, MapH - 16);
        addRailLine(8, MapH - 18, MapW - 10, MapH - 18);

        if (presetIndex_ == 1) {
            addRailLine(16, 24, 82, 24);
            setTile(78, 24, Tile::Port, Direction::East);
            addRoadLine(20, 30, 54, 30);
            setTile(24, 30, Tile::Holding, Direction::East);
        } else if (presetIndex_ == 2) {
            setTile(20, 40, Tile::Block, Direction::East);
            setTile(24, 40, Tile::Holding, Direction::East);
            setTile(30, 40, Tile::Mine, Direction::East);
            setTile(38, 40, Tile::Factory, Direction::East);
            setTile(48, 40, Tile::Refinery, Direction::East);
            setTile(62, 40, Tile::Port, Direction::East);
            for (int x = 21; x < 62; ++x) if (cells_[index(x, 40)].tile == Tile::Empty) {
                setTile(x, 40, x < 48 ? Tile::Conveyor : Tile::Rail, Direction::East);
            }
        }

        camera_ = {MapW * TileSize * 0.5f, MapH * TileSize * 0.5f};
        zoom_ = targetZoom_ = 0.70f;
        resetEconomy();
        addAlert("DIRECTIVE: EXPORT 50 REFINED. STABILITY ABOVE 60. VARIANCE BELOW 35.", rgba(205, 214, 202), 6.0f);
    }

    void zone(int x, int y, Tile z) {
        if (!inside(x, y)) return;
        Cell& c = cells_[index(x, y)];
        c.tile = z;
        c.zone = z;
        c.level = 0;
        c.growth = 0.0f;
        c.variant = static_cast<uint8_t>((x * 37 + y * 19 + static_cast<int>(z) * 13) & 15);
    }

    std::string savePath() const {
        return "administrative_grid.save";
    }

    void saveGame() {
        std::ofstream out(savePath());
        if (!out) {
            addAlert("SAVE FAILED: FILE NOT OPENED.", rgba(218, 139, 154), 3.0f);
            return;
        }
        out << "ADMIN_GRID_SAVE 3\n";
        out << cash_ << ' ' << profit_ << ' ' << upkeep_ << ' ' << stability_ << ' ' << purity_ << ' '
            << variance_ << ' ' << exported_ << ' ' << goalComplete_ << ' ' << auditFailed_ << ' '
            << auditPressure_ << ' ' << routeEfficiency_ << ' ' << exportPenalty_ << ' ' << containmentCost_ << ' '
            << presetIndex_ << ' ' << dehumanisation_ << '\n';
        out << camera_.x << ' ' << camera_.y << ' ' << zoom_ << ' ' << targetZoom_ << ' '
            << static_cast<int>(tool_) << ' ' << static_cast<int>(placementDir_) << '\n';
        out << MapW << ' ' << MapH << '\n';
        for (const Cell& c : cells_) {
            out << static_cast<int>(c.tile) << ' ' << static_cast<int>(c.zone) << ' ' << c.level << ' '
                << c.growth << ' ' << c.pollution << ' ' << c.desirability << ' '
                << static_cast<int>(c.variant) << ' ' << static_cast<int>(c.dir) << ' '
                << static_cast<int>(c.status) << ' ' << c.pink << ' ' << c.fuel << ' '
                << c.gold << ' ' << c.refined << ' ' << c.shutdownTimer << '\n';
        }
        addAlert("SAVE RECORDED: ADMINISTRATIVE GRID.", rgba(205, 214, 202), 3.0f);
    }

    void loadGame() {
        std::ifstream in(savePath());
        if (!in) {
            addAlert("LOAD FAILED: NO SAVE FILE.", rgba(218, 139, 154), 3.0f);
            return;
        }
        std::string magic;
        int version = 0;
        in >> magic >> version;
        if (magic != "ADMIN_GRID_SAVE" || version < 1 || version > 3) {
            addAlert("LOAD FAILED: SAVE FORMAT REJECTED.", rgba(218, 139, 154), 3.0f);
            return;
        }
        in >> cash_ >> profit_ >> upkeep_ >> stability_ >> purity_ >> variance_ >> exported_ >> goalComplete_ >> auditFailed_;
        if (version >= 2) {
            in >> auditPressure_ >> routeEfficiency_ >> exportPenalty_ >> containmentCost_;
            if (version >= 3) in >> presetIndex_;
            if (version >= 3) in >> dehumanisation_;
        } else {
            auditPressure_ = 0.0f;
            routeEfficiency_ = 1.0f;
            exportPenalty_ = 0.0f;
            containmentCost_ = 0.0f;
            presetIndex_ = 0;
            dehumanisation_ = 0.0f;
        }
        presetIndex_ = std::max(0, std::min(2, presetIndex_));
        int tool = 1;
        int dir = 0;
        in >> camera_.x >> camera_.y >> zoom_ >> targetZoom_ >> tool >> dir;
        int w = 0, h = 0;
        in >> w >> h;
        if (w != MapW || h != MapH || !in) {
            addAlert("LOAD FAILED: GRID SIZE MISMATCH.", rgba(218, 139, 154), 3.0f);
            return;
        }
        cells_.assign(MapW * MapH, Cell{});
        for (Cell& c : cells_) {
            int tile = 0, zone = 0, variant = 0, cellDir = 0, status = 0;
            in >> tile >> zone >> c.level >> c.growth >> c.pollution >> c.desirability
               >> variant >> cellDir >> status >> c.pink >> c.fuel >> c.gold >> c.refined;
            if (version >= 2) in >> c.shutdownTimer;
            c.recentFlow = 0.0f;
            c.blockedFlow = 0.0f;
            c.packetFlow = 0.0f;
            if (!in) {
                addAlert("LOAD FAILED: SAVE FILE INCOMPLETE.", rgba(218, 139, 154), 3.0f);
                generateMap();
                return;
            }
            c.tile = static_cast<Tile>(tile);
            c.zone = static_cast<Tile>(zone);
            c.variant = static_cast<uint8_t>(std::max(0, std::min(255, variant)));
            c.dir = static_cast<Direction>((cellDir % 4 + 4) % 4);
            c.status = static_cast<BuildingStatus>(std::max(0, std::min(6, status)));
        }
        tool_ = static_cast<Tool>(tool);
        placementDir_ = static_cast<Direction>((dir % 4 + 4) % 4);
        endState_ = EndState::None;
        auditReportClock_ = 0.0f;
        auditReportTtl_ = 0.0f;
        lastAuditReport_.clear();
        budget_ = cash_;
        alerts_.clear();
        addAlert("SAVE LOADED: LEDGER RESTORED.", rgba(205, 214, 202), 3.0f);
    }

    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running_ = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                createFramebuffer(e.window.data1, e.window.data2);
            }
            if (e.type == SDL_MOUSEWHEEL) {
                int mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                mouse_ = {static_cast<float>(mx), static_cast<float>(my)};
                if (isPointerOverUi(mouse_)) continue;
                zoomFocusScreen_ = mouse_;
                zoomFocusWorld_ = screenToWorld(zoomFocusScreen_);
                zoomFocus_ = true;
                targetZoom_ *= e.wheel.y > 0 ? 1.12f : 0.89f;
                targetZoom_ = std::max(0.25f, std::min(2.8f, targetZoom_));
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                mouse_ = {static_cast<float>(e.button.x), static_cast<float>(e.button.y)};
                if (mode_ == GameMode::Title && e.button.button == SDL_BUTTON_LEFT) {
                    handleTitleClick(mouse_);
                    continue;
                }
                if (mode_ == GameMode::Paused && e.button.button == SDL_BUTTON_LEFT) {
                    handlePauseClick(mouse_);
                    continue;
                }
                if (mode_ == GameMode::End && e.button.button == SDL_BUTTON_LEFT) {
                    handleEndClick(mouse_);
                    continue;
                }
                if (e.button.button == SDL_BUTTON_LEFT) {
                    Tool clickedTool = tool_;
                    if (toolAtUi(mouse_, clickedTool)) {
                        tool_ = clickedTool;
                        painting_ = false;
                        lastPaintX_ = -1;
                        lastPaintY_ = -1;
                        addAlert("TOOL SELECTED: " + toolLabel(tool_), toolColor(tool_), 1.6f);
                        continue;
                    }
                    if (isPointerOverUi(mouse_)) continue;
                    painting_ = true;
                    lastPaintX_ = -1;
                    lastPaintY_ = -1;
                    applyToolAtMouse();
                }
                if (e.button.button == SDL_BUTTON_RIGHT) {
                    if (isPointerOverUi(mouse_)) continue;
                    dragging_ = true;
                    zoomFocus_ = false;
                    lastMouse_ = {static_cast<float>(e.button.x), static_cast<float>(e.button.y)};
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    painting_ = false;
                    lastPaintX_ = -1;
                    lastPaintY_ = -1;
                }
                if (e.button.button == SDL_BUTTON_RIGHT) dragging_ = false;
            }
            if (e.type == SDL_MOUSEMOTION) {
                mouse_ = {static_cast<float>(e.motion.x), static_cast<float>(e.motion.y)};
                if (dragging_) {
                    Vec2 now{static_cast<float>(e.motion.x), static_cast<float>(e.motion.y)};
                    camera_.x -= (now.x - lastMouse_.x) / zoom_;
                    camera_.y -= (now.y - lastMouse_.y) / zoom_;
                    lastMouse_ = now;
                }
            }
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (mode_ == GameMode::Title) {
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE: running_ = false; break;
                        case SDLK_RETURN:
                        case SDLK_SPACE: mode_ = GameMode::Playing; break;
                        case SDLK_l: loadGame(); mode_ = GameMode::Playing; break;
                        case SDLK_LEFT: cyclePreset(-1); break;
                        case SDLK_RIGHT: cyclePreset(1); break;
                        case SDLK_1: presetIndex_ = 0; generateMap(); mode_ = GameMode::Title; break;
                        case SDLK_2: presetIndex_ = 1; generateMap(); mode_ = GameMode::Title; break;
                        case SDLK_3: presetIndex_ = 2; generateMap(); mode_ = GameMode::Title; break;
                        default: break;
                    }
                    continue;
                }
                if (mode_ == GameMode::Paused) {
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_SPACE: mode_ = GameMode::Playing; break;
                        case SDLK_s: saveGame(); break;
                        case SDLK_l: loadGame(); mode_ = GameMode::Playing; break;
                        case SDLK_r: generateMap(); mode_ = GameMode::Playing; break;
                        case SDLK_t: mode_ = GameMode::Title; break;
                        default: break;
                    }
                    continue;
                }
                if (mode_ == GameMode::End) {
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_t: mode_ = GameMode::Title; break;
                        case SDLK_RETURN:
                        case SDLK_SPACE:
                        case SDLK_r: generateMap(); mode_ = GameMode::Playing; break;
                        default: break;
                    }
                    continue;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: mode_ = GameMode::Paused; painting_ = false; dragging_ = false; break;
                    case SDLK_1: tool_ = Tool::Road; break;
                    case SDLK_2: tool_ = Tool::Conveyor; break;
                    case SDLK_3: tool_ = Tool::Rail; break;
                    case SDLK_4: tool_ = Tool::Block; break;
                    case SDLK_5: tool_ = Tool::Holding; break;
                    case SDLK_6: tool_ = Tool::Mine; break;
                    case SDLK_7: tool_ = Tool::Factory; break;
                    case SDLK_8: tool_ = Tool::Refinery; break;
                    case SDLK_9: tool_ = Tool::Port; break;
                    case SDLK_0: tool_ = Tool::Erase; break;
                    case SDLK_SPACE: mode_ = GameMode::Paused; break;
                    case SDLK_f: fog_ = !fog_; break;
                    case SDLK_p: pollutionOverlay_ = !pollutionOverlay_; break;
                    case SDLK_n: manualNight_ = !manualNight_; break;
                    case SDLK_r: generateMap(); break;
                    case SDLK_s: saveGame(); break;
                    case SDLK_l: loadGame(); break;
                    case SDLK_TAB: cycleOverlayMode(); break;
                    case SDLK_q: rotatePlacementDirection(-1); break;
                    case SDLK_e: rotatePlacementDirection(1); break;
                    default: break;
                }
            }
        }
    }

    Vec2 screenToWorld(Vec2 p) const {
        return Vec2{camera_.x + (p.x - viewW_ * 0.5f) / zoom_, camera_.y + (p.y - viewH_ * 0.5f) / zoom_};
    }

    std::array<Tool, 10> toolOrder() const {
        return {
            Tool::Road, Tool::Conveyor, Tool::Rail, Tool::Block, Tool::Holding,
            Tool::Mine, Tool::Factory, Tool::Refinery, Tool::Port, Tool::Erase
        };
    }

    bool pointInRect(Vec2 p, float x, float y, float w, float h) const {
        return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h;
    }

    float rightPanelWidth() const { return 336.0f; }
    float dockHeight() const { return 128.0f; }
    float topBarHeight() const { return 54.0f; }

    float dockWidth() const {
        return std::max(760.0f, static_cast<float>(viewW_) - rightPanelWidth() - 32.0f);
    }

    bool toolAtUi(Vec2 p, Tool& outTool) const {
        float bx = 34.0f;
        float by = static_cast<float>(viewH_) - 104.0f;
        const float bw = 72.0f;
        for (Tool tool : toolOrder()) {
            if (pointInRect(p, bx, by, bw - 8, 58.0f)) {
                outTool = tool;
                return true;
            }
            bx += bw;
        }
        return false;
    }

    bool isPointerOverUi(Vec2 p) const {
        const float rightX = static_cast<float>(viewW_) - rightPanelWidth();
        if (p.y <= topBarHeight()) return true;
        if (p.x >= rightX) return true;
        if (pointInRect(p, 16.0f, static_cast<float>(viewH_) - dockHeight() - 16.0f, dockWidth(), dockHeight())) return true;
        return false;
    }

    void handleTitleClick(Vec2 p) {
        float cx = viewW_ * 0.5f - 180.0f;
        float y = viewH_ * 0.5f - 36.0f;
        if (pointInRect(p, cx, y, 360, 40)) { mode_ = GameMode::Playing; return; }
        if (pointInRect(p, cx, y + 52, 360, 40)) { loadGame(); mode_ = GameMode::Playing; return; }
        if (pointInRect(p, cx, y + 104, 174, 40)) { cyclePreset(-1); return; }
        if (pointInRect(p, cx + 186, y + 104, 174, 40)) { cyclePreset(1); return; }
        if (pointInRect(p, cx, y + 156, 360, 40)) { running_ = false; return; }
    }

    void handlePauseClick(Vec2 p) {
        float cx = viewW_ * 0.5f - 140.0f;
        float y = viewH_ * 0.5f - 94.0f;
        if (pointInRect(p, cx, y, 280, 32)) { mode_ = GameMode::Playing; return; }
        if (pointInRect(p, cx, y + 40, 280, 32)) { saveGame(); return; }
        if (pointInRect(p, cx, y + 80, 280, 32)) { loadGame(); mode_ = GameMode::Playing; return; }
        if (pointInRect(p, cx, y + 120, 280, 32)) { generateMap(); mode_ = GameMode::Playing; return; }
        if (pointInRect(p, cx, y + 160, 280, 32)) { mode_ = GameMode::Title; return; }
    }

    void handleEndClick(Vec2 p) {
        float cx = viewW_ * 0.5f - 140.0f;
        float y = viewH_ * 0.5f + 62.0f;
        if (pointInRect(p, cx, y, 280, 34)) { generateMap(); mode_ = GameMode::Playing; return; }
        if (pointInRect(p, cx, y + 44, 280, 34)) { mode_ = GameMode::Title; return; }
    }

    Direction directionFromDelta(int dx, int dy) const {
        if (std::abs(dx) >= std::abs(dy)) return dx >= 0 ? Direction::East : Direction::West;
        return dy >= 0 ? Direction::South : Direction::North;
    }

    void applyToolAtMouse() {
        Vec2 w = screenToWorld(mouse_);
        int x = static_cast<int>(std::floor(w.x / TileSize));
        int y = static_cast<int>(std::floor(w.y / TileSize));
        if (!inside(x, y)) return;
        if (isRouteTool(tool_) && lastPaintX_ >= 0 && lastPaintY_ >= 0 && (x != lastPaintX_ || y != lastPaintY_)) {
            int dx = x - lastPaintX_;
            int dy = y - lastPaintY_;
            Direction routeDir = directionFromDelta(dx, dy);
            int sx = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
            int sy = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
            int cx = lastPaintX_;
            int cy = lastPaintY_;
            placementDir_ = routeDir;
            while (cx != x || cy != y) {
                if (std::abs(x - cx) >= std::abs(y - cy)) cx += sx;
                else cy += sy;
                placeToolAt(cx, cy, routeDir, true);
            }
            lastPaintX_ = x;
            lastPaintY_ = y;
            return;
        }
        if (placeToolAt(x, y, placementDir_, true)) {
            lastPaintX_ = x;
            lastPaintY_ = y;
        }
    }

    bool placeToolAt(int x, int y, Direction dir, bool showAlert) {
        if (!inside(x, y)) return false;
        Cell& c = cells_[index(x, y)];
        std::string reason;
        if (!canPlaceToolAt(x, y, &reason)) {
            if (showAlert && reason != "NO CHANGE" && reason != "EMPTY SITE") addAlert(reason + ": " + toolName(), rgba(218, 139, 154), 2.2f);
            return false;
        }
        cash_ -= toolCost(tool_);
        switch (tool_) {
            case Tool::Road: c = Cell{}; c.tile = Tile::Road; c.dir = dir; c.variant = static_cast<uint8_t>((x * 5 + y * 11) & 7); break;
            case Tool::Conveyor: c = Cell{}; c.tile = Tile::Conveyor; c.dir = dir; c.variant = static_cast<uint8_t>((x * 7 + y * 13) & 7); break;
            case Tool::Rail: c = Cell{}; c.tile = Tile::Rail; c.dir = dir; c.variant = static_cast<uint8_t>((x * 11 + y * 3) & 7); break;
            case Tool::Block: c = Cell{}; c.tile = Tile::Block; c.dir = dir; c.variant = static_cast<uint8_t>((x * 19 + y * 5) & 15); break;
            case Tool::Holding: c = Cell{}; c.tile = Tile::Holding; c.dir = dir; c.variant = static_cast<uint8_t>((x * 17 + y * 29) & 15); break;
            case Tool::Mine: c = Cell{}; c.tile = Tile::Mine; c.dir = dir; c.variant = static_cast<uint8_t>((x * 23 + y * 31) & 15); break;
            case Tool::Factory: c = Cell{}; c.tile = Tile::Factory; c.dir = dir; c.variant = static_cast<uint8_t>((x * 29 + y * 7) & 15); break;
            case Tool::Refinery: c = Cell{}; c.tile = Tile::Refinery; c.dir = dir; c.variant = static_cast<uint8_t>((x * 31 + y * 17) & 15); break;
            case Tool::Port: c = Cell{}; c.tile = Tile::Port; c.dir = dir; c.variant = static_cast<uint8_t>((x * 13 + y * 23) & 15); break;
            case Tool::Erase: if (c.tile != Tile::Empty) { c = Cell{}; cash_ -= 2.0f; } break;
        }
        return true;
    }

    bool hasNeighborTile(int x, int y, Tile tile) const {
        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i], ny = y + dy[i];
            if (inside(nx, ny) && cells_[index(nx, ny)].tile == tile) return true;
        }
        return false;
    }

    bool hasTile(Tile tile) const {
        return std::any_of(cells_.begin(), cells_.end(), [&](const Cell& c) { return c.tile == tile; });
    }

    bool hasTileWithLogistics(Tile tile) const {
        for (int y = 0; y < MapH; ++y) {
            for (int x = 0; x < MapW; ++x) {
                if (cells_[index(x, y)].tile == tile && hasLogisticsNeighbor(x, y)) return true;
            }
        }
        return false;
    }

    bool objectiveComplete(int step) const {
        switch (step) {
            case 0: return hasTile(Tile::Block);
            case 1: return hasTileWithLogistics(Tile::Mine) || milestoneSeen_[1];
            case 2: return hasTileWithLogistics(Tile::Factory) && (milestoneSeen_[2] || totalGold_ > 0.5f);
            case 3: return hasTileWithLogistics(Tile::Refinery) && (milestoneSeen_[3] || totalRefined_ > 0.5f);
            case 4: return hasTileWithLogistics(Tile::Port) && (milestoneSeen_[4] || exported_ > 0.1f);
        }
        return false;
    }

    int activeObjective() const {
        for (int i = 0; i < 5; ++i) if (!objectiveComplete(i)) return i;
        return 4;
    }

    int driftStage() const {
        if (dehumanisation_ > 0.62f) return 2;
        if (dehumanisation_ > 0.28f) return 1;
        return 0;
    }

    std::string materialTerm() const {
        int stage = driftStage();
        if (stage >= 2) return "CASES";
        if (stage == 1) return "UNITS";
        return "PINK";
    }

    std::string transferMetricName() const {
        int stage = driftStage();
        if (stage >= 2) return "REMOVED";
        if (stage == 1) return "TRANSFER";
        return "EXPORTED";
    }

    std::string objectiveText(int step) const {
        int stage = driftStage();
        switch (step) {
            case 0: return "BUILD BLOCK";
            case 1: return stage >= 2 ? "ROUTE CASES" : "CONNECT MINE";
            case 2: return stage >= 1 ? "SUPPLY FACTORY" : "FEED FACTORY";
            case 3: return stage >= 2 ? "DISPOSITION" : (stage == 1 ? "PROCESS GOLD" : "REFINE GOLD");
            case 4: return stage >= 2 ? "REMOVE BY PORT" : (stage == 1 ? "TRANSFER BY PORT" : "EXPORT VIA PORT");
        }
        return "";
    }

    bool hasLogisticsNeighbor(int x, int y) const {
        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!inside(nx, ny)) continue;
            Tile t = cells_[index(nx, ny)].tile;
            if (t == Tile::Road || t == Tile::Conveyor || t == Tile::Rail) return true;
        }
        return false;
    }

    bool isProductionTile(Tile tile) const {
        return tile == Tile::Block || tile == Tile::Mine || tile == Tile::Factory || tile == Tile::Refinery || tile == Tile::Port;
    }

    bool isStorageTile(Tile tile) const {
        return tile == Tile::Road || tile == Tile::Conveyor || tile == Tile::Rail || tile == Tile::Holding ||
               tile == Tile::Mine || tile == Tile::Factory || tile == Tile::Refinery || tile == Tile::Port;
    }

    bool isRouteTile(Tile tile) const {
        return tile == Tile::Road || tile == Tile::Conveyor || tile == Tile::Rail;
    }

    bool isRouteTool(Tool tool) const {
        return tool == Tool::Road || tool == Tool::Conveyor || tool == Tool::Rail;
    }

    bool isBuildingTool(Tool tool) const {
        return tool == Tool::Block || tool == Tool::Holding || tool == Tool::Mine ||
               tool == Tool::Factory || tool == Tool::Refinery || tool == Tool::Port;
    }

    bool canPlaceToolAt(int x, int y, std::string* reason = nullptr) const {
        if (!inside(x, y)) {
            if (reason) *reason = "OUTSIDE GRID";
            return false;
        }
        const Cell& c = cells_[index(x, y)];
        Tile newTile = toolTile(tool_);
        if (tool_ == Tool::Erase) {
            if (c.tile == Tile::Empty) {
                if (reason) *reason = "EMPTY SITE";
                return false;
            }
            return true;
        }
        if (cash_ < toolCost(tool_)) {
            if (reason) *reason = "FUNDS INSUFFICIENT";
            return false;
        }
        if (c.tile == newTile && (!isRouteTool(tool_) || c.dir == placementDir_)) {
            if (reason) *reason = "NO CHANGE";
            return false;
        }
        if (isRouteTool(tool_)) {
            if (c.tile == Tile::Empty || isRouteTile(c.tile)) return true;
            if (reason) *reason = "SITE OCCUPIED";
            return false;
        }
        if (isBuildingTool(tool_)) {
            if (c.tile == Tile::Empty) return true;
            if (reason) *reason = "CLEAR SITE REQUIRED";
            return false;
        }
        return true;
    }

    bool directionPointsTo(int fromX, int fromY, int toX, int toY, Direction dir) const {
        Vec2 d = dirVector(dir);
        return fromX + static_cast<int>(d.x) == toX && fromY + static_cast<int>(d.y) == toY;
    }

    std::string statusName(BuildingStatus status) const {
        switch (status) {
            case BuildingStatus::Idle: return "IDLE";
            case BuildingStatus::Running: return "RUNNING";
            case BuildingStatus::Starved: return "STARVED";
            case BuildingStatus::OutputBlocked: return "OUTPUT BLOCKED";
            case BuildingStatus::NoRail: return "NO RAIL EXPORT";
            case BuildingStatus::Full: return "STORAGE FULL";
            case BuildingStatus::Suspended: return "AUDIT SUSPENDED";
        }
        return "IDLE";
    }

    Color statusColor(BuildingStatus status) const {
        switch (status) {
            case BuildingStatus::Running: return rgba(158, 205, 166);
            case BuildingStatus::Starved: return rgba(224, 182, 116);
            case BuildingStatus::OutputBlocked: return rgba(218, 139, 154);
            case BuildingStatus::NoRail: return rgba(218, 139, 154);
            case BuildingStatus::Full: return rgba(214, 166, 92);
            case BuildingStatus::Suspended: return rgba(176, 184, 176);
            default: return rgba(154, 163, 155);
        }
    }

    int resourceKind(float Cell::*resource) const {
        if (resource == &Cell::fuel) return 1;
        if (resource == &Cell::gold) return 2;
        if (resource == &Cell::refined) return 3;
        return 0;
    }

    Color resourceColor(int kind, uint8_t alpha = 230) const {
        switch (kind) {
            case 1: return rgba(8, 8, 8, alpha);
            case 2: return rgba(219, 196, 112, alpha);
            case 3: return rgba(238, 218, 148, alpha);
            default: return rgba(236, 112, 174, alpha);
        }
    }

    float toolCost(Tool tool) const {
        switch (tool) {
            case Tool::Road: return 6.0f;
            case Tool::Conveyor: return 9.0f;
            case Tool::Rail: return 18.0f;
            case Tool::Block: return 45.0f;
            case Tool::Holding: return 55.0f;
            case Tool::Mine: return 90.0f;
            case Tool::Factory: return 150.0f;
            case Tool::Refinery: return 230.0f;
            case Tool::Port: return 260.0f;
            case Tool::Erase: return 0.0f;
        }
        return 0.0f;
    }

    void addAlert(const std::string& text, Color color, float ttl = 3.0f) {
        if (!alerts_.empty() && alerts_.back().text == text && alerts_.back().ttl > 1.0f) return;
        alerts_.push_back(Alert{text, ttl, color});
        if (alerts_.size() > 5) alerts_.erase(alerts_.begin());
    }

    void addAuditReport() {
        int stage = driftStage();
        if (variance_ > 0.46f) {
            lastAuditReport_ = stage >= 2 ? "VARIANCE NOTICE: UNROUTED CASES INCREASED. CONTAINMENT ADVISED."
                                          : "VARIANCE NOTICE: MATERIAL AWAITS ROUTE CONFIRMATION.";
        } else if (auditPressure_ > 0.52f) {
            lastAuditReport_ = stage >= 2 ? "CONTROL NOTICE: TEMPORARY HOLDS AUTHORISED BY LEDGER."
                                          : "CONTROL NOTICE: DELAYS FILED AS ACCEPTABLE LOSS.";
        } else if (exported_ > 10.0f) {
            lastAuditReport_ = stage >= 2 ? "AUDIT FINDING: CASES RECONCILED. EXIT AUTHORITY NOT RECORDED."
                                          : "AUDIT FINDING: TRANSFER CHAIN OPERATING WITH MINOR LOSS.";
        } else if (stage == 1) {
            lastAuditReport_ = "AUDIT FINDING: UNITS HELD. ROUTE EFFICIENCY ABOVE HUMAN REVIEW.";
        } else {
            lastAuditReport_ = "VARIANCE REVIEW: ROUTES ACCEPTABLE. EXPORT AUTHORITY PENDING.";
        }
        auditReportTtl_ = 10.0f;
        addAlert("AUDIT REPORT FILED.", rgba(205, 214, 202), 3.0f);
    }

    void triggerEnd(EndState state, const std::string& alert) {
        if (endState_ != EndState::None) return;
        endState_ = state;
        mode_ = GameMode::End;
        painting_ = false;
        dragging_ = false;
        addAlert(alert, state == EndState::Verified ? rgba(219, 196, 112) : rgba(218, 139, 154), 8.0f);
    }

    Tile toolTile(Tool tool) const {
        switch (tool) {
            case Tool::Road: return Tile::Road;
            case Tool::Conveyor: return Tile::Conveyor;
            case Tool::Rail: return Tile::Rail;
            case Tool::Block: return Tile::Block;
            case Tool::Holding: return Tile::Holding;
            case Tool::Mine: return Tile::Mine;
            case Tool::Factory: return Tile::Factory;
            case Tool::Refinery: return Tile::Refinery;
            case Tool::Port: return Tile::Port;
            case Tool::Erase: return Tile::Empty;
        }
        return Tile::Empty;
    }

    void update(float dt) {
        time_ += dt;
        for (Alert& alert : alerts_) alert.ttl -= dt;
        alerts_.erase(std::remove_if(alerts_.begin(), alerts_.end(), [](const Alert& alert) { return alert.ttl <= 0.0f; }), alerts_.end());
        if (auditReportTtl_ > 0.0f) auditReportTtl_ = std::max(0.0f, auditReportTtl_ - dt);
        float zoomEase = 1.0f - std::exp(-dt * 13.0f);
        zoom_ += (targetZoom_ - zoom_) * zoomEase;
        if (zoomFocus_) {
            camera_.x = zoomFocusWorld_.x - (zoomFocusScreen_.x - viewW_ * 0.5f) / zoom_;
            camera_.y = zoomFocusWorld_.y - (zoomFocusScreen_.y - viewH_ * 0.5f) / zoom_;
            if (std::abs(targetZoom_ - zoom_) < 0.0025f) {
                zoom_ = targetZoom_;
                zoomFocus_ = false;
            }
        }

        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        float speed = 560.0f / zoom_;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) camera_.x -= speed * dt;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) camera_.x += speed * dt;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) camera_.y -= speed * dt;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) camera_.y += speed * dt;

        if (mode_ == GameMode::Playing && painting_) applyToolAtMouse();
        if (mode_ != GameMode::Playing) return;

        simAccum_ += dt;
        if (simAccum_ < 0.35f) return;
        simAccum_ = 0.0f;
        simulateStep();
    }

    void simulateStep() {
        float pinkTotal = 0.0f;
        float fuelTotal = 0.0f;
        float goldTotal = 0.0f;
        float refinedTotal = 0.0f;
        float holdingPink = 0.0f;
        pollutionAverage_ = 0.0f;
        int roads = 0, conveyors = 0, rails = 0, productive = 0, holdings = 0, refineries = 0, ports = 0;
        networkFlow_ = 0.0f;
        blockedFlow_ = 0.0f;
        runningCount_ = starvedCount_ = blockedCount_ = noRailCount_ = fullCount_ = suspendedCount_ = 0;

        float instability = clamp01((0.50f - stability_) * 1.45f + (variance_ - 0.48f) * 1.18f + std::max(0.0f, -cash_ - 120.0f) * 0.0017f);
        auditPressure_ = clamp01(auditPressure_ * 0.965f + instability * 0.095f);
        routeEfficiency_ = clamp01(1.0f - auditPressure_ * 0.28f - std::max(0.0f, 0.42f - stability_) * 0.25f);
        routeEfficiency_ = std::max(0.55f, routeEfficiency_);
        exportPenalty_ = clamp01(auditPressure_ * 0.30f + std::max(0.0f, variance_ - 0.55f) * 0.40f);
        containmentCost_ = auditPressure_ * auditPressure_ * 12.0f;

        for (Cell& c : cells_) {
            c.recentFlow *= 0.68f;
            c.blockedFlow *= 0.58f;
            c.packetFlow *= 0.62f;
            if (c.packetFlow < 0.025f) c.packetKind = 0;
            if (c.shutdownTimer > 0) --c.shutdownTimer;
        }

        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};

        auto pull = [&](int x, int y, float Cell::*resource, float amount) {
            float gathered = 0.0f;
            for (int i = 0; i < 4 && gathered < amount; ++i) {
                int nx = x + dx[i], ny = y + dy[i];
                if (!inside(nx, ny)) continue;
                Cell& n = cells_[index(nx, ny)];
                if ((n.tile == Tile::Conveyor || n.tile == Tile::Rail) && !directionPointsTo(nx, ny, x, y, n.dir)) continue;
                float take = std::min(amount - gathered, n.*resource);
                n.*resource -= take;
                gathered += take;
            }
            return gathered;
        };

        auto tryAccept = [&](int nx, int ny, float Cell::*resource, float amount) {
            if (!inside(nx, ny) || amount <= 0.0f) return 0.0f;
            Cell& n = cells_[index(nx, ny)];
            if (!isStorageTile(n.tile)) return 0.0f;
            float capacity = n.tile == Tile::Holding || isProductionTile(n.tile) ? 18.0f : 5.0f;
            float space = std::max(0.0f, capacity - n.*resource);
            float give = std::min(space, amount * routeEfficiency_);
            n.*resource += give;
            return give;
        };

        auto push = [&](int x, int y, float Cell::*resource, float amount) {
            Cell& source = cells_[index(x, y)];
            float accepted = 0.0f;
            int kind = resourceKind(resource);
            if (source.tile == Tile::Conveyor || source.tile == Tile::Rail || isProductionTile(source.tile)) {
                Vec2 d = dirVector(source.dir);
                int nx = x + static_cast<int>(d.x), ny = y + static_cast<int>(d.y);
                accepted = tryAccept(nx, ny, resource, amount);
                float blocked = std::max(0.0f, amount - accepted);
                if (accepted > 0.0f) {
                    source.recentFlow = std::min(1.0f, source.recentFlow + accepted * 0.22f);
                    source.packetFlow = std::min(1.0f, source.packetFlow + accepted * 0.32f);
                    source.packetKind = kind;
                    networkFlow_ += accepted;
                    if (inside(nx, ny)) {
                        Cell& target = cells_[index(nx, ny)];
                        target.recentFlow = std::min(1.0f, target.recentFlow + accepted * 0.12f);
                        target.packetFlow = std::min(1.0f, target.packetFlow + accepted * 0.18f);
                        target.packetKind = kind;
                    }
                }
                if (blocked > 0.001f) {
                    source.blockedFlow = std::min(1.0f, source.blockedFlow + blocked * 0.35f);
                    blockedFlow_ += blocked;
                }
                return accepted;
            }

            int preferred = -1;
            for (int i = 0; i < 4; ++i) {
                int nx = x + dx[i], ny = y + dy[i];
                if (!inside(nx, ny)) continue;
                Tile t = cells_[index(nx, ny)].tile;
                if (t == Tile::Conveyor || t == Tile::Rail) {
                    preferred = i;
                    break;
                }
            }

            for (int pass = 0; pass < 2 && amount > 0.0f; ++pass) {
                for (int i = 0; i < 4 && amount > 0.0f; ++i) {
                    if ((pass == 0 && i != preferred) || (pass == 1 && i == preferred)) continue;
                    int nx = x + dx[i], ny = y + dy[i];
                    float give = tryAccept(nx, ny, resource, amount);
                    accepted += give;
                    if (give > 0.0f) {
                        Cell& target = cells_[index(nx, ny)];
                        target.recentFlow = std::min(1.0f, target.recentFlow + give * 0.12f);
                        target.packetFlow = std::min(1.0f, target.packetFlow + give * 0.18f);
                        target.packetKind = kind;
                    }
                    amount -= give;
                }
            }
            if (accepted > 0.0f) {
                source.recentFlow = std::min(1.0f, source.recentFlow + accepted * 0.18f);
                source.packetFlow = std::min(1.0f, source.packetFlow + accepted * 0.25f);
                source.packetKind = kind;
                networkFlow_ += accepted;
            }
            return accepted;
        };

        for (int y = 0; y < MapH; ++y) {
            for (int x = 0; x < MapW; ++x) {
                Cell& c = cells_[index(x, y)];
                c.pollution *= 0.92f;
                c.status = BuildingStatus::Idle;
                if (c.tile == Tile::Road) ++roads;
                if (c.tile == Tile::Conveyor) ++conveyors;
                if (c.tile == Tile::Rail) ++rails;
                if (c.tile == Tile::Holding) ++holdings;
                if (c.tile == Tile::Refinery) ++refineries;
                if (c.tile == Tile::Port) ++ports;
                bool trackStatus = isProductionTile(c.tile) || c.tile == Tile::Holding;
                bool suspended = trackStatus && c.shutdownTimer > 0;

                if (suspended) {
                    c.status = BuildingStatus::Suspended;
                } else if (c.tile == Tile::Block) {
                    c.pink = std::min(12.0f, c.pink + 0.34f);
                    float moved = std::min(1.0f, c.pink);
                    float accepted = push(x, y, &Cell::pink, moved);
                    c.pink -= accepted;
                    c.status = accepted > 0.0f ? BuildingStatus::Running : BuildingStatus::OutputBlocked;
                    productive++;
                } else if (c.tile == Tile::Mine) {
                    float in = pull(x, y, &Cell::pink, 0.52f);
                    c.fuel = std::min(12.0f, c.fuel + in * 0.68f);
                    c.pollution = clamp01(c.pollution + in * 0.012f);
                    float moved = std::min(0.8f, c.fuel);
                    float accepted = push(x, y, &Cell::fuel, moved);
                    c.fuel -= accepted;
                    c.status = in <= 0.0f ? BuildingStatus::Starved : (accepted > 0.0f ? BuildingStatus::Running : BuildingStatus::OutputBlocked);
                    productive++;
                } else if (c.tile == Tile::Factory) {
                    float pink = pull(x, y, &Cell::pink, 0.50f);
                    float fuel = pull(x, y, &Cell::fuel, 0.36f);
                    float made = std::min(pink, fuel * 1.35f) * 0.82f;
                    c.gold = std::min(12.0f, c.gold + made);
                    c.pollution = clamp01(c.pollution + made * 0.010f);
                    float moved = std::min(0.8f, c.gold);
                    float accepted = push(x, y, &Cell::gold, moved);
                    c.gold -= accepted;
                    c.status = made <= 0.0f ? BuildingStatus::Starved : (accepted > 0.0f ? BuildingStatus::Running : BuildingStatus::OutputBlocked);
                    productive++;
                } else if (c.tile == Tile::Refinery) {
                    float gold = pull(x, y, &Cell::gold, 0.62f);
                    float fuel = pull(x, y, &Cell::fuel, 0.23f);
                    float made = std::min(gold, fuel * 2.1f) * 0.80f;
                    c.refined = std::min(12.0f, c.refined + made);
                    c.pollution = clamp01(c.pollution + made * 0.014f);
                    float moved = std::min(0.9f, c.refined);
                    float accepted = push(x, y, &Cell::refined, moved);
                    c.refined -= accepted;
                    c.status = made <= 0.0f ? BuildingStatus::Starved : (accepted > 0.0f ? BuildingStatus::Running : BuildingStatus::OutputBlocked);
                    productive++;
                } else if (c.tile == Tile::Port) {
                    float shipment = pull(x, y, &Cell::refined, hasNeighborTile(x, y, Tile::Rail) ? 1.30f : 0.20f);
                    exported_ += shipment;
                    float income = shipment * 48.0f * (1.0f - exportPenalty_);
                    profit_ += income;
                    cash_ += income;
                    c.status = !hasNeighborTile(x, y, Tile::Rail) ? BuildingStatus::NoRail : (shipment > 0.0f ? BuildingStatus::Running : BuildingStatus::Starved);
                    productive++;
                } else if (c.tile == Tile::Road || c.tile == Tile::Conveyor || c.tile == Tile::Rail || c.tile == Tile::Holding) {
                    float leak = c.tile == Tile::Road ? 0.03f : 0.08f;
                    float movedPink = std::min(leak, c.pink);
                    float movedFuel = std::min(leak, c.fuel);
                    float movedGold = std::min(leak, c.gold);
                    float movedRefined = std::min(leak, c.refined);
                    c.pink -= push(x, y, &Cell::pink, movedPink);
                    c.fuel -= push(x, y, &Cell::fuel, movedFuel);
                    c.gold -= push(x, y, &Cell::gold, movedGold);
                    c.refined -= push(x, y, &Cell::refined, movedRefined);
                    if (c.tile == Tile::Holding && c.pink + c.fuel + c.gold + c.refined > 16.0f) c.status = BuildingStatus::Full;
                }
                if (trackStatus) {
                    float stockLocal = c.pink + c.fuel + c.gold + c.refined;
                    if (c.status == BuildingStatus::Running) c.growth = std::min(1.0f, c.growth + 0.12f);
                    else c.growth *= 0.90f;
                    c.level = std::max(0, std::min(4, static_cast<int>(stockLocal / 4.0f + c.growth * 2.0f)));
                    if (c.status == BuildingStatus::Running) ++runningCount_;
                    if (c.status == BuildingStatus::Starved) ++starvedCount_;
                    if (c.status == BuildingStatus::OutputBlocked) ++blockedCount_;
                    if (c.status == BuildingStatus::NoRail) ++noRailCount_;
                    if (c.status == BuildingStatus::Full) ++fullCount_;
                    if (c.status == BuildingStatus::Suspended) ++suspendedCount_;
                }
            }
        }

        for (const Cell& c : cells_) {
            pinkTotal += c.pink;
            fuelTotal += c.fuel;
            goldTotal += c.gold;
            refinedTotal += c.refined;
            if (c.tile == Tile::Holding) holdingPink += c.pink;
            pollutionAverage_ += c.pollution;
        }
        pollutionAverage_ /= static_cast<float>(MapW * MapH);
        float network = static_cast<float>(roads + conveyors + rails) / 160.0f;
        float stock = pinkTotal + fuelTotal + goldTotal + refinedTotal;
        stability_ = clamp01(0.64f + roads * 0.0019f + rails * 0.0013f - pollutionAverage_ * 0.62f - stock * 0.00065f - auditPressure_ * 0.08f);
        purity_ = clamp01(0.99f - pollutionAverage_ * 1.05f - std::max(0.0f, pinkTotal - 95.0f) * 0.0011f);
        variance_ = clamp01(0.025f + stock * 0.00135f + productive * 0.00155f - network * 0.045f + auditPressure_ * 0.035f);
        upkeep_ = roads * 0.55f + conveyors * 0.85f + rails * 1.35f + productive * 3.2f + containmentCost_;
        cash_ -= upkeep_ * 0.032f;
        budget_ = cash_;
        population_ = static_cast<int>(pinkTotal);
        jobs_ = static_cast<int>(exported_);
        totalPink_ = pinkTotal;
        totalFuel_ = fuelTotal;
        totalGold_ = goldTotal;
        totalRefined_ = refinedTotal;

        float bureaucraticDrift = holdingPink * 0.0014f + refineries * 0.0045f + ports * 0.002f + exported_ * 0.0009f + auditPressure_ * 0.018f;
        dehumanisation_ = clamp01(dehumanisation_ * 0.997f + bureaucraticDrift);
        auditReportClock_ += 0.35f;
        if (auditReportClock_ >= 24.0f + auditPressure_ * 18.0f) {
            auditReportClock_ = 0.0f;
            addAuditReport();
        }

        if (auditPressure_ > 0.70f && productive > 0) {
            std::uniform_real_distribution<float> roll(0.0f, 1.0f);
            if (roll(rng_) < 0.11f * auditPressure_) {
                int start = static_cast<int>(roll(rng_) * static_cast<float>(cells_.size()));
                for (int i = 0; i < static_cast<int>(cells_.size()); ++i) {
                    Cell& c = cells_[(start + i) % cells_.size()];
                    if (isProductionTile(c.tile) && c.shutdownTimer <= 0) {
                        c.shutdownTimer = 3 + static_cast<int>(roll(rng_) * 5.0f);
                        addAlert("AUDIT HOLD: UNIT TEMPORARILY SUSPENDED.", rgba(218, 139, 154), 4.0f);
                        break;
                    }
                }
            }
        }

        bool milestones[] = {
            totalPink_ > 3.0f,
            totalFuel_ > 2.0f,
            totalGold_ > 2.0f,
            totalRefined_ > 2.0f,
            exported_ > 0.5f
        };
        const char* messages[] = {
            "PINK FLOW REGISTERED.",
            "FUEL CONVERSION REGISTERED.",
            "GOLD OUTPUT REGISTERED.",
            "REFINED MATERIAL REGISTERED.",
            "FIRST EXPORT REGISTERED."
        };
        for (int i = 0; i < 5; ++i) {
            if (milestones[i] && !milestoneSeen_[i]) {
                milestoneSeen_[i] = true;
                addAlert(messages[i], rgba(205, 214, 202), 4.0f);
            }
        }

        if (!goalComplete_ && exported_ >= exportGoal_ && stability_ >= 0.60f && variance_ <= 0.35f) {
            goalComplete_ = true;
            triggerEnd(EndState::Verified, "QUOTA SATISFIED. AUDIT FILE CLOSED.");
        }
        if (!auditFailed_ && (stability_ < 0.35f || variance_ > 0.70f || cash_ < -250.0f)) {
            auditFailed_ = true;
            addAlert("AUDIT WARNING: ADMINISTRATIVE CONTROL DEGRADED.", rgba(218, 139, 154), 7.0f);
        }
        if (cash_ < -700.0f) {
            triggerEnd(EndState::Insolvency, "LEDGER CLOSED: AUTHORITY INSOLVENT.");
        } else if (purity_ < 0.55f) {
            triggerEnd(EndState::PurityBreach, "PURITY BREACH FILED: SITE QUARANTINED.");
        } else if (stability_ < 0.20f || (auditPressure_ > 0.98f && variance_ > 0.75f)) {
            triggerEnd(EndState::Collapse, "CONTROL LOSS FILED: GRID UNRECONCILED.");
        }
    }

    Color tileColor(const Cell& c) const {
        float hope = 1.0f - dehumanisation_;
        switch (c.tile) {
            case Tile::Road: return Color{0.18f + hope * 0.07f, 0.19f + hope * 0.08f, 0.18f + hope * 0.07f, 1.0f};
            case Tile::Conveyor: return Color{0.26f + hope * 0.03f, 0.25f + hope * 0.05f, 0.27f + hope * 0.03f, 1.0f};
            case Tile::Rail: return Color{0.16f + hope * 0.06f, 0.17f + hope * 0.06f, 0.16f + hope * 0.05f, 1.0f};
            case Tile::Block: return rgba(96, 70, 88);
            case Tile::Holding: return Color{0.25f + hope * 0.06f, 0.26f + hope * 0.08f, 0.27f + hope * 0.06f, 1.0f};
            case Tile::Mine: return Color{0.12f + hope * 0.035f, 0.12f + hope * 0.035f, 0.11f + hope * 0.03f, 1.0f};
            case Tile::Factory: return Color{0.38f + hope * 0.06f, 0.33f + hope * 0.07f, 0.19f + hope * 0.04f, 1.0f};
            case Tile::Refinery: return Color{0.49f + hope * 0.06f, 0.41f + hope * 0.08f, 0.18f + hope * 0.04f, 1.0f};
            case Tile::Port: return Color{0.29f + hope * 0.06f, 0.33f + hope * 0.07f, 0.34f + hope * 0.06f, 1.0f};
            default: return Color{0.34f + hope * 0.10f, 0.37f + hope * 0.11f, 0.34f + hope * 0.09f, 1.0f};
        }
    }

    int tileMaterial(Tile tile) const {
        switch (tile) {
            case Tile::Road: return 2;
            case Tile::Conveyor: return 3;
            case Tile::Rail: return 4;
            case Tile::Block: return 5;
            case Tile::Holding: return 6;
            case Tile::Mine: return 7;
            case Tile::Factory: return 8;
            case Tile::Refinery: return 9;
            case Tile::Port: return 10;
            default: return 1;
        }
    }

    static Color mod(Color c, float scale, float alpha = 1.0f) {
        return Color{clamp01(c.r * scale), clamp01(c.g * scale), clamp01(c.b * scale), clamp01(c.a * alpha)};
    }

    bool isRoad(int x, int y) const {
        return inside(x, y) && cells_[index(x, y)].tile == Tile::Road;
    }

    bool isTransport(int x, int y) const {
        if (!inside(x, y)) return false;
        Tile t = cells_[index(x, y)].tile;
        return t == Tile::Road || t == Tile::Conveyor || t == Tile::Rail;
    }

    void materialUv(int material, float u0, float v0, float u1, float v1, float& au, float& av, float& bu, float& bv, float& cu, float& cv, float& du, float& dv) const {
        constexpr float cols = 4.0f;
        constexpr float rows = 4.0f;
        float tx = static_cast<float>(material % 4) / cols;
        float ty = static_cast<float>(material / 4) / rows;
        float pad = 0.0025f;
        float sx = 1.0f / cols - pad * 2.0f;
        float sy = 1.0f / rows - pad * 2.0f;
        au = tx + pad + u0 * sx; av = ty + pad + v0 * sy;
        bu = tx + pad + u1 * sx; bv = ty + pad + v0 * sy;
        cu = tx + pad + u1 * sx; cv = ty + pad + v1 * sy;
        du = tx + pad + u0 * sx; dv = ty + pad + v1 * sy;
    }

    void addTexturedQuad(std::vector<Vertex>& out, float x, float y, float w, float h, Color c, float kind, int material, float repeat = 1.0f) {
        if (kind > -0.5f && dehumanisation_ > 0.015f) {
            float strength = dehumanisation_ * dehumanisation_ * 3.8f;
            float nx = std::sin((x + y) * 0.031f + time_ * 0.17f) + std::sin(y * 0.047f - time_ * 0.09f) * 0.7f;
            float ny = std::sin((x - y) * 0.026f - time_ * 0.13f) + std::sin(x * 0.039f + time_ * 0.11f) * 0.7f;
            x += nx * strength;
            y += ny * strength;
            w *= 1.0f + std::sin((x + y) * 0.019f) * dehumanisation_ * 0.012f;
            h *= 1.0f + std::cos((x - y) * 0.021f) * dehumanisation_ * 0.012f;
        }
        float au, av, bu, bv, cu, cv, du, dv;
        materialUv(material, 0.0f, 0.0f, repeat, repeat, au, av, bu, bv, cu, cv, du, dv);
        Vertex a{x, y, c.r, c.g, c.b, c.a, kind, au, av};
        Vertex b{x + w, y, c.r, c.g, c.b, c.a, kind, bu, bv};
        Vertex cc{x + w, y + h, c.r, c.g, c.b, c.a, kind, cu, cv};
        Vertex d{x, y + h, c.r, c.g, c.b, c.a, kind, du, dv};
        out.insert(out.end(), {a, b, cc, a, cc, d});
    }

    void addQuad(std::vector<Vertex>& out, float x, float y, float w, float h, Color c, float kind) {
        addTexturedQuad(out, x, y, w, h, c, kind, 0);
    }

    void addTileBorder(std::vector<Vertex>& out, float px, float py, Color c) {
        addQuad(out, px, py, TileSize, 1.0f, c, 0.0f);
        addQuad(out, px, py, 1.0f, TileSize, c, 0.0f);
    }

    void addBevel(std::vector<Vertex>& out, float x, float y, float w, float h, Color light, Color dark) {
        addQuad(out, x, y, w, 1.0f, light, 0.0f);
        addQuad(out, x, y, 1.0f, h, light, 0.0f);
        addQuad(out, x, y + h - 1.0f, w, 1.0f, dark, 0.0f);
        addQuad(out, x + w - 1.0f, y, 1.0f, h, dark, 0.0f);
    }

    void addRoadDetail(std::vector<Vertex>& out, int x, int y, float px, float py) {
        Color asphalt = rgba(30, 31, 30, 170);
        addQuad(out, px + 4, py + 4, TileSize - 4, TileSize - 4, rgba(0, 0, 0, 45), 0.0f);
        addTexturedQuad(out, px + 2, py + 2, TileSize - 4, TileSize - 4, asphalt, 1.0f, 2);
        Color line = rgba(134, 130, 112, 150);
        if (isRoad(x - 1, y) || isRoad(x + 1, y)) {
            addQuad(out, px + 2, py + TileSize * 0.5f - 0.75f, 7, 1.5f, line, 0.0f);
            addQuad(out, px + 15, py + TileSize * 0.5f - 0.75f, 7, 1.5f, line, 0.0f);
        }
        if (isRoad(x, y - 1) || isRoad(x, y + 1)) {
            addQuad(out, px + TileSize * 0.5f - 0.75f, py + 2, 1.5f, 7, line, 0.0f);
            addQuad(out, px + TileSize * 0.5f - 0.75f, py + 15, 1.5f, 7, line, 0.0f);
        }
        addQuad(out, px + 2, py + 2, TileSize - 4, 1, rgba(76, 78, 72, 90), 0.0f);
        addQuad(out, px + 2, py + TileSize - 3, TileSize - 4, 1, rgba(15, 16, 15, 100), 0.0f);
        addBevel(out, px + 2, py + 2, TileSize - 4, TileSize - 4, rgba(118, 121, 112, 70), rgba(0, 0, 0, 90));
    }

    void addDirectionArrow(std::vector<Vertex>& out, float px, float py, Direction dir, Color color) {
        float cx = px + TileSize * 0.5f;
        float cy = py + TileSize * 0.5f;
        switch (dir) {
            case Direction::East:
                addQuad(out, cx - 7, cy - 1, 11, 2, color, 0.0f);
                addQuad(out, cx + 3, cy - 4, 3, 8, color, 0.0f);
                addQuad(out, cx + 6, cy - 2, 3, 4, color, 0.0f);
                break;
            case Direction::South:
                addQuad(out, cx - 1, cy - 7, 2, 11, color, 0.0f);
                addQuad(out, cx - 4, cy + 3, 8, 3, color, 0.0f);
                addQuad(out, cx - 2, cy + 6, 4, 3, color, 0.0f);
                break;
            case Direction::West:
                addQuad(out, cx - 4, cy - 1, 11, 2, color, 0.0f);
                addQuad(out, cx - 6, cy - 4, 3, 8, color, 0.0f);
                addQuad(out, cx - 9, cy - 2, 3, 4, color, 0.0f);
                break;
            case Direction::North:
                addQuad(out, cx - 1, cy - 4, 2, 11, color, 0.0f);
                addQuad(out, cx - 4, cy - 6, 8, 3, color, 0.0f);
                addQuad(out, cx - 2, cy - 9, 4, 3, color, 0.0f);
                break;
        }
    }

    void addOutputPort(std::vector<Vertex>& out, float px, float py, Direction dir, Color color) {
        switch (dir) {
            case Direction::East: addQuad(out, px + TileSize - 4, py + 9, 3, 6, color, 0.0f); break;
            case Direction::South: addQuad(out, px + 9, py + TileSize - 4, 6, 3, color, 0.0f); break;
            case Direction::West: addQuad(out, px + 1, py + 9, 3, 6, color, 0.0f); break;
            case Direction::North: addQuad(out, px + 9, py + 1, 6, 3, color, 0.0f); break;
        }
    }

    void addRequirementMarks(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        float x = px + 4.0f;
        float y = py + TileSize - 6.0f;
        auto mark = [&](Color color) {
            addQuad(out, x, y, 4, 4, color, 0.0f);
            x += 6.0f;
        };
        if (c.tile == Tile::Mine) {
            mark(rgba(236, 112, 174, 210));
        } else if (c.tile == Tile::Factory) {
            mark(rgba(236, 112, 174, 210));
            mark(rgba(18, 18, 17, 230));
        } else if (c.tile == Tile::Refinery) {
            mark(rgba(219, 196, 112, 220));
            mark(rgba(18, 18, 17, 230));
        } else if (c.tile == Tile::Port) {
            mark(rgba(238, 218, 148, 220));
        }
        if (isProductionTile(c.tile) || c.tile == Tile::Holding) {
            addOutputPort(out, px, py, c.dir, statusColor(c.status));
        }
    }

    void addMaterialPacket(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        if (c.packetFlow <= 0.035f || c.tile == Tile::Empty) return;
        Vec2 d = dirVector(c.dir);
        float speed = c.tile == Tile::Rail ? 4.2f : (c.tile == Tile::Conveyor ? 3.4f : 2.4f);
        float phase = std::fmod(time_ * speed + static_cast<float>((static_cast<int>(px + py) / 7) % 11) * 0.09f, 1.0f);
        float cx = px + TileSize * 0.5f + d.x * (phase - 0.5f) * 18.0f;
        float cy = py + TileSize * 0.5f + d.y * (phase - 0.5f) * 18.0f;
        float size = 3.0f + std::min(2.5f, c.packetFlow * 3.0f);
        Color packet = resourceColor(c.packetKind, static_cast<uint8_t>(170 + std::min(70.0f, c.packetFlow * 90.0f)));
        addQuad(out, cx - size * 0.5f + 1, cy - size * 0.5f + 1, size + 1, size + 1, rgba(0, 0, 0, 115), 0.0f);
        addTexturedQuad(out, cx - size * 0.5f, cy - size * 0.5f, size, size, packet, 0.0f, c.packetKind == 1 ? 7 : (c.packetKind == 0 ? 5 : 13));
        if (c.packetKind == 1) addQuad(out, cx - size * 0.5f, cy - size * 0.5f, size, 1.0f, rgba(210, 205, 186, 120), 0.0f);
        if (c.packetFlow > 0.35f) {
            float tail = std::min(8.0f, 3.0f + c.packetFlow * 7.0f);
            if (d.x != 0.0f) {
                float tx = d.x > 0.0f ? cx - tail : cx;
                addQuad(out, tx, cy - 1.0f, tail, 2.0f, Color{packet.r, packet.g, packet.b, 0.22f}, 0.0f);
            } else {
                float ty = d.y > 0.0f ? cy - tail : cy;
                addQuad(out, cx - 1.0f, ty, 2.0f, tail, Color{packet.r, packet.g, packet.b, 0.22f}, 0.0f);
            }
        }
    }

    float storedAmount(const Cell& c) const {
        return c.pink + c.fuel + c.gold + c.refined;
    }

    Color dominantStoredColor(const Cell& c, uint8_t alpha = 190) const {
        float peak = c.pink;
        int kind = 0;
        if (c.fuel > peak) { peak = c.fuel; kind = 1; }
        if (c.gold > peak) { peak = c.gold; kind = 2; }
        if (c.refined > peak) { kind = 3; }
        return resourceColor(kind, alpha);
    }

    void addActivityState(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        if (!isProductionTile(c.tile) && c.tile != Tile::Holding) return;
        float stock = storedAmount(c);
        float fill = clamp01(stock / 18.0f);
        Color status = statusColor(c.status);

        if (c.status == BuildingStatus::Running) {
            float pulse = 0.45f + 0.35f * (std::sin(time_ * 5.0f + px * 0.07f) * 0.5f + 0.5f);
            addQuad(out, px + 4, py + 3, TileSize - 8, 1.0f, Color{status.r, status.g, status.b, pulse}, 0.0f);
            addQuad(out, px + 4, py + TileSize - 4, TileSize - 8, 1.0f, Color{status.r, status.g, status.b, pulse * 0.7f}, 0.0f);
        } else if (c.status == BuildingStatus::Starved) {
            addQuad(out, px + 5, py + 5, 3, 3, rgba(224, 182, 116, 190), 0.0f);
            addQuad(out, px + TileSize - 8, py + 5, 3, 3, rgba(224, 182, 116, 120), 0.0f);
        } else if (c.status == BuildingStatus::OutputBlocked || c.status == BuildingStatus::NoRail) {
            addQuad(out, px + 4, py + 4, TileSize - 8, 2, rgba(218, 139, 154, 185), 0.0f);
            addQuad(out, px + 4, py + TileSize - 6, TileSize - 8, 2, rgba(218, 139, 154, 185), 0.0f);
        } else if (c.status == BuildingStatus::Full) {
            addQuad(out, px + 4, py + 4, TileSize - 8, TileSize - 8, rgba(214, 166, 92, 54), 0.0f);
        } else if (c.status == BuildingStatus::Suspended) {
            addQuad(out, px + 5, py + 6, TileSize - 10, 2, rgba(176, 184, 176, 180), 0.0f);
            addQuad(out, px + 5, py + 16, TileSize - 10, 2, rgba(176, 184, 176, 150), 0.0f);
        }

        if (stock > 0.1f) {
            Color fillColor = dominantStoredColor(c, 170);
            addTexturedQuad(out, px + 5, py + TileSize - 8, (TileSize - 10) * fill, 3, fillColor, 0.0f, c.refined > c.gold ? 14 : 13);
        }

        if (c.tile == Tile::Mine && c.growth > 0.08f) {
            float puff = std::fmod(time_ * 1.4f + c.variant * 0.11f, 1.0f);
            addQuad(out, px + 13 + puff * 2.0f, py + 2 - puff * 4.0f, 3 + puff * 3.0f, 2, rgba(96, 91, 79, static_cast<uint8_t>(80 + c.growth * 80)), 0.0f);
        } else if (c.tile == Tile::Factory && c.growth > 0.08f) {
            addQuad(out, px + 6, py + 13, 12, 2, rgba(234, 182, 67, static_cast<uint8_t>(80 + c.growth * 120)), 0.0f);
            addQuad(out, px + 16, py + 4, 2, 5, rgba(234, 198, 104, static_cast<uint8_t>(60 + c.growth * 90)), 0.0f);
        } else if (c.tile == Tile::Refinery && c.growth > 0.08f) {
            float sweep = std::fmod(time_ * 1.8f + c.variant * 0.05f, 1.0f);
            addQuad(out, px + 5 + sweep * 11.0f, py + 6, 2, 14, rgba(238, 218, 148, static_cast<uint8_t>(70 + c.growth * 110)), 0.0f);
        } else if (c.tile == Tile::Port && c.growth > 0.08f) {
            addQuad(out, px + 6, py + 17, 12, 2, rgba(238, 218, 148, static_cast<uint8_t>(70 + c.growth * 100)), 0.0f);
            addQuad(out, px + 17, py + 8, 2, 8, rgba(172, 184, 184, static_cast<uint8_t>(55 + c.growth * 80)), 0.0f);
        }
    }

    void addZoneDetail(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        Color edge = rgba(25, 29, 26, 110);
        addQuad(out, px + 4, py + 5, TileSize - 4, TileSize - 4, rgba(0, 0, 0, 72), 0.0f);
        addQuad(out, px + 3, py + 3, TileSize - 6, 1, edge, 0.0f);
        addQuad(out, px + 3, py + TileSize - 4, TileSize - 6, 1, edge, 0.0f);
        addQuad(out, px + 3, py + 3, 1, TileSize - 6, edge, 0.0f);
        addQuad(out, px + TileSize - 4, py + 3, 1, TileSize - 6, edge, 0.0f);
        if (c.tile == Tile::Block) {
            addTexturedQuad(out, px + 7, py + 7, 10, 10, rgba(206, 105, 159, 150), 0.0f, 5);
            addTexturedQuad(out, px + 10, py + 4, 4, 16, rgba(238, 161, 197, 90), 0.0f, 5);
            addQuad(out, px + 5, py + 5, 14, 2, rgba(245, 176, 207, 80), 0.0f);
            addQuad(out, px + 17, py + 9, 2, 10, rgba(64, 35, 54, 120), 0.0f);
            for (int i = 0; i < c.level; ++i) addQuad(out, px + 6 + i * 3, py + 16, 2, 2, rgba(236, 112, 174, 130), 0.0f);
        } else if (c.tile == Tile::Holding) {
            addTexturedQuad(out, px + 5, py + 5, 14, 14, rgba(34, 34, 36, 180), 0.0f, 6);
            Color stored = dominantStoredColor(c, 165);
            addTexturedQuad(out, px + 7, py + 9, 10 * clamp01(storedAmount(c) / 18.0f), 2, stored, 0.0f, 5);
            addTexturedQuad(out, px + 7, py + 14, 10 * clamp01(storedAmount(c) / 12.0f), 2, stored, 0.0f, 5);
            for (int i = 0; i < 3; ++i) addQuad(out, px + 7 + i * 4, py + 6, 2, 2, rgba(126, 128, 122, 120), 0.0f);
            if (dehumanisation_ > 0.18f) {
                Color fence = rgba(12, 12, 12, static_cast<uint8_t>(70 + dehumanisation_ * 120));
                addQuad(out, px + 3, py + 2, 1, TileSize - 4, fence, 0.0f);
                addQuad(out, px + TileSize - 4, py + 2, 1, TileSize - 4, fence, 0.0f);
                addQuad(out, px + 5, py + 4, TileSize - 10, 1, fence, 0.0f);
                addQuad(out, px + 5, py + TileSize - 5, TileSize - 10, 1, fence, 0.0f);
            }
        } else if (c.tile == Tile::Mine) {
            addTexturedQuad(out, px + 4, py + 4, 16, 16, rgba(7, 7, 7, 220), 0.0f, 7);
            addTexturedQuad(out, px + 8, py + 8, 8, 8, rgba(52, 48, 42, 180), 0.0f, 7);
            addQuad(out, px + 5, py + 5, 5, 2, rgba(112, 107, 91, 90), 0.0f);
            addQuad(out, px + 13, py + 14, 6, 2, rgba(0, 0, 0, 170), 0.0f);
            addQuad(out, px + 10, py + 10, 2 + c.level, 2 + c.level, rgba(86, 78, 64, 110), 0.0f);
        } else if (c.tile == Tile::Factory) {
            addTexturedQuad(out, px + 4, py + 8, 16, 10, rgba(112, 91, 42, 210), 0.0f, 8);
            addQuad(out, px + 6, py + 5, 4, 5, rgba(38, 35, 29, 190), 0.0f);
            addQuad(out, px + 15, py + 3, 3, 15, rgba(32, 30, 28, 210), 0.0f);
            addQuad(out, px + 6, py + 11, 12, 2, rgba(202, 160, 70, 120), 0.0f);
            addQuad(out, px + 18, py + 2, 2, 4, rgba(88, 78, 65, 90), 0.0f);
            for (int i = 0; i < c.level; ++i) addQuad(out, px + 6 + i * 3, py + 16, 2, 2, rgba(219, 196, 112, 130), 0.0f);
        } else if (c.tile == Tile::Refinery) {
            addTexturedQuad(out, px + 4, py + 5, 16, 14, rgba(145, 118, 45, 210), 0.0f, 9);
            addQuad(out, px + 7, py + 3, 3, 18, rgba(45, 38, 30, 210), 0.0f);
            addQuad(out, px + 14, py + 3, 3, 18, rgba(45, 38, 30, 210), 0.0f);
            addQuad(out, px + 5, py + 9, 14, 2, rgba(232, 181, 62, 120), 0.0f);
            addQuad(out, px + 9, py + 4, 1, 16, rgba(75, 96, 72, 100), 0.0f);
            for (int i = 0; i < c.level; ++i) addQuad(out, px + 5 + i * 4, py + 17, 3, 1, rgba(238, 218, 148, 145), 0.0f);
            if (dehumanisation_ > 0.22f) {
                addQuad(out, px + 5, py + 5, 14, 2, rgba(128, 24, 24, static_cast<uint8_t>(60 + dehumanisation_ * 130)), 0.0f);
                addQuad(out, px + 18, py + 6, 2, 12, rgba(24, 12, 12, static_cast<uint8_t>(80 + dehumanisation_ * 110)), 0.0f);
            }
        } else if (c.tile == Tile::Port) {
            addTexturedQuad(out, px + 4, py + 6, 16, 12, rgba(50, 55, 58, 220), 0.0f, 10);
            addTexturedQuad(out, px + 6, py + 9, 12, 2, rgba(185, 145, 54, 120), 0.0f, 13);
            addQuad(out, px + 4, py + 18, 16, 2, rgba(31, 36, 38, 170), 0.0f);
            addQuad(out, px + 8, py + 5, 3, 14, rgba(96, 106, 107, 105), 0.0f);
            addQuad(out, px + 13, py + 6, 2 + c.level, 4, rgba(206, 214, 202, 75), 0.0f);
            if (dehumanisation_ > 0.30f) addQuad(out, px + 5, py + 5, 15, 1, rgba(128, 24, 24, static_cast<uint8_t>(80 + dehumanisation_ * 110)), 0.0f);
        }
        if (dehumanisation_ > 0.36f && (c.tile == Tile::Holding || c.tile == Tile::Refinery || c.tile == Tile::Port)) {
            addText(out, "X", px + 9, py + 8, 1.0f, rgba(118, 22, 22, static_cast<uint8_t>(75 + dehumanisation_ * 130)));
        }
        addActivityState(out, c, px, py);
        addBevel(out, px + 3, py + 3, TileSize - 6, TileSize - 6, rgba(220, 224, 208, 42), rgba(0, 0, 0, 115));
    }

    void addParkDetail(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        Color dark = rgba(31, 49, 34, 145);
        Color path = rgba(111, 112, 92, 105);
        addQuad(out, px + 3, py + 11, TileSize - 6, 2, path, 0.0f);
        addQuad(out, px + 11, py + 3, 2, TileSize - 6, path, 0.0f);
        for (int i = 0; i < 3; ++i) {
            float ox = 4.0f + static_cast<float>((c.variant * 5 + i * 7) % 14);
            float oy = 4.0f + static_cast<float>((c.variant * 3 + i * 9) % 14);
            addQuad(out, px + ox, py + oy, 4, 4, dark, 0.0f);
            addQuad(out, px + ox + 1, py + oy - 2, 2, 8, rgba(49, 72, 48, 120), 0.0f);
        }
    }

    void addWaterDetail(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        Color ripple = rgba(116, 133, 135, 65);
        addQuad(out, px + 3 + (c.variant % 3), py + 7, 12, 1.0f, ripple, 6.0f);
        addQuad(out, px + 9, py + 15 + (c.variant % 4), 10, 1.0f, ripple, 6.0f);
    }

    void addBuildingDetail(std::vector<Vertex>& out, const Cell& c, float px, float py, Color base) {
        float inset = 4.0f;
        float heightBias = static_cast<float>(c.level) * 1.6f;
        float w = TileSize - inset * 2.0f - static_cast<float>(c.variant % 3);
        float h = TileSize - inset * 2.0f - heightBias;
        float bx = px + inset + static_cast<float>((c.variant % 2) * 2);
        float by = py + inset + static_cast<float>((c.variant % 4));
        Color wall = mod(base, 0.86f);
        Color roof = mod(base, 1.05f);
        Color shadow = rgba(12, 13, 12, 95);

        addQuad(out, bx + 3, by + 4, w, h, shadow, 0.0f);
        addQuad(out, bx, by, w, h, wall, 7.0f);
        addQuad(out, bx + 1, by + 1, w - 2, 4, roof, 7.0f);
        addQuad(out, bx + w - 3, by + 2, 2, h - 3, mod(base, 0.62f), 7.0f);

        addQuad(out, bx + 3, by + 4, w - 6, 3, rgba(49, 45, 36, 120), 0.0f);
        addQuad(out, bx + 4, by + h - 5, w - 8, 2, rgba(151, 132, 91, 90), 0.0f);
    }

    void addText(std::vector<Vertex>& out, std::string text, float x, float y, float s, Color c) {
        const auto& gs = glyphs();
        float startX = x;
        for (char raw : text) {
            char ch = raw;
            if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
            if (ch == '\n') {
                y += 9.0f * s;
                x = startX;
                continue;
            }
            auto it = gs.find(ch);
            if (it == gs.end()) it = gs.find(' ');
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (it->second.rows[row][col] == '1') addQuad(out, x + col * s, y + row * s, s, s, c, 0.0f);
                }
            }
            x += 6.0f * s;
        }
    }

    void addAnalysisOverlay(std::vector<Vertex>& out, const Cell& c, float px, float py) {
        if (overlayMode_ == OverlayMode::Normal) return;

        if (overlayMode_ == OverlayMode::Resources) {
            float total = c.pink + c.fuel + c.gold + c.refined;
            if (total <= 0.08f) return;
            Color color = rgba(236, 112, 174, 120);
            float peak = c.pink;
            if (c.fuel > peak) { color = rgba(18, 18, 17, 180); peak = c.fuel; }
            if (c.gold > peak) { color = rgba(219, 196, 112, 145); peak = c.gold; }
            if (c.refined > peak) { color = rgba(238, 218, 148, 160); peak = c.refined; }
            color.a = std::min(0.62f, 0.18f + total * 0.035f);
            addTexturedQuad(out, px + 2, py + 2, TileSize - 4, TileSize - 4, color, 0.0f, 12);
            addQuad(out, px + 4, py + 4, (TileSize - 8) * clamp01(total / 18.0f), 2, rgba(232, 235, 220, 110), 0.0f);
            return;
        }

        if (overlayMode_ == OverlayMode::Flow) {
            if (c.recentFlow > 0.035f) {
                float a = std::min(0.68f, 0.18f + c.recentFlow * 0.55f);
                addTexturedQuad(out, px + 3, py + 3, TileSize - 6, TileSize - 6, Color{0.52f, 0.72f, 0.68f, a}, 0.0f, 12);
                addDirectionArrow(out, px, py, c.dir, rgba(206, 238, 222, 210));
            }
            if (c.blockedFlow > 0.035f) {
                float a = std::min(0.75f, 0.24f + c.blockedFlow * 0.60f);
                addTexturedQuad(out, px + 2, py + 2, TileSize - 4, TileSize - 4, Color{0.86f, 0.28f, 0.36f, a}, 0.0f, 15);
                addQuad(out, px + 5, py + 5, TileSize - 10, 2, rgba(246, 153, 164, 210), 0.0f);
                addQuad(out, px + 5, py + TileSize - 7, TileSize - 10, 2, rgba(246, 153, 164, 210), 0.0f);
            }
            return;
        }

        if (overlayMode_ == OverlayMode::Stress) {
            Color stress = rgba(218, 139, 154, 0);
            float amount = c.pollution * 0.55f;
            if (c.status == BuildingStatus::Starved) { stress = rgba(224, 182, 116, 150); amount = std::max(amount, 0.36f); }
            if (c.status == BuildingStatus::OutputBlocked || c.status == BuildingStatus::NoRail) { stress = rgba(218, 139, 154, 178); amount = std::max(amount, 0.48f); }
            if (c.status == BuildingStatus::Full) { stress = rgba(214, 166, 92, 150); amount = std::max(amount, 0.34f); }
            if (c.status == BuildingStatus::Suspended) { stress = rgba(176, 184, 176, 165); amount = std::max(amount, 0.42f); }
            if (amount <= 0.04f) return;
            stress.a = std::min(0.70f, amount);
            addTexturedQuad(out, px + 2, py + 2, TileSize - 4, TileSize - 4, stress, 0.0f, 15);
            addQuad(out, px + 4, py + TileSize - 6, (TileSize - 8) * clamp01(amount), 2, rgba(246, 210, 172, 180), 0.0f);
        }
    }

    void buildWorldBatch(std::vector<Vertex>& out) {
        out.clear();
        Vec2 topLeft = screenToWorld({0, 0});
        Vec2 bottomRight = screenToWorld({static_cast<float>(viewW_), static_cast<float>(viewH_)});
        int minX = std::max(0, static_cast<int>(topLeft.x / TileSize) - 2);
        int minY = std::max(0, static_cast<int>(topLeft.y / TileSize) - 2);
        int maxX = std::min(MapW - 1, static_cast<int>(bottomRight.x / TileSize) + 2);
        int maxY = std::min(MapH - 1, static_cast<int>(bottomRight.y / TileSize) + 2);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const Cell& c = cells_[index(x, y)];
                Color base = tileColor(c);
                float px = x * TileSize;
                float py = y * TileSize;
                float paper = 0.92f + static_cast<float>((x * 11 + y * 7 + c.variant) % 7) * 0.015f;
                addTexturedQuad(out, px + 1, py + 1, TileSize - 2, TileSize - 2, mod(base, paper), static_cast<float>(static_cast<int>(c.tile)), tileMaterial(c.tile));
                if (c.tile == Tile::Empty && ((x + y) % 11 == 0)) addQuad(out, px + 6, py + 6, 3, 1, rgba(115, 119, 111, 55), 0.0f);
                if (c.tile == Tile::Empty && ((x * 3 + y) % 17 == 0)) addQuad(out, px + 15, py + 14, 1, 4, rgba(34, 36, 34, 45), 0.0f);
                if (c.tile == Tile::Empty && dehumanisation_ < 0.20f && ((x * 5 + y * 7) % 29 == 0)) {
                    uint8_t alpha = static_cast<uint8_t>((0.20f - dehumanisation_) * 430.0f);
                    addQuad(out, px + 6, py + 6, 12, 1, rgba(156, 202, 166, alpha), 0.0f);
                    addQuad(out, px + 6, py + 6, 1, 12, rgba(156, 202, 166, alpha), 0.0f);
                }
                addTileBorder(out, px, py, rgba(22, 24, 22, 72));
                if (c.tile == Tile::Road) {
                    addRoadDetail(out, x, y, px, py);
                } else if (c.tile == Tile::Conveyor || c.tile == Tile::Rail) {
                    addQuad(out, px + 4, py + 4, TileSize - 4, TileSize - 4, rgba(0, 0, 0, 40), 0.0f);
                    Color stripe = c.tile == Tile::Rail ? rgba(150, 144, 126, 150) : rgba(210, 92, 150, 115);
                    if (isTransport(x - 1, y) || isTransport(x + 1, y)) {
                        addQuad(out, px + 2, py + 10, TileSize - 4, 2, stripe, 0.0f);
                        if (c.tile == Tile::Rail) {
                            addQuad(out, px + 2, py + 6, TileSize - 4, 1, rgba(88, 86, 78, 160), 0.0f);
                            addQuad(out, px + 2, py + 17, TileSize - 4, 1, rgba(88, 86, 78, 160), 0.0f);
                        }
                    }
                    if (isTransport(x, y - 1) || isTransport(x, y + 1)) {
                        addQuad(out, px + 10, py + 2, 2, TileSize - 4, stripe, 0.0f);
                        if (c.tile == Tile::Rail) {
                            addQuad(out, px + 6, py + 2, 1, TileSize - 4, rgba(88, 86, 78, 160), 0.0f);
                            addQuad(out, px + 17, py + 2, 1, TileSize - 4, rgba(88, 86, 78, 160), 0.0f);
                        }
                    }
                    addDirectionArrow(out, px, py, c.dir, c.tile == Tile::Rail ? rgba(220, 209, 166, 180) : rgba(236, 112, 174, 190));
                    addBevel(out, px + 2, py + 2, TileSize - 4, TileSize - 4, rgba(220, 224, 208, 32), rgba(0, 0, 0, 95));
                    if (c.recentFlow > 0.04f) {
                        Color flowColor = c.refined > 0.05f ? rgba(238, 218, 148, 210) :
                                          c.gold > 0.05f ? rgba(219, 196, 112, 210) :
                                          c.fuel > 0.05f ? rgba(18, 18, 17, 230) :
                                          rgba(236, 112, 174, 210);
                        Vec2 d = dirVector(c.dir);
                        float pulse = std::fmod(time_ * 3.2f + static_cast<float>((x * 13 + y * 7) % 10) * 0.1f, 1.0f);
                        float ox = d.x * (pulse - 0.5f) * 13.0f;
                        float oy = d.y * (pulse - 0.5f) * 13.0f;
                        addQuad(out, px + 10 + ox, py + 10 + oy, 4, 4, flowColor, 0.0f);
                    }
                    if (c.blockedFlow > 0.04f) {
                        addQuad(out, px + 4, py + 4, TileSize - 8, 2, rgba(218, 139, 154, 190), 0.0f);
                        addQuad(out, px + 4, py + TileSize - 6, TileSize - 8, 2, rgba(218, 139, 154, 190), 0.0f);
                    }
                    addMaterialPacket(out, c, px, py);
                } else if (c.tile == Tile::Block || c.tile == Tile::Holding || c.tile == Tile::Mine || c.tile == Tile::Factory || c.tile == Tile::Refinery || c.tile == Tile::Port) {
                    addZoneDetail(out, c, px, py);
                    addDirectionArrow(out, px, py, c.dir, rgba(224, 230, 214, 105));
                    addRequirementMarks(out, c, px, py);
                    if (isProductionTile(c.tile) || c.tile == Tile::Holding) {
                        addQuad(out, px + TileSize - 6, py + 3, 3, 3, statusColor(c.status), 0.0f);
                    }
                    addMaterialPacket(out, c, px, py);
                }
                if (c.pink > 0.15f) addTexturedQuad(out, px + 3, py + 19, std::min(18.0f, c.pink * 1.3f), 2, rgba(236, 105, 171, 180), 0.0f, 5);
                if (c.fuel > 0.15f) addTexturedQuad(out, px + 3, py + 16, std::min(18.0f, c.fuel * 1.3f), 2, rgba(5, 5, 5, 190), 0.0f, 7);
                if (c.gold > 0.15f) addTexturedQuad(out, px + 3, py + 13, std::min(18.0f, c.gold * 1.3f), 2, rgba(210, 165, 55, 190), 0.0f, 13);
                if (c.refined > 0.15f) addTexturedQuad(out, px + 3, py + 10, std::min(18.0f, c.refined * 1.3f), 2, rgba(232, 215, 148, 190), 0.0f, 14);
                if (pollutionOverlay_ && c.pollution > 0.05f) {
                    addTexturedQuad(out, px + 2, py + 2, TileSize - 4, TileSize - 4, Color{0.40f, 0.34f, 0.20f, c.pollution * 0.34f}, 8.0f, 15);
                    addTexturedQuad(out, px + 5, py + 5, 4 + c.pollution * 10.0f, 2, rgba(84, 73, 49, 80), 8.0f, 15);
                }
                addAnalysisOverlay(out, c, px, py);
            }
        }

        Vec2 hover = screenToWorld(mouse_);
        int hx = static_cast<int>(hover.x / TileSize);
        int hy = static_cast<int>(hover.y / TileSize);
        if (inside(hx, hy)) {
            bool valid = canPlaceToolAt(hx, hy);
            Color hoverColor = valid ? rgba(205, 214, 202, 210) : rgba(218, 139, 154, 220);
            if (painting_ && isRouteTool(tool_) && lastPaintX_ >= 0 && lastPaintY_ >= 0 && (hx != lastPaintX_ || hy != lastPaintY_)) {
                Direction routeDir = directionFromDelta(hx - lastPaintX_, hy - lastPaintY_);
                int sx = hx == lastPaintX_ ? 0 : (hx > lastPaintX_ ? 1 : -1);
                int sy = hy == lastPaintY_ ? 0 : (hy > lastPaintY_ ? 1 : -1);
                int cx = lastPaintX_;
                int cy = lastPaintY_;
                int guard = 0;
                while ((cx != hx || cy != hy) && guard++ < 160) {
                    if (std::abs(hx - cx) >= std::abs(hy - cy)) cx += sx;
                    else cy += sy;
                    if (!inside(cx, cy)) break;
                    bool stepValid = canPlaceToolAt(cx, cy);
                    Color pathColor = toolColor(tool_);
                    pathColor.a = stepValid ? 0.28f : 0.18f;
                    addTexturedQuad(out, cx * TileSize + 4, cy * TileSize + 4, TileSize - 8, TileSize - 8, pathColor, 0.0f, tileMaterial(toolTile(tool_)));
                    addDirectionArrow(out, cx * TileSize, cy * TileSize, routeDir, stepValid ? rgba(245, 247, 230, 150) : rgba(218, 139, 154, 150));
                }
            }
            Color preview = tool_ == Tool::Erase ? rgba(130, 68, 76, valid ? 92 : 48) : toolColor(tool_);
            preview.a = valid ? 0.34f : 0.22f;
            addTexturedQuad(out, hx * TileSize + 2, hy * TileSize + 2, TileSize - 4, TileSize - 4, preview, 0.0f, tileMaterial(toolTile(tool_)));
            addQuad(out, hx * TileSize, hy * TileSize, TileSize, 2, hoverColor, 0.0f);
            addQuad(out, hx * TileSize, (hy + 1) * TileSize - 2, TileSize, 2, hoverColor, 0.0f);
            addQuad(out, hx * TileSize, hy * TileSize, 2, TileSize, hoverColor, 0.0f);
            addQuad(out, (hx + 1) * TileSize - 2, hy * TileSize, 2, TileSize, hoverColor, 0.0f);
            if (tool_ != Tool::Erase) {
                addDirectionArrow(out, hx * TileSize, hy * TileSize, placementDir_, valid ? rgba(245, 247, 230, 190) : rgba(218, 139, 154, 180));
                Vec2 d = dirVector(placementDir_);
                int ox = hx + static_cast<int>(d.x);
                int oy = hy + static_cast<int>(d.y);
                if (inside(ox, oy)) {
                    addQuad(out, ox * TileSize + 7, oy * TileSize + 7, TileSize - 14, 2, rgba(245, 247, 230, 90), 0.0f);
                    addQuad(out, ox * TileSize + 7, oy * TileSize + TileSize - 9, TileSize - 14, 2, rgba(245, 247, 230, 90), 0.0f);
                    addQuad(out, ox * TileSize + 7, oy * TileSize + 7, 2, TileSize - 14, rgba(245, 247, 230, 90), 0.0f);
                    addQuad(out, ox * TileSize + TileSize - 9, oy * TileSize + 7, 2, TileSize - 14, rgba(245, 247, 230, 90), 0.0f);
                }
            }
            std::string reason;
            canPlaceToolAt(hx, hy, &reason);
            if (!valid && !reason.empty()) addText(out, reason, hx * TileSize + 2, hy * TileSize - 9, 1.0f, rgba(218, 139, 154, 210));
        }
    }

    std::string toolName() const {
        int stage = driftStage();
        switch (tool_) {
            case Tool::Road: return "ROAD";
            case Tool::Conveyor: return "CONVEYOR";
            case Tool::Rail: return "RAIL";
            case Tool::Block: return stage >= 2 ? "SOURCE" : "BLOCK";
            case Tool::Holding: return stage >= 2 ? "CASE HOLD" : (stage == 1 ? "HOLD AREA" : "HOLDING");
            case Tool::Mine: return "MINE";
            case Tool::Factory: return "FACTORY";
            case Tool::Refinery: return stage >= 2 ? "DISPOSITION" : (stage == 1 ? "PROCESSING" : "REFINERY");
            case Tool::Port: return stage >= 2 ? "REMOVAL" : (stage == 1 ? "TRANSFER" : "PORT");
            case Tool::Erase: return "ERASE";
        }
        return "";
    }

    std::string toolLabel(Tool tool) const {
        int stage = driftStage();
        switch (tool) {
            case Tool::Road: return "ROAD";
            case Tool::Conveyor: return "BELT";
            case Tool::Rail: return "RAIL";
            case Tool::Block: return stage >= 2 ? "SRC" : "BLOCK";
            case Tool::Holding: return stage >= 2 ? "CASE" : (stage == 1 ? "AREA" : "HOLD");
            case Tool::Mine: return "MINE";
            case Tool::Factory: return "FACT";
            case Tool::Refinery: return stage >= 2 ? "DISP" : (stage == 1 ? "PROC" : "REF");
            case Tool::Port: return stage >= 2 ? "RMVL" : (stage == 1 ? "TRAN" : "PORT");
            case Tool::Erase: return "ERASE";
        }
        return "";
    }

    std::string toolKey(Tool tool) const {
        switch (tool) {
            case Tool::Erase: return "0";
            default: return std::to_string(static_cast<int>(tool));
        }
    }

    std::string overlayName() const {
        switch (overlayMode_) {
            case OverlayMode::Normal: return "NORMAL";
            case OverlayMode::Resources: return "STOCK";
            case OverlayMode::Flow: return "FLOW";
            case OverlayMode::Stress: return "STRESS";
        }
        return "NORMAL";
    }

    Color toolColor(Tool tool) const {
        switch (tool) {
            case Tool::Road: return rgba(83, 86, 82);
            case Tool::Conveyor: return rgba(214, 92, 150);
            case Tool::Rail: return rgba(168, 162, 140);
            case Tool::Block: return rgba(225, 112, 171);
            case Tool::Holding: return rgba(118, 112, 124);
            case Tool::Mine: return rgba(28, 28, 27);
            case Tool::Factory: return rgba(171, 134, 48);
            case Tool::Refinery: return rgba(216, 172, 58);
            case Tool::Port: return rgba(102, 113, 118);
            case Tool::Erase: return rgba(130, 68, 76);
        }
        return rgba(200, 200, 200);
    }

    std::string tileName(Tile tile) const {
        int stage = driftStage();
        switch (tile) {
            case Tile::Road: return "ROAD";
            case Tile::Conveyor: return "CONVEYOR";
            case Tile::Rail: return "RAIL";
            case Tile::Block: return stage >= 2 ? "SOURCE" : "BLOCK";
            case Tile::Holding: return stage >= 2 ? "CASE HOLD" : (stage == 1 ? "HOLD AREA" : "HOLDING");
            case Tile::Mine: return "MINE";
            case Tile::Factory: return "FACTORY";
            case Tile::Refinery: return stage >= 2 ? "DISPOSITION" : (stage == 1 ? "PROCESSING" : "REFINERY");
            case Tile::Port: return stage >= 2 ? "REMOVAL" : (stage == 1 ? "TRANSFER" : "PORT");
            default: return "UNASSIGNED";
        }
    }

    std::string recipeText(Tile tile) const {
        int stage = driftStage();
        std::string m = materialTerm();
        switch (tile) {
            case Tile::Block: return "OUT " + m;
            case Tile::Mine: return "IN " + m + " OUT FUEL";
            case Tile::Factory: return "IN " + m + "+FUEL OUT GOLD";
            case Tile::Refinery: return stage >= 2 ? "DISPOSITION OFFICE" : (stage == 1 ? "IN GOLD+FUEL PROCESS" : "IN GOLD+FUEL OUT REF");
            case Tile::Port: return stage >= 2 ? "EXIT AUTHORITY" : (stage == 1 ? "IN REF TRANSFER" : "IN REF EXPORT");
            case Tile::Holding: return stage >= 2 ? "CASES HELD" : (stage == 1 ? "TEMPORARY HOLD" : "BUFFER ALL");
            case Tile::Conveyor: return "DIRECTIONAL ROUTE";
            case Tile::Rail: return stage >= 1 ? "TRANSFER ROUTE" : "EXPORT ROUTE";
            case Tile::Road: return "LOW VOLUME ROUTE";
            default: return "";
        }
    }

    void addPanel(std::vector<Vertex>& out, float x, float y, float w, float h, Color fill) {
        addTexturedQuad(out, x, y, w, h, fill, 0.0f, 11);
        addQuad(out, x + 2, y + 2, std::max(0.0f, w - 4), std::max(0.0f, h - 4), rgba(0, 0, 0, 72), 0.0f);
        addQuad(out, x, y, w, 1, rgba(210, 218, 204, 55), 0.0f);
        addQuad(out, x, y + h - 1, w, 1, rgba(0, 0, 0, 130), 0.0f);
        addQuad(out, x, y, 1, h, rgba(210, 218, 204, 45), 0.0f);
        addQuad(out, x + w - 1, y, 1, h, rgba(0, 0, 0, 160), 0.0f);
    }

    void addMetric(std::vector<Vertex>& out, const std::string& label, const std::string& value, float amount, float x, float y, Color accent) {
        addText(out, label, x, y, 1.55f, rgba(154, 163, 155));
        addText(out, value, x + 170, y - 1, 1.75f, accent);
        addTexturedQuad(out, x, y + 22, 252, 7, rgba(26, 29, 28, 220), 0.0f, 11);
        addTexturedQuad(out, x, y + 22, 252 * clamp01(amount), 7, accent, 0.0f, 12);
        addQuad(out, x, y + 29, 252, 1, rgba(214, 220, 210, 35), 0.0f);
    }

    void addObjectiveLine(std::vector<Vertex>& out, int step, float x, float y, bool active) {
        bool done = objectiveComplete(step);
        Color mark = done ? rgba(158, 205, 166) : (active ? rgba(219, 196, 112) : rgba(76, 84, 78));
        addQuad(out, x, y + 2, 8, 8, mark, 0.0f);
        if (done) {
            addQuad(out, x + 2, y + 5, 2, 5, rgba(8, 10, 10), 0.0f);
            addQuad(out, x + 4, y + 4, 5, 2, rgba(8, 10, 10), 0.0f);
        }
        addText(out, objectiveText(step), x + 18, y, 1.05f, done ? rgba(160, 170, 162) : (active ? rgba(232, 236, 226) : rgba(118, 128, 122)));
    }

    void addToolIcon(std::vector<Vertex>& out, Tool tool, float x, float y, Color accent) {
        addTexturedQuad(out, x, y, 28, 28, rgba(18, 20, 20, 210), 0.0f, 11);
        addTexturedQuad(out, x + 1, y + 1, 26, 26, rgba(40, 43, 42, 155), 0.0f, 12);
        switch (tool) {
            case Tool::Road:
                addQuad(out, x + 6, y + 12, 16, 4, accent, 0.0f);
                addQuad(out, x + 8, y + 14, 3, 1, rgba(220, 216, 188), 0.0f);
                addQuad(out, x + 16, y + 14, 3, 1, rgba(220, 216, 188), 0.0f);
                break;
            case Tool::Conveyor:
                addQuad(out, x + 5, y + 13, 18, 3, accent, 0.0f);
                addQuad(out, x + 18, y + 9, 3, 11, accent, 0.0f);
                break;
            case Tool::Rail:
                addQuad(out, x + 5, y + 8, 18, 2, accent, 0.0f);
                addQuad(out, x + 5, y + 18, 18, 2, accent, 0.0f);
                addQuad(out, x + 8, y + 7, 2, 14, rgba(70, 70, 64), 0.0f);
                addQuad(out, x + 17, y + 7, 2, 14, rgba(70, 70, 64), 0.0f);
                break;
            case Tool::Block:
                addQuad(out, x + 8, y + 8, 12, 12, accent, 0.0f);
                addQuad(out, x + 12, y + 5, 4, 18, rgba(242, 160, 199, 160), 0.0f);
                break;
            case Tool::Holding:
                addQuad(out, x + 6, y + 6, 16, 16, rgba(28, 28, 30), 0.0f);
                addQuad(out, x + 8, y + 11, 12, 2, accent, 0.0f);
                addQuad(out, x + 8, y + 17, 12, 2, accent, 0.0f);
                break;
            case Tool::Mine:
                addQuad(out, x + 6, y + 6, 16, 16, accent, 0.0f);
                addQuad(out, x + 10, y + 10, 8, 8, rgba(76, 70, 60), 0.0f);
                break;
            case Tool::Factory:
                addQuad(out, x + 5, y + 12, 18, 9, accent, 0.0f);
                addQuad(out, x + 8, y + 8, 4, 6, rgba(42, 38, 30), 0.0f);
                addQuad(out, x + 18, y + 5, 3, 16, rgba(42, 38, 30), 0.0f);
                break;
            case Tool::Refinery:
                addQuad(out, x + 5, y + 9, 18, 12, accent, 0.0f);
                addQuad(out, x + 9, y + 5, 3, 18, rgba(52, 44, 34), 0.0f);
                addQuad(out, x + 16, y + 5, 3, 18, rgba(52, 44, 34), 0.0f);
                break;
            case Tool::Port:
                addQuad(out, x + 5, y + 8, 18, 13, accent, 0.0f);
                addQuad(out, x + 8, y + 12, 12, 2, rgba(220, 178, 72), 0.0f);
                break;
            case Tool::Erase:
                addQuad(out, x + 7, y + 7, 14, 3, accent, 0.0f);
                addQuad(out, x + 7, y + 18, 14, 3, accent, 0.0f);
                addQuad(out, x + 12, y + 8, 4, 10, accent, 0.0f);
                break;
        }
    }

    Color minimapColor(const Cell& c) const {
        if (c.blockedFlow > 0.06f) return rgba(218, 139, 154, 230);
        if (c.recentFlow > 0.06f) return rgba(158, 205, 166, 210);
        if (c.refined > 0.2f) return rgba(238, 218, 148, 225);
        if (c.gold > 0.2f) return rgba(219, 196, 112, 220);
        if (c.fuel > 0.2f) return rgba(18, 18, 17, 235);
        if (c.pink > 0.2f) return rgba(236, 112, 174, 220);
        switch (c.tile) {
            case Tile::Road: return rgba(76, 78, 73, 180);
            case Tile::Conveyor: return rgba(142, 71, 108, 190);
            case Tile::Rail: return rgba(152, 146, 122, 200);
            case Tile::Block: return rgba(183, 84, 138, 210);
            case Tile::Holding: return rgba(110, 108, 116, 200);
            case Tile::Mine: return rgba(20, 20, 19, 230);
            case Tile::Factory: return rgba(155, 119, 42, 220);
            case Tile::Refinery: return rgba(196, 148, 45, 230);
            case Tile::Port: return rgba(82, 94, 98, 220);
            default: return rgba(47, 50, 48, 160);
        }
    }

    void addMiniMap(std::vector<Vertex>& out, float x, float y, float size) {
        addPanel(out, x - 8, y - 22, size + 16, size + 30, rgba(9, 11, 12, 222));
        addText(out, "OVERVIEW", x, y - 15, 1.0f, rgba(154, 163, 155));
        float s = size / static_cast<float>(MapW);
        for (int gy = 0; gy < MapH; gy += 2) {
            for (int gx = 0; gx < MapW; gx += 2) {
                const Cell& c = cells_[index(gx, gy)];
                addQuad(out, x + gx * s, y + gy * s, s * 2.0f + 0.4f, s * 2.0f + 0.4f, minimapColor(c), 0.0f);
            }
        }
        Vec2 topLeft = screenToWorld({0, 0});
        Vec2 bottomRight = screenToWorld({static_cast<float>(viewW_), static_cast<float>(viewH_)});
        float vx = x + clamp01(topLeft.x / (MapW * TileSize)) * size;
        float vy = y + clamp01(topLeft.y / (MapH * TileSize)) * size;
        float vw = std::max(5.0f, (bottomRight.x - topLeft.x) / (MapW * TileSize) * size);
        float vh = std::max(5.0f, (bottomRight.y - topLeft.y) / (MapH * TileSize) * size);
        addQuad(out, vx, vy, vw, 1.0f, rgba(232, 235, 220, 180), 0.0f);
        addQuad(out, vx, vy + vh, vw, 1.0f, rgba(232, 235, 220, 180), 0.0f);
        addQuad(out, vx, vy, 1.0f, vh, rgba(232, 235, 220, 180), 0.0f);
        addQuad(out, vx + vw, vy, 1.0f, vh, rgba(232, 235, 220, 180), 0.0f);
    }

    float nightAmount() const {
        if (manualNight_) return 0.82f;
        float day = std::sin(time_ * 0.035f) * 0.5f + 0.5f;
        return smoothstep(0.48f, 0.04f, day);
    }

    static float smoothstep(float edge0, float edge1, float x) {
        float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    void renderBatch(const std::vector<Vertex>& verts, Mat4 proj) {
        glUseProgram(tileProgram_);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, atlasTex_);
        glUniform1i(glGetUniformLocation(tileProgram_, "uAtlas"), 1);
        glUniformMatrix4fv(glGetUniformLocation(tileProgram_, "uProj"), 1, GL_FALSE, proj.m);
        glUniform1f(glGetUniformLocation(tileProgram_, "uTime"), time_);
        glUniform1f(glGetUniformLocation(tileProgram_, "uNight"), nightAmount());
        glUniform1f(glGetUniformLocation(tileProgram_, "uFog"), fog_ ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(tileProgram_, "uUgly"), dehumanisation_);
        glBindVertexArray(tileVao_);
        glBindBuffer(GL_ARRAY_BUFFER, tileVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));
    }

    void renderWorld() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, viewW_, viewH_);
        float flicker = std::sin(time_ * 5.7f) * 0.003f + std::sin(time_ * 13.1f) * 0.002f;
        float hope = 1.0f - dehumanisation_;
        glClearColor(0.060f + hope * 0.030f + flicker, 0.066f + hope * 0.040f + flicker, 0.066f + hope * 0.030f + flicker, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        buildWorldBatch(worldVerts_);
        float halfW = viewW_ * 0.5f / zoom_;
        float halfH = viewH_ * 0.5f / zoom_;
        Mat4 worldProj = ortho(camera_.x - halfW, camera_.x + halfW, camera_.y + halfH, camera_.y - halfH);
        renderBatch(worldVerts_, worldProj);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, viewW_, viewH_);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(postProgram_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fboTex_);
        glUniform1i(glGetUniformLocation(postProgram_, "uScene"), 0);
        glUniform1f(glGetUniformLocation(postProgram_, "uTime"), time_);
        glUniform1f(glGetUniformLocation(postProgram_, "uNight"), nightAmount());
        glUniform1f(glGetUniformLocation(postProgram_, "uPollution"), pollutionOverlay_ ? pollutionAverage_ : 0.0f);
        glUniform1f(glGetUniformLocation(postProgram_, "uUgly"), dehumanisation_);
        glBindVertexArray(screenVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void renderUi() {
        uiVerts_.clear();
        const float topH = topBarHeight();
        const float rightW = rightPanelWidth();
        const float dockH = dockHeight();
        const float rightX = static_cast<float>(viewW_) - rightW;
        const float dockW = dockWidth();

        if (mode_ == GameMode::Title) {
            addTitleMenu(uiVerts_);
            Mat4 uiProj = ortho(0.0f, static_cast<float>(viewW_), static_cast<float>(viewH_), 0.0f);
            renderBatch(uiVerts_, uiProj);
            return;
        }

        addPanel(uiVerts_, 0, 0, static_cast<float>(viewW_), topH, rgba(8, 10, 12, 232));
        addText(uiVerts_, "ADMINISTRATIVE GRID", 20, 16, 2.05f, rgba(226, 232, 222));
        std::string modeLabel = mode_ == GameMode::Paused ? "PAUSED" : (mode_ == GameMode::End ? "CLOSED" : "RUNNING");
        addText(uiVerts_, modeLabel, 300, 18, 1.45f, mode_ == GameMode::End ? rgba(218, 139, 154) : (mode_ == GameMode::Paused ? rgba(222, 154, 154) : rgba(158, 205, 166)));
        addPanel(uiVerts_, 408, 12, 196, 30, rgba(18, 21, 21, 220));
        addText(uiVerts_, "TOOL", 422, 21, 1.15f, rgba(126, 136, 128));
        addText(uiVerts_, toolName(), 482, 20, 1.45f, toolColor(tool_));
        addPanel(uiVerts_, 620, 12, 120, 30, rgba(18, 21, 21, 220));
        addText(uiVerts_, "DIR", 634, 21, 1.15f, rgba(126, 136, 128));
        addText(uiVerts_, directionName(placementDir_), 680, 20, 1.35f, rgba(190, 198, 184));
        addPanel(uiVerts_, 756, 12, 156, 30, rgba(18, 21, 21, 220));
        addText(uiVerts_, "VIEW", 770, 21, 1.15f, rgba(126, 136, 128));
        addText(uiVerts_, overlayName(), 826, 20, 1.35f, rgba(205, 214, 202));

        addPanel(uiVerts_, rightX, topH, rightW, static_cast<float>(viewH_) - topH, rgba(5, 6, 7, 238));
        addText(uiVerts_, "LEDGER", rightX + 24, topH + 22, 1.9f, rgba(226, 232, 222));
        addText(uiVerts_, "FLOW AUDIT", rightX + 202, topH + 25, 1.15f, rgba(126, 136, 128));

        std::ostringstream money;
        money << "$" << static_cast<int>(budget_);
        float metricX = rightX + 24.0f;
        addMetric(uiVerts_, "CASH", money.str(), clamp01((budget_ + 300.0f) / 1600.0f), metricX, topH + 62, rgba(219, 196, 112));
        addMetric(uiVerts_, "STABILITY", std::to_string(static_cast<int>(stability_ * 100)) + "%", stability_, metricX, topH + 110, rgba(176, 204, 177));
        addMetric(uiVerts_, "PURITY", std::to_string(static_cast<int>(purity_ * 100)) + "%", purity_, metricX, topH + 158, rgba(200, 207, 198));
        addMetric(uiVerts_, "VARIANCE", std::to_string(static_cast<int>(variance_ * 100)) + "%", variance_, metricX, topH + 206, rgba(218, 139, 154));
        addMetric(uiVerts_, transferMetricName(), std::to_string(static_cast<int>(exported_)), clamp01(exported_ / exportGoal_), metricX, topH + 254, rgba(219, 196, 112));
        if (dehumanisation_ > 0.08f) {
            addText(uiVerts_, "CASE DRIFT " + std::to_string(static_cast<int>(dehumanisation_ * 100.0f)) + "%", metricX, topH + 296, 0.9f, dehumanisation_ > 0.45f ? rgba(218, 139, 154) : rgba(126, 136, 128));
        }

        addPanel(uiVerts_, rightX + 18, topH + 318, rightW - 36, 72, rgba(10, 12, 12, 236));
        addText(uiVerts_, "THROUGHPUT", rightX + 34, topH + 334, 1.35f, rgba(164, 172, 164));
        addText(uiVerts_, std::to_string(static_cast<int>(networkFlow_ * 10.0f)) + "/STEP", rightX + 190, topH + 334, 1.35f, rgba(205, 214, 202));
        addText(uiVerts_, "BLOCKED", rightX + 34, topH + 362, 1.1f, rgba(126, 136, 128));
        addText(uiVerts_, std::to_string(static_cast<int>(blockedFlow_ * 10.0f)), rightX + 190, topH + 362, 1.1f, blockedFlow_ > 0.05f ? rgba(218, 139, 154) : rgba(126, 136, 128));
        addText(uiVerts_, "EFF " + std::to_string(static_cast<int>(routeEfficiency_ * 100.0f)) + "%  AUDIT " + std::to_string(static_cast<int>(auditPressure_ * 100.0f)) + "%", rightX + 34, topH + 380, 0.9f, auditPressure_ > 0.35f ? rgba(218, 139, 154) : rgba(126, 136, 128));
        addQuad(uiVerts_, rightX + 260, topH + 335, 10, 10, overlayMode_ == OverlayMode::Flow ? rgba(158, 205, 166) : rgba(66, 74, 70), 0.0f);
        addQuad(uiVerts_, rightX + 276, topH + 335, 10, 10, blockedFlow_ > 0.05f ? rgba(218, 139, 154) : rgba(66, 74, 70), 0.0f);

        addPanel(uiVerts_, rightX + 18, topH + 404, rightW - 36, 74, rgba(10, 12, 12, 236));
        addText(uiVerts_, "OPERATIONS", rightX + 34, topH + 420, 1.25f, rgba(164, 172, 164));
        addText(uiVerts_, "RUN " + std::to_string(runningCount_) + "   STARVE " + std::to_string(starvedCount_), rightX + 34, topH + 448, 1.05f, starvedCount_ > 0 ? rgba(224, 182, 116) : rgba(190, 198, 184));
        addText(uiVerts_, "BLOCK " + std::to_string(blockedCount_) + "   NORAIL " + std::to_string(noRailCount_) + "   HOLD " + std::to_string(suspendedCount_), rightX + 34, topH + 468, 0.95f, (blockedCount_ + noRailCount_ + suspendedCount_) > 0 ? rgba(218, 139, 154) : rgba(126, 136, 128));

        addPanel(uiVerts_, rightX + 18, topH + 492, rightW - 36, 126, rgba(10, 12, 12, 238));
        addText(uiVerts_, "DIRECTIVE", rightX + 34, topH + 506, 1.25f, rgba(176, 184, 176));
        addText(uiVerts_, goalComplete_ ? "VERIFIED" : "PENDING", rightX + 198, topH + 507, 1.0f, goalComplete_ ? rgba(219, 196, 112) : rgba(126, 136, 128));
        int activeStep = activeObjective();
        addObjectiveLine(uiVerts_, 0, rightX + 34, topH + 532, activeStep == 0);
        addObjectiveLine(uiVerts_, 1, rightX + 34, topH + 552, activeStep == 1);
        addObjectiveLine(uiVerts_, 2, rightX + 34, topH + 572, activeStep == 2);
        addObjectiveLine(uiVerts_, 3, rightX + 34, topH + 592, activeStep == 3);
        addObjectiveLine(uiVerts_, 4, rightX + 174, topH + 592, activeStep == 4);
        addTexturedQuad(uiVerts_, rightX + 238, topH + 510, 48, 6, rgba(26, 29, 28, 220), 0.0f, 11);
        addTexturedQuad(uiVerts_, rightX + 238, topH + 510, 48 * clamp01(exported_ / exportGoal_), 6, rgba(219, 196, 112), 0.0f, 12);

        Vec2 hover = screenToWorld(mouse_);
        int hx = static_cast<int>(hover.x / TileSize);
        int hy = static_cast<int>(hover.y / TileSize);
        addPanel(uiVerts_, rightX + 18, topH + 632, rightW - 36, 92, rgba(10, 12, 12, 238));
        addText(uiVerts_, "INSPECT", rightX + 34, topH + 646, 1.15f, rgba(176, 184, 176));
        if (inside(hx, hy)) {
            const Cell& c = cells_[index(hx, hy)];
            addText(uiVerts_, tileName(c.tile), rightX + 34, topH + 670, 1.2f, rgba(226, 232, 222));
            std::string dir = (c.tile == Tile::Empty) ? "" : " " + directionName(c.dir);
            std::string recipe = recipeText(c.tile);
            addText(uiVerts_, recipe.empty() ? "NO RECIPE" : recipe, rightX + 34, topH + 692, 0.85f, rgba(150, 160, 151));
            addText(uiVerts_, "P" + std::to_string(static_cast<int>(c.pink)) + " F" + std::to_string(static_cast<int>(c.fuel)) + " G" + std::to_string(static_cast<int>(c.gold)) + " R" + std::to_string(static_cast<int>(c.refined)) + dir, rightX + 34, topH + 710, 0.82f, rgba(180, 190, 181));
            if (isProductionTile(c.tile) || c.tile == Tile::Holding) {
                addText(uiVerts_, statusName(c.status), rightX + 196, topH + 710, 0.82f, statusColor(c.status));
            } else if (c.tile == Tile::Conveyor || c.tile == Tile::Rail || c.tile == Tile::Road) {
                addText(uiVerts_, "FLOW " + std::to_string(static_cast<int>(c.recentFlow * 100.0f)) + "  BLOCK " + std::to_string(static_cast<int>(c.blockedFlow * 100.0f)), rightX + 196, topH + 710, 0.82f, c.blockedFlow > 0.05f ? rgba(218, 139, 154) : rgba(158, 205, 166));
            } else {
                std::string reason;
                bool valid = canPlaceToolAt(hx, hy, &reason);
                addText(uiVerts_, valid ? "SITE READY" : reason, rightX + 196, topH + 710, 0.82f, valid ? rgba(158, 205, 166) : rgba(218, 139, 154));
            }
        } else {
            addText(uiVerts_, "NO CELL", rightX + 34, topH + 670, 1.2f, rgba(126, 136, 128));
        }

        addPanel(uiVerts_, 16, static_cast<float>(viewH_) - dockH - 16, dockW, dockH, rgba(8, 10, 12, 224));
        addText(uiVerts_, "CONSTRUCTION", 34, static_cast<float>(viewH_) - dockH + 2, 1.45f, rgba(154, 163, 155));
        float bx = 34.0f;
        float by = static_cast<float>(viewH_) - 104.0f;
        const float bw = 72.0f;
        Tool hoverTool = tool_;
        bool hoveringTool = toolAtUi(mouse_, hoverTool);
        for (Tool tool : toolOrder()) {
            bool selected = tool == tool_;
            bool hovered = hoveringTool && hoverTool == tool;
            Color accent = toolColor(tool);
            addPanel(uiVerts_, bx, by, bw - 8, 58, selected ? rgba(34, 40, 38, 244) : (hovered ? rgba(22, 26, 25, 238) : rgba(14, 16, 16, 224)));
            if (selected || hovered) addQuad(uiVerts_, bx, by, bw - 8, selected ? 3 : 2, accent, 0.0f);
            addText(uiVerts_, toolKey(tool), bx + 7, by + 7, 1.25f, rgba(196, 204, 194));
            addToolIcon(uiVerts_, tool, bx + 25, by + 6, accent);
            addText(uiVerts_, toolLabel(tool), bx + 7, by + 41, 1.15f, selected ? rgba(230, 236, 226) : rgba(146, 156, 148));
            if (tool != Tool::Erase) addText(uiVerts_, "$" + std::to_string(static_cast<int>(toolCost(tool))), bx + 7, by + 29, 0.9f, rgba(150, 142, 105));
            bx += bw;
        }

        if (dockW > 880.0f) {
            addMiniMap(uiVerts_, 16.0f + dockW - 98.0f, static_cast<float>(viewH_) - dockH + 26.0f, 74.0f);
        }

        addText(uiVerts_, "LMB BUILD   TAB VIEW   Q/E ROTATE   S SAVE   L LOAD   F FOG   P STAIN   N NIGHT   R RESET", 34, static_cast<float>(viewH_) - 24, 1.2f, rgba(132, 144, 136));

        float ay = 70.0f;
        for (const Alert& alert : alerts_) {
            addPanel(uiVerts_, 24, ay, 520, 30, rgba(8, 10, 12, 215));
            addText(uiVerts_, alert.text, 38, ay + 9, 1.2f, alert.color);
            ay += 36.0f;
        }

        if (auditReportTtl_ > 0.0f && !lastAuditReport_.empty()) {
            float rx = std::max(24.0f, static_cast<float>(viewW_) * 0.5f - 360.0f);
            float ry = topH + 16.0f;
            addPanel(uiVerts_, rx, ry, 620, 54, rgba(8, 10, 12, 230));
            addText(uiVerts_, "AUDIT REPORT", rx + 16, ry + 10, 1.0f, rgba(219, 196, 112));
            addText(uiVerts_, lastAuditReport_, rx + 16, ry + 30, 0.86f, rgba(205, 214, 202));
        }

        if (mode_ == GameMode::Paused) addPauseMenu(uiVerts_);
        if (mode_ == GameMode::End) addEndMenu(uiVerts_);
        Mat4 uiProj = ortho(0.0f, static_cast<float>(viewW_), static_cast<float>(viewH_), 0.0f);
        renderBatch(uiVerts_, uiProj);
    }

    void addMenuButton(std::vector<Vertex>& out, const std::string& text, float x, float y, float w, float h, bool active = false) {
        addPanel(out, x, y, w, h, active ? rgba(35, 40, 38, 244) : rgba(12, 14, 14, 242));
        addText(out, text, x + 18, y + 11, 1.15f, active ? rgba(230, 236, 226) : rgba(190, 198, 184));
    }

    void addTitleMenu(std::vector<Vertex>& out) {
        addQuad(out, 0, 0, static_cast<float>(viewW_), static_cast<float>(viewH_), rgba(0, 0, 0, 112), 0.0f);
        float cx = viewW_ * 0.5f - 260.0f;
        float y = viewH_ * 0.5f - 202.0f;
        addPanel(out, cx, y, 520, 404, rgba(5, 8, 7, 248));
        addText(out, "ADMINISTRATIVE GRID", cx + 42, y + 34, 2.25f, rgba(236, 242, 232));
        addText(out, "EMPTY LANDSCAPE", cx + 42, y + 82, 1.05f, rgba(156, 188, 160));
        addText(out, "ORGANISE FLOWS", cx + 220, y + 82, 1.05f, rgba(156, 188, 160));
        addText(out, "AWAIT AUDIT", cx + 382, y + 82, 1.05f, rgba(156, 188, 160));
        addPanel(out, cx + 42, y + 112, 436, 38, rgba(13, 20, 17, 242));
        addText(out, "START PRESET", cx + 60, y + 124, 1.05f, rgba(126, 136, 128));
        addText(out, presetName(), cx + 210, y + 123, 1.2f, rgba(219, 196, 112));
        float bx = viewW_ * 0.5f - 180.0f;
        float by = viewH_ * 0.5f - 36.0f;
        addMenuButton(out, "NEW ADMINISTRATION", bx, by, 360, 40, true);
        addMenuButton(out, "LOAD SAVE", bx, by + 52, 360, 40);
        addMenuButton(out, "PRESET PREV", bx, by + 104, 174, 40);
        addMenuButton(out, "PRESET NEXT", bx + 186, by + 104, 174, 40);
        addMenuButton(out, "QUIT", bx, by + 156, 360, 40);
        addText(out, "ENTER START   LEFT RIGHT PRESET   ESC QUIT", cx + 42, y + 368, 1.0f, rgba(126, 136, 128));
    }

    void addPauseMenu(std::vector<Vertex>& out) {
        addQuad(out, 0, 0, static_cast<float>(viewW_), static_cast<float>(viewH_), rgba(0, 0, 0, 118), 0.0f);
        float cx = viewW_ * 0.5f - 170.0f;
        float y = viewH_ * 0.5f - 142.0f;
        addPanel(out, cx, y, 340, 276, rgba(5, 6, 7, 246));
        addText(out, "PAUSE MENU", cx + 30, y + 24, 1.8f, rgba(232, 236, 226));
        addText(out, "AUDIT CLOCK HELD", cx + 30, y + 56, 0.9f, rgba(126, 136, 128));
        float bx = viewW_ * 0.5f - 140.0f;
        float by = viewH_ * 0.5f - 94.0f;
        addMenuButton(out, "RESUME", bx, by, 280, 32, true);
        addMenuButton(out, "SAVE GRID", bx, by + 40, 280, 32);
        addMenuButton(out, "LOAD GRID", bx, by + 80, 280, 32);
        addMenuButton(out, "NEW GRID", bx, by + 120, 280, 32);
        addMenuButton(out, "TITLE", bx, by + 160, 280, 32);
        addText(out, "ESC RESUME   S SAVE   L LOAD   R NEW", cx + 30, y + 246, 0.86f, rgba(126, 136, 128));
    }

    std::string endStateTitle() const {
        switch (endState_) {
            case EndState::Verified: return "VERIFICATION ENTERED";
            case EndState::Collapse: return "CONTROL UNRECONCILED";
            case EndState::Insolvency: return "LEDGER INSOLVENT";
            case EndState::PurityBreach: return "PURITY BREACH FILED";
            default: return "FILE CLOSED";
        }
    }

    std::array<std::string, 3> endStateBody() const {
        switch (endState_) {
            case EndState::Verified:
                return {"QUOTA SATISFIED. THE MAP ACCEPTS THE ROUTE.", "AUDIT NOTES: AUTHORITY REMAINS EXTERNAL.", "NO PERSON IS NAMED IN THE EXPORT RECORD."};
            case EndState::Collapse:
                return {"ROUTES CONTRADICT. HOLDINGS EXCEED FORM CAPACITY.", "THE GRID REMAINS VISIBLE BUT NO LONGER AGREES WITH ITSELF.", "RECONCILIATION REQUIRES AN OFFICE NOT PRESENT ON SITE."};
            case EndState::Insolvency:
                return {"CASH AUTHORITY EXHAUSTED. WORK ORDERS CONTINUE.", "THE LEDGER REFUSES FURTHER EXPLANATION.", "RESPONSIBILITY IS TRANSFERRED TO A FUTURE CLERK."};
            case EndState::PurityBreach:
                return {"PURITY STANDARD FAILED. SITE PLACED UNDER QUIET SEAL.", "EXPORT PERMISSION IS WITHHELD PENDING INDEPENDENT SOURCES.", "THE MATERIAL REMAINS WHERE THE PROCESS LEFT IT."};
            default:
                return {"THE FILE HAS NO CLOSING AUTHORITY.", "THE LANDSCAPE WAITS FOR A NEW ADMINISTRATION.", ""};
        }
    }

    void addEndMenu(std::vector<Vertex>& out) {
        addQuad(out, 0, 0, static_cast<float>(viewW_), static_cast<float>(viewH_), rgba(0, 0, 0, 138), 0.0f);
        float cx = viewW_ * 0.5f - 270.0f;
        float y = viewH_ * 0.5f - 176.0f;
        addPanel(out, cx, y, 540, 352, rgba(5, 6, 7, 248));
        addText(out, endStateTitle(), cx + 36, y + 30, 1.75f, endState_ == EndState::Verified ? rgba(219, 196, 112) : rgba(218, 139, 154));
        addText(out, "AUDIT TERMINAL", cx + 36, y + 66, 0.95f, rgba(126, 136, 128));
        auto body = endStateBody();
        addText(out, body[0], cx + 36, y + 106, 1.05f, rgba(205, 214, 202));
        addText(out, body[1], cx + 36, y + 132, 1.05f, rgba(176, 184, 176));
        if (!body[2].empty()) addText(out, body[2], cx + 36, y + 158, 1.05f, rgba(176, 184, 176));
        std::string figures = "CASH $" + std::to_string(static_cast<int>(cash_)) +
                              "   STABILITY " + std::to_string(static_cast<int>(stability_ * 100.0f)) + "%" +
                              "   VARIANCE " + std::to_string(static_cast<int>(variance_ * 100.0f)) + "%";
        addPanel(out, cx + 36, y + 196, 468, 36, rgba(12, 14, 14, 242));
        addText(out, figures, cx + 54, y + 208, 0.95f, rgba(190, 198, 184));
        float bx = viewW_ * 0.5f - 140.0f;
        float by = viewH_ * 0.5f + 62.0f;
        addMenuButton(out, "NEW GRID", bx, by, 280, 34, true);
        addMenuButton(out, "TITLE", bx, by + 44, 280, 34);
        addText(out, "ENTER NEW   T TITLE", cx + 36, y + 324, 0.9f, rgba(126, 136, 128));
    }

    void render() {
        renderWorld();
        renderUi();
        SDL_GL_SwapWindow(window_);
    }

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_ = nullptr;
    bool running_ = true;
    bool paused_ = false;
    GameMode mode_ = GameMode::Title;
    EndState endState_ = EndState::None;
    bool painting_ = false;
    bool dragging_ = false;
    bool zoomFocus_ = false;
    bool fog_ = true;
    bool pollutionOverlay_ = true;
    bool manualNight_ = false;
    int viewW_ = ScreenW;
    int viewH_ = ScreenH;
    uint32_t lastTicks_ = 0;
    float time_ = 0.0f;
    float simAccum_ = 0.0f;
    float zoom_ = 0.75f;
    float targetZoom_ = 0.75f;
    Vec2 camera_;
    Vec2 mouse_;
    Vec2 lastMouse_;
    Vec2 zoomFocusScreen_;
    Vec2 zoomFocusWorld_;
    Tool tool_ = Tool::Road;
    Direction placementDir_ = Direction::East;
    OverlayMode overlayMode_ = OverlayMode::Normal;
    int lastPaintX_ = -1;
    int lastPaintY_ = -1;
    int presetIndex_ = 0;
    int population_ = 0;
    int jobs_ = 0;
    float budget_ = 25000.0f;
    float pollutionAverage_ = 0.0f;
    float profit_ = 0.0f;
    float cash_ = 700.0f;
    float upkeep_ = 0.0f;
    float stability_ = 0.72f;
    float purity_ = 0.96f;
    float variance_ = 0.08f;
    float exported_ = 0.0f;
    float exportGoal_ = 50.0f;
    float networkFlow_ = 0.0f;
    float blockedFlow_ = 0.0f;
    float auditPressure_ = 0.0f;
    float routeEfficiency_ = 1.0f;
    float exportPenalty_ = 0.0f;
    float containmentCost_ = 0.0f;
    float dehumanisation_ = 0.0f;
    float auditReportClock_ = 0.0f;
    float auditReportTtl_ = 0.0f;
    float totalPink_ = 0.0f;
    float totalFuel_ = 0.0f;
    float totalGold_ = 0.0f;
    float totalRefined_ = 0.0f;
    int runningCount_ = 0;
    int starvedCount_ = 0;
    int blockedCount_ = 0;
    int noRailCount_ = 0;
    int fullCount_ = 0;
    int suspendedCount_ = 0;
    bool goalComplete_ = false;
    bool auditFailed_ = false;
    bool milestoneSeen_[5]{};
    std::string lastAuditReport_;
    std::mt19937 rng_;
    std::vector<Cell> cells_;
    std::vector<Vertex> worldVerts_;
    std::vector<Vertex> uiVerts_;
    std::vector<Alert> alerts_;

    GLuint tileProgram_ = 0;
    GLuint postProgram_ = 0;
    GLuint tileVao_ = 0;
    GLuint tileVbo_ = 0;
    GLuint screenVao_ = 0;
    GLuint screenVbo_ = 0;
    GLuint fbo_ = 0;
    GLuint fboTex_ = 0;
    GLuint atlasTex_ = 0;
};

} // namespace civic

int main(int, char**) {
    civic::Game game;
    if (!game.init()) return 1;
    game.run();
    game.shutdown();
    return 0;
}
