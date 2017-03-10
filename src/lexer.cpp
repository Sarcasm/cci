#include <string>
#include <vector>
#include <tuple>
#include <algorithm>
#include <cassert>
#include <experimental/string_view>
#include <fmt/format.h>
#include <fmt/format.cc>
#include "cpp/contracts.hpp"
#include "cpp/optional.hpp"

using std::experimental::string_view;

enum class TokenType
{
  Identifier,
  Plus,
  Minus,
  Times,
  Divide,
  Integer,
  Float,
};

const auto TOKENS_STR = std::vector<std::pair<TokenType, string_view>>
{
  {TokenType::Plus, "+"},
  {TokenType::Minus, "-"},
  {TokenType::Times, "*"},
  {TokenType::Divide, "/"},
};


// Whites are used to tell apart from different tokens.
constexpr auto is_white(char c) -> bool
{
  return c == ' '  ||
         c == '\t' ||
         c == '('  ||
         c == ')'  ||
         c == '{'  ||
         c == '}'  ||
         c == ','  ||
         c == ':'  ||
         c == '?';
}

constexpr auto is_space(char c) -> bool
{
  return c == ' ' || c == '\t';
}

constexpr auto is_digit(char c) -> bool
{
  return c >= '0' & c <= '9';
}

constexpr auto is_hexdigit(char c) -> bool
{
  return is_digit(c) || (c >= 'A' & c <= 'F') || (c >= 'a' & c <= 'f');
}

constexpr auto is_octdigit(char c) -> bool
{
  return c >= '0' & c <= '7';
}

constexpr auto is_alpha(char c) -> bool
{
  return (c >= 'A' & c <= 'Z') || (c >= 'a' & c <= 'z');
}

constexpr auto is_alphanum(char c) -> bool
{
  return is_alpha(c) || is_digit(c);
}

auto is_token(const char* begin, const char* end, string_view tok) -> bool
{
  return string_view(begin, static_cast<size_t>(std::distance(begin, end))) == tok;
}

// TODO it deserves an elaborated code.
struct SourceLocation
{
  const char* begin;
  const char* end;

  constexpr explicit SourceLocation(const char* begin, const char* end) noexcept :
    begin(begin), end(end)
  {}
};

auto find_token_between_whites(const char* begin, const char* end) -> optional<SourceLocation>
{
  auto first = std::find_if_not(begin, end, is_white);
  auto last = std::find_if(first, end, is_white);

  if (first != end)
  {
    return SourceLocation(first, last);
  }

  return nullopt;
}


struct TokenData
{
  TokenType type;
  string_view data;

  constexpr explicit TokenData(TokenType type, const char* begin, const char* end) noexcept :
    type(type),
    data(begin, static_cast<size_t>(end - begin))
  {}
};

struct LexerContext
{
  std::vector<TokenData> tokens;

  void add_token(TokenType type, const char* begin, const char* end)
  {
    tokens.emplace_back(type, begin, end);
  }

  void add_token(TokenType type, const SourceLocation& local)
  {
    tokens.emplace_back(type, local.begin, local.end);
  }

  // TODO
  template <typename... FormatArgs>
  void error(const SourceLocation& local, const char* msg, FormatArgs&&... args)
  {
    Unreachable();
  }

  // TODO
  template <typename... FormatArgs>
  void error(const char* local, const char* msg, FormatArgs&&... args)
  {
    Unreachable();
  }
};

auto lexer_parse_constant(LexerContext& lexer, const char* begin, const char* end) -> const char*
{
  if (begin == end)
  {
    return end;
  }

  auto is_integer = [] (const char* it, const char* end) -> bool
  {
    const bool has_hex_prefix = std::distance(it, end) > 2 && it[0] == '0' && (it[1] == 'x' || it[1] == 'X');
    
    // skip 0x
    if (has_hex_prefix)
    {
      it = std::next(it, 2);
    }

    for (; it != end; ++it)
    {
      if (has_hex_prefix? is_hexdigit(*it) : is_digit(*it))
      {
        continue;
      }
      
      // TODO signal error message.

      return false;
    }

    return true;
  };

  auto is_float = [] (const char* it, const char* end) -> bool
  {
    const auto dot_it = std::find_if(it, end, [] (char c) { return c == '.'; });
    const auto f_suffix_it = std::find_if(dot_it, end, [] (char c) { return c == 'f' || c == 'F'; });

    if (f_suffix_it != end && std::next(f_suffix_it) != end)
    {
      return false;
    }

    for (; it != end; ++it)
    {
      if (it == dot_it || it == f_suffix_it)
      {
        continue;
      }

      if (is_digit(*it))
      {
        continue;
      }

      // TODO signal error message.

      return false;
    }

    return true;
  };

  auto token = find_token_between_whites(begin, end).value();

  if (is_integer(token.begin, token.end))
  {
    lexer.add_token(TokenType::Integer, token);
  }
  else if (is_float(token.begin, token.end))
  {
    lexer.add_token(TokenType::Float, token);
  }

  return token.end;
}

namespace test
{

void test_lexer_parse_constant()
{
  // Integers.
  {
    string_view input = "42 314 0x22fx";
    LexerContext lexer{};

    auto it1 = lexer_parse_constant(lexer, input.begin(), input.end());
    auto it2 = lexer_parse_constant(lexer, it1, input.end());
    lexer_parse_constant(lexer, it2, input.end());

    auto token1 = lexer.tokens[0];
    auto token2 = lexer.tokens[1];

    assert(token1.type == TokenType::Integer);
    assert(token1.data == "42");

    assert(token2.type == TokenType::Integer);
    assert(token2.data == "314");
  }

  // Floats.
  {
    string_view input = "123.f 0.2 1. (1.f) 0.ff .0f ";
    LexerContext lexer{};

    auto parse = [&] (const char* it)
    {
      return lexer_parse_constant(lexer, it, input.end());
    };

    // 123.f
    auto it1 = parse(input.begin());
    
    // 0.2
    auto it2 = parse(it1);

    // 1.
    auto it3 = parse(it2);

    // 1.f
    auto it4 = parse(it3);

    // 0.ff (failure)
    auto it5 = parse(it4);

    assert(lexer.tokens.size() == 4);

    // .0f
    parse(it5);

    assert(lexer.tokens[0].type == TokenType::Float);
    assert(lexer.tokens[0].data == "123.f");

    assert(lexer.tokens[1].type == TokenType::Float);
    assert(lexer.tokens[1].data == "0.2");

    assert(lexer.tokens[2].type == TokenType::Float);
    assert(lexer.tokens[2].data == "1.");

    assert(lexer.tokens[3].type == TokenType::Float);
    assert(lexer.tokens[3].data == "1.f");

    assert(lexer.tokens[4].type == TokenType::Float);
    assert(lexer.tokens[4].data == ".0f");
  }
}

}

int main()
{
  test::test_lexer_parse_constant();
}
