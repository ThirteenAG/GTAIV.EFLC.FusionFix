#pragma once

#include <optional>
#include <functional>
#include <utility>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <cstdint>
#include "injector\injector.hpp"

template<typename T, bool AutoUnprotect = false>
class GameRef
{
private:
    mutable std::optional<T*> ptr{ std::nullopt };
    mutable std::optional<std::function<T* ()>> deferredResolver{ std::nullopt };

    void EnsureResolved() const
    {
        if (deferredResolver.has_value())
        {
            ptr = (*deferredResolver)();
            deferredResolver.reset();
        }

        if (!ptr.has_value())
        {
            assert(false && "GameRef accessed before resolution!");
            throw std::runtime_error("GameRef accessed before address resolution");
        }
    }

    T& assign(const T& value)
    {
        return assign(T(value));
    }

    T& assign(T&& value)
    {
        EnsureResolved();
        if (*ptr == nullptr)
            throw std::runtime_error("GameRef: Address resolution failed (pattern returned nullptr)");

        if constexpr (AutoUnprotect)
        {
            DWORD oldProtect;
            injector::UnprotectMemory(*ptr, sizeof(T), oldProtect);
        }

        **ptr = std::move(value);
        return **ptr;
    }

public:
    GameRef() = default;

    explicit GameRef(T* address)
    {
        if (address == nullptr)
            throw std::invalid_argument("GameRef::SetAddress called with null pointer");
        ptr = address;
    }

    template<typename Fn>
        requires std::invocable<Fn>&& std::convertible_to<std::invoke_result_t<Fn>, T*>
    explicit GameRef(Fn&& fn)
    {
        T* result = std::invoke(fn);

        if (result != nullptr)
        {
            ptr = result;
        }
        else
        {
            deferredResolver.emplace(std::forward<Fn>(fn));
        }
    }

    GameRef(const GameRef& other) = default;
    GameRef& operator=(const GameRef& other) = default;
    GameRef(GameRef&& other) noexcept = default;
    GameRef& operator=(GameRef&& other) noexcept = default;

    template<typename AddressT>
        requires (std::is_pointer_v<std::remove_reference_t<AddressT>> || std::is_integral_v<std::remove_reference_t<AddressT>>)
    void SetAddress(AddressT address)
    {
        using RawAddressT = std::remove_cvref_t<AddressT>;
        auto resolvedAddress = [&]() -> T*
        {
            if constexpr (std::is_pointer_v<RawAddressT>)
                return reinterpret_cast<T*>(address);
            else
                return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
        }();

        if (resolvedAddress == nullptr)
            throw std::invalid_argument("GameRef::SetAddress called with null pointer");

        ptr = resolvedAddress;
        deferredResolver.reset();
    }

    T& get()
    {
        EnsureResolved();
        if (*ptr == nullptr)
            throw std::runtime_error("GameRef: Address resolution failed (pattern not found)");
        return **ptr;
    }

    const T& get() const
    {
        EnsureResolved();
        if (*ptr == nullptr)
            throw std::runtime_error("GameRef: Address resolution failed (pattern not found)");
        return **ptr;
    }

    bool is_initialized() const noexcept
    {
        return ptr.has_value() || deferredResolver.has_value();
    }

    T* get_ptr()
    {
        EnsureResolved();
        return *ptr;
    }

    const T* get_ptr() const
    {
        EnsureResolved();
        return *ptr;
    }

    operator T& () { return get(); }
    operator const T& () const { return get(); }

    explicit operator bool() const requires std::convertible_to<T, bool> { return static_cast<bool>(get()); }

    T& operator=(const T& value) { return assign(T(value)); }
    T& operator=(T&& value) { return assign(std::move(value)); }

    template<typename U> T& operator+=(const U& v) { T val = get() + v; return assign(std::move(val)); }
    template<typename U> T& operator-=(const U& v) { T val = get() - v; return assign(std::move(val)); }
    template<typename U> T& operator*=(const U& v) { T val = get() * v; return assign(std::move(val)); }
    template<typename U> T& operator/=(const U& v) { T val = get() / v; return assign(std::move(val)); }
    template<typename U> T& operator%=(const U& v) { T val = get() % v; return assign(std::move(val)); }

    template<typename U> T& operator&=(const U& v) { T val = get() & v; return assign(std::move(val)); }
    template<typename U> T& operator|=(const U& v) { T val = get() | v; return assign(std::move(val)); }
    template<typename U> T& operator^=(const U& v) { T val = get() ^ v; return assign(std::move(val)); }
    template<typename U> T& operator<<=(const U& v) { T val = get() << v; return assign(std::move(val)); }
    template<typename U> T& operator>>=(const U& v) { T val = get() >> v; return assign(std::move(val)); }

    T& operator++() { return ++get(); }
    T  operator++(int) { return get()++; }

    T& operator--() { return --get(); }
    T  operator--(int) { return get()--; }

    T operator+() const { return +get(); }
    T operator-() const { return -get(); }
    bool operator!() const { return !get(); }
    T operator~() const { return ~get(); }

    template<typename U> auto operator+(const U& v) const { return get() + v; }
    template<typename U> auto operator-(const U& v) const { return get() - v; }
    template<typename U> auto operator*(const U& v) const { return get() * v; }
    template<typename U> auto operator/(const U& v) const { return get() / v; }
    template<typename U> auto operator%(const U& v) const { return get() % v; }

    template<typename U> auto operator&(const U& v) const { return get() & v; }
    template<typename U> auto operator|(const U& v) const { return get() | v; }
    template<typename U> auto operator^(const U& v) const { return get() ^ v; }
    template<typename U> auto operator<<(const U& v) const { return get() << v; }
    template<typename U> auto operator>>(const U& v) const { return get() >> v; }

    template<typename U> bool operator==(const U& v) const
    {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
            return std::cmp_equal(get(), v);
        else
            return get() == v;
    }

    template<typename U> bool operator!=(const U& v) const
    {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
            return std::cmp_not_equal(get(), v);
        else
            return get() != v;
    }

    template<typename U> bool operator<(const U& v) const
    {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
            return std::cmp_less(get(), v);
        else
            return get() < v;
    }

    template<typename U> bool operator>(const U& v) const
    {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
            return std::cmp_greater(get(), v);
        else
            return get() > v;
    }

    template<typename U> bool operator<=(const U& v) const
    {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
            return std::cmp_less_equal(get(), v);
        else
            return get() <= v;
    }

    template<typename U> bool operator>=(const U& v) const
    {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
            return std::cmp_greater_equal(get(), v);
        else
            return get() >= v;
    }

    T& operator*() { return get(); }
    const T& operator*() const { return get(); }

    auto operator->()
    {
        if constexpr (std::is_pointer_v<T>)
            return get();
        else
            return &get();
    }

    auto operator->() const
    {
        if constexpr (std::is_pointer_v<T>)
            return get();
        else
            return &get();
    }

    template<typename U>
    auto operator[](const U& index) { return get()[index]; }

    template<typename U>
    auto operator[](const U& index) const { return get()[index]; }
};

template<typename T>
using ProtectedGameRef = GameRef<T, true>;