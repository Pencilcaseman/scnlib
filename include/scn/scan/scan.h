// Copyright 2017 Elias Kosunen
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

#ifndef SCN_DETAIL_SCAN_H
#define SCN_DETAIL_SCAN_H

#include "../detail/context.h"
#include "../detail/parse_context.h"
#include "../reader/common.h"
#include "../detail/result.h"
#include "../util/optional.h"
#include "../scan/vscan.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename Error, typename Range>
        using generic_scan_result_for_range = decltype(detail::wrap_result(
            SCN_DECLVAL(Error),
            SCN_DECLVAL(detail::range_tag<Range>),
            SCN_DECLVAL(range_wrapper_for_t<Range>)));
        template <typename Range>
        using scan_result_for_range =
            generic_scan_result_for_range<wrapped_error, Range>;

        template <typename Range, typename Format, typename... Args>
        auto scan_boilerplate(Range&& r, const Format& f, Args&... a)
            -> detail::scan_result_for_range<Range>
        {
            static_assert(sizeof...(Args) > 0,
                          "Have to scan at least a single argument");
            static_assert(SCN_CHECK_CONCEPT(ranges::range<Range>),
                          "Input needs to be a Range");

            auto range = wrap(SCN_FWD(r));
            auto format = detail::to_format(f);
            auto args = make_args_for(range, format, a...);
            auto ret = vscan(SCN_MOVE(range), format, {args});
            return detail::wrap_result(wrapped_error{ret.err},
                                       detail::range_tag<Range>{},
                                       SCN_MOVE(ret.range));
        }

        template <typename Range, typename... Args>
        auto scan_boilerplate_default(Range&& r, Args&... a)
            -> detail::scan_result_for_range<Range>
        {
            static_assert(sizeof...(Args) > 0,
                          "Have to scan at least a single argument");
            static_assert(SCN_CHECK_CONCEPT(ranges::range<Range>),
                          "Input needs to be a Range");

            auto range = wrap(SCN_FWD(r));
            auto format = static_cast<int>(sizeof...(Args));
            auto args = make_args_for(range, format, a...);
            auto ret = vscan_default(SCN_MOVE(range), format, {args});
            return detail::wrap_result(wrapped_error{ret.err},
                                       detail::range_tag<Range>{},
                                       SCN_MOVE(ret.range));
        }

        template <typename Locale,
                  typename Range,
                  typename Format,
                  typename... Args>
        auto scan_boilerplate_localized(const Locale& loc,
                                        Range&& r,
                                        const Format& f,
                                        Args&... a)
            -> detail::scan_result_for_range<Range>
        {
            static_assert(sizeof...(Args) > 0,
                          "Have to scan at least a single argument");
            static_assert(SCN_CHECK_CONCEPT(ranges::range<Range>),
                          "Input needs to be a Range");

            auto range = wrap(SCN_FWD(r));
            auto format = detail::to_format(f);
            SCN_CLANG_PUSH_IGNORE_UNDEFINED_TEMPLATE
            auto locale =
                make_locale_ref<typename decltype(range)::char_type>(loc);
            SCN_CLANG_POP_IGNORE_UNDEFINED_TEMPLATE

            auto args = make_args_for(range, format, a...);
            auto ret = vscan_localized(SCN_MOVE(range), SCN_MOVE(locale),
                                       format, {args});
            return detail::wrap_result(wrapped_error{ret.err},
                                       detail::range_tag<Range>{},
                                       SCN_MOVE(ret.range));
        }
    }  // namespace detail

    // scan

    /**
     * The most fundamental part of the scanning API.
     * Reads from the range in \c r according to the format string \c f.
     *
     * \code{.cpp}
     * int i;
     * scn::scan("123", "{}", i);
     * // i == 123
     * \endcode
     */
    template <typename Range, typename Format, typename... Args>
    SCN_NODISCARD auto scan(Range&& r, const Format& f, Args&... a)
        -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate(SCN_FWD(r), f, a...);
    }

    // default format

    /**
     * Equivalent to \ref scan, but with a
     * format string with the appropriate amount of space-separated `"{}"`s for
     * the number of arguments. Because this function doesn't have to parse the
     * format string, performance is improved.
     *
     * Adapted from the example for \ref scan
     * \code{.cpp}
     * int i;
     * scn::scan_default("123", i);
     * // i == 123
     * \endcode
     *
     * \see scan
     */
    template <typename Range, typename... Args>
    SCN_NODISCARD auto scan_default(Range&& r, Args&... a)
        -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate_default(std::forward<Range>(r), a...);
    }

    // scan localized

    /**
     * Read from the range in \c r using the locale in \c loc.
     * \c loc must be a \c std::locale. The parameter is a template to avoid
     * inclusion of `<locale>`.
     *
     * Use of this function is discouraged, due to the overhead involved with
     * locales. Note, that the other functions are completely locale-agnostic,
     * and aren't affected by changes to the global C locale.
     *
     * \code{.cpp}
     * double d;
     * scn::scan_localized(std::locale{"fi_FI"}, "3,14", "{}", d);
     * // d == 3.14
     * \endcode
     *
     * \see scan
     */
    template <typename Locale,
              typename Range,
              typename Format,
              typename... Args>
    SCN_NODISCARD auto scan_localized(const Locale& loc,
                                      Range&& r,
                                      const Format& f,
                                      Args&... a)
        -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate_localized(loc, std::forward<Range>(r),
                                                  f, a...);
    }

    // value

    /**
     * Scans a single value with the default options, returning it instead of
     * using an output parameter.
     *
     * The parsed value is in `ret.value()`, if `ret == true`.
     * The return type of this function is otherwise similar to other scanning
     * functions.
     *
     * \code{.cpp}
     * auto ret = scn::scan_value<int>("42");
     * if (ret) {
     *   // ret.value() == 42
     * }
     * \endcode
     */
    template <typename T, typename Range>
    SCN_NODISCARD auto scan_value(Range&& r)
        -> detail::generic_scan_result_for_range<expected<T>, Range>
    {
        T value;
        auto range = wrap(SCN_FWD(r));
        auto args = make_args_for(range, 1, value);
        auto ctx = make_context(SCN_MOVE(range));
        auto pctx = make_parse_context(1, ctx.locale());

        auto err = visit(ctx, pctx, {args});
        if (!err) {
            return detail::wrap_result(expected<T>{err},
                                       detail::range_tag<Range>{},
                                       SCN_MOVE(ctx.range()));
        }
        return detail::wrap_result(expected<T>{value},
                                   detail::range_tag<Range>{},
                                   SCN_MOVE(ctx.range()));
    }

    // input

    /**
     * Otherwise equivalent to \ref scan, expect reads from `stdin`.
     * Character type is determined by the format string.
     * Syncs with `<cstdio>`.
     */
    template <typename Format,
              typename... Args,
              typename CharT = ranges::range_value_t<Format>>
    SCN_NODISCARD auto input(const Format& f, Args&... a)
        -> detail::scan_result_for_range<basic_file<CharT>&>
    {
        auto& range = stdin_range<CharT>();
        auto ret = detail::scan_boilerplate(range, f, a...);
        range.sync();
        return ret;
    }

    // prompt

    namespace detail {
        inline void put_stdout(const char* str)
        {
            std::fputs(str, stdout);
        }
        inline void put_stdout(const wchar_t* str)
        {
            std::fputws(str, stdout);
        }
    }  // namespace detail

    /**
     * Equivalent to \ref input, except writes what's in `p` to `stdout`.
     *
     * \code{.cpp}
     * int i{};
     * scn::prompt("What's your favorite number? ", "{}", i);
     * // Equivalent to:
     * //   std::fputs("What's your favorite number? ", stdout);
     * //   scn::input("{}", i);
     * \endcode
     */
    template <typename CharT, typename Format, typename... Args>
    SCN_NODISCARD auto prompt(const CharT* p, const Format& f, Args&... a)
        -> decltype(input(f, a...))
    {
        SCN_EXPECT(p != nullptr);
        detail::put_stdout(p);

        return input(f, a...);
    }

    // parse_integer

    /**
     * Parses an integer into \c val in base \c base from \c str.
     * Returns a pointer past the last character read, or an error.
     *
     * @param str source, can't be empty, cannot have:
     *   - preceding whitespace
     *   - preceding \c "0x" or \c "0" (base is determined by the \c base
     * parameter)
     *   - \c '+' sign (\c '-' is fine)
     * @param val parsed integer, must be value-constructed
     * @param base between [2,36]
     */
    template <typename T, typename CharT>
    SCN_NODISCARD expected<const CharT*>
    parse_integer(basic_string_view<CharT> str, T& val, int base = 10)
    {
        SCN_EXPECT(!str.empty());
        auto s = detail::simple_integer_scanner<T>{};
        SCN_CLANG_PUSH_IGNORE_UNDEFINED_TEMPLATE
        auto ret =
            s.scan_lower(span<const CharT>(str.data(), str.size()), val, base);
        SCN_CLANG_POP_IGNORE_UNDEFINED_TEMPLATE
        if (!ret) {
            return ret.error();
        }
        return {ret.value()};
    }

    /**
     * Parses float into \c val from \c str.
     * Returns a pointer past the last character read, or an error.
     *
     * @param str source, can't be empty
     * @param val parsed float, must be value-constructed
     */
    template <typename T, typename CharT>
    SCN_NODISCARD expected<const CharT*> parse_float(
        basic_string_view<CharT> str,
        T& val)
    {
        SCN_EXPECT(!str.empty());
        auto s = detail::float_scanner_access<T>{};
        auto ret = s._read_float(val, make_span(str.data(), str.size()),
                                 detail::ascii_widen<CharT>('.'));
        if (!ret) {
            return ret.error();
        }
        return {str.data() + ret.value()};
    }

    /**
     * A convenience function for creating scanners for user-provided types.
     *
     * Wraps \ref vscan_usertype
     *
     * Example use:
     *
     * \code{.cpp}
     * // Type has two integers, and its textual representation is
     * // "[val1, val2]"
     * struct user_type {
     *     int val1;
     *     int val2;
     * };
     *
     * template <>
     * struct scn::scanner<user_type> : public scn::empty_parser {
     *     template <typename Context>
     *     error scan(user_type& val, Context& ctx)
     *     {
     *         return scan_usertype(ctx, "[{}, {}]", val.val1, val.val2);
     *     }
     * };
     * \endcode
     *
     * \param ctx Context given to the scanning function
     * \param f Format string to parse
     * \param a Member types (etc) to parse
     */
    template <typename WrappedRange, typename Format, typename... Args>
    SCN_NODISCARD error scan_usertype(basic_context<WrappedRange>& ctx,
                                      const Format& f,
                                      Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");

        using char_type = typename WrappedRange::char_type;
        auto args = make_args<basic_context<WrappedRange>,
                              basic_parse_context<char_type>>(a...);
        return vscan_usertype(ctx, basic_string_view<char_type>(f), {args});
    }

    // scanning api

    // getline

    namespace detail {
        template <typename CharT>
        struct until_pred {
            array<CharT, 4> until;
            size_t size;

            constexpr until_pred(CharT ch) : until({{ch}}), size(1) {}
            until_pred(code_point cp)
            {
                auto ret = encode_code_point(until.begin(), until.end(), cp);
                SCN_ENSURE(ret);
                size = ret.value() - until.begin();
            }

            SCN_CONSTEXPR14 bool operator()(span<const CharT> ch) const
            {
                if (ch.size() != size) {
                    return false;
                }
                for (size_t i = 0; i < ch.size(); ++i) {
                    if (ch[i] != until[i]) {
                        return false;
                    }
                }
                return true;
            }
            static constexpr bool is_localized()
            {
                return false;
            }
            constexpr bool is_multibyte() const
            {
                return size != 1;
            }
        };

        template <typename WrappedRange,
                  typename String,
                  typename Until,
                  typename CharT = typename WrappedRange::char_type>
        error getline_impl(WrappedRange& r, String& str, Until until)
        {
            auto pred = until_pred<CharT>{until};
            auto s = read_until_space_zero_copy(r, pred, true);
            if (!s) {
                return s.error();
            }
            if (s.value().size() != 0) {
                auto size = s.value().size();
                if (pred(s.value().last(1))) {
                    --size;
                }
                str.clear();
                str.resize(size);
                std::copy(s.value().begin(), s.value().begin() + size,
                          str.begin());
                return {};
            }

            String tmp;
            auto out = std::back_inserter(tmp);
            auto e = read_until_space(r, out, until_pred<CharT>{until}, true);
            if (!e) {
                return e;
            }
            if (pred(span<const CharT>(&*(tmp.end() - 1), 1))) {
                tmp.pop_back();
            }
            r.advance();
            str = SCN_MOVE(tmp);
            return {};
        }
        template <typename WrappedRange,
                  typename Until,
                  typename CharT = typename WrappedRange::char_type>
        error getline_impl(WrappedRange& r,
                           basic_string_view<CharT>& str,
                           Until until)
        {
            static_assert(
                WrappedRange::is_contiguous,
                "Cannot getline a string_view from a non-contiguous range");
            auto pred = until_pred<CharT>{until};
            auto s = read_until_space_zero_copy(r, pred, true);
            if (!s) {
                return s.error();
            }
            SCN_ASSERT(s.value().size(), "");
            auto size = s.value().size();
            if (pred(s.value().last(1))) {
                --size;
            }
            str = basic_string_view<CharT>{s.value().data(), size};
            return {};
        }
#if SCN_HAS_STRING_VIEW
        template <typename WrappedRange,
                  typename Until,
                  typename CharT = typename WrappedRange::char_type>
        auto getline_impl(WrappedRange& r,
                          std::basic_string_view<CharT>& str,
                          Until until) -> error
        {
            auto sv = ::scn::basic_string_view<CharT>{};
            auto ret = getline_impl(r, sv, until);
            str = ::std::basic_string_view<CharT>{sv.data(), sv.size()};
            return ret;
        }
#endif
    }  // namespace detail

    /**
     * Read the range in \c r into \c str until \c until is found.
     * \c until will be skipped in parsing: it will not be pushed into \c
     * str, and the returned range will go past it.
     *
     * \c r and \c str must share character types, which must be \c CharT.
     *
     * If `str` is convertible to a `basic_string_view`:
     *  - And if `r` is a `contiguous_range`:
     *    - `str` is set to point inside `r` with the appropriate length
     *  - if not, returns an error
     *
     * Otherwise, clears `str` by calling `str.clear()`, and then reads the
     * range into `str` as if by repeatedly calling \c str.push_back.
     * `str.reserve` is also required to be present.
     *
     * \code{.cpp}
     * auto source = "hello\nworld"
     * std::string line;
     * auto result = scn::getline(source, line, '\n');
     * // line == "hello"
     * // result.range() == "world"
     *
     * // Using the other overload
     * result = scn::getline(result.range(), line);
     * // line == "world"
     * // result.empty() == true
     * \endcode
     */
    template <typename Range, typename String, typename Until>
    SCN_NODISCARD auto getline(Range&& r, String& str, Until until)
        -> detail::scan_result_for_range<Range>
    {
        auto wrapped = wrap(SCN_FWD(r));
        auto err = getline_impl(wrapped, str, until);
        if (!err) {
            auto e = wrapped.reset_to_rollback_point();
            if (!e) {
                err = e;
            }
        }
        else {
            wrapped.set_rollback_point();
        }
        return detail::wrap_result(
            wrapped_error{err}, detail::range_tag<Range>{}, SCN_MOVE(wrapped));
    }

    /**
     * Equivalent to \ref getline with the last parameter set to
     * <tt>'\\n'</tt> with the appropriate character type.
     *
     * In other words, reads `r` into `str` until <tt>'\\n'</tt> is found.
     *
     * The character type is determined by `r`.
     */
    template <typename Range,
              typename String,
              typename CharT = typename detail::extract_char_type<
                  ranges::iterator_t<range_wrapper_for_t<Range>>>::type>
    SCN_NODISCARD auto getline(Range&& r, String& str)
        -> detail::scan_result_for_range<Range>
    {
        return getline(std::forward<Range>(r), str,
                       detail::ascii_widen<CharT>('\n'));
    }

    // ignore

    namespace detail {
        template <typename CharT>
        struct ignore_iterator {
            using value_type = CharT;
            using pointer = value_type*;
            using reference = value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::output_iterator_tag;

            constexpr ignore_iterator() = default;

            SCN_CONSTEXPR14 ignore_iterator& operator=(CharT) noexcept
            {
                return *this;
            }
            constexpr const ignore_iterator& operator=(CharT) const noexcept
            {
                return *this;
            }

            SCN_CONSTEXPR14 ignore_iterator& operator*() noexcept
            {
                return *this;
            }
            constexpr const ignore_iterator& operator*() const noexcept
            {
                return *this;
            }

            SCN_CONSTEXPR14 ignore_iterator& operator++() noexcept
            {
                return *this;
            }
            constexpr const ignore_iterator& operator++() const noexcept
            {
                return *this;
            }
        };

        template <typename CharT>
        struct ignore_iterator_n {
            using value_type = CharT;
            using pointer = value_type*;
            using reference = value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::output_iterator_tag;

            ignore_iterator_n() = default;
            ignore_iterator_n(difference_type n) : i(n) {}

            constexpr const ignore_iterator_n& operator=(CharT) const noexcept
            {
                return *this;
            }

            constexpr const ignore_iterator_n& operator*() const noexcept
            {
                return *this;
            }

            SCN_CONSTEXPR14 ignore_iterator_n& operator++() noexcept
            {
                ++i;
                return *this;
            }

            constexpr bool operator==(const ignore_iterator_n& o) const noexcept
            {
                return i == o.i;
            }
            constexpr bool operator!=(const ignore_iterator_n& o) const noexcept
            {
                return !(*this == o);
            }

            difference_type i{0};
        };

        template <typename WrappedRange,
                  typename Until,
                  typename CharT = typename WrappedRange::char_type>
        error ignore_until_impl(WrappedRange& r, Until until)
        {
            ignore_iterator<CharT> it{};
            return read_until_space(r, it, until_pred<CharT>{until}, false);
        }

        template <typename WrappedRange,
                  typename Until,
                  typename CharT = typename WrappedRange::char_type>
        error ignore_until_n_impl(WrappedRange& r,
                                  ranges::range_difference_t<WrappedRange> n,
                                  Until until)
        {
            ignore_iterator_n<CharT> begin{}, end{n};
            return read_until_space_ranged(r, begin, end,
                                           until_pred<CharT>{until}, false);
        }
    }  // namespace detail

    /**
     * Advances the beginning of \c r until \c until is found.
     */
    template <typename Range, typename Until>
    SCN_NODISCARD auto ignore_until(Range&& r, Until until)
        -> detail::scan_result_for_range<Range>
    {
        auto wrapped = wrap(SCN_FWD(r));
        auto err = detail::ignore_until_impl(wrapped, until);
        if (!err) {
            auto e = wrapped.reset_to_rollback_point();
            if (!e) {
                err = e;
            }
        }
        else {
            wrapped.set_rollback_point();
        }
        return detail::wrap_result(
            wrapped_error{err}, detail::range_tag<Range>{}, SCN_MOVE(wrapped));
    }

    /**
     * Advances the beginning of \c r until \c until is found, or the
     * beginning has been advanced \c n times. The character type of \c r
     * must be \c CharT.
     */
    template <typename Range, typename Until>
    SCN_NODISCARD auto ignore_until_n(Range&& r,
                                      ranges::range_difference_t<Range> n,
                                      Until until)
        -> detail::scan_result_for_range<Range>
    {
        auto wrapped = wrap(SCN_FWD(r));
        auto err = detail::ignore_until_n_impl(wrapped, n, until);
        if (!err) {
            auto e = wrapped.reset_to_rollback_point();
            if (!e) {
                err = e;
            }
        }
        return detail::wrap_result(
            wrapped_error{err}, detail::range_tag<Range>{}, SCN_MOVE(wrapped));
    }

    /**
     * Adapts a `span` into a type that can be read into using \ref
     * scan_list. This way, potentially unnecessary dynamic memory
     * allocations can be avoided. To use as a parameter to \ref scan_list,
     * use \ref make_span_list_wrapper.
     *
     * \code{.cpp}
     * std::vector<int> buffer(8, 0);
     * scn::span<int> s = scn::make_span(buffer);
     *
     * auto wrapper = scn::span_list_wrapper<int>(s);
     * scn::scan_list("123 456", wrapper);
     * // s[0] == buffer[0] == 123
     * // s[1] == buffer[1] == 456
     * \endcode
     *
     * \see scan_list
     * \see make_span_list_wrapper
     */
    template <typename T>
    struct span_list_wrapper {
        using value_type = T;

        span_list_wrapper(span<T> s) : m_span(s) {}

        void push_back(T val)
        {
            SCN_EXPECT(n < max_size());
            m_span[n] = SCN_MOVE(val);
            ++n;
        }

        SCN_NODISCARD constexpr std::size_t size() const noexcept
        {
            return n;
        }
        SCN_NODISCARD constexpr std::size_t max_size() const noexcept
        {
            return m_span.size();
        }

        span<T> m_span;
        std::size_t n{0};
    };

    namespace detail {
        template <typename T>
        using span_list_wrapper_for =
            span_list_wrapper<typename decltype(make_span(
                SCN_DECLVAL(T&)))::value_type>;
    }

    /**
     * Adapts a contiguous buffer into a type containing a `span` that can
     * be read into using \ref scan_list.
     *
     * Example adapted from \ref span_list_wrapper:
     * \code{.cpp}
     * std::vector<int> buffer(8, 0);
     * scn::scan_list("123 456", scn::make_span_list_wrapper(buffer));
     * // s[0] == buffer[0] == 123
     * // s[1] == buffer[1] == 456
     * \endcode
     *
     * \see scan_list
     * \see span_list_wrapper
     */
    template <typename T>
    auto make_span_list_wrapper(T& s)
        -> temporary<detail::span_list_wrapper_for<T>>
    {
        auto _s = make_span(s);
        return temp(span_list_wrapper<typename decltype(_s)::value_type>(_s));
    }

    namespace detail {
        template <typename CharT>
        struct zero_value;
        template <>
        struct zero_value<char> : std::integral_constant<char, 0> {
        };
        template <>
        struct zero_value<wchar_t> : std::integral_constant<wchar_t, 0> {
        };

        template <typename WrappedRange, typename CharT>
        expected<CharT> read_single(WrappedRange& r, CharT)
        {
            return read_code_unit(r);
        }
        template <typename WrappedRange>
        expected<code_point> read_single(WrappedRange& r, code_point)
        {
            unsigned char buf[4] = {0};
            auto ret = read_code_point(r, make_span(buf, 4), true);
            if (!ret) {
                return ret.error();
            }
            return ret.value().cp;
        }
    }  // namespace detail

    /**
     * Reads values repeatedly from `r` and writes them into `c`.
     * The values read are of type `Container::value_type`, and they are
     * written into `c` using `c.push_back`.
     *
     * The values must be separated by separator
     * character `separator`, followed by whitespace. If `separator == 0`,
     * no separator character is expected.
     *
     * The range is read, until:
     *  - `c.max_size()` is reached, or
     *  - range `EOF` was reached, or
     *  - unexpected separator character was found between values.
     *
     * In all these cases, an error will not be returned, and the beginning
     * of the returned range will point to the first character after the
     * scanned list.
     *
     * To scan into `span`, use \ref span_list_wrapper.
     * \ref make_span_list_wrapper
     *
     * \code{.cpp}
     * std::vector<int> vec{};
     * auto result = scn::scan_list("123 456", vec);
     * // vec == [123, 456]
     * // result.empty() == true
     *
     * result = scn::scan_list("123, 456", vec, ',');
     * // vec == [123, 456]
     * // result.empty() == true
     * \endcode
     */
    template <typename Range,
              typename Container,
              typename Separator = typename detail::extract_char_type<
                  ranges::iterator_t<Range>>::type>
    SCN_NODISCARD auto scan_list(
        Range&& r,
        Container& c,
        Separator separator = detail::zero_value<Separator>::value)
        -> detail::scan_result_for_range<Range>
    {
        using value_type = typename Container::value_type;
        value_type value;

        auto range = wrap(SCN_FWD(r));
        using char_type = typename decltype(range)::char_type;

        auto args = make_args_for(range, 1, value);
        auto ctx = make_context(SCN_MOVE(range));
        auto pctx = make_parse_context(1, ctx.locale());
        auto cargs = basic_args<char_type>{args};

        while (true) {
            if (c.size() == c.max_size()) {
                break;
            }

            pctx.reset_args_left(1);
            auto err = visit(ctx, pctx, cargs);
            if (!err) {
                if (err == error::end_of_range) {
                    break;
                }
                return detail::wrap_result(wrapped_error{err},
                                           detail::range_tag<Range>{},
                                           SCN_MOVE(ctx.range()));
            }
            c.push_back(SCN_MOVE(value));

            if (separator != 0) {
                auto sep_ret = detail::read_single(ctx.range(), separator);
                if (!sep_ret) {
                    if (sep_ret.error() == scn::error::end_of_range) {
                        break;
                    }
                    return detail::wrap_result(wrapped_error{sep_ret.error()},
                                               detail::range_tag<Range>{},
                                               SCN_MOVE(ctx.range()));
                }
                if (sep_ret.value() == separator) {
                    continue;
                }
                else {
                    // Unexpected character, assuming end
                    break;
                }
            }
        }
        return detail::wrap_result(wrapped_error{}, detail::range_tag<Range>{},
                                   SCN_MOVE(ctx.range()));
    }

    /**
     * Otherwise equivalent to \ref scan_list, except with an additional
     * case of stopping scanning: if `until` is found where a separator was
     * expected.
     *
     * \see scan_list
     *
     * \code{.cpp}
     * std::vector<int> vec{};
     * auto result = scn::scan_list_until("123 456\n789", vec, '\n');
     * // vec == [123, 456]
     * // result.range() == "789"
     * \endcode
     */
    template <typename Range,
              typename Container,
              typename Separator = typename detail::extract_char_type<
                  ranges::iterator_t<Range>>::type>
    SCN_NODISCARD auto scan_list_until(
        Range&& r,
        Container& c,
        Separator until,
        Separator separator = detail::zero_value<Separator>::value)
        -> detail::scan_result_for_range<Range>
    {
        using value_type = typename Container::value_type;
        value_type value;

        auto range = wrap(SCN_FWD(r));
        using char_type = typename decltype(range)::char_type;

        auto args = make_args_for(range, 1, value);
        auto ctx = make_context(SCN_MOVE(range));

        bool scanning = true;
        while (scanning) {
            if (c.size() == c.max_size()) {
                break;
            }

            auto pctx = make_parse_context(1, ctx.locale());
            auto err = visit(ctx, pctx, basic_args<char_type>{args});
            if (!err) {
                if (err == error::end_of_range) {
                    break;
                }
                return detail::wrap_result(wrapped_error{err},
                                           detail::range_tag<Range>{},
                                           SCN_MOVE(ctx.range()));
            }
            c.push_back(SCN_MOVE(value));

            bool sep_found = false;
            while (true) {
                auto next = read_code_unit(ctx.range(), false);
                if (!next) {
                    if (next.error() == scn::error::end_of_range) {
                        scanning = false;
                        break;
                    }
                    return detail::wrap_result(wrapped_error{next.error()},
                                               detail::range_tag<Range>{},
                                               SCN_MOVE(ctx.range()));
                }

                if (next.value() == until) {
                    scanning = false;
                    break;
                }

                if (ctx.locale().get_static().is_space(next.value())) {
                    ctx.range().advance();
                    continue;
                }

                if (separator != 0) {
                    if (next.value() != separator || sep_found) {
                        break;
                    }
                    else {
                        ctx.range().advance();
                        sep_found = true;
                    }
                }
                else {
                    break;
                }
            }
        }
        return detail::wrap_result(wrapped_error{}, detail::range_tag<Range>{},
                                   SCN_MOVE(ctx.range()));
    }

    template <typename T>
    struct discard_type {
        discard_type() = default;
    };

    /**
     * Scans an instance of `T`, but doesn't store it anywhere.
     * Uses `scn::temp` internally, so the user doesn't have to bother.
     *
     * \code{.cpp}
     * int i{};
     * // 123 is discarded, 456 is read into `i`
     * auto result = scn::scan("123 456", "{} {}", scn::discard<T>(), i);
     * // result == true
     * // i == 456
     * \endcode
     */
    template <typename T>
    discard_type<T>& discard()
    {
        return temp(discard_type<T>{})();
    }

    template <typename T>
    struct scanner<discard_type<T>> : public scanner<T> {
        template <typename Context>
        error scan(discard_type<T>&, Context& ctx)
        {
            T tmp;
            return scanner<T>::scan(tmp, ctx);
        }
    };

    SCN_END_NAMESPACE
}  // namespace scn

#endif  // SCN_DETAIL_SCAN_H