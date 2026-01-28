# Problem-Solving Session 11: The Unit Confusion

## Compile-Time Dimensional Analysis

**Estimated time:** 60–75 minutes  
**Prerequisites:** Templates, basic physics concepts  
**Fat-P components:** None (external libraries)

---

## Guarantee Legend

| Mark | Meaning |
|------|---------|
| ✅ **Compile-time** | Adding meters to seconds is a compile error |

---

## The Bug

Your autopilot software calculates landing approach:

```cpp
double altitude = 10000;     // feet? meters?
double descent_rate = 500;   // feet/minute? meters/second?
double time_to_landing = altitude / descent_rate;  // What unit is this?

// Later, someone assumes metric
send_to_display(time_to_landing);  // Shows wrong value
adjust_throttle(descent_rate);     // Expects m/s, gets ft/min → CRASH
```

Nobody can tell from the code what units these values represent. Comments get outdated. Conventions get forgotten.

---

## Real-World Disasters

### Mars Climate Orbiter (1999)

**Cost: $327 million**

One team provided thrust data in pound-seconds. Another team expected newton-seconds. The spacecraft entered the atmosphere at the wrong angle and was destroyed.

```cpp
// Lockheed Martin (imperial)
double impulse = calculate_impulse_lbf_s();

// NASA JPL (expected metric)
void apply_correction(double impulse_newton_seconds) {
    // Used the value directly—wrong by factor of 4.45
}
```

### Gimli Glider (1983)

Air Canada Flight 143 ran out of fuel mid-flight because fuel was calculated in pounds but loaded in kilograms. The plane glided to an emergency landing.

### Medication Dosing

Hospitals have reported fatal overdoses from unit confusion between milligrams and micrograms, or between mg/kg and mg total.

---

## Questions to Consider

1. **Q1:** Why doesn't "be careful" work?
2. **Q2:** How can types encode units?
3. **Q3:** What libraries are available?
4. **Q4:** How does dimensional analysis work at compile time?
5. **Q5:** What's the runtime cost?

---

## Q1: Why Discipline Fails

The type `double` carries no unit information:

```cpp
double velocity;      // m/s? km/h? mph? ft/s?
double distance;      // meters? feet? kilometers?
double force;         // newtons? pounds? dynes?
```

Code reviews miss unit bugs because:
- Comments may be wrong or missing
- Variable names are ambiguous (`speed`, `rate`, `value`)
- Formulas look correct even with wrong units
- Tests pass with consistent wrong units

**The fundamental issue:** The type system doesn't participate in unit checking. `double + double` always compiles, even when adding meters to seconds.

---

## Q2: Types Can Encode Units

The solution: make `Distance` and `Duration` different types:

```cpp
class Meters {
    double value_;
public:
    explicit Meters(double v) : value_(v) {}
    double count() const { return value_; }
};

class Seconds {
    double value_;
public:
    explicit Seconds(double v) : value_(v) {}
    double count() const { return value_; }
};

Meters operator+(Meters a, Meters b) {
    return Meters{a.count() + b.count()};
}

// No operator+(Meters, Seconds) defined → compile error
Meters d = Meters{100};
Seconds t = Seconds{10};
auto bad = d + t;  // Error: no match for 'operator+'
```

For derived units, define operations that produce new types:

```cpp
class MetersPerSecond {
    double value_;
public:
    explicit MetersPerSecond(double v) : value_(v) {}
    double count() const { return value_; }
};

MetersPerSecond operator/(Meters d, Seconds t) {
    return MetersPerSecond{d.count() / t.count()};
}

Meters operator*(MetersPerSecond v, Seconds t) {
    return Meters{v.count() * t.count()};
}

auto velocity = Meters{100} / Seconds{10};  // MetersPerSecond
auto distance = velocity * Seconds{5};       // Meters
```

---

## Q3: Available Libraries

### mp-units (C++20, Proposed for C++ Standard)

The most modern, comprehensive solution:

```cpp
#include <mp-units/systems/si/si.h>
using namespace mp_units;
using namespace mp_units::si;

quantity<metre> distance = 100 * m;
quantity<second> time = 9.58 * s;
quantity<metre_per_second> speed = distance / time;

// Compile error: can't add distance to time
auto bad = distance + time;

// Conversions are explicit
quantity<kilo<metre>> km = distance;  // 0.1 km
quantity<foot> ft = value_cast<foot>(distance);  // ~328 ft
```

**Pros:**
- Modern C++20 design with concepts
- Proposed for C++ standard (maybe C++29)
- Comprehensive SI and other unit systems
- Clear error messages

**Cons:**
- Requires C++20
- Learning curve

### Boost.Units

Mature, battle-tested:

```cpp
#include <boost/units/systems/si.hpp>
using namespace boost::units;
using namespace boost::units::si;

quantity<length> distance(100.0 * meters);
quantity<time> t(9.58 * seconds);
quantity<velocity> speed = distance / t;

// Compile error
auto bad = distance + t;
```

**Pros:**
- Mature, well-tested
- Works with C++03
- Extensive documentation

**Cons:**
- Verbose syntax
- Long compile times
- Large dependency

### nholthaus/units

Header-only, clean syntax:

```cpp
#include <units.h>
using namespace units::literals;

auto distance = 100_m;
auto time = 9.58_s;
auto speed = distance / time;

auto bad = distance + time;  // Compile error
```

**Pros:**
- Header-only
- C++14 compatible
- Clean literal syntax

**Cons:**
- Less comprehensive than mp-units
- Fewer unit systems

---

## Q4: How Dimensional Analysis Works

At compile time, units are tracked as type parameters:

```cpp
template<int LengthExp, int TimeExp, int MassExp>
class Quantity {
    double value_;
public:
    explicit Quantity(double v) : value_(v) {}
    double count() const { return value_; }
};

// Length: L^1, T^0, M^0
using Length = Quantity<1, 0, 0>;

// Time: L^0, T^1, M^0
using Time = Quantity<0, 1, 0>;

// Velocity: L^1, T^-1, M^0
using Velocity = Quantity<1, -1, 0>;

// Multiplication adds exponents
template<int L1, int T1, int M1, int L2, int T2, int M2>
Quantity<L1+L2, T1+T2, M1+M2>
operator*(Quantity<L1,T1,M1> a, Quantity<L2,T2,M2> b) {
    return Quantity<L1+L2, T1+T2, M1+M2>{a.count() * b.count()};
}

// Division subtracts exponents
template<int L1, int T1, int M1, int L2, int T2, int M2>
Quantity<L1-L2, T1-T2, M1-M2>
operator/(Quantity<L1,T1,M1> a, Quantity<L2,T2,M2> b) {
    return Quantity<L1-L2, T1-T2, M1-M2>{a.count() / b.count()};
}

// Addition requires same dimensions
template<int L, int T, int M>
Quantity<L,T,M> operator+(Quantity<L,T,M> a, Quantity<L,T,M> b) {
    return Quantity<L,T,M>{a.count() + b.count()};
}
```

Now the compiler checks dimensional correctness:

```cpp
Length distance{100};
Time time{10};

auto velocity = distance / time;  // Quantity<1,-1,0> = Velocity ✓
auto acceleration = velocity / time;  // Quantity<1,-2,0> ✓
auto force = Quantity<0,0,1>{10} * acceleration;  // Quantity<1,-2,1> = Force ✓

auto bad = distance + time;  // Error: Quantity<1,0,0> + Quantity<0,1,0>
                             // No matching operator+
```

---

## Q5: Runtime Cost

**Zero.** Units exist only in the type system. At runtime, it's just `double` arithmetic:

```cpp
// With units
quantity<metre> a = 100 * m;
quantity<metre> b = 200 * m;
quantity<metre> c = a + b;

// Compiles to identical code as:
double a = 100;
double b = 200;
double c = a + b;
```

The unit tags are compile-time only—they occupy no memory and generate no runtime checks.

---

## Defining Custom Units

### Application-Specific Units

```cpp
// Game engine: pixels and tiles
namespace game {
    inline constexpr struct pixel : mp_units::named_unit<"px"> {} pixel;
    inline constexpr struct tile : mp_units::named_unit<"tile", 32 * pixel> {} tile;
    
    // 1 tile = 32 pixels
}

quantity<game::pixel> sprite_width = 64 * game::pixel;
quantity<game::tile> map_width = 100 * game::tile;

// Convert tiles to pixels
quantity<game::pixel> map_width_px = value_cast<game::pixel>(map_width);
// = 3200 pixels
```

### Domain Units

```cpp
// Finance
inline constexpr struct USD : mp_units::named_unit<"USD"> {} USD;
inline constexpr struct EUR : mp_units::named_unit<"EUR"> {} EUR;

quantity<USD> price_usd = 99.99 * USD;
quantity<EUR> price_eur = price_usd;  // Error: no implicit conversion

// Explicit conversion with exchange rate
quantity<EUR> price_eur = price_usd * (0.85 * EUR/USD);
```

---

## Integration with Existing Code

### Wrapping C APIs

```cpp
// External API uses raw doubles
extern "C" {
    void set_motor_speed(double rpm);
    double get_temperature();
}

// Internal: typed units
void control_motor(quantity<revolution_per_minute> speed) {
    set_motor_speed(speed.numerical_value_in(rpm));
}

quantity<celsius> read_temperature() {
    return get_temperature() * deg_C;
}
```

### Serialization

```cpp
// JSON: store value and unit string
void to_json(json& j, quantity<metre> dist) {
    j = {
        {"value", dist.numerical_value_in(m)},
        {"unit", "m"}
    };
}

void from_json(const json& j, quantity<metre>& dist) {
    double value = j["value"];
    std::string unit = j["unit"];
    
    if (unit == "m") {
        dist = value * m;
    } else if (unit == "ft") {
        dist = value * ft;
    } else if (unit == "km") {
        dist = value * km;
    } else {
        throw std::runtime_error("Unknown unit: " + unit);
    }
}
```

---

## Complete Example: Physics Simulation

```cpp
#include <mp-units/systems/si/si.h>
#include <mp-units/systems/international/international.h>

using namespace mp_units;
using namespace mp_units::si;

struct Projectile {
    quantity<metre> x, y;
    quantity<metre_per_second> vx, vy;
};

void simulate(Projectile& p, quantity<second> dt) {
    constexpr auto g = 9.81 * m / (s * s);  // Acceleration due to gravity
    
    // Update position
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    
    // Update velocity (gravity acts downward)
    p.vy -= g * dt;
    
    // These would be compile errors:
    // p.x += p.vx;           // Error: metre + metre_per_second
    // p.y += g;              // Error: metre + metre_per_second_squared
    // p.vy -= g;             // Error: metre_per_second - metre_per_second_squared
}

int main() {
    Projectile ball{
        .x = 0 * m,
        .y = 0 * m,
        .vx = 10 * m / s,
        .vy = 20 * m / s
    };
    
    for (int i = 0; i < 100; ++i) {
        simulate(ball, 0.1 * s);
        std::cout << "t=" << (i * 0.1) << "s: "
                  << "x=" << ball.x << ", y=" << ball.y << "\n";
    }
}
```

---

## When to Use Units Libraries

### Worth the Complexity

- Physics simulations
- Engineering calculations
- Financial systems
- Embedded systems (sensor data)
- Aerospace and automotive
- Scientific computing
- Any multi-unit domain

### Probably Overkill

- Single-unit domains
- Throwaway scripts
- Pure business logic (no physical quantities)
- Performance-critical inner loops (verify first)

---

## Summary

| Problem | Solution |
|---------|----------|
| Unit confusion | Encode units in types |
| Adding incompatible units | Compile error |
| Wrong unit at boundary | Explicit conversion required |
| Runtime overhead | Zero (compile-time only) |

### Key Principles

1. **Types encode units** — `Meters` and `Seconds` are different types

2. **Operations track dimensions** — `distance / time` produces `velocity`

3. **Incompatible operations fail** — `distance + time` doesn't compile

4. **Conversions are explicit** — `value_cast<foot>(meters)` required

5. **Zero runtime cost** — unit tracking is purely compile-time

### The Guideline in One Sentence

> Use a units library to make dimensional analysis automatic and unit confusion impossible.

---

## Exercises

1. **Basic:** Use mp-units or nholthaus/units to implement a function that calculates kinetic energy: `E = ½mv²`.

2. **Conversion:** Write a temperature converter that handles Celsius, Fahrenheit, and Kelvin with type-safe conversions.

3. **Custom units:** Define custom units for your domain (e.g., pixels, frames, currency) and demonstrate type-safe operations.

4. **Integration:** Wrap a C API that takes raw doubles with type-safe unit wrappers.

---

## Further Reading

- [mp-units documentation](https://mpusz.github.io/mp-units/)
- [Boost.Units](https://www.boost.org/doc/libs/release/doc/html/boost_units.html)
- [nholthaus/units](https://github.com/nholthaus/units)
- [P1935: A C++ Units Library](http://wg21.link/p1935) — standards proposal
- Session 1: Strong Typedefs — similar pattern for non-physical types
