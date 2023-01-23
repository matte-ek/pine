#pragma once

namespace Panels
{
    namespace State
    {
        inline bool Level = false;
        inline bool Game = false;
        inline bool EntityList = false;
        inline bool Properties = false;
    }

    void ShowViewports();

    void ShowEntityList();

    void ShowProperties();

}

