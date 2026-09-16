#pragma once

namespace Pine
{
    class Entity;
}

namespace Panels::EntityList
{
    void SetActive(bool value);
    bool GetActive();

    // Discard an in-progress drag before its source entity is destroyed.
    void CancelEntityDrag(Pine::Entity* entity);

    void Render();
}
