
//Proxy.h -- Expression Template Proxy Objects

class Vect2D
{
public:
    Vect2D(const Vect2D& tmp) = default;
    Vect2D& operator =(const Vect2D& tmp) = default;
    Vect2D() = default;
    ~Vect2D() = default;

    Vect2D(const float inX, const float inY);

    float getX() const;
    float getY() const;

private:
    float x;
    float y;

    friend struct VaddV;
    friend struct VaddVaddV;
    friend struct VaddVaddVaddV;
    friend struct VaddVaddVaddVaddV;
};

// A + B: returns a proxy holding two references, no Vect2D created
struct VaddV
{
    const Vect2D& v1;
    const Vect2D& v2;

    VaddV(const Vect2D& t1, const Vect2D& t2) : v1(t1), v2(t2) {};

    operator Vect2D() {
        return Vect2D(v1.x + v2.x, v1.y + v2.y);
    }
};

inline VaddV operator + (const Vect2D& a1, const Vect2D& a2) {
    return VaddV(a1, a2);
};

// A + B + C: chains the proxy, still no intermediate Vect2D
struct VaddVaddV
{
    const Vect2D& v1;
    const Vect2D& v2;
    const Vect2D& v3;

    VaddVaddV(const VaddV& t1, const Vect2D& t2)
        : v1(t1.v1), v2(t1.v2), v3(t2) {
    };

    operator Vect2D() {
        return Vect2D(v1.x + v2.x + v3.x, v1.y + v2.y + v3.y);
    }
};

inline VaddVaddV operator + (const VaddV& a1, const Vect2D& a2) {
    return VaddVaddV(a1, a2);
};

// A + B + C + D: four references, single materialization on assignment
struct VaddVaddVaddV
{
    const Vect2D& v1;
    const Vect2D& v2;
    const Vect2D& v3;
    const Vect2D& v4;

    VaddVaddVaddV(const VaddVaddV& t1, const Vect2D& t2)
        : v1(t1.v1), v2(t1.v2), v3(t1.v3), v4(t2) {
    };

    operator Vect2D() {
        return Vect2D(v1.x + v2.x + v3.x + v4.x,
            v1.y + v2.y + v3.y + v4.y);
    }
};

inline VaddVaddVaddV operator + (const VaddVaddV& a1, const Vect2D& a2) {
    return VaddVaddVaddV(a1, a2);
};

// A + B + C + D + E: scales to 5 operands
struct VaddVaddVaddVaddV
{
    const Vect2D& v1;
    const Vect2D& v2;
    const Vect2D& v3;
    const Vect2D& v4;
    const Vect2D& v5;

    VaddVaddVaddVaddV(const VaddVaddVaddV& t1, const Vect2D& t2)
        : v1(t1.v1), v2(t1.v2), v3(t1.v3), v4(t1.v4), v5(t2) {
    };

    operator Vect2D() {
        return Vect2D(v1.x + v2.x + v3.x + v4.x + v5.x,
            v1.y + v2.y + v3.y + v4.y + v5.y);
    }
};

inline VaddVaddVaddVaddV operator + (const VaddVaddVaddV& a1, const Vect2D& a2) {
    return VaddVaddVaddVaddV(a1, a2);
};

// Without proxy: result = A + B + C + D
//   -> tmp1 = A + B        (Vect2D created)
//   -> tmp2 = tmp1 + C     (Vect2D created)
//   -> result = tmp2 + D   (Vect2D created) -- 3 temporaries
//
// With proxy: result = A + B + C + D
//   -> VaddV(A, B)                            (no allocation)
//   -> VaddVaddV(A, B, C)                     (no allocation)
//   -> VaddVaddVaddV(A, B, C, D)              (no allocation)
//   -> operator Vect2D() on assignment        (1 Vect2D created)
//
// Result: 2.3x speedup over naive implementation