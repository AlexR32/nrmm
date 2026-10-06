#pragma once
#include "pch.h"
#include "libs/imgui/imgui.h"

class Overlay {
public:
    static void Box(int x, int y, int w, int h, ImColor color = IM_COL32(255, 255, 255, 255), float cornerSize = 1.0f) {
        ImDrawList* Drawlist = ImGui::GetBackgroundDrawList();

        const int lineW = static_cast<int>(((w * 0.5f) * cornerSize) + 1);
        const int lineH = static_cast<int>(((h * 0.5f) * cornerSize) + 1);

        // Int to float
#define IV2(x, y) ImVec2(static_cast<float>(x), static_cast<float>(y))

        // Black Outlines
        // Top Left
        Drawlist->AddLine(IV2(x - 1, y), IV2(x + lineW + 1, y), IM_COL32(0, 0, 0, 255), 3.0f);
        Drawlist->AddLine(IV2(x, y - 1), IV2(x, y + lineH + 1), IM_COL32(0, 0, 0, 255), 3.0f);

        // Top Right
        Drawlist->AddLine(IV2(x + w + 2, y), IV2(x + w - lineW - 1, y), IM_COL32(0, 0, 0, 255), 3.0f);
        Drawlist->AddLine(IV2(x + w, y - 1), IV2(x + w, y + lineH + 1), IM_COL32(0, 0, 0, 255), 3.0f);

        // Bottom Left
        Drawlist->AddLine(IV2(x - 1, y + h), IV2(x + lineW + 1, y + h), IM_COL32(0, 0, 0, 255), 3.0f);
        Drawlist->AddLine(IV2(x, y + h + 2), IV2(x, y + h - lineH - 1), IM_COL32(0, 0, 0, 255), 3.0f);

        // Bottom Right
        Drawlist->AddLine(IV2(x + w + 2, y + h), IV2(x + w - lineW - 1, y + h), IM_COL32(0, 0, 0, 255), 3.0f);
        Drawlist->AddLine(IV2(x + w, y + h + 2), IV2(x + w, y + h - lineH - 1), IM_COL32(0, 0, 0, 255), 3.0f);

        // Corners
        // Top Left
        Drawlist->AddLine(IV2(x, y), IV2(x + lineW, y), color, 1.0f);
        Drawlist->AddLine(IV2(x, y), IV2(x, y + lineH), color, 1.0f);

        // Top Right
        Drawlist->AddLine(IV2(x + w + 1, y), IV2(x + w - lineW, y), color, 1.0f);
        Drawlist->AddLine(IV2(x + w, y), IV2(x + w, y + lineH), color, 1.0f);

        // Bottom Left
        Drawlist->AddLine(IV2(x, y + h + 1), IV2(x, y + h - lineH), color, 1.0f);
        Drawlist->AddLine(IV2(x, y + h), IV2(x + lineW, y + h), color, 1.0f);

        // Bottom Right
        Drawlist->AddLine(IV2(x + w + 1, y + h), IV2(x + w - lineW, y + h), color, 1.0f);
        Drawlist->AddLine(IV2(x + w, y + h + 1), IV2(x + w, y + h - lineH), color, 1.0f);

#undef IV2
    }

    static void Text(const char* text, ImVec2 position, ImColor color = IM_COL32(255, 255, 255, 255)) {
        ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), 13.0f, position, color, text);
    }

    static void TextOutlined(const char* text, ImVec2 position, ImColor color = IM_COL32(255, 255, 255, 255), ImColor outlineColor = IM_COL32(0, 0, 0, 255), int outlineThickness = 1) {
        ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
        ImFont* Font = ImGui::GetFont();

        DrawList->AddText(Font, 13.0f, ImVec2(position.x - outlineThickness, position.y), outlineColor, text);
        DrawList->AddText(Font, 13.0f, ImVec2(position.x + outlineThickness, position.y), outlineColor, text);
        DrawList->AddText(Font, 13.0f, ImVec2(position.x, position.y - outlineThickness), outlineColor, text);
        DrawList->AddText(Font, 13.0f, ImVec2(position.x, position.y + outlineThickness), outlineColor, text);

        DrawList->AddText(Font, 13.0f, position, color, text);
    }

    static void TextCentered(const char* text, ImVec2 position, ImColor color = IM_COL32(255, 255, 255, 255)) {
        float textSizeX = ImGui::CalcTextSize(text).x;
        ImVec2 textPosition = ImVec2(position.x - (textSizeX * 0.5f), position.y);
        Text(text, textPosition, color);
    }

    static void TextOutlinedCentered(const char* text, ImVec2 position, ImColor color = IM_COL32(255, 255, 255, 255), ImColor outlineColor = IM_COL32(0, 0, 0, 255), int outlineThickness = 1) {
        float textSizeX = ImGui::CalcTextSize(text).x;
        ImVec2 textPosition = ImVec2(position.x - (textSizeX * 0.5f), position.y);
        TextOutlined(text, textPosition, color, outlineColor, outlineThickness);
    }
};
