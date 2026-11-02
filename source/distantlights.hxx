#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace DistantLights
{
    enum class Mode : int32_t { Default, Console };

    struct Point { int16_t x, y, z; };
    struct String
    {
        int16_t x, y, z, radius;
        uint16_t firstPoint, pointCount;
    };
    static_assert(sizeof(Point) == 6 && sizeof(String) == 12);

    struct alignas(16) Vector
    {
        float x{}, y{}, z{}, w{};
        Vector operator+(const Vector& b) const { return { x + b.x, y + b.y, z + b.z }; }
        Vector operator-(const Vector& b) const { return { x - b.x, y - b.y, z - b.z }; }
        Vector operator*(float s) const { return { x * s, y * s, z * s }; }
        float Length() const { return std::sqrt(x * x + y * y + z * z); }
    };

    inline float Ramp(float start, float end, float distance)
    {
        return std::clamp((distance - start) / (end - start), 0.0f, 1.0f);
    }

    // Reuse distance attenuation once, capped by the camera's height above
    // this sprite (0..100 m). PC's distance fade spans 150 m; Xbox's 100 m.
    inline float HeightLimitedDistance(float distance, float cameraZ, float spriteZ, float distanceSpan)
    {
        return std::min(distance, 150.0f + distanceSpan * Ramp(0.0f, 100.0f, cameraZ - spriteZ));
    }

    inline float HeightLimitedOpacity(float distanceFade, float cameraZ, float modelZ)
    {
        return std::min(distanceFade, Ramp(0.0f, 100.0f, cameraZ - modelZ));
    }

    // Map the game's 15-bit rand() output to the retail Xbox range [170,255).
    inline uint32_t XboxBrightness(uint32_t sample)
    {
        // Exact equivalent of trunc((sample / 32768.0f) * 85.0f) + 170.
        return 170u + ((sample * 85u) >> 15);
    }

    inline uint32_t XboxSpriteColor(uint32_t rgb, float intensity, float distance, uint32_t brightness)
    {
        // Xbox quantizes traffic's input channels before distance attenuation.
        intensity = std::clamp(intensity, 0.0f, 1.0f);
        if (distance < 250.0f)
            brightness = uint32_t(std::max(0.0f, (distance - 150.0f) * 0.01f) * float(brightness));
        auto channel = [&](uint32_t shift)
        {
            const auto input = uint32_t(float((rgb >> shift) & 255u) * intensity);
            return ((input * brightness) >> 8) << shift;
        };
        return 0xFF000000u | channel(16) | channel(8) | channel(0);
    }

    // Xbox movement uses an eight-bit phase derived from the millisecond timer.
    inline float FirstDistance(uint32_t milliseconds, bool red)
    {
        const float phase = static_cast<float>((milliseconds / (red ? 57u : 43u)) & 255u);
        return red ? 45.5f - phase * (45.5f / 256.0f) : phase * (33.0f / 256.0f);
    }

    struct Segment { Vector start, direction; float length, distance; };
    struct Sample { Vector position, direction; };

    // Carry spacing across bends; repeated vertices do not consume distance.
    class Road
    {
        std::vector<Segment> segments;
        float length{};
    public:
        void Assign(std::span<const Point> points)
        {
            segments.clear();
            length = 0.0f;
            for (size_t i = 1; i < points.size(); ++i)
            {
                Vector a{ float(points[i - 1].x), float(points[i - 1].y), float(points[i - 1].z) };
                Vector b{ float(points[i].x), float(points[i].y), float(points[i].z) };
                const auto delta = b - a;
                const float distance = delta.Length();
                if (distance <= 0.0001f)
                    continue;
                segments.push_back({ a, delta * (1.0f / distance), distance, length });
                length += distance;
            }
        }
        float Length() const { return length; }
        Sample At(float distance) const
        {
            auto it = std::lower_bound(segments.begin(), segments.end(), distance,
                [](const Segment& s, float d) { return s.distance + s.length < d; });
            if (it == segments.end())
                it = segments.end() - 1;
            return { it->start + it->direction * (distance - it->distance), it->direction };
        }
    };

    // Xbox roads are centerlines: both streams belong on each road. Never use
    // this enumerator on the native PC's directed carriageway strings.
    template<typename Callback>
    void ForEachVehicle(const Road& road, uint32_t milliseconds, Callback&& visit)
    {
        for (bool red : { false, true })
        {
            uint32_t index = 0;
            for (float s = FirstDistance(milliseconds, red); s < road.Length();
                s += red ? 45.5f : 33.0f, ++index)
            {
                auto light = road.At(s).position;
                light.z += 2.0f;
                visit(light, red, index);
            }
        }
    }

    // Wrap negative coordinate hashes into the 16 dusk/dawn thresholds.
    // The original signed remainder reads before the table in parts of Alderney.
    inline float StreetlightThreshold(const Point& first)
    {
        constexpr std::array<float, 16> table{
            .1f, .7f, .5f, .96f, .34f, .75f, .2f, .4f,
            .65f, .05f, .76f, .83f, .59f, .34f, .66f, .43f
        };
        const int index = (int(first.x) / 300 + int(first.y) / 500) % 16;
        return table[index < 0 ? index + 16 : index];
    }

    struct Dataset
    {
        std::array<uint32_t, 64> firstString{}, coastalCount{}, interiorCount{};
        std::vector<Point> points;
        std::vector<String> strings;

        // Decode big-endian data; preserve the current dataset on invalid input.
        bool Decode(std::span<const uint8_t> bytes)
        {
            constexpr size_t header = 8 + 3 * 64 * 4;
            if (bytes.size() < header)
                return false;
            auto u16 = [&](size_t at) -> uint16_t
            {
                return (uint16_t(bytes[at]) << 8) | bytes[at + 1];
            };
            auto i16 = [&](size_t at) { return std::bit_cast<int16_t>(u16(at)); };
            auto u32 = [&](size_t at) -> uint32_t
            {
                return (uint32_t(u16(at)) << 16) | u16(at + 2);
            };
            const uint32_t pointCount = u32(0), stringCount = u32(4);
            if (pointCount < 2 || pointCount > 12000 || stringCount == 0 || stringCount > 1100 ||
                bytes.size() != header + size_t(pointCount) * 6 + size_t(stringCount) * 12)
                return false;
            Dataset decoded;
            std::vector<bool> covered(stringCount, false);
            for (size_t b = 0; b < 64; ++b)
            {
                const auto first = u32(8 + b * 4);
                const auto coastal = u32(264 + b * 4), inland = u32(520 + b * 4);
                if (first > stringCount || coastal > stringCount - first || inland > stringCount - first - coastal)
                    return false;
                decoded.firstString[b] = first;
                decoded.coastalCount[b] = coastal;
                decoded.interiorCount[b] = inland;
                for (uint32_t i = first; i < first + coastal + inland; ++i)
                {
                    if (covered[i])
                        return false;
                    covered[i] = true;
                }
            }
            if (std::find(covered.begin(), covered.end(), false) != covered.end())
                return false;
            decoded.points.reserve(pointCount);
            for (size_t i = 0, at = header; i < pointCount; ++i, at += 6)
                decoded.points.push_back({ i16(at), i16(at + 2), i16(at + 4) });
            decoded.strings.reserve(stringCount);
            for (size_t i = 0, at = header + size_t(pointCount) * 6; i < stringCount; ++i, at += 12)
            {
                String line{ i16(at), i16(at + 2), i16(at + 4), i16(at + 6), u16(at + 8), u16(at + 10) };
                if (line.radius < 0 || line.pointCount < 2 ||
                    uint32_t(line.firstPoint) + line.pointCount > pointCount)
                    return false;
                decoded.strings.push_back(line);
            }
            *this = std::move(decoded);
            return true;
        }
    };
}
