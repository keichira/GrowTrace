#pragma once
#include "Precompiled.h"
#include <imgui.h>

struct EditorContext;

class Panel
{
public:
    string name;
    bool isOpen = true;

    Panel(const string& _name) : name(_name) {}
    virtual ~Panel() = default;

    virtual void OnUpdate(EditorContext& ctx, float dt) {}

    void Render(EditorContext& ctx)
    {
        if (!isOpen)
            return;

        if (ImGui::Begin(name.c_str(), &isOpen))
        {
            OnImGuiRender(ctx);
        }
        ImGui::End();
    }

protected:
    virtual void OnImGuiRender(EditorContext& ctx) = 0;
};
