#pragma once

namespace Pine
{

    class Color
    {
    public:
        Color() = default;

        Color(int r, int g, int b) :
            r(r),
            g(g),
            b(b),
            a(255)
        {
        }

        Color(int r, int g, int b, int a) :
                r(r),
                g(g),
                b(b),
                a(a)
        {
        }

        int r = 0;
        int g = 0;
        int b = 0;
        int a = 0;
    };

}