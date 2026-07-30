#include "Engine/UI/UiSystem.h"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "rlImGui.h"

#include <string>

namespace
{
    ImGuiCond ToImGuiCondition(Engine::UiCondition condition)
    {
        return condition == Engine::UiCondition::Always
            ? ImGuiCond_Always
            : ImGuiCond_FirstUseEver;
    }

    ImVec4 ToImGuiColor(Engine::Color color)
    {
        constexpr float InverseByte = 1.0f / 255.0f;
        return {
            color.red * InverseByte,
            color.green * InverseByte,
            color.blue * InverseByte,
            color.alpha * InverseByte
        };
    }
}

namespace Engine
{
    UiSystem::UiSystem()
    {
        rlImGuiSetup(true);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.WindowPadding = {12.0f, 12.0f};
    }

    UiSystem::~UiSystem()
    {
        rlImGuiShutdown();
    }

    void UiSystem::BeginFrame()
    {
        rlImGuiBegin();
    }

    void UiSystem::EndFrame()
    {
        rlImGuiEnd();
    }

    void UiSystem::SetNextWindowPosition(Vector2 position, UiCondition condition)
    {
        ImGui::SetNextWindowPos(
            {position.x, position.y},
            ToImGuiCondition(condition));
    }

    void UiSystem::SetNextWindowSize(Vector2 size, UiCondition condition)
    {
        ImGui::SetNextWindowSize(
            {size.x, size.y},
            ToImGuiCondition(condition));
    }

    bool UiSystem::BeginPanel(
        std::string_view title,
        bool movable,
        bool resizable,
        bool decorated)
    {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
        if (!movable)
        {
            flags |= ImGuiWindowFlags_NoMove;
        }
        if (!resizable)
        {
            flags |= ImGuiWindowFlags_NoResize;
        }
        if (!decorated)
        {
            flags |=
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse;
        }

        const std::string terminatedTitle(title);
        return ImGui::Begin(terminatedTitle.c_str(), nullptr, flags);
    }

    void UiSystem::EndPanel()
    {
        ImGui::End();
    }

    bool UiSystem::Button(std::string_view label, Vector2 size)
    {
        const std::string terminatedLabel(label);
        return ImGui::Button(
            terminatedLabel.c_str(),
            {size.x, size.y});
    }

    bool UiSystem::BeginChild(std::string_view id, Vector2 size, bool border)
    {
        const std::string terminatedId(id);
        return ImGui::BeginChild(
            terminatedId.c_str(),
            {size.x, size.y},
            border);
    }

    void UiSystem::EndChild()
    {
        ImGui::EndChild();
    }

    bool UiSystem::InputText(
        std::string_view label,
        std::string_view hint,
        std::string& value,
        bool submitOnEnter)
    {
        const std::string terminatedLabel(label);
        const std::string terminatedHint(hint);
        const ImGuiInputTextFlags flags = submitOnEnter
            ? ImGuiInputTextFlags_EnterReturnsTrue
            : ImGuiInputTextFlags_None;

        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::InputTextWithHint(
            terminatedLabel.c_str(),
            terminatedHint.c_str(),
            &value,
            flags);
    }

    void UiSystem::Text(std::string_view text, Color color)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ToImGuiColor(color));
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopStyleColor();
    }

    void UiSystem::TextWrapped(
        std::string_view text,
        float wrapWidth,
        Color color)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ToImGuiColor(color));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    void UiSystem::Separator()
    {
        ImGui::Separator();
    }

    void UiSystem::Spacing()
    {
        ImGui::Spacing();
    }

    void UiSystem::SetKeyboardFocusHere()
    {
        ImGui::SetKeyboardFocusHere();
    }

    void UiSystem::ScrollToBottom()
    {
        ImGui::SetScrollHereY(1.0f);
    }

    float UiSystem::GetAvailableWidth() const
    {
        return ImGui::GetContentRegionAvail().x;
    }

    float UiSystem::GetTextWidth(std::string_view text) const
    {
        return ImGui::CalcTextSize(
            text.data(),
            text.data() + text.size()).x;
    }

    float UiSystem::GetCursorX() const
    {
        return ImGui::GetCursorPosX();
    }

    void UiSystem::SetCursorX(float localX)
    {
        ImGui::SetCursorPosX(localX);
    }

    float UiSystem::GetInputHeight() const
    {
        return ImGui::GetFrameHeightWithSpacing();
    }
}
