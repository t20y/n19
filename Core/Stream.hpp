/*
* Copyright (c) 2024 Diago Lima
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef STREAM_HPP
#define STREAM_HPP
#include <Sys/IODevice.hpp>
#include <Core/Bytes.hpp>
#include <Core/Platform.hpp>
#include <Core/Concepts.hpp>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <system_error>
#include <charconv>
#include <limits>

#if defined(N19_WIN32)
#  include <windows.h>
#  include <vector>
#else //POSIX
#  include <locale.h>
#endif

#ifdef N19_LARGE_OSTREAM_BUFFERS
#  define N19_OSTREAM_BUFFSIZE 2048
#else
#  define N19_OSTREAM_BUFFSIZE 1024
#endif

BEGIN_NAMESPACE(n19);

inline constexpr struct Flush_ {
  unsigned char dummy_value_ = 0;
  constexpr Flush_() = default;
} Flush;

inline constexpr struct Endl_ {
  unsigned char dummy_value_ = 0;
  constexpr Endl_() = default;
} Endl;

class OStream {
public:
  using Index_ = size_t;
  using Char_  = Byte;
  using Span_  = std::span<const Char_>;

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Factories

  static auto from_stdout() -> OStream;
  static auto from_stderr() -> OStream;
  static auto from(const sys::IODevice&) -> OStream;

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Begin inline class methods

  FORCEINLINE_ auto operator<<(const Flush_&) -> OStream& {
    this->flush();   /// Flush but don't add a newline
    return *this;    /// return this stream
  }

  FORCEINLINE_ auto operator<<(const Endl_&) -> OStream& {
    *this << '\n';   /// add a new line
    this->flush();   /// Flush
    return *this;    /// return this stream
  }

  template<Character T>
  auto operator<<(const T value) -> OStream& {
    T buffer[ 2 ];
    buffer[0] = value;
    buffer[1] = static_cast<T>(0);
    return *this << buffer;
  }

  template<IntOrFloat T> requires(!IsCharacter<T>)
  auto operator<<(const T value) -> OStream& {
    char buff[ 40 ] = { 0 };   /// Conversion buffer
    auto [ptr, ec]  = std::to_chars(buff, buff + sizeof(buff) - 1, value);

    buff[39] = '\0';           /// shizophrenia
    if(ec == std::errc()) *this << std::string_view{buff};
    return *this;
  }

  template<Pointer T> requires(!IsCharacter<RemovePointer<T>>)
  auto operator<<(const T value) -> OStream& {
    const auto conv  = reinterpret_cast<uintptr_t>(value);
    char buff [ 40 ] = { 0 };  /// conversion buffer
    auto [ptr, ec]   = std::to_chars(buff, buff + sizeof(buff) - 1, conv, 16);

    buff[39] = '\0';           /// shizophrenia
    if(ec == std::errc()) *this << std::string_view{buff};
    return *this;
  }

  auto operator<<(const std::string_view& str) -> OStream& {
    if(!str.empty()) this->write(as_bytes(str));
    return *this;
  }

  auto operator<<(const std::u8string_view& str) -> OStream& {
    if(!str.empty()) this->write(as_bytes(str));
    /// For windows, this relies on the fact that
    /// the code page for the console should have been
    /// set to UTF8 ahead of time. If not, this won't work.
    return *this;
  }

#if defined(N19_WIN32)
  auto operator<<(const std::wstring_view& str) -> COStream& {
    if(str.empty()) return *this;

    const int req_size = ::WideCharToMultiByte(
      CP_UTF8,         /// Code page: UTF-8
      0,               /// Conversion flags
      str.data(),      /// Source UTF-16 string
      str.size(),      /// Number of codepoints
      nullptr,         /// No output buffer yet
      0,               /// Request buffer size
      nullptr, nullptr /// Default char mappings (unused for UTF-8)
    );

    if(req_size == 0) return *this;
    std::vector<char> outbuf((size_t)req_size, '\0');

    const int result = ::WideCharToMultiByte(
      CP_UTF8,         /// Code page: UTF-8
      0,               /// Conversion flags
      str.data(),      /// Source UTF-16 string
      str.size(),      /// Number of codepoints
      outbuf.data(),   /// Destination buffer
      outbuf.size(),   /// Buffer size
      nullptr, nullptr /// Default char mappings (unused for UTF-8)
    );

    std::string_view the_view{ outbuf.begin(), outbuf.end() };
    return *this << the_view;
  }

#else // POSIX
  auto operator<<(const std::wstring_view& str) -> OStream& {
    if(!str.empty()) this->write(as_bytes(str));
    /// For POSIX systems, who cares.
    /// This will never get called anyways,
    /// since UTF16 is only ever used on Windows.
    return *this;
  }
#endif

  virtual auto write(const Span_& buff) -> OStream& {
    fd_.write(buff);     /// Write to the underlying IODevice object.
    return *this;        /// return this stream
  }

  virtual auto flush() -> OStream& {
    fd_.flush_handle();  /// sync handle
    return *this;
  }

  virtual ~OStream() = default;
  OStream() = default;
protected:
  sys::IODevice fd_;
};


template<size_t size_ = N19_OSTREAM_BUFFSIZE>
class BufferedOStream : public OStream {
public:
  constexpr static size_t len_   = size_;
  constexpr static size_t begin_ = 0;
  constexpr static size_t end_   = len_ - 1;

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Factories

  static auto from_stdout() -> BufferedOStream {
    auto the_stream = BufferedOStream<size_>();
    the_stream.fd_  = sys::IODevice::from_stdout();
    return the_stream;
  }

  static auto from_stderr() -> BufferedOStream {
    auto the_stream = BufferedOStream<size_>();
    the_stream.fd_  = sys::IODevice::from_stderr();
    return the_stream;
  }

  static auto from(const sys::IODevice& dev) -> BufferedOStream {
    auto the_stream = BufferedOStream<size_>();
    the_stream.fd_  = dev;
    return the_stream;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Type aliases

  using Buffer_ = OStream::Char_[ len_ ];
  using Index_  = OStream::Index_;

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Begin inline class methods

  FORCEINLINE_ auto to_buffer(const Span_& buff) -> OStream& {
    ASSERT(curr_ <= len_, "Invalid current buffer index.");
    ASSERT(buff.size_bytes() <= (len_ - curr_), "Buffer overrun!");
    ASSERT(buff.size_bytes() != 0, "to_buffer: empty span!");

    std::memcpy(&buff_[curr_], buff.data(), buff.size_bytes());
    curr_ += buff.size_bytes();
    return *this;
  }

  FORCEINLINE_ auto flush() -> OStream& override {
    ASSERT(curr_ <= len_, "Buffer overrun!");
    if(curr_ > begin_) {
      fd_.write(Bytes{&buff_[begin_], curr_});
      curr_ = begin_;         /// Reset the buffer index.
    }                         ///

    fd_.flush_handle();       /// FIXME: this is retarded.
    return *this;             /// return instance
  }

  auto write(const Span_& buff) -> OStream& override {
    const size_t remaining = len_ - curr_;
    const size_t size_new  = buff.size_bytes();
    const bool   no_space  = remaining < size_new;

    if(buff.empty()) {        /// Disallow empty buffers.
      return *this;           /// Early return.
    } if(size_new > len_) {   /// buffer is too large.
      flush();                ///
      fd_.write(buff);        /// write contents directly.
    } else if(no_space) {     /// remaining size is too small,
      flush();                /// flush the buffer.
      to_buffer(buff);        /// copy the new contents over.
    } else {                  ///
      to_buffer(buff);        /// copy the new contents over.
    }                         ///

    return *this;
  }

 ~BufferedOStream() override = default;
  BufferedOStream() = default;
public:
  Buffer_ buff_{};
  Index_ curr_ {begin_};
};

class NullOStream final : public OStream {
public:
  auto write(const Span_&) -> OStream & override { return *this; }
  auto flush() -> OStream & override { return *this; }

 ~NullOStream() override = default;
  NullOStream() = default;
};

class IStream {
public:
  static auto from_stdin() -> IStream;
  static auto from(const sys::IODevice&) -> IStream;

  FORCEINLINE_ ~IStream() { fd_.flush_handle(); }
  FORCEINLINE_ IStream()  { /* ... */ }
private:
  sys::IODevice fd_;
};

END_NAMESPACE(n19);
#endif //STREAM_HPP
