#pragma once

namespace Pine::Math
{

    template <typename T>
    class Vector2
    {
    public:
        Vector2() = default;

        explicit Vector2(T value) : x(value), y(value)
        {
        }

        Vector2(T x, T y) : x(x), y(y)
        {
        }

        T x = 0;
        T y = 0;
    };

    using Vector2f = Vector2<float>;
    using Vector2i = Vector2<int>;

}