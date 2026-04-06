// ---------------------------------------------------------------------------
// smoke_mock.cpp — Full raylib + GLFW stubs for headless smoke testing.
// Links against main.cpp + all src/ files to exercise the full app without GPU.
// ---------------------------------------------------------------------------
#include "raylib.h"
#include "GLFW/glfw3.h"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

static std::chrono::steady_clock::time_point g_start_time;

static std::atomic<int> g_frame_counter{0};

// ---------------------------------------------------------------------------
// Raylib core stubs
// ---------------------------------------------------------------------------
extern "C" {

void SetConfigFlags(unsigned int flags) { (void)flags; }
void InitWindow(int width, int height, const char* title) {
    (void)width; (void)height; (void)title;
    g_start_time = std::chrono::steady_clock::now();
}
void CloseWindow(void) {}
void SetTargetFPS(int fps) { (void)fps; }
void SetExitKey(int key) { (void)key; }

bool WindowShouldClose(void) {
    ++g_frame_counter;
    auto elapsed = std::chrono::steady_clock::now() - g_start_time;
    return elapsed >= std::chrono::seconds(10);
}

void BeginDrawing(void) {}
void EndDrawing(void) {}
void ClearBackground(Color color) { (void)color; }

int GetCurrentMonitor(void) { return 0; }
int GetMonitorWidth(int monitor) { (void)monitor; return 1920; }
int GetMonitorHeight(int monitor) { (void)monitor; return 1080; }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowState(unsigned int flags) { (void)flags; }

const char* GetApplicationDirectory(void) { return "./"; }

// Input stubs — simulate ESC press after 1 second of wall-clock time
bool IsKeyPressed(int key) {
    if (key == 256 /* KEY_ESCAPE */) {
        auto elapsed = std::chrono::steady_clock::now() - g_start_time;
        if (elapsed >= std::chrono::seconds(1)) return true;
    }
    return false;
}
bool IsMouseButtonPressed(int button) { (void)button; return false; }
bool IsMouseButtonReleased(int button) { (void)button; return false; }
Vector2 GetMousePosition(void) { return {0.0f, 0.0f}; }
Vector2 GetWindowPosition(void) { return {0.0f, 0.0f}; }

// Texture stubs
Texture2D LoadTexture(const char* fileName) {
    (void)fileName;
    return Texture2D{};
}
void UnloadTexture(Texture2D texture) { (void)texture; }
void SetTextureFilter(Texture2D texture, int filter) {
    (void)texture; (void)filter;
}

// Font stubs
Font LoadFontEx(const char* fileName, int fontSize, int* codepoints, int codepointCount) {
    (void)fileName; (void)fontSize; (void)codepoints; (void)codepointCount;
    return Font{};
}
void UnloadFont(Font font) { (void)font; }
Font GetFontDefault(void) { return Font{}; }

// Text measurement
Vector2 MeasureTextEx(Font font, const char* text, float fontSize, float spacing) {
    (void)font; (void)text; (void)fontSize; (void)spacing;
    return {100.0f, 16.0f};
}

// Drawing stubs
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint) {
    (void)texture; (void)source; (void)dest; (void)origin; (void)rotation; (void)tint;
}
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color) {
    (void)rec; (void)roundness; (void)segments; (void)color;
}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                                  float lineThick, Color color) {
    (void)rec; (void)roundness; (void)segments; (void)lineThick; (void)color;
}
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color) {
    (void)v1; (void)v2; (void)v3; (void)color;
}
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color) {
    (void)startPos; (void)endPos; (void)thick; (void)color;
}
void DrawTextEx(Font font, const char* text, Vector2 position,
                float fontSize, float spacing, Color tint) {
    (void)font; (void)text; (void)position; (void)fontSize; (void)spacing; (void)tint;
}

} // extern "C"

// ---------------------------------------------------------------------------
// GLFW stubs (used by input_handler.cpp)
// ---------------------------------------------------------------------------
GLFWwindow* glfwGetCurrentContext(void) {
    static int dummy_window;
    return reinterpret_cast<GLFWwindow*>(&dummy_window);
}

GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow* window, GLFWmousebuttonfun callback) {
    (void)window; (void)callback;
    return nullptr;
}
