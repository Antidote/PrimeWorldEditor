#ifndef CVECTOR2F
#define CVECTOR2F

#include <FileIO/CInputStream.h>
#include <FileIO/COutputStream.h>

class CVector2f
{
public:
    float x, y;
    CVector2f();
    CVector2f(float xy);
    CVector2f(float _x, float _y);
    CVector2f(CInputStream& Input);
    void Write(COutputStream& Output);

    CVector2f operator+(const CVector2f& other) const;
    CVector2f operator-(const CVector2f& other) const;
    CVector2f operator*(const CVector2f& other) const;
    CVector2f operator/(const CVector2f& other) const;
    void operator+=(const CVector2f& other);
    void operator-=(const CVector2f& other);
    void operator*=(const CVector2f& other);
    void operator/=(const CVector2f& other);
    CVector2f operator+(const float other) const;
    CVector2f operator-(const float other) const;
    CVector2f operator*(const float other) const;
    CVector2f operator/(const float other) const;
    void operator+=(const float other);
    void operator-=(const float other);
    void operator*=(const float other);
    void operator/=(const float other);
    bool operator==(const CVector2f& other) const;
    bool operator!=(const CVector2f& other) const;
    float& operator[](long index);

    // Static Members
    static const CVector2f skZero;
    static const CVector2f skOne;
    static const CVector2f skUp;
    static const CVector2f skRight;
    static const CVector2f skDown;
    static const CVector2f skLeft;
};

#endif // CVECTOR2F
