/*
* Copyright (c) 2024 Diago Lima
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef MAYBE_HPP
#define MAYBE_HPP
#include <Core/Platform.hpp>
#include <Core/Panic.hpp>
#include <Core/Nothing.hpp>
#include <Core/Concepts.hpp>
#include <utility>
#include <new>
BEGIN_NAMESPACE(n19)

template<Concrete T>
class Maybe {
public:
  using ValueType     = T;
  using ReferenceType = T&;
  using PointerType   = T*;
    
  FORCEINLINE_ auto value() & -> ReferenceType {
    ASSERT(has_value_, "Maybe has no contained type!");
    return *std::launder<T>(reinterpret_cast<T*>(&value_));
  }

  FORCEINLINE_ auto value() const& -> const T& {
    ASSERT(has_value_, "Maybe has no contained type!");
    return *std::launder<const T>(reinterpret_cast<const T*>(&value_));
  }

  FORCEINLINE_ auto value() && -> ValueType {
    ASSERT(has_value_, "Maybe has no contained type!");
    return release_value(); /// rvalues should release the contained type.
  }                         ///

  FORCEINLINE_ auto release_value() -> ValueType {
    ASSERT(has_value_, "Maybe has no contained type!");
    T released = std::move(value());
    value().~T();           /// We'll have to manually call the destructor.
    has_value_ = false;     ///
    return released;
  }

  auto operator->(this auto&& self) -> decltype(auto) {
    ASSERT( self.has_value_ == true, "Bad Maybe access!" );
    return &self.value();
  }

  auto operator*(this auto&& self) -> decltype(auto) {
    ASSERT( self.has_value_ == true, "Bad Maybe access!" );
    return self.value();
  }

  FORCEINLINE_ auto clear() -> void {
    if(has_value_)          /// Manually call the destructor
      value().~T();         ///

    has_value_ = false;
  }

  FORCEINLINE_ auto value_or(T&& other) const -> ValueType {
    if(has_value_) return value();
    return other;
  }

  FORCEINLINE_ auto operator=(Maybe&& other) noexcept -> Maybe& {
    if(&other == this) return *this;
    clear();                /// clear the current value
    if(other.has_value_) {  /// Check the other Maybe's state...
      has_value_ = true;
      ::new (&value_) T(other.release_value());
    }

    return *this;
  }

  FORCEINLINE_ auto operator=(const Maybe& other) -> Maybe& {
    if(&other == this) return *this;
    clear();                /// clear the current value
    if(other.has_value_) {  /// Check the other Maybe's state...
      has_value_ = true;
      ::new(&value_) T(other.value());
    }
    
    return *this;
  }

  template<typename ...Args>
  FORCEINLINE_ auto emplace(Args&&... args) -> void {
    clear();
    ::new (&value_) T(std::forward<Args>(args)...);
    has_value_ = true;
  }

  template<typename ...Args>
  FORCEINLINE_ static auto create(Args&&... args) -> Maybe {
    return Maybe{ std::forward<Args>(args)... };
  }

  template<typename ...Args> requires std::constructible_from<T, Args...>
  FORCEINLINE_ Maybe(Args&&... args) {
    ::new (&value_) T(std::forward<Args>(args)...);
    has_value_ = true;
  }

  FORCEINLINE_ Maybe(const Maybe& other) {
    if(!other.has_value_) return;
    ::new (&value_) T(other.value());
    has_value_ = true;
  }

  FORCEINLINE_ Maybe(Maybe&& other) noexcept {
    if(!other.has_value_) return;
    ::new (&value_) T(other.release_value());
    has_value_ = true;
  }

  auto has_value() const -> bool { return has_value_; }
  explicit operator bool() const { return has_value_; }
  
 ~Maybe() { clear(); }
  Maybe(const Nothing_&){}
  Maybe() = default;
private:
  bool has_value_{false};
  alignas(T) uint8_t value_[ sizeof(T) ]{};
};

END_NAMESPACE(n19);
#endif //MAYBE_HPP
