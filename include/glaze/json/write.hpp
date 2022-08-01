// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif  // !FMT_HEADER_ONLY
#include "fmt/format.h"
#include "fmt/compile.h"

#include <charconv>
#include <iterator>
#include <ostream>

#include "glaze/core/format.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/dump.hpp"

namespace glaze
{
   namespace detail
   {
      template <>
      struct write<json>
      {
         template <bool C, class T, class B>
         static void op(T&& value, B&& b) {
            to_json<std::decay_t<T>>::template op<C>(std::forward<T>(value), std::forward<B>(b));
         }
      };
      
      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_json<T>
      {
         template <bool C>
         static void op(const bool value, auto&& b) noexcept
         {
            if (value) {
               dump<"true">(b);
            }
            else {
               dump<"false">(b);
            }
         }
      };
      
      template <num_t T>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
               // more efficient strings in C++23:
             https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
             }*/
            fmt::format_to(std::back_inserter(b), FMT_COMPILE("{}"), value);
         }
      };

      template <class T>
      requires str_t<T> || char_t<T>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'"'>(b);
            const auto write_char = [&](auto&& c) {
               switch (c) {
               case '\\':
               case '"':
                  dump<'\\'>(b);
                  break;
               }
               dump(c, b);
            };
            if constexpr (char_t<T>) {
               write_char(value);
            }
            else {
               const std::string_view str = value;
               for (auto&& c : str) {
                  write_char(c);
               }
            }
            dump<'"'>(b);
         }
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_json<T>
      {
         template <bool C>
         static void op(T&& value, auto&& b) noexcept {
            dump(value.str, b);
         }
      };
      
      template <array_t T>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'['>(b);
            if (!value.empty()) {
               auto it = value.cbegin();
               write<json>::op<C>(*it, b);
               ++it;
               const auto end = value.cend();
               for (; it != end; ++it) {
                  dump<','>(b);
                  write<json>::op<C>(*it, b);
               }
            }
            dump<']'>(b);
         }
      };

      template <map_t T>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'{'>(b);
            auto it = value.cbegin();
            auto write_pair = [&] {
               using Key = decltype(it->first);
               if constexpr (str_t<Key> || char_t<Key>) {
                  write<json>::op<C>(it->first, b);
               }
               else {
                  dump<'"'>(b);
                  write<json>::op<C>(it->first, b);
                  dump<'"'>(b);
               }
               dump<':'>(b);
               write<json>::op<C>(it->second, b);
            };
            write_pair();
            ++it;
            
            const auto end = value.cend();
            for (; it != end; ++it) {
               dump<','>(b);
               write_pair();
            }
            dump<'}'>(b);
         }
      };
      
      template <nullable_t T>
      struct to_json<T>
      {
         template <bool C, class B>
         static void op(auto&& value, B&& b) noexcept
         {
            if (value)
               write<json>::op<C>(*value, std::forward<B>(b));
            else {
               dump<"null">(b);
            }
         }
      };

      template <class T>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            static constexpr auto N = []() constexpr
            {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }
            ();
            
            dump<'['>(b);
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<C>(value.*std::get<I>(meta_v<V>), b);
               }
               else {
                  write<json>::op<C>(std::get<I>(value), b);
               }
               if constexpr (I < N - 1) {
                  dump<','>(b);
               }
            });
            dump<']'>(b);
         }
      };
      
      template <class T>
      requires glaze_object_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump<'{'>(b);
            for_each<N>([&](auto I) {
               static constexpr auto item = std::get<I>(meta_v<V>);
               using Key =
                  typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;
               if constexpr (str_t<Key> || char_t<Key>) {
                  write<json>::op<C>(std::get<0>(item), b);
                  dump<':'>(b);
               }
               else {
                  static constexpr auto quoted =
                     concat_arrays(concat_arrays("\"", std::get<0>(item)), "\":");
                  write<json>::op<C>(quoted, b);
               }
               write<json>::op<C>(value.*std::get<1>(item), b);
               constexpr auto S = std::tuple_size_v<decltype(item)>;
               if constexpr (C && S > 2) {
                  static_assert(
                     std::is_same_v<std::decay_t<decltype(std::get<2>(item))>,
                                    comment_t>);
                  constexpr auto comment = std::get<2>(item).str;
                  if constexpr (comment.size() > 0) {
                     dump<"/*">(b);
                     dump(comment, b);
                     dump<"*/">(b);
                  }
               }
               if constexpr (I < N - 1) {
                  dump<','>(b);
               }
            });
            dump<'}'>(b);
         }
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline void write_json(T&& value, Buffer&& buffer) {
      write_c<json, false>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T, class Buffer>
   inline void write_jsonc(T&& value, Buffer&& buffer) {
      write_c<json, true>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}  // namespace glaze