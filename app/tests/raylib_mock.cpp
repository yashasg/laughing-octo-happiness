#include "raylib.h"
#include <string>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// Mock call recorder
// ---------------------------------------------------------------------------
std::vector<std::string> g_mock_calls;

void mock_reset() { g_mock_calls.clear(); }

bool mock_was_called(const std::string& name) {
    return std::find(g_mock_calls.begin(), g_mock_calls.end(), name) != g_mock_calls.end();
}

// ---------------------------------------------------------------------------
// Stub implementations
// ---------------------------------------------------------------------------
extern "C" {

Texture2D LoadTexture(const char* fileName) {
    g_mock_calls.push_back("LoadTexture");
    (void)fileName;
    return Texture2D{};
}

void UnloadTexture(Texture2D texture) {
    g_mock_calls.push_back("UnloadTexture");
    (void)texture;
}

void SetTextureFilter(Texture2D texture, int filter) {
    g_mock_calls.push_back("SetTextureFilter");
    (void)texture; (void)filter;
}

Font LoadFontEx(const char* fileName, int fontSize, int* codepoints, int codepointCount) {
    g_mock_calls.push_back("LoadFontEx");
    (void)fileName; (void)fontSize; (void)codepoints; (void)codepointCount;
    return Font{};
}

void UnloadFont(Font font) {
    g_mock_calls.push_back("UnloadFont");
    (void)font;
}

Font GetFontDefault(void) {
    g_mock_calls.push_back("GetFontDefault");
    return Font{};
}

Vector2 MeasureTextEx(Font font, const char* text, float fontSize, float spacing) {
    g_mock_calls.push_back("MeasureTextEx");
    (void)font; (void)text; (void)fontSize; (void)spacing;
    return Vector2{0.0f, 0.0f};
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint) {
    g_mock_calls.push_back("DrawTexturePro");
    (void)texture; (void)source; (void)dest; (void)origin; (void)rotation; (void)tint;
}

void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color) {
    g_mock_calls.push_back("DrawRectangleRounded");
    (void)rec; (void)roundness; (void)segments; (void)color;
}

void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                                  float lineThick, Color color) {
    g_mock_calls.push_back("DrawRectangleRoundedLinesEx");
    (void)rec; (void)roundness; (void)segments; (void)lineThick; (void)color;
}

void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color) {
    g_mock_calls.push_back("DrawTriangle");
    (void)v1; (void)v2; (void)v3; (void)color;
}

void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color) {
    g_mock_calls.push_back("DrawLineEx");
    (void)startPos; (void)endPos; (void)thick; (void)color;
}

void DrawTextEx(Font font, const char* text, Vector2 position,
                float fontSize, float spacing, Color tint) {
    g_mock_calls.push_back("DrawTextEx");
    (void)font; (void)text; (void)position; (void)fontSize; (void)spacing; (void)tint;
}

} // extern "C"
