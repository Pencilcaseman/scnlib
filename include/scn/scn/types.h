// Copyright 2017-2018 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a part of scnlib:
//     https://github.com/eliaskosunen/scnlib

#ifndef SCN_TYPES_H
#define SCN_TYPES_H

#include "core.h"
#include "util.h"

#include <vector>

namespace scn {
    namespace detail {
        template <typename CharT>
        struct empty_parser {
            template <typename Context>
            expected<void, error> parse(Context& ctx)
            {
                if (*ctx.parse_context().begin() != CharT('{')) {
                    return make_unexpected(error::invalid_format_string);
                }
                ctx.parse_context().advance();
                return {};
            }
        };
    }  // namespace detail

    template <typename CharT>
    struct basic_value_scanner<CharT, CharT>
        : public detail::empty_parser<CharT> {
        template <typename Context>
        expected<void, error> scan(CharT& val, Context& ctx)
        {
            auto ch = ctx.stream().read_char();
            if (!ch) {
                return make_unexpected(ch.error());
            }
            val = ch.value();
            return {};
        }
    };

    template <typename CharT>
    struct basic_value_scanner<CharT, span<CharT>>
        : public detail::empty_parser<CharT> {
        template <typename Context>
        expected<void, error> scan(span<CharT> val, Context& ctx)
        {
            if (val.size() == 0) {
                return {};
            }

            std::vector<CharT> buf(static_cast<size_t>(val.size()));
            auto it = buf.begin();
            for (; it != buf.end(); ++it) {
                auto ch = ctx.stream().read_char();
                if (!ch) {
                    for (auto i = buf.begin(); i != it - 1; ++i) {
                        auto pb = ctx.stream().putback(*i);
                        if (!pb) {
                            return pb;
                        }
                    }
                    return make_unexpected(ch.error());
                }
                if (ctx.locale().is_space(ch.value())) {
                    break;
                }
                *it = ch.value();
            }
            std::copy(buf.begin(), buf.end(), val.begin());

            return {};
        }
    };

    template <typename CharT>
    struct basic_value_scanner<CharT, bool>
        : public detail::empty_parser<CharT> {
        template <typename Context>
        expected<void, error> scan(bool& val, Context& ctx)
        {
            auto tmp = ctx.stream().read_char();
            if (!tmp) {
                return make_unexpected(tmp.error());
            }
            if (tmp == CharT('0')) {
                val = false;
                return {};
            }
            if (tmp == CharT('1')) {
                val = true;
                return {};
            }
            {
                auto pb = ctx.stream().putback(tmp.value());
                if (!pb) {
                    return pb;
                }
            }

            const auto max_len = std::max(ctx.locale().truename().size(),
                                          ctx.locale().falsename().size());
            if (max_len < 1) {
                return make_unexpected(error::invalid_scanned_value);
            }
            std::vector<CharT> buf(static_cast<size_t>(max_len));
            auto it = buf.begin();
            for (; it != buf.end(); ++it) {
                auto ch = ctx.stream().read_char();
                if (!ch) {
                    if (ch.error() == error::end_of_stream) {
                        break;
                    }
                    for (auto i = buf.begin(); i != it - 1; ++i) {
                        auto pb = ctx.stream().putback(*i);
                        if (!pb) {
                            return pb;
                        }
                    }
                    return make_unexpected(ch.error());
                }
                *it = ch.value();

                if (std::equal(buf.begin(), it + 1,
                               ctx.locale().falsename().begin())) {
                    val = false;
                    return {};
                }
                if (std::equal(buf.begin(), it + 1,
                               ctx.locale().truename().begin())) {
                    val = true;
                    return {};
                }
            }

            return make_unexpected(error::invalid_scanned_value);
        }
    };

    template <typename CharT, typename T>
    struct basic_value_scanner<
        CharT,
        T,
        typename std::enable_if<std::is_integral<T>::value &&
                                !std::is_same<T, CharT>::value &&
                                !std::is_same<T, bool>::value>::type> {
        template <typename Context>
        expected<void, error> parse(Context& ctx)
        {
            ctx.parse_context().advance();
            const auto ch = *ctx.parse_context().begin();
            if (ch == CharT('}')) {
                base = 10;
                localized = true;
            }
            else if (ch == CharT('d')) {
                base = 10;
            }
            else if (ch == CharT('x')) {
                base = 16;
            }
            else if (ch == CharT('b')) {
                base = 2;
            }
            else if (ch == CharT('o')) {
                base = 8;
            }
            else {
                return make_unexpected(error::invalid_format_string);
            }
            if (ch != CharT('}')) {
                ctx.parse_context().advance();
            }
            return {};
        }

        template <typename Context>
        expected<void, error> scan(T& val, Context& ctx)
        {
            std::vector<CharT> buf(
                static_cast<size_t>(detail::max_digits<T>()) + 1);

            // Copied from span<CharT>
            for (auto it = buf.begin(); it != buf.end(); ++it) {
                auto ch = ctx.stream().read_char();
                if (!ch) {
                    if (ch.error() == error::end_of_stream) {
                        break;
                    }
                    for (auto i = buf.begin(); i != it - 1; ++i) {
                        auto pb = ctx.stream().putback(*i);
                        if (!pb) {
                            return pb;
                        }
                    }
                    return make_unexpected(ch.error());
                }
                if (ctx.locale().is_space(ch.value())) {
                    break;
                }
                if (ctx.locale().thousands_separator() == ch.value()) {
                    continue;
                }
                *it = ch.value();
            }

            T tmp = 0;
            auto it = buf.begin();
            auto sign_tmp = [&]() -> expected<bool, error> {
                if (std::is_unsigned<T>::value) {
                    if (*it == CharT('-')) {
                        return make_unexpected(error::invalid_scanned_value);
                    }
                }
                else {
                    if (*it == CharT('-')) {
                        return false;
                    }
                }

                if (*it == CharT('+')) {
                    return true;
                }
                if (detail::is_digit(ctx.locale(), *it, base, localized)) {
                    tmp = tmp * static_cast<T>(base) -
                          detail::char_to_int<T>(*it, base, localized);
                    return true;
                }
                return make_unexpected(error::invalid_scanned_value);
            }();
            if (!sign_tmp) {
                return make_unexpected(sign_tmp.error());
            }
            const bool sign = sign_tmp.value();
            ++it;

            for (; it != buf.end(); ++it) {
                if (detail::is_digit(ctx.locale(), *it, base, localized)) {
                    tmp = tmp * static_cast<T>(base) -
                          detail::char_to_int<T>(*it, base, localized);
                }
                else {
                    break;
                }
            }

            if (sign) {
#if SCN_MSVC
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
                tmp = -tmp;
#if SCN_MSVC
#pragma warning(pop)
#endif
            }

            val = tmp;
            return {};
        }

        int base{10};
        bool localized{false};
    };

    template <typename CharT, typename T>
    struct basic_value_scanner<
        CharT,
        T,
        typename std::enable_if<std::is_floating_point<T>::value>::type>
        : public detail::empty_parser<CharT> {
        template <typename Context>
        expected<void, error> scan(T& val, Context& ctx)
        {
            std::array<CharT, 64> buf{};
            buf.fill(CharT{0});

            bool point = false;
            for (auto it = buf.begin(); it != buf.end(); ++it) {
                auto tmp = ctx.stream().read_char();
                if (!tmp) {
                    for (auto i = buf.begin(); i != it - 1; ++i) {
                        auto pb = ctx.stream().putback(*i);
                        if (!pb) {
                            return pb;
                        }
                    }
                    return make_unexpected(tmp.error());
                }
                if (tmp.value() == CharT('.')) {
                    if (point) {
                        auto pb = ctx.stream().putback(tmp.value());
                        if (!pb) {
                            return pb;
                        }
                        break;
                    }
                    point = true;
                    *it = tmp.value();
                    continue;
                }
                if (!detail::is_digit(ctx.locale(), tmp.value(), 10, false)) {
                    auto pb = ctx.stream().putback(tmp.value());
                    if (!pb) {
                        return pb;
                    }
                    break;
                }
                *it = tmp.value();
            }

            if (buf[0] == CharT(0)) {
                return make_unexpected(error::invalid_scanned_value);
            }

            CharT* end = buf.data();
            T tmp = detail::str_to_floating<T, CharT>(buf.data(), &end,
                                                      ctx.locale());
            if (&*std::find(buf.begin(), buf.end(), 0) != end) {
                return make_unexpected(error::invalid_scanned_value);
            }
            val = tmp;

            return {};
        }
    };
}  // namespace scn

#endif  // SCN_TYPES_H