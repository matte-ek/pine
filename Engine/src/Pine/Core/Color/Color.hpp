#pragma once

namespace Pine
{

    class Color
    {
    public:
        Color() = default;

        Color(const int r, const int g, const int b) :
            r(r),
            g(g),
            b(b),
            a(255)
        {
        }

        Color(const int r, const int g, const int b, const int a) :
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

        static const Color White;
        static const Color Black;
        static const Color Red;
        static const Color Green;
        static const Color Blue;
    };

}