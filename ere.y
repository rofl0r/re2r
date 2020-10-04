%{
#include "yydefs.h"
%}

/* this is the ERE grammar from
   https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap09.html#tag_09_05_03

   however we removed support for the following things:
   1) collation    [.xxx.]
   2) equivalence  [=xxx=]
      these 2 are locale-dependent features which are only supported in POSIX,
      no other regex library supports those things.
   3) posix character classes like [:digit:]
      it's trivial to replace those with what they would expand to using some sort
      of a preprocessor, e.g. [:digit:] -> 0-9
   4) character ranges ending in '-', such as #--
      this is a really unusual special case which is very unlikely to be
      encountered. if needed though, it can be worked around by ending the range
      expression on character before the '-' character and put the minus character
      as single character to the start of the regex expression.

   note that the original POSIX grammar rejects empty set of parens (), unlike
   most implementations.
   we haven't added a workaround to make this undefined construct work.
*/

%token  ORD_CHAR QUOTED_CHAR DUP_COUNT
%start  extended_reg_exp
%%

/* --------------------------------------------
   Extended Regular Expression
   --------------------------------------------
*/
extended_reg_exp   :                      ERE_branch
                   | extended_reg_exp '|' ERE_branch
                   ;
ERE_branch         :            ERE_expression
                   | ERE_branch ERE_expression
                   ;
ERE_expression     : one_char_or_coll_elem_ERE
                   | '^'
                   | '$'
                   | '(' extended_reg_exp ')'
                   | ERE_expression ERE_dupl_symbol
                   ;
one_char_or_coll_elem_ERE  : ORD_CHAR
                   | QUOTED_CHAR
                   | '.'
                   | bracket_expression
                   ;
ERE_dupl_symbol    : '*'
                   | '+'
                   | '?'
                   | '{' DUP_COUNT               '}'
                   | '{' DUP_COUNT ','           '}'
                   | '{' DUP_COUNT ',' DUP_COUNT '}'
                   ;


/* --------------------------------------------
   Bracket Expression
   -------------------------------------------
*/
bracket_expression : '[' matching_list ']'
               | '[' nonmatching_list ']'
               ;
matching_list  : bracket_list
               ;
nonmatching_list : '^' bracket_list
               ;
bracket_list   : follow_list
               ;
follow_list    :             expression_term
               | follow_list expression_term
               ;
expression_term : single_expression
               | range_expression
               ;

single_expression : one_char_ERE
               ;

range_expression : start_range end_range
               ;

start_range    : end_range '-'
               ;

one_char_ERE   : ORD_CHAR
               | QUOTED_CHAR
               ;

end_range      : one_char_ERE
               ;

