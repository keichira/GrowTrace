#pragma once
#include "Panel.h"
#include "Precompiled.h"
#include <memory>

class PanelManager
{
public:
    void RegisterPanel(std::unique_ptr<Panel> panel) { m_panels.push_back(std::move(panel)); }

    template <typename T> T* GetPanel()
    {
        for (auto& panel : m_panels)
        {
            if (T* target = dynamic_cast<T*>(panel.get()))
                return target;
        }
        return nullptr;
    }

    void UpdateAll(EditorContext& ctx, float dt)
    {
        for (auto& panel : m_panels)
        {
            panel->OnUpdate(ctx, dt);
        }
    }

    void RenderAll(EditorContext& ctx)
    {
        for (auto& panel : m_panels)
        {
            panel->Render(ctx);
        }
    }

private:
    std::vector<std::unique_ptr<Panel>> m_panels;
};
