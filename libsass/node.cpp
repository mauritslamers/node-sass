#include <sstream>
#include <algorithm>
#include "node.hpp"
#include "error.hpp"
#include <iostream>

namespace Sass {
  using namespace std;

  // ------------------------------------------------------------------------
  // Node method implementations
  // ------------------------------------------------------------------------

  void Node::flatten()
  {
    if (type() != block && type() != expansion && type() != root && type() != for_through_directive && type() != for_to_directive) return;
    // size can change during flattening, so we need to call size() on each pass
    for (size_t i = 0; i < size(); ++i) {
      Type i_type = at(i).type();
      if ((i_type == expansion) || (i_type == block) || (i_type == for_through_directive) || (i_type == for_to_directive)) {
        Node expn(at(i));
        if (expn.has_expansions()) expn.flatten();
        ip_->has_statements |= expn.has_statements();
        ip_->has_blocks     |= expn.has_blocks();
        ip_->has_expansions |= expn.has_expansions();
        // TO DO: make this more efficient -- replace with a dummy node instead of erasing
        ip_->children.erase(begin() + i);
        insert(begin() + i, expn.begin(), expn.end());
        // skip over what we just spliced in
        i += expn.size() - 1;
      }
    }
  }

  bool Node::operator==(Node rhs) const
  {
    Type t = type();
    if (t != rhs.type()) return false;

    switch (t)
    {
      case comma_list:
      case space_list:
      case expression:
      case term:
      case numeric_color: {
        if (size() != rhs.size()) return false;
        for (size_t i = 0, L = size(); i < L; ++i) {
          if (at(i) == rhs[i]) continue;
          else return false;
        }
        return true;
      } break;
      
      case variable:
      case identifier:
      case uri:
      case textual_percentage:
      case textual_dimension:
      case textual_number:
      case textual_hex:
      case string_constant: {
        return token().unquote() == rhs.token().unquote();
      } break;
      
      case number:
      case numeric_percentage: {
        return numeric_value() == rhs.numeric_value();
      } break;
      
      case numeric_dimension: {
        if (unit() == rhs.unit()) {
          return numeric_value() == rhs.numeric_value();
        }
        else {
          return false;
        }
      } break;
      
      case boolean: {
        return boolean_value() == rhs.boolean_value();
      } break;
      
      default: {
        return true;
      } break;
    }
    return false;
  }
  
  bool Node::operator!=(Node rhs) const
  { return !(*this == rhs); }
  
  bool Node::operator<(Node rhs) const
  {
    Type lhs_type = type();
    Type rhs_type = rhs.type();
    
    // comparing atomic numbers
    if ((lhs_type == number             && rhs_type == number) ||
        (lhs_type == numeric_percentage && rhs_type == numeric_percentage)) {
      return numeric_value() < rhs.numeric_value();
    }

    // comparing numbers with units
    else if (lhs_type == numeric_dimension && rhs_type == numeric_dimension) {
      if (unit() == rhs.unit()) {
        return numeric_value() < rhs.numeric_value();
      }
      else {
        throw Error(Error::evaluation, path(), line(), "incompatible units");
      }
    }

    // comparing colors
    else if (lhs_type == numeric_color && rhs_type == numeric_color) {
      return lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
    }

    // comparing identifiers and strings (treat them as comparable)
    else if ((lhs_type == identifier || lhs_type == string_constant || lhs_type == value) &&
             (rhs_type == identifier || lhs_type == string_constant || rhs_type == value)) {
      return token().unquote() < rhs.token().unquote();
    }

    // COMPARING SELECTORS -- IMPORTANT FOR INHERITANCE
    else if ((type()     >= selector_group && type()     <=selector_schema) &&
             (rhs.type() >= selector_group && rhs.type() <=selector_schema)) {

      // if they're not the same kind, just compare type tags
      if (type() != rhs.type()) return type() < rhs.type();

      // otherwise we have to do more work
      switch (type())
      {
        case simple_selector:
        case pseudo: {
          return token() < rhs.token();
        } break;

        case selector:
        case attribute_selector: {
          return lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
        } break;



        default: {
          return false;
        } break;

      }
    }
    // END OF SELECTOR COMPARISON

    // catch-all
    else {
      throw Error(Error::evaluation, path(), line(), "incomparable types");
    }
  }
  
  bool Node::operator<=(Node rhs) const
  { return *this < rhs || *this == rhs; }
  
  bool Node::operator>(Node rhs) const
  { return !(*this <= rhs); }
  
  bool Node::operator>=(Node rhs) const
  { return !(*this < rhs); }


  // ------------------------------------------------------------------------
  // Token method implementations
  // ------------------------------------------------------------------------
  
  string Token::unquote() const
  {
    string result;
    const char* p = begin;
    if (*begin == '\'' || *begin == '"') {
      ++p;
      while (p < end) {
        if (*p == '\\') {
          switch (*(++p)) {
            case 'n':  result += '\n'; break;
            case 't':  result += '\t'; break;
            case 'b':  result += '\b'; break;
            case 'r':  result += '\r'; break;
            case 'f':  result += '\f'; break;
            case 'v':  result += '\v'; break;
            case 'a':  result += '\a'; break;
            case '\\': result += '\\'; break;
            default: result += *p; break;
          }
        }
        else if (p == end - 1) {
          return result;
        }
        else {
          result += *p;
        }
        ++p;
      }
      return result;
    }
    else {
      while (p < end) {
        result += *(p++);
      }
      return result;
    }
  }
  
  void Token::unquote_to_stream(std::stringstream& buf) const
  {
    const char* p = begin;
    if (*begin == '\'' || *begin == '"') {
      ++p;
      while (p < end) {
        if (*p == '\\') {
          switch (*(++p)) {
            case 'n':  buf << '\n'; break;
            case 't':  buf << '\t'; break;
            case 'b':  buf << '\b'; break;
            case 'r':  buf << '\r'; break;
            case 'f':  buf << '\f'; break;
            case 'v':  buf << '\v'; break;
            case 'a':  buf << '\a'; break;
            case '\\': buf << '\\'; break;
            default: buf << *p; break;
          }
        }
        else if (p == end - 1) {
          return;
        }
        else {
          buf << *p;
        }
        ++p;
      }
      return;
    }
    else {
      while (p < end) {
        buf << *(p++);
      }
      return;
    }
  }
  
  bool Token::operator<(const Token& rhs) const
  {
    const char* first1 = begin;
    const char* last1  = end;
    const char* first2 = rhs.begin;
    const char* last2  = rhs.end;
    while (first1!=last1)
    {
      if (first2 == last2 || *first2 < *first1) return false;
      else if (*first1 < *first2) return true;
      ++first1; ++first2;
    }
    return (first2 != last2);
  }
  
  bool Token::operator==(const Token& rhs) const
  {
    if (length() != rhs.length()) return false;
    
    if ((begin[0]     == '"' || begin[0]     == '\'') &&
        (rhs.begin[0] == '"' || rhs.begin[0] == '\''))
    { return unquote() == rhs.unquote(); }
    
    const char* p = begin;
    const char* q = rhs.begin;
    for (; p < end; ++p, ++q) if (*p != *q) return false;
    return true;
  }


  // ------------------------------------------------------------------------
  // Node_Impl method implementations
  // ------------------------------------------------------------------------

  double Node_Impl::numeric_value()
  {
    switch (type)
    {
      case Node::number:
      case Node::numeric_percentage:
        return value.numeric;
      case Node::numeric_dimension:
        return value.dimension.numeric;
      default:
        break;
        // throw an exception?
    }
    // if you reach this point, you've got a logic error somewhere
    return 0;
  }
  
  extern const char percent_str[] = "%";
  extern const char empty_str[]   = "";
  Token Node_Impl::unit()
  {
    switch (type)
    {
      case Node::numeric_percentage: {
        return Token::make(percent_str);
      } break;
  
      case Node::numeric_dimension: {
        return value.dimension.unit;
      } break;
      
      default: break;
    }
    return Token::make(empty_str);
  }

}