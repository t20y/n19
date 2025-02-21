/*
* Copyright (c) 2024 Diago Lima
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef ARGPARSE_HPP
#define ARGPARSE_HPP
#include <Sys/String.hpp>
#include <Core/Concepts.hpp>
#include <Core/Maybe.hpp>
#include <Core/ConIO.hpp>
#include <Core/ClassTraits.hpp>
#include <Core/Stream.hpp>
#include <Core/Platform.hpp>
#include <Core/Result.hpp>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <deque>
BEGIN_NAMESPACE(n19::argp);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin Value Classes:
// Each class here is a polymorphic wrapper around a value
// that can be set by the user when passing command line arguments.

class ValueBase {
public:
  virtual auto convert(const sys::String&) -> Result<void> = 0;
  virtual ~ValueBase() = default;
};

template<typename T>
class Value final : public ValueBase {
public:
  auto convert(const sys::String&) -> Result<void> override = delete;
  auto value() -> void = delete;
private:
  ~Value() override = delete;
  Value()  = delete;
};

template<>
class Value<int64_t> final : public ValueBase {
N19_MAKE_COMPARABLE_ON(int64_t, value_);
public:
  auto convert(const sys::String&) -> Result<void> override;
  FORCEINLINE_ auto value() -> int64_t& { return value_; }

  Value() = default;
 ~Value() override = default;
private:
  int64_t value_ = 0;
};

template<>
class Value<bool> final : public ValueBase {
N19_MAKE_COMPARABLE_ON(bool, value_);
public:
  auto convert(const sys::String&)  -> Result<void> override;
  FORCEINLINE_ auto value() -> bool& { return value_; }

  Value() = default;
 ~Value() override = default;
private:
  bool value_ = false;
};

template<>
class Value<double> final : public ValueBase {
N19_MAKE_COMPARABLE_ON(double, value_);
public:
  auto convert(const sys::String&) -> Result<void> override;
  FORCEINLINE_ auto value() -> double& { return value_; }

  Value() = default;
 ~Value() override = default;
private:
  double value_ = 0.00;
};

template<>
class Value<sys::String> final : public ValueBase {
N19_MAKE_COMPARABLE_ON(sys::String, value_);
public:
  auto convert(const std::string &) -> Result<void> override;
  FORCEINLINE_ auto value() -> sys::String& { return value_; }

  Value() = default;
 ~Value() override = default;
private:
  sys::String value_{};
};

using PackType = std::vector<sys::String>;

template<>
class Value<PackType> final : public ValueBase {
public:
  auto convert(const sys::String &) -> Result<void> override;
  FORCEINLINE_ auto value() -> PackType& { return value_; }

  Value() = default;
 ~Value() = default;
private:
  PackType value_{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Additional helper structures and enumerations.

struct Parameter {
  sys::String long_;
  sys::String short_;
  sys::String desc_;
  std::unique_ptr<ValueBase> val_;
};

enum class ArgStyle : uint8_t {
  UNIX = 0, /// long form args begin with "--", short form uses "-".
  DOS  = 1, /// Both long and short form begin with "/".
  Masq = 2  /// Long form: "//", short form: "/" (I made this one :D).
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The main Parser class that can be inherited from

class Parser {
N19_MAKE_DEFAULT_MOVE_ASSIGNABLE(Parser);
N19_MAKE_NONCOPYABLE(Parser);
public:
  template<typename T>
  NODISCARD_ auto arg(
    const sys::StringView& longf,        /// Long form flag name
    const sys::StringView& shortf,       /// Short form flag name
    const sys::StringView& desc = "",    /// Optional description
    const Maybe<T>& d = Nothing ) -> T&  /// Default value (optional)
  {
    Parameter param;
    param.long_  = longf;
    param.short_ = shortf;
    param.desc_  = desc;

    auto ptr     = std::make_unique<Value<T>>();
    T& outval    = ptr->value();
    outval       = d.value_or(T{});
    param.val_   = std::move(ptr);

    params_.emplace_back(std::move(param));
    return outval;
  }

private:
  auto print_chunk_error_(size_t at, OStream&, const std::string& msg) const -> void;
  auto already_passed_(size_t index) const -> bool;
  auto is_flag_begin_(const sys::StringView&) const -> bool;

public:
  auto style(const ArgStyle s = ArgStyle::UNIX) -> Parser& {
    arg_style_ = s;       /// set flag name style
    return *this;         ///
  }

  auto take_argv(std::vector<sys::String>&& a) -> Parser& {
    args_ = std::move(a); /// take ownership
    return *this;         ///
  }

  auto take_argv(const int argc, sys::Char** argv) -> Parser& {
    for(int i = 1; i < argc; i++) args_.emplace_back(argv[i]);
    return *this;
  }

  auto parse(OStream& stream) -> Result<void>;
  auto help(OStream& stream) const -> void;

 ~Parser() = default;
  Parser() = default;
protected:
  bool print_errors_ = false;
  ArgStyle arg_style_ = ArgStyle::UNIX;
  std::vector<sys::String> args_;
  std::deque<Parameter> params_;
};

END_NAMESPACE(n19::argp);
#endif //ARGPARSE_HPP
