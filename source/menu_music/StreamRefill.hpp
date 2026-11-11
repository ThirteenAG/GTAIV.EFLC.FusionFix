#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MenuMusic
{
    inline std::atomic<void*> loopingMenuWaveSlot = nullptr;

    enum class StreamEncoding { Adpcm, Pcm };

    template<StreamEncoding Encoding> class StreamBufferRefill
    {
    public:
        using Decode = int(__thiscall*)(void*, void*, const void*, int, unsigned int);

        void Before(void* voice, bool initial, bool owned, std::uint32_t time)
        {
            auto* tail = Find(voice);
            if (!tail)
                return;
            *tail = {};
            if (!owned || initial || Field<std::uint8_t>(voice, TerminalOffset) ||
                !(Field<std::uint8_t>(voice, 140) & 0x10) || Field<unsigned int>(voice, 288) != 2)
                return;
            const auto half = Field<unsigned int>(voice, 272);
            const auto read = Field<unsigned int>(voice, 284);
            const auto rate = Field<unsigned int>(voice, 12);
            auto* buffer = Field<std::byte*>(voice, BufferOffset);
            if (half > 1 || read > 1 || !rate || !buffer || !Field<void*>(voice, OutputOffset))
                return;
            const auto first = (std::min)(BlockSize, Field<unsigned int>(voice, 204 + read * 64));
            const auto next = first && first == Field<unsigned int>(voice, 204 + read * 64) ? read ^ 1 : read;
            const auto count = first + (std::min)(BlockSize - first, Field<unsigned int>(voice, 204 + next * 64));
            if (count == BlockSize)
                return;
            *tail = {voice, Field<void*>(voice, 16), buffer, buffer + half * 0x10000 + count * OutputScale,
                BlockSize - count, half, Field<unsigned int>(voice, 292) + 1, time, 32768000u / rate, Field<void*>(voice, OutputOffset)};
        }

        void Submitted(void* voice, bool owned, std::uint32_t time, Decode decodeSamples = nullptr)
        {
            auto* tail = Find(voice);
            if (!tail || !tail->remaining)
                return;
            if (!owned || tail->voice != voice || tail->wave != Field<void*>(voice, 16) ||
                tail->buffer != Field<std::byte*>(voice, BufferOffset) || !Field<std::uint8_t>(voice, TerminalOffset) ||
                Field<unsigned int>(voice, 292) != tail->refills || time - tail->time >= tail->lifetime)
            {
                *tail = {};
                return;
            }
            auto* output = Field<void*>(voice, OutputOffset);
            if (!output || output != tail->output || (Field<unsigned int>(output, 12) >= 0x10000) == tail->half)
            {
                *tail = {};
                return;
            }

            if constexpr (Encoding == StreamEncoding::Pcm)
            {
                if (Field<unsigned int>(voice, 276) != 1 || !Field<unsigned int>(output, 36) ||
                    Field<std::uint8_t>(output, 97))
                {
                    *tail = {};
                    return;
                }
            }

            // A short final packet can exhaust both descriptors before the wrapped packet arrives.
            for (unsigned int i = 0; i < 2 && tail->remaining; ++i)
            {
                auto& read = Field<unsigned int>(voice, 284);
                if (read > 1)
                    break;
                auto* packet = static_cast<std::byte*>(voice) + 144 + read * 64;
                auto& offset = Field<unsigned int>(packet, 56);
                auto& remaining = Field<unsigned int>(packet, 60);
                const auto count = (std::min)(tail->remaining, remaining);
                if (!count)
                    break;
                const auto* input = Field<const std::byte*>(packet, 0) + offset;
                if constexpr (Encoding == StreamEncoding::Pcm)
                    std::memcpy(tail->destination, input, count);
                else
                    decodeSamples(static_cast<std::byte*>(voice) + 334, tail->destination, input, 0, count);
                offset += count;
                remaining -= count;
                tail->destination += count * OutputScale;
                tail->remaining -= count;
                if (!remaining)
                    read ^= 1;
            }
            if (!tail->remaining)
            {
                Field<std::uint8_t>(voice, TerminalOffset) = 0;
                Field<unsigned int>(voice, 276) = 0;
                *tail = {};
            }
        }

    private:
        static constexpr bool Pcm = Encoding == StreamEncoding::Pcm;
        static constexpr unsigned int BlockSize = Pcm ? 0x10000 : 0x4000;
        static constexpr unsigned int OutputScale = Pcm ? 1 : 4;
        static constexpr std::size_t TerminalOffset = Pcm ? 316 : 332;
        static constexpr std::size_t BufferOffset = Pcm ? 312 : 328;
        static constexpr std::size_t OutputOffset = Pcm ? 304 : 320;

        template<typename T> static T& Field(void* object, std::size_t offset)
        {
            return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
        }

        struct Pending
        {
            void* voice = nullptr;
            void* wave = nullptr;
            std::byte* buffer = nullptr;
            std::byte* destination = nullptr;
            unsigned int remaining = 0, half = 0, refills = 0;
            std::uint32_t time = 0, lifetime = 0;
            void* output = nullptr;
        };

        Pending* Find(void* voice)
        {
            const auto index = Field<unsigned int>(voice, 136);
            return index < pending.size() ? &pending[index] : nullptr;
        }

        std::array<Pending, 256> pending{};
    };

    using StreamRefill = StreamBufferRefill<StreamEncoding::Adpcm>;
    using PcmStreamRefill = StreamBufferRefill<StreamEncoding::Pcm>;
}
