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

#ifndef SCN_DETAIL_RESULT_H
#define SCN_DETAIL_RESULT_H

#include "../util/expected.h"
#include "error.h"
#include "range.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    /**
     * Base class for the result type returned by most scanning functions
     * (except for \ref scan_value). \ref scn::detail::scan_result_base inherits
     * either from this class or \ref expected.
     */
    struct wrapped_error {
        wrapped_error() = default;
        wrapped_error(::scn::error e) : err(e) {}

        /// Get underlying error
        SCN_NODISCARD ::scn::error error() const
        {
            return err;
        }

        /// Did the operation succeed -- true means success
        explicit operator bool() const
        {
            return err.operator bool();
        }

        ::scn::error err{};
    };

    namespace detail {
        template <typename Base>
        class scan_result_base_wrapper : public Base {
        public:
            scan_result_base_wrapper(Base&& b) : Base(SCN_MOVE(b)) {}

        protected:
            void set_base(const Base& b)
            {
                static_cast<Base&>(*this) = b;
            }
            void set_base(Base&& b)
            {
                static_cast<Base&>(*this) = SCN_MOVE(b);
            }
        };

        template <typename Range>
        struct result_range_storage_for_view {
            static_assert(SCN_CHECK_CONCEPT(ranges::view<Range>), "");

            using range_type = Range;
            using iterator = ranges::iterator_t<range_type>;
            using sentinel = ranges::sentinel_t<range_type>;
            using char_type = typename extract_char_type<iterator>::type;

            static constexpr bool enable_contiguous_access =
                SCN_CHECK_CONCEPT(ranges::contiguous_range<Range>);

            result_range_storage_for_view(range_type r) : range(SCN_MOVE(r)) {}

            iterator get_begin() const noexcept(
                noexcept(ranges::begin(SCN_DECLVAL(const range_type&))))
            {
                return ranges::begin(range);
            }

            range_type& get_range() & noexcept
            {
                return range;
            }
            const range_type& get_range() const& noexcept
            {
                return range;
            }
            range_type&& get_range() && noexcept
            {
                return SCN_MOVE(range);
            }

            range_type range;
        };

        template <typename Range>
        struct result_range_storage_general_base {
            using range_type = Range;
            using iterator = ranges::iterator_t<range_type>;
            using sentinel = ranges::sentinel_t<range_type>;
            using difference_type = ranges::range_difference_t<range_type>;
            using char_type = typename extract_char_type<iterator>::type;

            static constexpr bool enable_contiguous_access = false;

            result_range_storage_general_base(range_type r,
                                              difference_type begin_diff)
                : range(SCN_MOVE(r)), begin(ranges::begin(range))
            {
                ranges::advance(begin, begin_diff);
            }
            result_range_storage_general_base(range_type&& r, iterator b)
                : range(SCN_MOVE(r)), begin(SCN_MOVE(b))
            {
            }

            iterator get_begin() const noexcept(
                noexcept(std::is_nothrow_copy_assignable<iterator>::value))
            {
                return begin;
            }

            range_type range;
            iterator begin;
        };

        template <typename Range>
        struct result_range_storage_string
            : public result_range_storage_general_base<Range> {
            using base = result_range_storage_general_base<Range>;
            using char_type = typename Range::value_type;

            using base::base;

            basic_string_view<char_type> get_range()
            {
                return {to_address(base::begin), to_address(base::range.end())};
            }
        };

        template <typename Range>
        struct result_range_storage_erased
            : public result_range_storage_general_base<Range> {
            using base = result_range_storage_general_base<Range>;
            using char_type = typename Range::char_type;

            using base::base;

            basic_erased_view<char_type> get_range()
            {
                SCN_EXPECT(base::begin == ranges::begin(base::range));
                return {base::range};
            }
        };

        template <typename Range>
        struct result_range_storage_general
            : public result_range_storage_general_base<Range> {
            using base = result_range_storage_general_base<Range>;

            using base::base;

            basic_erased_range<typename base::char_type> get_range() &&
            {
                const auto diff =
                    ranges::distance(ranges::begin(base::range), base::begin);
                auto r = erase_range(SCN_MOVE(base::range));
                auto b = r.begin();
                ranges::advance(b, diff);
                return {SCN_MOVE(b), r.end()};
            }
        };

        template <typename Storage, typename Base>
        class scan_result_base : public scan_result_base_wrapper<Base> {
            using base_type = scan_result_base_wrapper<Base>;

        public:
            using storage_type = Storage;
            using range_type = typename storage_type::range_type;
            using iterator = typename storage_type::iterator;
            using sentinel = typename storage_type::sentinel;
            using char_type = typename storage_type::char_type;

            scan_result_base(Base&& b, storage_type&& r)
                : base_type(SCN_MOVE(b)), m_storage(SCN_MOVE(r))
            {
            }

            /// Beginning of the leftover range
            iterator begin() const
                noexcept(noexcept(SCN_DECLVAL(const storage_type&).get_begin()))
            {
                return m_storage.get_begin();
            }
            SCN_GCC_PUSH
            SCN_GCC_IGNORE("-Wnoexcept")
            // Mitigate problem where Doxygen would think that SCN_GCC_PUSH was
            // a part of the definition of end()
        public:
            /// End of the leftover range
            sentinel end() const noexcept(
                noexcept(ranges::end(SCN_DECLVAL(const storage_type&).range)))
            {
                return ranges::end(m_storage.range);
            }

            /// Whether the leftover range is empty
            bool empty() const noexcept(noexcept(begin() == end()))
            {
                return begin() == end();
            }
            SCN_GCC_POP
            // See above at SCN_GCC_PUSH
        public:
            /// A subrange pointing to the leftover range
            ranges::subrange<iterator, sentinel> subrange() const
            {
                return {begin(), end()};
            }

            /**
             * Leftover range.
             * If the leftover range is used to scan a new value, this member
             * function should be used.
             *
             * \see range_wrapper
             */
            range_type range()
            {
                return m_storage.get_range();
            }

            /**
             * \defgroup range_as_range Contiguous leftover range convertors
             *
             * These member functions enable more convenient use of the
             * leftover range for non-scnlib use cases. The range must be
             * contiguous. The leftover range is not advanced, and can still be
             * used.
             *
             * @{
             */

            /**
             * \ingroup range_as_range
             * Return a view into the leftover range as a \c string_view.
             * Operations done to the leftover range after a call to this may
             * cause issues with iterator invalidation. The returned range will
             * reference to the leftover range, so be wary of
             * use-after-free-problems.
             */
            template <typename S = storage_type,
                      typename = typename std::enable_if<
                          S::enable_contiguous_access>::type>
            basic_string_view<char_type> range_as_string_view() const
            {
                return {
                    ranges::data(m_storage.range),
                    static_cast<std::size_t>(ranges::size(m_storage.range))};
            }
            /**
             * \ingroup range_as_range
             * Return a view into the leftover range as a \c span.
             * Operations done to the leftover range after a call to this may
             * cause issues with iterator invalidation. The returned range will
             * reference to the leftover range, so be wary of
             * use-after-free-problems.
             */
            template <typename S = storage_type,
                      typename = typename std::enable_if<
                          S::enable_contiguous_access>::type>
            span<const char_type> range_as_span() const
            {
                return {
                    ranges::data(m_storage.range),
                    static_cast<std::size_t>(ranges::size(m_storage.range))};
            }
            /**
             * \ingroup range_as_range
             * Return the leftover range as a string. The contents are copied
             * into the string, so using this will not lead to lifetime issues.
             */
            template <typename S = storage_type,
                      typename = typename std::enable_if<
                          S::enable_contiguous_access>::type>
            std::basic_string<char_type> range_as_string() const
            {
                return {
                    ranges::data(m_storage.range),
                    static_cast<std::size_t>(ranges::size(m_storage.range))};
            }
            /// @}

            storage_type& get_storage()
            {
                return m_storage;
            }
            const storage_type& get_storage() const
            {
                return m_storage;
            }

        private:
            /**
             * Reconstructs a range of the original type, independent of the
             * leftover range, beginning from \ref begin and ending in \ref end.
             *
             * Compiles only if range is reconstructible.
             */
            template <typename R = range_type>
            R reconstruct() const;

            storage_type m_storage;
        };

        // already contains the original range type
        template <typename Storage, typename Base>
        class reconstructed_scan_result
            : public scan_result_base<Storage, Base> {
            using base = scan_result_base<Storage, Base>;

        public:
            reconstructed_scan_result(Base&& b, Storage&& s)
                : base(SCN_MOVE(b), SCN_MOVE(s))
            {
            }

            template <typename R = typename Storage::range_type>
            R reconstruct() const
            {
                return base::range();
            }
        };

        // contains some combination of (erased) range and begin iterator,
        // .reconstruct() to get original range, if possible
        template <typename OriginalRange, typename Storage, typename Base>
        class non_reconstructed_scan_result
            : public scan_result_base<Storage, Base> {
            using base = scan_result_base<Storage, Base>;

        public:
            non_reconstructed_scan_result(Base&& b, Storage&& s)
                : base(SCN_MOVE(b), SCN_MOVE(s))
            {
            }

            non_reconstructed_scan_result& operator=(
                const reconstructed_scan_result<Storage, Base>& other)
            {
                scan_result_base_wrapper<Base>::set_base(other);
                base::get_storage() = other.get_storage();
                return *this;
            }

            template <typename OR>
            non_reconstructed_scan_result& operator=(
                const non_reconstructed_scan_result<OR, Storage, Base>& other)
            {
                scan_result_base_wrapper<Base>::set_base(other);
                base::get_storage() = other.get_storage();
                return *this;
            }

            template <typename R = OriginalRange>
            R reconstruct() &&
            {
                return {base::begin(), base::end()};
            }
        };

        template <typename Range>
        struct range_tag {
        };

        namespace _wrap_result {
            struct fn {
            private:
                // Input = std::string&&
                // Prepared = std::string&&
                // Result = string_view
                template <typename Error,
                          typename CharT,
                          typename Allocator,
                          typename String = std::basic_string<CharT, Allocator>>
                static auto impl(Error e,
                                 range_tag<std::basic_string<CharT, Allocator>>,
                                 std::basic_string<CharT, Allocator>& prepared,
                                 basic_string_view<CharT> result,
                                 priority_tag<3>) noexcept
                    -> non_reconstructed_scan_result<
                        String,
                        result_range_storage_string<String>,
                        Error>
                {
                    const auto size_diff = prepared.size() - result.size();
                    return {SCN_MOVE(e), {SCN_MOVE(prepared), size_diff}};
                }

                // Input = std::string&
                // Prepared = string_view
                // Result = string_view
                template <typename Error,
                          typename CharT,
                          typename Allocator,
                          typename String = std::basic_string<CharT, Allocator>>
                static auto impl(
                    Error e,
                    range_tag<std::basic_string<CharT, Allocator>&>,
                    basic_string_view<CharT>&,
                    basic_string_view<CharT> result,
                    priority_tag<3>) noexcept
                    -> non_reconstructed_scan_result<
                        String,
                        result_range_storage_for_view<basic_string_view<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result)}};
                }

                // Input = C-array &
                // Prepared = string_view
                // Result = string_view
                template <typename Error,
                          typename CharT,
                          typename InputCharT,
                          std::size_t N>
                static auto impl(Error e,
                                 range_tag<InputCharT (&)[N]>,
                                 basic_string_view<CharT>&,
                                 basic_string_view<CharT> result,
                                 priority_tag<3>) noexcept
                    -> reconstructed_scan_result<
                        result_range_storage_for_view<basic_string_view<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result)}};
                }

                // Input = string_view
                // Prepared = string_view
                // Result = string_view
                template <typename Error, typename CharT>
                static auto impl(Error e,
                                 basic_string_view<CharT>,
                                 basic_string_view<CharT>&,
                                 basic_string_view<CharT> result,
                                 priority_tag<3>) noexcept
                    -> reconstructed_scan_result<
                        result_range_storage_for_view<basic_string_view<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result)}};
                }

                // Input = string-like view
                // Prepared = string_view
                // Result = string_view
                template <typename Error, typename InputRange, typename CharT>
                static auto impl(Error e,
                                 range_tag<InputRange>,
                                 basic_string_view<CharT>&,
                                 basic_string_view<CharT> result,
                                 priority_tag<2>)
                    -> non_reconstructed_scan_result<
                        remove_cvref_t<InputRange>,
                        result_range_storage_for_view<basic_string_view<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result)}};
                }

                // Input = erased_view
                // Prepared = erased_view
                // Result = erased_view
                template <typename Error, typename CharT>
                static auto impl(Error e,
                                 range_tag<basic_erased_view<CharT>>,
                                 basic_erased_view<CharT>&,
                                 basic_erased_view<CharT> result,
                                 priority_tag<1>) noexcept
                    -> reconstructed_scan_result<
                        result_range_storage_for_view<basic_erased_view<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result)}};
                }

                // Input = erased_range&
                // Prepared = erased_view
                // Result = erased_view
                template <typename Error, typename CharT>
                static auto impl(Error e,
                                 range_tag<basic_erased_range<CharT>&>,
                                 basic_erased_view<CharT>&,
                                 basic_erased_view<CharT> result,
                                 priority_tag<1>) noexcept
                    -> non_reconstructed_scan_result<
                        basic_erased_range<CharT>,
                        result_range_storage_for_view<basic_erased_view<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result)}};
                }

                // Input = erased_range&&
                // Prepared = erased_range
                // Result = erased_view
                template <typename Error, typename CharT>
                static auto impl(Error e,
                                 range_tag<basic_erased_range<CharT>>,
                                 basic_erased_range<CharT>&,
                                 basic_erased_view<CharT> result,
                                 priority_tag<1>)
                    -> non_reconstructed_scan_result<
                        basic_erased_range<CharT>,
                        result_range_storage_erased<basic_erased_range<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result).get(), 0}};
                }

                // Input = Range
                // Prepared = erased_range
                // Result = erased_view
                template <typename Error, typename InputRange, typename CharT>
                static auto impl(Error e,
                                 range_tag<InputRange>,
                                 basic_erased_range<CharT>&,
                                 basic_erased_view<CharT> result,
                                 priority_tag<0>)
                    -> non_reconstructed_scan_result<
                        remove_cvref_t<InputRange>,
                        result_range_storage_erased<basic_erased_range<CharT>>,
                        Error>
                {
                    return {SCN_MOVE(e), {SCN_MOVE(result).get(), 0}};
                }

            public:
                template <typename Error,
                          typename InputRange,
                          typename PreparedRange,
                          typename ResultRange>
                auto operator()(Error e,
                                range_tag<InputRange> tag,
                                PreparedRange& prepared,
                                ResultRange result) const
                    noexcept(noexcept(fn::impl(SCN_MOVE(e),
                                               tag,
                                               prepared,
                                               SCN_MOVE(result),
                                               priority_tag<3>{})))
                        -> decltype(fn::impl(SCN_MOVE(e),
                                             tag,
                                             prepared,
                                             SCN_MOVE(result),
                                             priority_tag<3>{}))
                {
                    return fn::impl(SCN_MOVE(e), tag, prepared,
                                    SCN_MOVE(result), priority_tag<3>{});
                }
            };
        }  // namespace _wrap_result
        namespace {
            static constexpr auto& wrap_result =
                static_const<_wrap_result::fn>::value;
        }

        template <typename Error,
                  typename InputRange,
                  typename PreparedRange = decltype(prepare(SCN_DECLVAL(
                      typename std::remove_reference<InputRange>::type&))),
                  typename PreparedTargetRange =
                      decltype(SCN_DECLVAL(PreparedRange&).get()),
                  typename ResultRange = typename PreparedRange::target_type>
        struct result_type_for {
            using type = decltype(wrap_result(
                SCN_DECLVAL(Error &&),
                SCN_DECLVAL(range_tag<InputRange>),
                SCN_DECLVAL(typename PreparedRange::stored_type&),
                SCN_DECLVAL(ResultRange&&)));
        };
        template <typename Error, typename InputRange>
        using result_type_for_t =
            typename result_type_for<Error, InputRange>::type;
    }  // namespace detail

    template <typename Error = wrapped_error, typename Range>
    auto make_result(Range&& r) -> detail::result_type_for_t<Error, Range>
    {
        auto prepared = prepare(r);
        return detail::wrap_result(Error{}, detail::range_tag<Range>{},
                                   prepared, prepared.get());
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif  // SCN_DETAIL_RESULT_H
