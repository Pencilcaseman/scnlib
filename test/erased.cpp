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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

TEST_CASE("erase_if_necessary")
{
    SUBCASE("lvalue range_wrapper -> not")
    {
        auto wrapped = scn::wrap("123 456");
        auto r = scn::detail::erase_if_necessary(wrapped);

        static_assert(std::is_same<decltype(r), decltype(wrapped)>::value, "");
    }
    SUBCASE("rvalue range_wrapper -> not")
    {
        auto wrapped = scn::wrap("123 456");
        auto r = scn::detail::erase_if_necessary(SCN_MOVE(wrapped));

        static_assert(std::is_same<decltype(r), decltype(wrapped)>::value, "");
    }
    SUBCASE("string literal -> string_view")
    {
        auto r = scn::detail::erase_if_necessary("123 456");

        static_assert(std::is_same<decltype(r), scn::string_view>::value, "");
    }
    SUBCASE("string_view -> string_view")
    {
        auto r = scn::detail::erase_if_necessary(scn::string_view{"123 456"});

        static_assert(std::is_same<decltype(r), scn::string_view>::value, "");
    }
    SUBCASE("span -> string_view")
    {
        auto str = scn::string_view{"123 456"};
        auto r = scn::detail::erase_if_necessary(
            scn::span<const char>{str.data(), str.size()});

        static_assert(std::is_same<decltype(r), scn::string_view>::value, "");
    }
    SUBCASE("lvalue string -> string_view")
    {
        auto str = std::string{"123 456"};
        auto r = scn::detail::erase_if_necessary(str);

        static_assert(std::is_same<decltype(r), scn::string_view>::value, "");
    }
    SUBCASE("rvalue string -> string")
    {
        auto r = scn::detail::erase_if_necessary(std::string{"123 456"});

        static_assert(std::is_same<decltype(r), std::string>::value, "");
    }
    SUBCASE("lvalue file -> lvalue file")
    {
        auto f = scn::file{};
        auto& r = scn::detail::erase_if_necessary(f);

        static_assert(std::is_same<decltype(r), scn::file&>::value, "");
    }
    SUBCASE("rvalue file -> rvalue file")
    {
        auto r = scn::detail::erase_if_necessary(scn::file{});

        static_assert(std::is_same<decltype(r), scn::file>::value, "");
    }
    SUBCASE("lvalue other -> erased")
    {
        auto s = get_deque<char>("123");
        auto r = scn::detail::erase_if_necessary(s);

        static_assert(std::is_same<decltype(r), scn::erased_range>::value, "");
    }
    SUBCASE("rvalue other -> erased")
    {
        auto r = scn::detail::erase_if_necessary(get_deque<char>("123"));

        static_assert(std::is_same<decltype(r), scn::erased_range>::value, "");
    }
}

TEST_CASE("erased")
{
    auto source = std::string{"abc"};
    auto r = scn::erase_range(source);

    auto it = r.begin();
    auto ret = *it;
    CHECK(ret);
    CHECK(ret.value() == 'a');
    ++it;
    CHECK(it != r.end());

    ret = *it;
    CHECK(ret);
    CHECK(ret.value() == 'b');
    ++it;
    CHECK(it != r.end());

    ret = *it;
    CHECK(ret);
    CHECK(ret.value() == 'c');
    ++it;
    CHECK(it == r.end());

    ret = *it;
    CHECK(!ret);
    CHECK(ret.error() == scn::error::end_of_range);
}

TEST_CASE("wrapped")
{
    auto source = std::string{"123 foo"};
    auto range = scn::erase_range(source);
    auto wrapped = scn::wrap(source);

    std::string str{};
    auto it = std::back_inserter(str);
    auto is_space = scn::detail::make_is_space_predicate(
        scn::make_default_locale_ref<char>(), false);

    auto ret = scn::read_until_space(wrapped, it, is_space, false);
    CHECK(ret);
    CHECK(str == "123");

    // space
    unsigned char buf[4] = {0};
    auto cp = scn::read_code_point(wrapped, scn::make_span(buf, 4));
    CHECK(cp);
    CHECK(cp.value().cp == scn::make_code_point(' '));

    auto s = scn::read_all_zero_copy(wrapped);
    CHECK(s);
    CHECK(s.value().size() == 3);
    CHECK(std::string{s.value().begin(), s.value().end()} == "foo");
}

static_assert(scn::custom_ranges::view<scn::string_view>::value, "");
static_assert(!scn::custom_ranges::view<std::string>::value, "");
static_assert(SCN_CHECK_CONCEPT(scn::ranges::view<scn::string_view>), "");
static_assert(SCN_CHECK_CONCEPT(!scn::ranges::view<std::string>), "");

TEST_CASE("scan")
{
    auto source = std::string{"123 foo"};
    auto range = scn::erase_range(source);

    int i{};
    auto ret = scn::scan(source, "{}", i);
    CHECK(ret);
    CHECK(i == 123);

    std::string str{};
    ret = scn::scan(ret.range(), "{}", str);
    CHECK(ret);
    CHECK(str == "foo");

    ret = scn::scan(ret.range(), "{}", i);
    CHECK(!ret);
    CHECK(ret.error() == scn::error::end_of_range);
}