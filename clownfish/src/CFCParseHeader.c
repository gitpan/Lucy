/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>
#line 24 "../src/CFCParseHeader.y"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "CFC.h"
#ifndef true
  #define true 1
  #define false 0
#endif

static CFCClass*
S_start_class(CFCParser *state, CFCDocuComment *docucomment, char *exposure,
              char *declaration_modifier_list, char *class_name,
              char *class_cnick, char *inheritance) {
    const char *source_class = CFCParser_get_source_class(state);
    int is_final = false;
    int is_inert = false;
    if (declaration_modifier_list) {
        is_final = !!strstr(declaration_modifier_list, "final");
        is_inert = !!strstr(declaration_modifier_list, "inert");
    }
    CFCParser_set_class_name(state, class_name);
    CFCParser_set_class_cnick(state, class_cnick);
    CFCClass *klass = CFCClass_create(CFCParser_get_parcel(state), exposure,
                                      class_name, class_cnick, NULL,
                                      docucomment, source_class, inheritance,
                                      is_final, is_inert);
    CFCBase_decref((CFCBase*)docucomment);
    return klass;
}

static CFCVariable*
S_new_var(CFCParser *state, char *exposure, char *modifiers, CFCType *type,
          char *name) {
    int inert = false;
    if (modifiers) {
        if (strcmp(modifiers, "inert") != 0) {
            CFCUtil_die("Illegal variable modifiers: '%s'", modifiers);
        }
        inert = true;
    }

    const char *class_name = NULL;
    const char *class_cnick = NULL;
    if (exposure && strcmp(exposure, "local") != 0) {
        class_name  = CFCParser_get_class_name(state);
        class_cnick = CFCParser_get_class_cnick(state);
    }
    CFCVariable *var = CFCVariable_new(CFCParser_get_parcel(state), exposure,
                                       class_name, class_cnick,
                                       name, type, inert);

    /* Consume tokens. */
    CFCBase_decref((CFCBase*)type);

    return var;
}

static CFCBase*
S_new_sub(CFCParser *state, CFCDocuComment *docucomment, 
          char *exposure, char *declaration_modifier_list,
          CFCType *type, char *name, CFCParamList *param_list) {
    CFCParcel  *parcel      = CFCParser_get_parcel(state);
    const char *class_name  = CFCParser_get_class_name(state);
    const char *class_cnick = CFCParser_get_class_cnick(state);

    /* Find modifiers by scanning the list. */
    int is_abstract = false;
    int is_final    = false;
    int is_inline   = false;
    int is_inert    = false;
    if (declaration_modifier_list) {
        is_abstract = !!strstr(declaration_modifier_list, "abstract");
        is_final    = !!strstr(declaration_modifier_list, "final");
        is_inline   = !!strstr(declaration_modifier_list, "inline");
        is_inert    = !!strstr(declaration_modifier_list, "inert");
    }

    /* If "inert", it's a function, otherwise it's a method. */
    CFCBase *sub;
    if (is_inert) {
        sub = (CFCBase*)CFCFunction_new(parcel, exposure, class_name,
                                         class_cnick, name, type, param_list,
                                         docucomment, is_inline);
    }
    else {
        sub = (CFCBase*)CFCMethod_new(parcel, exposure, class_name,
                                       class_cnick, name, type, param_list,
                                       docucomment, is_final, is_abstract);
    }

    /* Consume tokens. */
    CFCBase_decref((CFCBase*)docucomment);
    CFCBase_decref((CFCBase*)type);
    CFCBase_decref((CFCBase*)param_list);

    return sub;
}

static CFCType*
S_new_type(CFCParser *state, int flags, char *type_name,
           char *asterisk_postfix, char *array_postfix) {
    (void)state; /* unused */
    CFCType *type = NULL;
    size_t type_name_len = strlen(type_name);
    int indirection = asterisk_postfix ? strlen(asterisk_postfix) : 0;

    /* Apply "nullable" to outermost pointer, but "const", etc to innermost
     * type. This is an ugly kludge and the Clownfish header language needs to
     * be fixed, either to support C's terrible pointer type syntax, or to do
     * something better. */
    int composite_flags = 0;
    if (indirection) {
        composite_flags = flags & CFCTYPE_NULLABLE;
        flags &= ~CFCTYPE_NULLABLE;
    }

    if (!strcmp(type_name, "int8_t")
        || !strcmp(type_name, "int16_t")
        || !strcmp(type_name, "int32_t")
        || !strcmp(type_name, "int64_t")
        || !strcmp(type_name, "uint8_t")
        || !strcmp(type_name, "uint16_t")
        || !strcmp(type_name, "uint32_t")
        || !strcmp(type_name, "uint64_t")
        || !strcmp(type_name, "char")
        || !strcmp(type_name, "short")
        || !strcmp(type_name, "int")
        || !strcmp(type_name, "long")
        || !strcmp(type_name, "size_t")
        || !strcmp(type_name, "bool_t")
       ) {
        type = CFCType_new_integer(flags, type_name);
    }
    else if (!strcmp(type_name, "float")
             || !strcmp(type_name, "double")
            ) {
        type = CFCType_new_float(flags, type_name);
    }
    else if (!strcmp(type_name, "void")) {
        type = CFCType_new_void(!!(flags & CFCTYPE_CONST));
    }
    else if (!strcmp(type_name, "va_list")) {
        type = CFCType_new_va_list();
    }
    else if (type_name_len > 2
             && !strcmp(type_name + type_name_len - 2, "_t")
            ) {
        type = CFCType_new_arbitrary(CFCParser_get_parcel(state), type_name);
    }
    else if (indirection > 0) {
        /* The only remaining possibility is an object type, and we can let
         * the constructor perform the complex validation of the type name. */
        indirection--;
        if (indirection == 0) {
            flags |= composite_flags;
            composite_flags = 0;
        }
        type = CFCType_new_object(flags, CFCParser_get_parcel(state), type_name, 1);
    }
    else {
        CFCUtil_die("Invalid type specification at/near '%s'", type_name);
    }

    if (indirection) {
        CFCType *composite = CFCType_new_composite(composite_flags, type,
                                                   indirection, NULL);
        CFCBase_decref((CFCBase*)type);
        type = composite;
    }
    else if (array_postfix) {
        CFCType *composite = CFCType_new_composite(composite_flags, type,
                                                   0, array_postfix);
        CFCBase_decref((CFCBase*)type);
        type = composite;
    }

    return type;
}

#line 190 "../src/CFCParseHeader.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    CFCParseHeaderTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is CFCParseHeaderTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    CFCParseHeaderARG_SDECL     A static variable declaration for the %extra_argument
**    CFCParseHeaderARG_PDECL     A parameter declaration for the %extra_argument
**    CFCParseHeaderARG_STORE     Code to store %extra_argument into yypParser
**    CFCParseHeaderARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 78
#define YYACTIONTYPE unsigned short int
#define CFCParseHeaderTOKENTYPE char*
typedef union {
  int yyinit;
  CFCParseHeaderTOKENTYPE yy0;
  CFCFile* yy19;
  CFCParamList* yy40;
  CFCType* yy41;
  CFCCBlock* yy55;
  CFCBase* yy58;
  CFCParcel* yy66;
  CFCClass* yy71;
  CFCVariable* yy111;
  int yy130;
  CFCDocuComment* yy151;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define CFCParseHeaderARG_SDECL  CFCParser *state ;
#define CFCParseHeaderARG_PDECL , CFCParser *state 
#define CFCParseHeaderARG_FETCH  CFCParser *state  = yypParser->state 
#define CFCParseHeaderARG_STORE yypParser->state  = state 
#define YYNSTATE 199
#define YYNRULE 124
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* The yyzerominor constant is used to initialize instances of
** YYMINORTYPE objects to zero. */
static const YYMINORTYPE yyzerominor = { 0 };

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
#define YY_ACTTAB_COUNT (1550)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */   105,  306,  306,   57,  306,  126,   89,  130,  129,  128,
 /*    10 */   127,   98,   97,   96,   95,  120,  119,  118,  117,  104,
 /*    20 */   103,  102,  101,  114,  111,  113,  147,    1,  310,  310,
 /*    30 */   307,   34,  307,    9,   19,  112,  140,  197,   20,   92,
 /*    40 */   253,   35,   51,  253,  174,   18,  253,   99,  253,  253,
 /*    50 */   253,  253,  253,  253,  253,  253,  253,  253,  253,  253,
 /*    60 */   253,  253,  253,  253,   78,  133,   94,   93,   62,  139,
 /*    70 */   115,   23,    4,  173,  115,   23,    8,  199,   22,   16,
 /*    80 */   253,  257,   22,  124,  257,   99,  308,  257,  308,  257,
 /*    90 */   257,  257,  257,  257,  257,  257,  257,  257,  257,  257,
 /*   100 */   257,  257,  257,  257,  257,  280,   60,  280,  280,  280,
 /*   110 */   280,  116,  115,   23,  200,  280,  280,  280,  280,   21,
 /*   120 */    22,  257,  263,  100,  233,  263,  233,  309,  263,  309,
 /*   130 */   263,  263,  263,  263,  263,  263,  263,  263,  263,  263,
 /*   140 */   263,  263,  263,  263,  263,  263,  281,   59,  281,  281,
 /*   150 */   281,  281,  201,  115,   23,  122,  281,  281,  281,  281,
 /*   160 */    76,   22,  263,  264,  100,  237,  264,  237,   47,  264,
 /*   170 */   162,  264,  264,  264,  264,  264,  264,  264,  264,  264,
 /*   180 */   264,  264,  264,  264,  264,  264,  264,  282,   69,  282,
 /*   190 */   282,  282,  282,  125,  115,   23,   88,  282,  282,  282,
 /*   200 */   282,   76,   22,  264,  262,  100,  235,  262,  235,   46,
 /*   210 */   262,  160,  262,  262,  262,  262,  262,  262,  262,  262,
 /*   220 */   262,  262,  262,  262,  262,  262,  262,  262,  283,   75,
 /*   230 */   283,  283,  283,  283,   87,  115,   23,   86,  283,  283,
 /*   240 */   283,  283,   76,   22,  262,  261,  100,  203,  261,  312,
 /*   250 */   312,  261,   33,  261,  261,  261,  261,  261,  261,  261,
 /*   260 */   261,  261,  261,  261,  261,  261,  261,  261,  261,  285,
 /*   270 */    85,  285,  285,  285,  285,  217,   43,  217,   38,  285,
 /*   280 */   285,  285,  285,  109,   99,  261,  254,  266,  239,  254,
 /*   290 */   239,  204,  254,  266,  254,  254,  254,  254,  254,  254,
 /*   300 */   254,  254,  254,  254,  254,  254,  254,  254,  254,  254,
 /*   310 */   284,  141,  284,  284,  284,  284,   26,  298,  298,   36,
 /*   320 */   284,  284,  284,  284,   76,   99,  254,  260,  293,  234,
 /*   330 */   260,  234,  205,  260,  293,  260,  260,  260,  260,  260,
 /*   340 */   260,  260,  260,  260,  260,  260,  260,  260,  260,  260,
 /*   350 */   260,  126,  293,  130,  129,  128,  127,  299,  299,  107,
 /*   360 */   144,  120,  119,  118,  117,   76,  206,  260,  256,  142,
 /*   370 */    49,  256,  136,   45,  256,  158,  256,  256,  256,  256,
 /*   380 */   256,  256,  256,  256,  256,  256,  256,  256,  256,  256,
 /*   390 */   256,  256,  170,  172,  171,  169,  168,  167,  166,   66,
 /*   400 */   207,  241,  317,  241,  317,  115,   23,    7,  256,  259,
 /*   410 */    15,  146,  259,   22,  124,  259,   99,  259,  259,  259,
 /*   420 */   259,  259,  259,  259,  259,  259,  259,  259,  259,  259,
 /*   430 */   259,  259,  259,  292,   61,  193,  193,   76,  192,  292,
 /*   440 */   115,   23,  305,   43,  305,   11,   37,  110,   22,  259,
 /*   450 */   255,   99,   99,  255,  322,  322,  255,  292,  255,  255,
 /*   460 */   255,  255,  255,  255,  255,  255,  255,  255,  255,  255,
 /*   470 */   255,  255,  255,  255,   83,   66,  305,  305,  305,   81,
 /*   480 */   305,  115,   23,    6,  193,   90,   12,  192,   84,   22,
 /*   490 */   255,  258,   99,  306,  258,   90,   82,  258,   79,  258,
 /*   500 */   258,  258,  258,  258,  258,  258,  258,  258,  258,  258,
 /*   510 */   258,  258,  258,  258,  258,   78,  132,    9,  175,   61,
 /*   520 */     9,  115,   23,  300,  300,  115,   23,    9,    9,   22,
 /*   530 */    17,  258,  245,   22,  245,  251,   99,  134,  251,   80,
 /*   540 */   251,  251,  251,  251,  251,  251,  251,  251,  251,  251,
 /*   550 */   251,  251,  251,  251,  251,  251,   72,   32,  135,   48,
 /*   560 */   123,  136,  115,   23,  321,  321,  193,   14,   76,  192,
 /*   570 */    22,    9,  251,   99,  294,  238,  252,  238,  131,  252,
 /*   580 */   294,  252,  252,  252,  252,  252,  252,  252,  252,  252,
 /*   590 */   252,  252,  252,  252,  252,  252,  252,  269,   72,  294,
 /*   600 */   225,  191,  225,  269,  115,   23,  301,  301,  221,   13,
 /*   610 */   221,   76,   22,  252,  316,   99,  302,  302,  316,   74,
 /*   620 */   316,  141,  316,  316,  316,  316,  316,  316,  316,  316,
 /*   630 */   316,  316,  316,  316,  316,  316,  316,  316,  250,  303,
 /*   640 */   303,  250,    9,  250,  250,  250,  250,  250,  250,  250,
 /*   650 */   250,  250,  250,  250,  250,  250,  250,  250,  250,  304,
 /*   660 */   304,  311,  311,  313,  313,  229,  219,  229,  219,  227,
 /*   670 */   223,  227,  223,  106,  143,  250,  202,  271,   71,   44,
 /*   680 */    67,  156,  126,  271,  130,  129,  128,  127,   98,   97,
 /*   690 */    96,   95,  120,  119,  118,  117,  104,  103,  102,  101,
 /*   700 */   108,  271,  271,  126,   31,  130,  129,  128,  127,   98,
 /*   710 */    97,   96,   95,  120,  119,  118,  117,  104,  103,  102,
 /*   720 */   101,  190,  231,  243,  231,  243,  218,   30,  218,  226,
 /*   730 */   222,  226,  222,  189,   29,  324,    2,   92,  187,  183,
 /*   740 */   147,    1,  185,  184,   56,  194,  195,   68,    3,  186,
 /*   750 */   115,   23,    5,   65,  247,   10,  247,  193,   22,   76,
 /*   760 */   192,   99,  126,  188,  130,  129,  128,  127,   98,   97,
 /*   770 */    96,   95,  120,  119,  118,  117,  104,  103,  102,  101,
 /*   780 */   286,   28,  286,  295,  286,  286,  286,  286,  193,  295,
 /*   790 */    76,  192,  286,  286,  286,  286,  286,  286,  286,  286,
 /*   800 */   287,   27,  287,  297,  287,  287,  287,  287,  295,  297,
 /*   810 */    55,   54,  287,  287,  287,  287,  287,  287,  287,  287,
 /*   820 */   288,   53,  288,  296,  288,  288,  288,  288,  297,  296,
 /*   830 */    52,   25,  288,  288,  288,  288,  288,  288,  288,  288,
 /*   840 */   289,   50,  289,  267,  289,  289,  289,  289,  296,  267,
 /*   850 */    24,  198,  289,  289,  289,  289,  289,  289,  289,  289,
 /*   860 */   291,  138,  291,  270,  291,  291,  291,  291,  177,  270,
 /*   870 */   181,  180,  291,  291,  291,  291,  291,  291,  291,  291,
 /*   880 */   290,   42,  290,  154,  290,  290,  290,  290,  177,  179,
 /*   890 */   145,  182,  290,  290,  290,  290,  290,  290,  290,  290,
 /*   900 */   276,   41,  276,  152,  276,  276,  276,  276,  230,  163,
 /*   910 */   230,  100,  276,  276,  276,  276,  276,  276,  276,  276,
 /*   920 */   277,   40,  277,  150,  277,  277,  277,  277,  220,  161,
 /*   930 */   220,  325,  277,  277,  277,  277,  277,  277,  277,  277,
 /*   940 */   278,   39,  278,  148,  278,  278,  278,  278,  228,  159,
 /*   950 */   228,  157,  278,  278,  278,  278,  278,  278,  278,  278,
 /*   960 */   279,  155,  279,  153,  279,  279,  279,  279,  224,  151,
 /*   970 */   224,  325,  279,  279,  279,  279,  279,  279,  279,  279,
 /*   980 */    77,  149,  126,  165,  130,  129,  128,  127,  232,  164,
 /*   990 */   232,  325,  120,  119,  118,  117,  104,  103,  102,  101,
 /*  1000 */    70,  325,  126,  325,  130,  129,  128,  127,  178,  325,
 /*  1010 */    91,  325,  120,  119,  118,  117,  104,  103,  102,  101,
 /*  1020 */    64,  325,  126,  325,  130,  129,  128,  127,  249,  325,
 /*  1030 */   249,  325,  120,  119,  118,  117,  104,  103,  102,  101,
 /*  1040 */    58,  325,  126,  325,  130,  129,  128,  127,  325,  325,
 /*  1050 */   325,  325,  120,  119,  118,  117,  104,  103,  102,  101,
 /*  1060 */    73,  325,  126,  325,  130,  129,  128,  127,  325,  325,
 /*  1070 */   325,  325,  120,  119,  118,  117,  104,  103,  102,  101,
 /*  1080 */    63,  325,  121,  325,  130,  129,  128,  127,  325,  325,
 /*  1090 */   325,  325,  120,  119,  118,  117,  104,  103,  102,  101,
 /*  1100 */   126,  325,  130,  129,  128,  127,  325,   63,  325,  123,
 /*  1110 */   120,  119,  118,  117,  104,  103,  102,  101,  210,  325,
 /*  1120 */   325,  325,  210,  104,  103,  102,  101,  325,  325,  325,
 /*  1130 */   210,  210,  210,  210,  325,  325,  325,  325,  210,  210,
 /*  1140 */   210,  210,  211,  272,  325,  325,  211,  273,  325,  272,
 /*  1150 */   325,  325,  325,  273,  211,  211,  211,  211,  210,  325,
 /*  1160 */   325,  210,  211,  211,  211,  211,  212,  272,  272,  325,
 /*  1170 */   212,  273,  273,  325,  325,  325,  325,  325,  212,  212,
 /*  1180 */   212,  212,  211,  325,  325,  211,  212,  212,  212,  212,
 /*  1190 */   213,  274,  325,  325,  213,  275,  325,  274,  325,  325,
 /*  1200 */   325,  275,  213,  213,  213,  213,  212,  325,  325,  212,
 /*  1210 */   213,  213,  213,  213,  214,  274,  274,  325,  214,  275,
 /*  1220 */   275,  325,  325,  325,  325,  325,  214,  214,  214,  214,
 /*  1230 */   213,  325,  325,  213,  214,  214,  214,  214,  215,  325,
 /*  1240 */   325,  318,  215,  265,  318,  325,  318,  325,  325,  265,
 /*  1250 */   215,  215,  215,  215,  214,  325,  325,  214,  215,  215,
 /*  1260 */   215,  215,  216,  268,  325,  325,  216,  137,  177,  268,
 /*  1270 */   325,  325,  325,  325,  216,  216,  216,  216,  215,  325,
 /*  1280 */   318,  215,  216,  216,  216,  216,  319,  137,  177,  325,
 /*  1290 */   319,  325,  236,  325,  236,  325,  325,  325,  319,  319,
 /*  1300 */   319,  319,  216,  325,  325,  216,  319,  319,  319,  319,
 /*  1310 */   320,  325,  325,  325,  320,  325,  240,  325,  240,  325,
 /*  1320 */   325,  325,  320,  320,  320,  320,  319,  325,   76,  319,
 /*  1330 */   320,  320,  320,  320,  209,  325,  325,  325,  209,  325,
 /*  1340 */   325,  325,  325,  325,  325,  325,  209,  209,  209,  209,
 /*  1350 */   320,  325,   76,  320,  209,  209,  209,  209,  208,  325,
 /*  1360 */   325,  325,   57,  325,  325,  325,  325,  325,  325,  325,
 /*  1370 */    98,   97,   96,   95,  209,  325,  325,  209,  104,  103,
 /*  1380 */   102,  101,  325,  325,  126,  325,  130,  129,  128,  127,
 /*  1390 */   325,  325,  325,  325,  120,  119,  118,  117,   92,  314,
 /*  1400 */   325,   51,  314,  325,  314,  325,  325,  325,  126,  325,
 /*  1410 */   130,  129,  128,  127,  325,  196,   67,  325,  120,  119,
 /*  1420 */   118,  117,  325,  325,   98,   97,   96,   95,  325,  325,
 /*  1430 */   325,  325,  104,  103,  102,  101,  314,  176,  314,  314,
 /*  1440 */   315,   64,  325,  315,  325,  315,  314,  325,  325,  325,
 /*  1450 */   275,  242,  325,  242,  325,   77,  325,  104,  103,  102,
 /*  1460 */   101,  325,  325,  325,  325,  325,  325,  325,  275,  275,
 /*  1470 */   325,  104,  103,  102,  101,  325,   70,  315,  325,  315,
 /*  1480 */   315,   58,  325,  314,  325,  193,  314,   76,  192,  246,
 /*  1490 */    73,  246,  104,  103,  102,  101,  325,  104,  103,  102,
 /*  1500 */   101,  244,  325,  244,  325,  325,  104,  103,  102,  101,
 /*  1510 */   325,  325,  248,  325,  248,  325,  325,  325,  325,  325,
 /*  1520 */   325,  325,  325,  193,  325,   76,  192,  325,  325,  325,
 /*  1530 */   325,  325,  325,  325,  325,  193,  325,   76,  192,  325,
 /*  1540 */   325,  325,  325,  325,  325,  325,  193,  325,   76,  192,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     1,   35,   36,    4,   38,    6,    2,    8,    9,   10,
 /*    10 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*    20 */    21,   22,   23,   49,   50,   51,   52,   53,   35,   36,
 /*    30 */     0,   38,    2,   34,   60,   61,   26,   27,   64,   40,
 /*    40 */     0,   67,   43,    3,   35,   36,    6,   73,    8,    9,
 /*    50 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*    60 */    20,   21,   22,   23,   56,   57,   54,   55,   56,   26,
 /*    70 */    62,   63,   60,   35,   62,   63,   64,    0,   70,   67,
 /*    80 */    40,    0,   70,    6,    3,   73,    0,    6,    2,    8,
 /*    90 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   100 */    19,   20,   21,   22,   23,    6,   56,    8,    9,   10,
 /*   110 */    11,   62,   62,   63,    0,   16,   17,   18,   19,   70,
 /*   120 */    70,   40,    0,   73,    5,    3,    7,    0,    6,    2,
 /*   130 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   140 */    18,   19,   20,   21,   22,   23,    6,   56,    8,    9,
 /*   150 */    10,   11,    0,   62,   63,    6,   16,   17,   18,   19,
 /*   160 */    41,   70,   40,    0,   73,    5,    3,    7,   66,    6,
 /*   170 */    68,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   180 */    17,   18,   19,   20,   21,   22,   23,    6,   56,    8,
 /*   190 */     9,   10,   11,    6,   62,   63,    2,   16,   17,   18,
 /*   200 */    19,   41,   70,   40,    0,   73,    5,    3,    7,   66,
 /*   210 */     6,   68,    8,    9,   10,   11,   12,   13,   14,   15,
 /*   220 */    16,   17,   18,   19,   20,   21,   22,   23,    6,   56,
 /*   230 */     8,    9,   10,   11,    2,   62,   63,    2,   16,   17,
 /*   240 */    18,   19,   41,   70,   40,    0,   73,    0,    3,   35,
 /*   250 */    36,    6,   38,    8,    9,   10,   11,   12,   13,   14,
 /*   260 */    15,   16,   17,   18,   19,   20,   21,   22,   23,    6,
 /*   270 */     2,    8,    9,   10,   11,    5,   65,    7,   67,   16,
 /*   280 */    17,   18,   19,    2,   73,   40,    0,    0,    5,    3,
 /*   290 */     7,    0,    6,    6,    8,    9,   10,   11,   12,   13,
 /*   300 */    14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   310 */     6,   24,    8,    9,   10,   11,   64,   35,   36,   67,
 /*   320 */    16,   17,   18,   19,   41,   73,   40,    0,    0,    5,
 /*   330 */     3,    7,    0,    6,    6,    8,    9,   10,   11,   12,
 /*   340 */    13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
 /*   350 */    23,    6,   24,    8,    9,   10,   11,   35,   36,   44,
 /*   360 */    45,   16,   17,   18,   19,   41,    0,   40,    0,   71,
 /*   370 */    72,    3,   74,   66,    6,   68,    8,    9,   10,   11,
 /*   380 */    12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
 /*   390 */    22,   23,   27,   28,   29,   30,   31,   32,   33,   56,
 /*   400 */     0,    5,    5,    7,    7,   62,   63,   64,   40,    0,
 /*   410 */    67,    6,    3,   70,    6,    6,   73,    8,    9,   10,
 /*   420 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   430 */    21,   22,   23,    0,   56,   39,   39,   41,   42,    6,
 /*   440 */    62,   63,    0,   65,    2,   67,   67,    2,   70,   40,
 /*   450 */     0,   73,   73,    3,   44,   45,    6,   24,    8,    9,
 /*   460 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   470 */    20,   21,   22,   23,    2,   56,   34,   35,   36,    2,
 /*   480 */    38,   62,   63,   64,   39,    2,   67,   42,    2,   70,
 /*   490 */    40,    0,   73,    0,    3,    2,    2,    6,    2,    8,
 /*   500 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   510 */    19,   20,   21,   22,   23,   56,   57,   34,   59,   56,
 /*   520 */    34,   62,   63,   35,   36,   62,   63,   34,   34,   70,
 /*   530 */    67,   40,    5,   70,    7,    3,   73,   69,    6,    2,
 /*   540 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   550 */    18,   19,   20,   21,   22,   23,   56,   65,   71,   72,
 /*   560 */     6,   74,   62,   63,   44,   45,   39,   67,   41,   42,
 /*   570 */    70,   34,   40,   73,    0,    5,    3,    7,   65,    6,
 /*   580 */     6,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   590 */    17,   18,   19,   20,   21,   22,   23,    0,   56,   25,
 /*   600 */     5,   58,    7,    6,   62,   63,   35,   36,    5,   67,
 /*   610 */     7,   41,   70,   40,    0,   73,   35,   36,    4,   69,
 /*   620 */     6,   24,    8,    9,   10,   11,   12,   13,   14,   15,
 /*   630 */    16,   17,   18,   19,   20,   21,   22,   23,    3,   35,
 /*   640 */    36,    6,   34,    8,    9,   10,   11,   12,   13,   14,
 /*   650 */    15,   16,   17,   18,   19,   20,   21,   22,   23,   35,
 /*   660 */    36,   35,   36,   35,   36,    5,    5,    7,    7,    5,
 /*   670 */     5,    7,    7,   44,   45,   40,    0,    0,   69,   66,
 /*   680 */     4,   68,    6,    6,    8,    9,   10,   11,   12,   13,
 /*   690 */    14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   700 */     3,   24,   25,    6,   65,    8,    9,   10,   11,   12,
 /*   710 */    13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
 /*   720 */    23,   58,    5,    5,    7,    7,    5,   65,    7,    5,
 /*   730 */     5,    7,    7,   58,   65,   47,   48,   40,   50,   51,
 /*   740 */    52,   53,   54,   55,   56,   57,   58,   69,   60,   61,
 /*   750 */    62,   63,   64,   69,    5,   67,    7,   39,   70,   41,
 /*   760 */    42,   73,    6,   58,    8,    9,   10,   11,   12,   13,
 /*   770 */    14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   780 */     4,   65,    6,    0,    8,    9,   10,   11,   39,    6,
 /*   790 */    41,   42,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   800 */     4,   65,    6,    0,    8,    9,   10,   11,   25,    6,
 /*   810 */    69,   69,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   820 */     4,   69,    6,    0,    8,    9,   10,   11,   25,    6,
 /*   830 */    69,   65,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   840 */     4,   69,    6,    0,    8,    9,   10,   11,   25,    6,
 /*   850 */    65,   58,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   860 */     4,   74,    6,    0,    8,    9,   10,   11,   25,    6,
 /*   870 */    58,   58,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   880 */     4,   66,    6,   68,    8,    9,   10,   11,   25,   58,
 /*   890 */    76,   66,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   900 */     4,   66,    6,   68,    8,    9,   10,   11,    5,   68,
 /*   910 */     7,   73,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   920 */     4,   66,    6,   68,    8,    9,   10,   11,    5,   68,
 /*   930 */     7,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   940 */     4,   66,    6,   68,    8,    9,   10,   11,    5,   68,
 /*   950 */     7,   68,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   960 */     4,   68,    6,   68,    8,    9,   10,   11,    5,   68,
 /*   970 */     7,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*   980 */     4,   68,    6,   75,    8,    9,   10,   11,    5,   75,
 /*   990 */     7,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*  1000 */     4,   77,    6,   77,    8,    9,   10,   11,    5,   77,
 /*  1010 */     7,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*  1020 */     4,   77,    6,   77,    8,    9,   10,   11,    5,   77,
 /*  1030 */     7,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*  1040 */     4,   77,    6,   77,    8,    9,   10,   11,   77,   77,
 /*  1050 */    77,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*  1060 */     4,   77,    6,   77,    8,    9,   10,   11,   77,   77,
 /*  1070 */    77,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*  1080 */     4,   77,    6,   77,    8,    9,   10,   11,   77,   77,
 /*  1090 */    77,   77,   16,   17,   18,   19,   20,   21,   22,   23,
 /*  1100 */     6,   77,    8,    9,   10,   11,   77,    4,   77,    6,
 /*  1110 */    16,   17,   18,   19,   20,   21,   22,   23,    0,   77,
 /*  1120 */    77,   77,    4,   20,   21,   22,   23,   77,   77,   77,
 /*  1130 */    12,   13,   14,   15,   77,   77,   77,   77,   20,   21,
 /*  1140 */    22,   23,    0,    0,   77,   77,    4,    0,   77,    6,
 /*  1150 */    77,   77,   77,    6,   12,   13,   14,   15,   40,   77,
 /*  1160 */    77,   43,   20,   21,   22,   23,    0,   24,   25,   77,
 /*  1170 */     4,   24,   25,   77,   77,   77,   77,   77,   12,   13,
 /*  1180 */    14,   15,   40,   77,   77,   43,   20,   21,   22,   23,
 /*  1190 */     0,    0,   77,   77,    4,    0,   77,    6,   77,   77,
 /*  1200 */    77,    6,   12,   13,   14,   15,   40,   77,   77,   43,
 /*  1210 */    20,   21,   22,   23,    0,   24,   25,   77,    4,   24,
 /*  1220 */    25,   77,   77,   77,   77,   77,   12,   13,   14,   15,
 /*  1230 */    40,   77,   77,   43,   20,   21,   22,   23,    0,   77,
 /*  1240 */    77,    2,    4,    0,    5,   77,    7,   77,   77,    6,
 /*  1250 */    12,   13,   14,   15,   40,   77,   77,   43,   20,   21,
 /*  1260 */    22,   23,    0,    0,   77,   77,    4,   24,   25,    6,
 /*  1270 */    77,   77,   77,   77,   12,   13,   14,   15,   40,   77,
 /*  1280 */    41,   43,   20,   21,   22,   23,    0,   24,   25,   77,
 /*  1290 */     4,   77,    5,   77,    7,   77,   77,   77,   12,   13,
 /*  1300 */    14,   15,   40,   77,   77,   43,   20,   21,   22,   23,
 /*  1310 */     0,   77,   77,   77,    4,   77,    5,   77,    7,   77,
 /*  1320 */    77,   77,   12,   13,   14,   15,   40,   77,   41,   43,
 /*  1330 */    20,   21,   22,   23,    0,   77,   77,   77,    4,   77,
 /*  1340 */    77,   77,   77,   77,   77,   77,   12,   13,   14,   15,
 /*  1350 */    40,   77,   41,   43,   20,   21,   22,   23,    0,   77,
 /*  1360 */    77,   77,    4,   77,   77,   77,   77,   77,   77,   77,
 /*  1370 */    12,   13,   14,   15,   40,   77,   77,   43,   20,   21,
 /*  1380 */    22,   23,   77,   77,    6,   77,    8,    9,   10,   11,
 /*  1390 */    77,   77,   77,   77,   16,   17,   18,   19,   40,    2,
 /*  1400 */    77,   43,    5,   77,    7,   77,   77,   77,    6,   77,
 /*  1410 */     8,    9,   10,   11,   77,   37,    4,   77,   16,   17,
 /*  1420 */    18,   19,   77,   77,   12,   13,   14,   15,   77,   77,
 /*  1430 */    77,   77,   20,   21,   22,   23,   39,   35,   41,   42,
 /*  1440 */     2,    4,   77,    5,   77,    7,    2,   77,   77,   77,
 /*  1450 */     6,    5,   77,    7,   77,    4,   77,   20,   21,   22,
 /*  1460 */    23,   77,   77,   77,   77,   77,   77,   77,   24,   25,
 /*  1470 */    77,   20,   21,   22,   23,   77,    4,   39,   77,   41,
 /*  1480 */    42,    4,   77,   39,   77,   39,   42,   41,   42,    5,
 /*  1490 */     4,    7,   20,   21,   22,   23,   77,   20,   21,   22,
 /*  1500 */    23,    5,   77,    7,   77,   77,   20,   21,   22,   23,
 /*  1510 */    77,   77,    5,   77,    7,   77,   77,   77,   77,   77,
 /*  1520 */    77,   77,   77,   39,   77,   41,   42,   77,   77,   77,
 /*  1530 */    77,   77,   77,   77,   77,   39,   77,   41,   42,   77,
 /*  1540 */    77,   77,   77,   77,   77,   77,   39,   77,   41,   42,
};
#define YY_SHIFT_USE_DFLT (-35)
#define YY_SHIFT_COUNT (198)
#define YY_SHIFT_MIN   (-34)
#define YY_SHIFT_MAX   (1507)
static const short yy_shift_ofst[] = {
 /*     0 */    -1,  697, 1358,  676,  756, 1076, 1056, 1094, 1094, 1402,
 /*    10 */  1036, 1016,  996,  976, 1094, 1094, 1094, 1094, 1378, 1412,
 /*    20 */  1103, 1263, 1243,  345, 1507, 1496, 1486, 1484, 1446,  749,
 /*    30 */   718,  527,  396,  365,  365, 1477, 1472, 1451, 1437, 1311,
 /*    40 */  1287,  570,  324,  445,  283,  201,  160,  119,  863,  843,
 /*    50 */   493,  629,  537,  494,  486,  483,   77,  554,  554,  408,
 /*    60 */   408,  408,  408,  554,  554,  608,  408,  554,  608,  408,
 /*    70 */   554,  608,  408,  554,  608,  408,  554,  554,  408,  491,
 /*    80 */   450,  409,  368,  327,  286,  245,  204,  163,  122,   81,
 /*    90 */    40,  635,  614,  573,  532,  956,  936,  916,  896,  876,
 /*   100 */   856,  836,  816,  796,  776, 1334, 1310, 1286, 1262, 1238,
 /*   110 */  1214, 1190, 1166, 1142, 1118,  304,  263,  222,  181,  140,
 /*   120 */    99, 1444, 1438, 1397,  442, 1239, 1195, 1191, 1147, 1143,
 /*   130 */   677,  397,  214,   -7,  -34,  597,  823,  433,  803,  783,
 /*   140 */   574,  328,  287,  520,  410,  315, 1023, 1003,  983,  963,
 /*   150 */   943,  923,  903,  725,  724,  721,  717,  665,  664,  661,
 /*   160 */   660,  603,  595,  270,  628,  626,  624,  604,  581,  571,
 /*   170 */   488,  322,  282,  127,   86,    9,   30,   10,  405,  496,
 /*   180 */   477,  472,  281,  400,  366,  332,  291,  247,  268,  235,
 /*   190 */   232,  194,  187,  149,  152,  114,   38,   43,    4,
};
#define YY_REDUCE_USE_DFLT (-27)
#define YY_REDUCE_COUNT (78)
#define YY_REDUCE_MIN   (-26)
#define YY_REDUCE_MAX   (914)
static const short yy_reduce_ofst[] = {
 /*     0 */   688,   12,  -26,  419,  343,  378,  542,  500,  463,  459,
 /*    10 */    91,   50,  132,  173,  173,  132,   91,   50,    8,  252,
 /*    20 */   211,  487,  298,   49,  875,  855,  379,  835,  815,  613,
 /*    30 */   307,  143,  102,  914,  908,  838,  838,  838,  838,  913,
 /*    40 */   901,  895,  893,  825,  883,  881,  861,  841,  787,  787,
 /*    50 */   793,  814,  831,  813,  812,  793,  772,  785,  766,  761,
 /*    60 */   752,  742,  741,  736,  716,  705,  684,  669,  675,  678,
 /*    70 */   662,  663,  609,  639,  543,  550,  513,  492,  468,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    10 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    20 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    30 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    40 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    50 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    60 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    70 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    80 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*    90 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   100 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   110 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   120 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   130 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   140 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   150 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   160 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   170 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   180 */   323,  323,  323,  323,  323,  323,  323,  323,  323,  323,
 /*   190 */   323,  323,  323,  323,  323,  323,  323,  323,  323,
};

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyidxMax;                 /* Maximum value of yyidx */
#endif
  int yyerrcnt;                 /* Shifts left before out of the error */
  CFCParseHeaderARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void CFCParseHeaderTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "FILE_START",    "SEMICOLON",     "RIGHT_CURLY_BRACE",
  "CLASS",         "COLON",         "IDENTIFIER",    "LEFT_CURLY_BRACE",
  "VOID",          "VA_LIST",       "INTEGER_TYPE_NAME",  "FLOAT_TYPE_NAME",
  "PUBLIC",        "PRIVATE",       "PARCEL",        "LOCAL",       
  "CONST",         "NULLABLE",      "INCREMENTED",   "DECREMENTED", 
  "INERT",         "INLINE",        "ABSTRACT",      "FINAL",       
  "ASTERISK",      "LEFT_SQUARE_BRACKET",  "RIGHT_SQUARE_BRACKET",  "INTEGER_LITERAL",
  "HEX_LITERAL",   "FLOAT_LITERAL",  "STRING_LITERAL",  "TRUE",        
  "FALSE",         "NULL",          "LEFT_PAREN",    "RIGHT_PAREN", 
  "COMMA",         "ELLIPSIS",      "EQUALS",        "SCOPE_OP",    
  "DOCUCOMMENT",   "INHERITS",      "CNICK",         "CBLOCK_START",
  "CBLOCK_CLOSE",  "BLOB",          "error",         "result",      
  "file",          "major_block",   "parcel_definition",  "class_declaration",
  "class_head",    "class_defs",    "var_declaration_statement",  "subroutine_declaration_statement",
  "type",          "param_variable",  "param_list",    "param_list_elems",
  "docucomment",   "cblock",        "type_qualifier",  "type_qualifier_list",
  "exposure_specifier",  "qualified_id",  "cnick",         "declaration_modifier_list",
  "class_inheritance",  "declarator",    "type_name",     "asterisk_postfix",
  "array_postfix",  "declaration_modifier",  "array_postfix_elem",  "scalar_constant",
  "blob",        
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "result ::= type",
 /*   1 */ "result ::= param_list",
 /*   2 */ "result ::= param_variable",
 /*   3 */ "result ::= docucomment",
 /*   4 */ "result ::= parcel_definition",
 /*   5 */ "result ::= cblock",
 /*   6 */ "result ::= var_declaration_statement",
 /*   7 */ "result ::= subroutine_declaration_statement",
 /*   8 */ "result ::= class_declaration",
 /*   9 */ "result ::= file",
 /*  10 */ "file ::= FILE_START",
 /*  11 */ "file ::= file major_block",
 /*  12 */ "major_block ::= class_declaration",
 /*  13 */ "major_block ::= cblock",
 /*  14 */ "major_block ::= parcel_definition",
 /*  15 */ "parcel_definition ::= exposure_specifier qualified_id SEMICOLON",
 /*  16 */ "parcel_definition ::= exposure_specifier qualified_id cnick SEMICOLON",
 /*  17 */ "class_declaration ::= class_defs RIGHT_CURLY_BRACE",
 /*  18 */ "class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id cnick class_inheritance",
 /*  19 */ "class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id cnick class_inheritance",
 /*  20 */ "class_head ::= docucomment declaration_modifier_list CLASS qualified_id cnick class_inheritance",
 /*  21 */ "class_head ::= declaration_modifier_list CLASS qualified_id cnick class_inheritance",
 /*  22 */ "class_head ::= docucomment exposure_specifier CLASS qualified_id cnick class_inheritance",
 /*  23 */ "class_head ::= exposure_specifier CLASS qualified_id cnick class_inheritance",
 /*  24 */ "class_head ::= docucomment CLASS qualified_id cnick class_inheritance",
 /*  25 */ "class_head ::= CLASS qualified_id cnick class_inheritance",
 /*  26 */ "class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id class_inheritance",
 /*  27 */ "class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id class_inheritance",
 /*  28 */ "class_head ::= docucomment declaration_modifier_list CLASS qualified_id class_inheritance",
 /*  29 */ "class_head ::= declaration_modifier_list CLASS qualified_id class_inheritance",
 /*  30 */ "class_head ::= docucomment exposure_specifier CLASS qualified_id class_inheritance",
 /*  31 */ "class_head ::= exposure_specifier CLASS qualified_id class_inheritance",
 /*  32 */ "class_head ::= docucomment CLASS qualified_id class_inheritance",
 /*  33 */ "class_head ::= CLASS qualified_id class_inheritance",
 /*  34 */ "class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id cnick",
 /*  35 */ "class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id cnick",
 /*  36 */ "class_head ::= docucomment declaration_modifier_list CLASS qualified_id cnick",
 /*  37 */ "class_head ::= declaration_modifier_list CLASS qualified_id cnick",
 /*  38 */ "class_head ::= docucomment exposure_specifier CLASS qualified_id cnick",
 /*  39 */ "class_head ::= exposure_specifier CLASS qualified_id cnick",
 /*  40 */ "class_head ::= docucomment CLASS qualified_id cnick",
 /*  41 */ "class_head ::= CLASS qualified_id cnick",
 /*  42 */ "class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id",
 /*  43 */ "class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id",
 /*  44 */ "class_head ::= docucomment declaration_modifier_list CLASS qualified_id",
 /*  45 */ "class_head ::= declaration_modifier_list CLASS qualified_id",
 /*  46 */ "class_head ::= docucomment exposure_specifier CLASS qualified_id",
 /*  47 */ "class_head ::= exposure_specifier CLASS qualified_id",
 /*  48 */ "class_head ::= docucomment CLASS qualified_id",
 /*  49 */ "class_head ::= CLASS qualified_id",
 /*  50 */ "class_head ::= class_head COLON IDENTIFIER",
 /*  51 */ "class_defs ::= class_head LEFT_CURLY_BRACE",
 /*  52 */ "class_defs ::= class_defs var_declaration_statement",
 /*  53 */ "class_defs ::= class_defs subroutine_declaration_statement",
 /*  54 */ "var_declaration_statement ::= type declarator SEMICOLON",
 /*  55 */ "var_declaration_statement ::= exposure_specifier type declarator SEMICOLON",
 /*  56 */ "var_declaration_statement ::= declaration_modifier_list type declarator SEMICOLON",
 /*  57 */ "var_declaration_statement ::= exposure_specifier declaration_modifier_list type declarator SEMICOLON",
 /*  58 */ "subroutine_declaration_statement ::= type declarator param_list SEMICOLON",
 /*  59 */ "subroutine_declaration_statement ::= declaration_modifier_list type declarator param_list SEMICOLON",
 /*  60 */ "subroutine_declaration_statement ::= exposure_specifier declaration_modifier_list type declarator param_list SEMICOLON",
 /*  61 */ "subroutine_declaration_statement ::= exposure_specifier type declarator param_list SEMICOLON",
 /*  62 */ "subroutine_declaration_statement ::= docucomment type declarator param_list SEMICOLON",
 /*  63 */ "subroutine_declaration_statement ::= docucomment declaration_modifier_list type declarator param_list SEMICOLON",
 /*  64 */ "subroutine_declaration_statement ::= docucomment exposure_specifier declaration_modifier_list type declarator param_list SEMICOLON",
 /*  65 */ "subroutine_declaration_statement ::= docucomment exposure_specifier type declarator param_list SEMICOLON",
 /*  66 */ "type ::= type_name",
 /*  67 */ "type ::= type_name asterisk_postfix",
 /*  68 */ "type ::= type_name array_postfix",
 /*  69 */ "type ::= type_qualifier_list type_name",
 /*  70 */ "type ::= type_qualifier_list type_name asterisk_postfix",
 /*  71 */ "type ::= type_qualifier_list type_name array_postfix",
 /*  72 */ "type_name ::= VOID",
 /*  73 */ "type_name ::= VA_LIST",
 /*  74 */ "type_name ::= INTEGER_TYPE_NAME",
 /*  75 */ "type_name ::= FLOAT_TYPE_NAME",
 /*  76 */ "type_name ::= IDENTIFIER",
 /*  77 */ "exposure_specifier ::= PUBLIC",
 /*  78 */ "exposure_specifier ::= PRIVATE",
 /*  79 */ "exposure_specifier ::= PARCEL",
 /*  80 */ "exposure_specifier ::= LOCAL",
 /*  81 */ "type_qualifier ::= CONST",
 /*  82 */ "type_qualifier ::= NULLABLE",
 /*  83 */ "type_qualifier ::= INCREMENTED",
 /*  84 */ "type_qualifier ::= DECREMENTED",
 /*  85 */ "type_qualifier_list ::= type_qualifier",
 /*  86 */ "type_qualifier_list ::= type_qualifier_list type_qualifier",
 /*  87 */ "declaration_modifier ::= INERT",
 /*  88 */ "declaration_modifier ::= INLINE",
 /*  89 */ "declaration_modifier ::= ABSTRACT",
 /*  90 */ "declaration_modifier ::= FINAL",
 /*  91 */ "declaration_modifier_list ::= declaration_modifier",
 /*  92 */ "declaration_modifier_list ::= declaration_modifier_list declaration_modifier",
 /*  93 */ "asterisk_postfix ::= ASTERISK",
 /*  94 */ "asterisk_postfix ::= asterisk_postfix ASTERISK",
 /*  95 */ "array_postfix_elem ::= LEFT_SQUARE_BRACKET RIGHT_SQUARE_BRACKET",
 /*  96 */ "array_postfix_elem ::= LEFT_SQUARE_BRACKET INTEGER_LITERAL RIGHT_SQUARE_BRACKET",
 /*  97 */ "array_postfix ::= array_postfix_elem",
 /*  98 */ "array_postfix ::= array_postfix array_postfix_elem",
 /*  99 */ "scalar_constant ::= HEX_LITERAL",
 /* 100 */ "scalar_constant ::= FLOAT_LITERAL",
 /* 101 */ "scalar_constant ::= INTEGER_LITERAL",
 /* 102 */ "scalar_constant ::= STRING_LITERAL",
 /* 103 */ "scalar_constant ::= TRUE",
 /* 104 */ "scalar_constant ::= FALSE",
 /* 105 */ "scalar_constant ::= NULL",
 /* 106 */ "declarator ::= IDENTIFIER",
 /* 107 */ "param_variable ::= type declarator",
 /* 108 */ "param_list ::= LEFT_PAREN RIGHT_PAREN",
 /* 109 */ "param_list ::= LEFT_PAREN param_list_elems RIGHT_PAREN",
 /* 110 */ "param_list ::= LEFT_PAREN param_list_elems COMMA ELLIPSIS RIGHT_PAREN",
 /* 111 */ "param_list_elems ::= param_list_elems COMMA param_variable",
 /* 112 */ "param_list_elems ::= param_list_elems COMMA param_variable EQUALS scalar_constant",
 /* 113 */ "param_list_elems ::= param_variable",
 /* 114 */ "param_list_elems ::= param_variable EQUALS scalar_constant",
 /* 115 */ "qualified_id ::= IDENTIFIER",
 /* 116 */ "qualified_id ::= qualified_id SCOPE_OP IDENTIFIER",
 /* 117 */ "docucomment ::= DOCUCOMMENT",
 /* 118 */ "class_inheritance ::= INHERITS qualified_id",
 /* 119 */ "cnick ::= CNICK IDENTIFIER",
 /* 120 */ "cblock ::= CBLOCK_START blob CBLOCK_CLOSE",
 /* 121 */ "cblock ::= CBLOCK_START CBLOCK_CLOSE",
 /* 122 */ "blob ::= BLOB",
 /* 123 */ "blob ::= blob BLOB",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to CFCParseHeader and CFCParseHeaderFree.
*/
void *CFCParseHeaderAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyidxMax = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    yyGrowStack(pParser);
#endif
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  CFCParseHeaderARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    case 47: /* result */
    case 49: /* major_block */
    case 55: /* subroutine_declaration_statement */
{
#line 229 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy58)); 
#line 1038 "../src/CFCParseHeader.c"
}
      break;
    case 48: /* file */
{
#line 230 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy19)); 
#line 1045 "../src/CFCParseHeader.c"
}
      break;
    case 50: /* parcel_definition */
{
#line 232 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy66)); 
#line 1052 "../src/CFCParseHeader.c"
}
      break;
    case 51: /* class_declaration */
    case 52: /* class_head */
    case 53: /* class_defs */
{
#line 233 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy71)); 
#line 1061 "../src/CFCParseHeader.c"
}
      break;
    case 54: /* var_declaration_statement */
    case 57: /* param_variable */
{
#line 236 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy111)); 
#line 1069 "../src/CFCParseHeader.c"
}
      break;
    case 56: /* type */
{
#line 238 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy41)); 
#line 1076 "../src/CFCParseHeader.c"
}
      break;
    case 58: /* param_list */
    case 59: /* param_list_elems */
{
#line 240 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy40)); 
#line 1084 "../src/CFCParseHeader.c"
}
      break;
    case 60: /* docucomment */
{
#line 242 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy151)); 
#line 1091 "../src/CFCParseHeader.c"
}
      break;
    case 61: /* cblock */
{
#line 243 "../src/CFCParseHeader.y"
 CFCBase_decref((CFCBase*)(yypminor->yy55)); 
#line 1098 "../src/CFCParseHeader.c"
}
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];

  if( pParser->yyidx<0 ) return 0;
#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor(pParser, yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from CFCParseHeaderAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void CFCParseHeaderFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int CFCParseHeaderStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyidxMax;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if( stateno>YY_SHIFT_COUNT
   || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   CFCParseHeaderARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
   CFCParseHeaderARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer to the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( yypParser->yyidx>yypParser->yyidxMax ){
    yypParser->yyidxMax = yypParser->yyidx;
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 47, 1 },
  { 48, 1 },
  { 48, 2 },
  { 49, 1 },
  { 49, 1 },
  { 49, 1 },
  { 50, 3 },
  { 50, 4 },
  { 51, 2 },
  { 52, 7 },
  { 52, 6 },
  { 52, 6 },
  { 52, 5 },
  { 52, 6 },
  { 52, 5 },
  { 52, 5 },
  { 52, 4 },
  { 52, 6 },
  { 52, 5 },
  { 52, 5 },
  { 52, 4 },
  { 52, 5 },
  { 52, 4 },
  { 52, 4 },
  { 52, 3 },
  { 52, 6 },
  { 52, 5 },
  { 52, 5 },
  { 52, 4 },
  { 52, 5 },
  { 52, 4 },
  { 52, 4 },
  { 52, 3 },
  { 52, 5 },
  { 52, 4 },
  { 52, 4 },
  { 52, 3 },
  { 52, 4 },
  { 52, 3 },
  { 52, 3 },
  { 52, 2 },
  { 52, 3 },
  { 53, 2 },
  { 53, 2 },
  { 53, 2 },
  { 54, 3 },
  { 54, 4 },
  { 54, 4 },
  { 54, 5 },
  { 55, 4 },
  { 55, 5 },
  { 55, 6 },
  { 55, 5 },
  { 55, 5 },
  { 55, 6 },
  { 55, 7 },
  { 55, 6 },
  { 56, 1 },
  { 56, 2 },
  { 56, 2 },
  { 56, 2 },
  { 56, 3 },
  { 56, 3 },
  { 70, 1 },
  { 70, 1 },
  { 70, 1 },
  { 70, 1 },
  { 70, 1 },
  { 64, 1 },
  { 64, 1 },
  { 64, 1 },
  { 64, 1 },
  { 62, 1 },
  { 62, 1 },
  { 62, 1 },
  { 62, 1 },
  { 63, 1 },
  { 63, 2 },
  { 73, 1 },
  { 73, 1 },
  { 73, 1 },
  { 73, 1 },
  { 67, 1 },
  { 67, 2 },
  { 71, 1 },
  { 71, 2 },
  { 74, 2 },
  { 74, 3 },
  { 72, 1 },
  { 72, 2 },
  { 75, 1 },
  { 75, 1 },
  { 75, 1 },
  { 75, 1 },
  { 75, 1 },
  { 75, 1 },
  { 75, 1 },
  { 69, 1 },
  { 57, 2 },
  { 58, 2 },
  { 58, 3 },
  { 58, 5 },
  { 59, 3 },
  { 59, 5 },
  { 59, 1 },
  { 59, 3 },
  { 65, 1 },
  { 65, 3 },
  { 60, 1 },
  { 68, 2 },
  { 66, 2 },
  { 61, 3 },
  { 61, 2 },
  { 76, 1 },
  { 76, 2 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  CFCParseHeaderARG_FETCH;
  yymsp = &yypParser->yystack[yypParser->yyidx];
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno>=0 
        && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  /*memset(&yygotominor, 0, sizeof(yygotominor));*/
  yygotominor = yyzerominor;


  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0: /* result ::= type */
#line 246 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy41);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy41);
}
#line 1520 "../src/CFCParseHeader.c"
        break;
      case 1: /* result ::= param_list */
#line 251 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy40);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy40);
}
#line 1528 "../src/CFCParseHeader.c"
        break;
      case 2: /* result ::= param_variable */
      case 6: /* result ::= var_declaration_statement */ yytestcase(yyruleno==6);
#line 256 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy111);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy111);
}
#line 1537 "../src/CFCParseHeader.c"
        break;
      case 3: /* result ::= docucomment */
#line 261 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy151);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy151);
}
#line 1545 "../src/CFCParseHeader.c"
        break;
      case 4: /* result ::= parcel_definition */
#line 266 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy66);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy66);
}
#line 1553 "../src/CFCParseHeader.c"
        break;
      case 5: /* result ::= cblock */
#line 271 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy55);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy55);
}
#line 1561 "../src/CFCParseHeader.c"
        break;
      case 7: /* result ::= subroutine_declaration_statement */
#line 281 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy58);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy58);
}
#line 1569 "../src/CFCParseHeader.c"
        break;
      case 8: /* result ::= class_declaration */
#line 286 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy71);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy71);
}
#line 1577 "../src/CFCParseHeader.c"
        break;
      case 9: /* result ::= file */
#line 291 "../src/CFCParseHeader.y"
{
    CFCParser_set_result(state, (CFCBase*)yymsp[0].minor.yy19);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy19);
}
#line 1585 "../src/CFCParseHeader.c"
        break;
      case 10: /* file ::= FILE_START */
#line 297 "../src/CFCParseHeader.y"
{
    yygotominor.yy19 = CFCFile_new(CFCParser_get_source_class(state));
}
#line 1592 "../src/CFCParseHeader.c"
        break;
      case 11: /* file ::= file major_block */
#line 301 "../src/CFCParseHeader.y"
{
    yygotominor.yy19 = yymsp[-1].minor.yy19;
    CFCFile_add_block(yygotominor.yy19, yymsp[0].minor.yy58);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy58);
}
#line 1601 "../src/CFCParseHeader.c"
        break;
      case 12: /* major_block ::= class_declaration */
#line 307 "../src/CFCParseHeader.y"
{ yygotominor.yy58 = (CFCBase*)yymsp[0].minor.yy71; }
#line 1606 "../src/CFCParseHeader.c"
        break;
      case 13: /* major_block ::= cblock */
#line 308 "../src/CFCParseHeader.y"
{ yygotominor.yy58 = (CFCBase*)yymsp[0].minor.yy55; }
#line 1611 "../src/CFCParseHeader.c"
        break;
      case 14: /* major_block ::= parcel_definition */
#line 309 "../src/CFCParseHeader.y"
{ yygotominor.yy58 = (CFCBase*)yymsp[0].minor.yy66; }
#line 1616 "../src/CFCParseHeader.c"
        break;
      case 15: /* parcel_definition ::= exposure_specifier qualified_id SEMICOLON */
#line 312 "../src/CFCParseHeader.y"
{
    if (strcmp(yymsp[-2].minor.yy0, "parcel") != 0) {
        /* Instead of this kludgy post-parse error trigger, we should require
         * PARCEL in this production as opposed to exposure_specifier.
         * However, that causes a parsing conflict because the keyword
         * "parcel" has two meanings in the Clownfish header language (parcel
         * declaration and exposure specifier). */
         CFCUtil_die("yygotominor.yy66 syntax error was detected when parsing '%s'", yymsp[-2].minor.yy0);
    }
    yygotominor.yy66 = CFCParcel_singleton(yymsp[-1].minor.yy0, NULL);
    CFCBase_incref((CFCBase*)yygotominor.yy66);
    CFCParser_set_parcel(state, yygotominor.yy66);
}
#line 1633 "../src/CFCParseHeader.c"
        break;
      case 16: /* parcel_definition ::= exposure_specifier qualified_id cnick SEMICOLON */
#line 327 "../src/CFCParseHeader.y"
{
    if (strcmp(yymsp[-3].minor.yy0, "parcel") != 0) {
         CFCUtil_die("yygotominor.yy66 syntax error was detected when parsing '%s'", yymsp[-3].minor.yy0);
    }
    yygotominor.yy66 = CFCParcel_singleton(yymsp[-2].minor.yy0, yymsp[-1].minor.yy0);
    CFCBase_incref((CFCBase*)yygotominor.yy66);
    CFCParser_set_parcel(state, yygotominor.yy66);
}
#line 1645 "../src/CFCParseHeader.c"
        break;
      case 17: /* class_declaration ::= class_defs RIGHT_CURLY_BRACE */
#line 337 "../src/CFCParseHeader.y"
{
    yygotominor.yy71 = yymsp[-1].minor.yy71;
    CFCParser_set_class_name(state, NULL);
    CFCParser_set_class_cnick(state, NULL);
}
#line 1654 "../src/CFCParseHeader.c"
        break;
      case 18: /* class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id cnick class_inheritance */
#line 343 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-6].minor.yy151,    yymsp[-5].minor.yy0,    yymsp[-4].minor.yy0,    yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1659 "../src/CFCParseHeader.c"
        break;
      case 19: /* class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id cnick class_inheritance */
#line 344 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-5].minor.yy0,    yymsp[-4].minor.yy0,    yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1664 "../src/CFCParseHeader.c"
        break;
      case 20: /* class_head ::= docucomment declaration_modifier_list CLASS qualified_id cnick class_inheritance */
#line 345 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-5].minor.yy151,    NULL, yymsp[-4].minor.yy0,    yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1669 "../src/CFCParseHeader.c"
        break;
      case 21: /* class_head ::= declaration_modifier_list CLASS qualified_id cnick class_inheritance */
#line 346 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, yymsp[-4].minor.yy0,    yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1674 "../src/CFCParseHeader.c"
        break;
      case 22: /* class_head ::= docucomment exposure_specifier CLASS qualified_id cnick class_inheritance */
#line 347 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-5].minor.yy151,    yymsp[-4].minor.yy0,    NULL, yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1679 "../src/CFCParseHeader.c"
        break;
      case 23: /* class_head ::= exposure_specifier CLASS qualified_id cnick class_inheritance */
#line 348 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-4].minor.yy0,    NULL, yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1684 "../src/CFCParseHeader.c"
        break;
      case 24: /* class_head ::= docucomment CLASS qualified_id cnick class_inheritance */
#line 349 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-4].minor.yy151,    NULL, NULL, yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1689 "../src/CFCParseHeader.c"
        break;
      case 25: /* class_head ::= CLASS qualified_id cnick class_inheritance */
#line 350 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, NULL, yymsp[-2].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0   ); }
#line 1694 "../src/CFCParseHeader.c"
        break;
      case 26: /* class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id class_inheritance */
#line 351 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-5].minor.yy151,    yymsp[-4].minor.yy0,    yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1699 "../src/CFCParseHeader.c"
        break;
      case 27: /* class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id class_inheritance */
#line 352 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-4].minor.yy0,    yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1704 "../src/CFCParseHeader.c"
        break;
      case 28: /* class_head ::= docucomment declaration_modifier_list CLASS qualified_id class_inheritance */
#line 353 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-4].minor.yy151,    NULL, yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1709 "../src/CFCParseHeader.c"
        break;
      case 29: /* class_head ::= declaration_modifier_list CLASS qualified_id class_inheritance */
#line 354 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1714 "../src/CFCParseHeader.c"
        break;
      case 30: /* class_head ::= docucomment exposure_specifier CLASS qualified_id class_inheritance */
#line 355 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-4].minor.yy151,    yymsp[-3].minor.yy0,    NULL, yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1719 "../src/CFCParseHeader.c"
        break;
      case 31: /* class_head ::= exposure_specifier CLASS qualified_id class_inheritance */
#line 356 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-3].minor.yy0,    NULL, yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1724 "../src/CFCParseHeader.c"
        break;
      case 32: /* class_head ::= docucomment CLASS qualified_id class_inheritance */
#line 357 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-3].minor.yy151,    NULL, NULL, yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1729 "../src/CFCParseHeader.c"
        break;
      case 33: /* class_head ::= CLASS qualified_id class_inheritance */
#line 358 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, NULL, yymsp[-1].minor.yy0,    NULL, yymsp[0].minor.yy0   ); }
#line 1734 "../src/CFCParseHeader.c"
        break;
      case 34: /* class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id cnick */
#line 359 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-5].minor.yy151,    yymsp[-4].minor.yy0,    yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1739 "../src/CFCParseHeader.c"
        break;
      case 35: /* class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id cnick */
#line 360 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-4].minor.yy0,    yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1744 "../src/CFCParseHeader.c"
        break;
      case 36: /* class_head ::= docucomment declaration_modifier_list CLASS qualified_id cnick */
#line 361 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-4].minor.yy151,    NULL, yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1749 "../src/CFCParseHeader.c"
        break;
      case 37: /* class_head ::= declaration_modifier_list CLASS qualified_id cnick */
#line 362 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, yymsp[-3].minor.yy0,    yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1754 "../src/CFCParseHeader.c"
        break;
      case 38: /* class_head ::= docucomment exposure_specifier CLASS qualified_id cnick */
#line 363 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-4].minor.yy151,    yymsp[-3].minor.yy0,    NULL, yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1759 "../src/CFCParseHeader.c"
        break;
      case 39: /* class_head ::= exposure_specifier CLASS qualified_id cnick */
#line 364 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-3].minor.yy0,    NULL, yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1764 "../src/CFCParseHeader.c"
        break;
      case 40: /* class_head ::= docucomment CLASS qualified_id cnick */
#line 365 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-3].minor.yy151,    NULL, NULL, yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1769 "../src/CFCParseHeader.c"
        break;
      case 41: /* class_head ::= CLASS qualified_id cnick */
#line 366 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, NULL, yymsp[-1].minor.yy0,    yymsp[0].minor.yy0,    NULL ); }
#line 1774 "../src/CFCParseHeader.c"
        break;
      case 42: /* class_head ::= docucomment exposure_specifier declaration_modifier_list CLASS qualified_id */
#line 367 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-4].minor.yy151,    yymsp[-3].minor.yy0,    yymsp[-2].minor.yy0,    yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1779 "../src/CFCParseHeader.c"
        break;
      case 43: /* class_head ::= exposure_specifier declaration_modifier_list CLASS qualified_id */
#line 368 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-3].minor.yy0,    yymsp[-2].minor.yy0,    yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1784 "../src/CFCParseHeader.c"
        break;
      case 44: /* class_head ::= docucomment declaration_modifier_list CLASS qualified_id */
#line 369 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-3].minor.yy151,    NULL, yymsp[-2].minor.yy0,    yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1789 "../src/CFCParseHeader.c"
        break;
      case 45: /* class_head ::= declaration_modifier_list CLASS qualified_id */
#line 370 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, yymsp[-2].minor.yy0,    yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1794 "../src/CFCParseHeader.c"
        break;
      case 46: /* class_head ::= docucomment exposure_specifier CLASS qualified_id */
#line 371 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-3].minor.yy151,    yymsp[-2].minor.yy0,    NULL, yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1799 "../src/CFCParseHeader.c"
        break;
      case 47: /* class_head ::= exposure_specifier CLASS qualified_id */
#line 372 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, yymsp[-2].minor.yy0,    NULL, yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1804 "../src/CFCParseHeader.c"
        break;
      case 48: /* class_head ::= docucomment CLASS qualified_id */
#line 373 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, yymsp[-2].minor.yy151,    NULL, NULL, yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1809 "../src/CFCParseHeader.c"
        break;
      case 49: /* class_head ::= CLASS qualified_id */
#line 374 "../src/CFCParseHeader.y"
{ yygotominor.yy71 = S_start_class(state, NULL, NULL, NULL, yymsp[0].minor.yy0,    NULL, NULL ); }
#line 1814 "../src/CFCParseHeader.c"
        break;
      case 50: /* class_head ::= class_head COLON IDENTIFIER */
#line 377 "../src/CFCParseHeader.y"
{
    yygotominor.yy71 = yymsp[-2].minor.yy71;
    CFCClass_add_attribute(yygotominor.yy71, yymsp[0].minor.yy0, "1");
}
#line 1822 "../src/CFCParseHeader.c"
        break;
      case 51: /* class_defs ::= class_head LEFT_CURLY_BRACE */
#line 383 "../src/CFCParseHeader.y"
{
    yygotominor.yy71 = yymsp[-1].minor.yy71;
}
#line 1829 "../src/CFCParseHeader.c"
        break;
      case 52: /* class_defs ::= class_defs var_declaration_statement */
#line 387 "../src/CFCParseHeader.y"
{
    yygotominor.yy71 = yymsp[-1].minor.yy71;
    if (CFCVariable_inert(yymsp[0].minor.yy111)) {
        CFCClass_add_inert_var(yygotominor.yy71, yymsp[0].minor.yy111);
    }
    else {
        CFCClass_add_member_var(yygotominor.yy71, yymsp[0].minor.yy111);
    }
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy111);
}
#line 1843 "../src/CFCParseHeader.c"
        break;
      case 53: /* class_defs ::= class_defs subroutine_declaration_statement */
#line 398 "../src/CFCParseHeader.y"
{
    yygotominor.yy71 = yymsp[-1].minor.yy71;
    if (strcmp(CFCBase_get_cfc_class(yymsp[0].minor.yy58), "Clownfish::CFC::Function") == 0) {
        CFCClass_add_function(yygotominor.yy71, (CFCFunction*)yymsp[0].minor.yy58);
    }
    else {
        CFCClass_add_method(yygotominor.yy71, (CFCMethod*)yymsp[0].minor.yy58);
    }
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy58);
}
#line 1857 "../src/CFCParseHeader.c"
        break;
      case 54: /* var_declaration_statement ::= type declarator SEMICOLON */
#line 411 "../src/CFCParseHeader.y"
{
    yygotominor.yy111 = S_new_var(state, CFCParser_dupe(state, "parcel"), NULL, yymsp[-2].minor.yy41, yymsp[-1].minor.yy0);
}
#line 1864 "../src/CFCParseHeader.c"
        break;
      case 55: /* var_declaration_statement ::= exposure_specifier type declarator SEMICOLON */
#line 417 "../src/CFCParseHeader.y"
{
    yygotominor.yy111 = S_new_var(state, yymsp[-3].minor.yy0, NULL, yymsp[-2].minor.yy41, yymsp[-1].minor.yy0);
}
#line 1871 "../src/CFCParseHeader.c"
        break;
      case 56: /* var_declaration_statement ::= declaration_modifier_list type declarator SEMICOLON */
#line 423 "../src/CFCParseHeader.y"
{
    yygotominor.yy111 = S_new_var(state, CFCParser_dupe(state, "parcel"), yymsp[-3].minor.yy0, yymsp[-2].minor.yy41, yymsp[-1].minor.yy0);
}
#line 1878 "../src/CFCParseHeader.c"
        break;
      case 57: /* var_declaration_statement ::= exposure_specifier declaration_modifier_list type declarator SEMICOLON */
#line 430 "../src/CFCParseHeader.y"
{
    yygotominor.yy111 = S_new_var(state, yymsp[-4].minor.yy0, yymsp[-3].minor.yy0, yymsp[-2].minor.yy41, yymsp[-1].minor.yy0);
}
#line 1885 "../src/CFCParseHeader.c"
        break;
      case 58: /* subroutine_declaration_statement ::= type declarator param_list SEMICOLON */
#line 436 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, NULL, NULL, NULL, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1892 "../src/CFCParseHeader.c"
        break;
      case 59: /* subroutine_declaration_statement ::= declaration_modifier_list type declarator param_list SEMICOLON */
#line 442 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, NULL, NULL, yymsp[-4].minor.yy0, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1899 "../src/CFCParseHeader.c"
        break;
      case 60: /* subroutine_declaration_statement ::= exposure_specifier declaration_modifier_list type declarator param_list SEMICOLON */
#line 449 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, NULL, yymsp[-5].minor.yy0, yymsp[-4].minor.yy0, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1906 "../src/CFCParseHeader.c"
        break;
      case 61: /* subroutine_declaration_statement ::= exposure_specifier type declarator param_list SEMICOLON */
#line 455 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, NULL, yymsp[-4].minor.yy0, NULL, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1913 "../src/CFCParseHeader.c"
        break;
      case 62: /* subroutine_declaration_statement ::= docucomment type declarator param_list SEMICOLON */
#line 461 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, yymsp[-4].minor.yy151, NULL, NULL, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1920 "../src/CFCParseHeader.c"
        break;
      case 63: /* subroutine_declaration_statement ::= docucomment declaration_modifier_list type declarator param_list SEMICOLON */
#line 468 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, yymsp[-5].minor.yy151, NULL, yymsp[-4].minor.yy0, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1927 "../src/CFCParseHeader.c"
        break;
      case 64: /* subroutine_declaration_statement ::= docucomment exposure_specifier declaration_modifier_list type declarator param_list SEMICOLON */
#line 476 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, yymsp[-6].minor.yy151, yymsp[-5].minor.yy0, yymsp[-4].minor.yy0, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1934 "../src/CFCParseHeader.c"
        break;
      case 65: /* subroutine_declaration_statement ::= docucomment exposure_specifier type declarator param_list SEMICOLON */
#line 483 "../src/CFCParseHeader.y"
{
    yygotominor.yy58 = S_new_sub(state, yymsp[-5].minor.yy151, yymsp[-4].minor.yy0, NULL, yymsp[-3].minor.yy41, yymsp[-2].minor.yy0, yymsp[-1].minor.yy40);
}
#line 1941 "../src/CFCParseHeader.c"
        break;
      case 66: /* type ::= type_name */
#line 488 "../src/CFCParseHeader.y"
{
    yygotominor.yy41 = S_new_type(state, 0, yymsp[0].minor.yy0, NULL, NULL);
}
#line 1948 "../src/CFCParseHeader.c"
        break;
      case 67: /* type ::= type_name asterisk_postfix */
#line 492 "../src/CFCParseHeader.y"
{
    yygotominor.yy41 = S_new_type(state, 0, yymsp[-1].minor.yy0, yymsp[0].minor.yy0, NULL);
}
#line 1955 "../src/CFCParseHeader.c"
        break;
      case 68: /* type ::= type_name array_postfix */
#line 496 "../src/CFCParseHeader.y"
{
    yygotominor.yy41 = S_new_type(state, 0, yymsp[-1].minor.yy0, NULL, yymsp[0].minor.yy0);
}
#line 1962 "../src/CFCParseHeader.c"
        break;
      case 69: /* type ::= type_qualifier_list type_name */
#line 500 "../src/CFCParseHeader.y"
{
    yygotominor.yy41 = S_new_type(state, yymsp[-1].minor.yy130, yymsp[0].minor.yy0, NULL, NULL);
}
#line 1969 "../src/CFCParseHeader.c"
        break;
      case 70: /* type ::= type_qualifier_list type_name asterisk_postfix */
#line 504 "../src/CFCParseHeader.y"
{
    yygotominor.yy41 = S_new_type(state, yymsp[-2].minor.yy130, yymsp[-1].minor.yy0, yymsp[0].minor.yy0, NULL);
}
#line 1976 "../src/CFCParseHeader.c"
        break;
      case 71: /* type ::= type_qualifier_list type_name array_postfix */
#line 508 "../src/CFCParseHeader.y"
{
    yygotominor.yy41 = S_new_type(state, yymsp[-2].minor.yy130, yymsp[-1].minor.yy0, NULL, yymsp[0].minor.yy0);
}
#line 1983 "../src/CFCParseHeader.c"
        break;
      case 72: /* type_name ::= VOID */
      case 73: /* type_name ::= VA_LIST */ yytestcase(yyruleno==73);
      case 74: /* type_name ::= INTEGER_TYPE_NAME */ yytestcase(yyruleno==74);
      case 75: /* type_name ::= FLOAT_TYPE_NAME */ yytestcase(yyruleno==75);
      case 76: /* type_name ::= IDENTIFIER */ yytestcase(yyruleno==76);
      case 77: /* exposure_specifier ::= PUBLIC */ yytestcase(yyruleno==77);
      case 78: /* exposure_specifier ::= PRIVATE */ yytestcase(yyruleno==78);
      case 79: /* exposure_specifier ::= PARCEL */ yytestcase(yyruleno==79);
      case 80: /* exposure_specifier ::= LOCAL */ yytestcase(yyruleno==80);
      case 87: /* declaration_modifier ::= INERT */ yytestcase(yyruleno==87);
      case 88: /* declaration_modifier ::= INLINE */ yytestcase(yyruleno==88);
      case 89: /* declaration_modifier ::= ABSTRACT */ yytestcase(yyruleno==89);
      case 90: /* declaration_modifier ::= FINAL */ yytestcase(yyruleno==90);
      case 91: /* declaration_modifier_list ::= declaration_modifier */ yytestcase(yyruleno==91);
      case 93: /* asterisk_postfix ::= ASTERISK */ yytestcase(yyruleno==93);
      case 97: /* array_postfix ::= array_postfix_elem */ yytestcase(yyruleno==97);
      case 99: /* scalar_constant ::= HEX_LITERAL */ yytestcase(yyruleno==99);
      case 100: /* scalar_constant ::= FLOAT_LITERAL */ yytestcase(yyruleno==100);
      case 101: /* scalar_constant ::= INTEGER_LITERAL */ yytestcase(yyruleno==101);
      case 102: /* scalar_constant ::= STRING_LITERAL */ yytestcase(yyruleno==102);
      case 103: /* scalar_constant ::= TRUE */ yytestcase(yyruleno==103);
      case 104: /* scalar_constant ::= FALSE */ yytestcase(yyruleno==104);
      case 105: /* scalar_constant ::= NULL */ yytestcase(yyruleno==105);
      case 115: /* qualified_id ::= IDENTIFIER */ yytestcase(yyruleno==115);
      case 118: /* class_inheritance ::= INHERITS qualified_id */ yytestcase(yyruleno==118);
      case 119: /* cnick ::= CNICK IDENTIFIER */ yytestcase(yyruleno==119);
      case 122: /* blob ::= BLOB */ yytestcase(yyruleno==122);
#line 512 "../src/CFCParseHeader.y"
{ yygotominor.yy0 = yymsp[0].minor.yy0; }
#line 2014 "../src/CFCParseHeader.c"
        break;
      case 81: /* type_qualifier ::= CONST */
#line 523 "../src/CFCParseHeader.y"
{ yygotominor.yy130 = CFCTYPE_CONST; }
#line 2019 "../src/CFCParseHeader.c"
        break;
      case 82: /* type_qualifier ::= NULLABLE */
#line 524 "../src/CFCParseHeader.y"
{ yygotominor.yy130 = CFCTYPE_NULLABLE; }
#line 2024 "../src/CFCParseHeader.c"
        break;
      case 83: /* type_qualifier ::= INCREMENTED */
#line 525 "../src/CFCParseHeader.y"
{ yygotominor.yy130 = CFCTYPE_INCREMENTED; }
#line 2029 "../src/CFCParseHeader.c"
        break;
      case 84: /* type_qualifier ::= DECREMENTED */
#line 526 "../src/CFCParseHeader.y"
{ yygotominor.yy130 = CFCTYPE_DECREMENTED; }
#line 2034 "../src/CFCParseHeader.c"
        break;
      case 85: /* type_qualifier_list ::= type_qualifier */
#line 529 "../src/CFCParseHeader.y"
{
    yygotominor.yy130 = yymsp[0].minor.yy130;
}
#line 2041 "../src/CFCParseHeader.c"
        break;
      case 86: /* type_qualifier_list ::= type_qualifier_list type_qualifier */
#line 533 "../src/CFCParseHeader.y"
{
    yygotominor.yy130 = yymsp[-1].minor.yy130;
    yygotominor.yy130 |= yymsp[0].minor.yy130;
}
#line 2049 "../src/CFCParseHeader.c"
        break;
      case 92: /* declaration_modifier_list ::= declaration_modifier_list declaration_modifier */
#line 545 "../src/CFCParseHeader.y"
{
    size_t size = strlen(yymsp[-1].minor.yy0) + strlen(yymsp[0].minor.yy0) + 2;
    yygotominor.yy0 = (char*)CFCParser_allocate(state, size);
    sprintf(yygotominor.yy0, "%s %s", yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
}
#line 2058 "../src/CFCParseHeader.c"
        break;
      case 94: /* asterisk_postfix ::= asterisk_postfix ASTERISK */
#line 553 "../src/CFCParseHeader.y"
{
    size_t size = strlen(yymsp[-1].minor.yy0) + 2;
    yygotominor.yy0 = (char*)CFCParser_allocate(state, size);
    sprintf(yygotominor.yy0, "%s*", yymsp[-1].minor.yy0);
}
#line 2067 "../src/CFCParseHeader.c"
        break;
      case 95: /* array_postfix_elem ::= LEFT_SQUARE_BRACKET RIGHT_SQUARE_BRACKET */
#line 560 "../src/CFCParseHeader.y"
{
    yygotominor.yy0 = CFCParser_dupe(state, "[]");
}
#line 2074 "../src/CFCParseHeader.c"
        break;
      case 96: /* array_postfix_elem ::= LEFT_SQUARE_BRACKET INTEGER_LITERAL RIGHT_SQUARE_BRACKET */
#line 564 "../src/CFCParseHeader.y"
{
    size_t size = strlen(yymsp[-1].minor.yy0) + 3;
    yygotominor.yy0 = (char*)CFCParser_allocate(state, size);
    sprintf(yygotominor.yy0, "[%s]", yymsp[-1].minor.yy0);
}
#line 2083 "../src/CFCParseHeader.c"
        break;
      case 98: /* array_postfix ::= array_postfix array_postfix_elem */
      case 123: /* blob ::= blob BLOB */ yytestcase(yyruleno==123);
#line 572 "../src/CFCParseHeader.y"
{
    size_t size = strlen(yymsp[-1].minor.yy0) + strlen(yymsp[0].minor.yy0) + 1;
    yygotominor.yy0 = (char*)CFCParser_allocate(state, size);
    sprintf(yygotominor.yy0, "%s%s", yymsp[-1].minor.yy0, yymsp[0].minor.yy0);
}
#line 2093 "../src/CFCParseHeader.c"
        break;
      case 106: /* declarator ::= IDENTIFIER */
#line 587 "../src/CFCParseHeader.y"
{
    yygotominor.yy0 = yymsp[0].minor.yy0;
}
#line 2100 "../src/CFCParseHeader.c"
        break;
      case 107: /* param_variable ::= type declarator */
#line 592 "../src/CFCParseHeader.y"
{
    yygotominor.yy111 = S_new_var(state, NULL, NULL, yymsp[-1].minor.yy41, yymsp[0].minor.yy0);
}
#line 2107 "../src/CFCParseHeader.c"
        break;
      case 108: /* param_list ::= LEFT_PAREN RIGHT_PAREN */
#line 597 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = CFCParamList_new(false);
}
#line 2114 "../src/CFCParseHeader.c"
        break;
      case 109: /* param_list ::= LEFT_PAREN param_list_elems RIGHT_PAREN */
#line 601 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = yymsp[-1].minor.yy40;
}
#line 2121 "../src/CFCParseHeader.c"
        break;
      case 110: /* param_list ::= LEFT_PAREN param_list_elems COMMA ELLIPSIS RIGHT_PAREN */
#line 605 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = yymsp[-3].minor.yy40;
    CFCParamList_set_variadic(yygotominor.yy40, true);
}
#line 2129 "../src/CFCParseHeader.c"
        break;
      case 111: /* param_list_elems ::= param_list_elems COMMA param_variable */
#line 610 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = yymsp[-2].minor.yy40;
    CFCParamList_add_param(yygotominor.yy40, yymsp[0].minor.yy111, NULL);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy111);
}
#line 2138 "../src/CFCParseHeader.c"
        break;
      case 112: /* param_list_elems ::= param_list_elems COMMA param_variable EQUALS scalar_constant */
#line 616 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = yymsp[-4].minor.yy40;
    CFCParamList_add_param(yygotominor.yy40, yymsp[-2].minor.yy111, yymsp[0].minor.yy0);
    CFCBase_decref((CFCBase*)yymsp[-2].minor.yy111);
}
#line 2147 "../src/CFCParseHeader.c"
        break;
      case 113: /* param_list_elems ::= param_variable */
#line 622 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = CFCParamList_new(false);
    CFCParamList_add_param(yygotominor.yy40, yymsp[0].minor.yy111, NULL);
    CFCBase_decref((CFCBase*)yymsp[0].minor.yy111);
}
#line 2156 "../src/CFCParseHeader.c"
        break;
      case 114: /* param_list_elems ::= param_variable EQUALS scalar_constant */
#line 628 "../src/CFCParseHeader.y"
{
    yygotominor.yy40 = CFCParamList_new(false);
    CFCParamList_add_param(yygotominor.yy40, yymsp[-2].minor.yy111, yymsp[0].minor.yy0);
    CFCBase_decref((CFCBase*)yymsp[-2].minor.yy111);
}
#line 2165 "../src/CFCParseHeader.c"
        break;
      case 116: /* qualified_id ::= qualified_id SCOPE_OP IDENTIFIER */
#line 636 "../src/CFCParseHeader.y"
{
    size_t size = strlen(yymsp[-2].minor.yy0) + strlen(yymsp[0].minor.yy0) + 3;
    yygotominor.yy0 = (char*)CFCParser_allocate(state, size);
    sprintf(yygotominor.yy0, "%s::%s", yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
#line 2174 "../src/CFCParseHeader.c"
        break;
      case 117: /* docucomment ::= DOCUCOMMENT */
#line 642 "../src/CFCParseHeader.y"
{ yygotominor.yy151 = CFCDocuComment_parse(yymsp[0].minor.yy0); }
#line 2179 "../src/CFCParseHeader.c"
        break;
      case 120: /* cblock ::= CBLOCK_START blob CBLOCK_CLOSE */
#line 645 "../src/CFCParseHeader.y"
{ yygotominor.yy55 = CFCCBlock_new(yymsp[-1].minor.yy0); }
#line 2184 "../src/CFCParseHeader.c"
        break;
      case 121: /* cblock ::= CBLOCK_START CBLOCK_CLOSE */
#line 646 "../src/CFCParseHeader.y"
{ yygotominor.yy55 = CFCCBlock_new(""); }
#line 2189 "../src/CFCParseHeader.c"
        break;
      default:
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = (YYACTIONTYPE)yyact;
      yymsp->major = (YYCODETYPE)yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else{
    assert( yyact == YYNSTATE + YYNRULE + 1 );
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  CFCParseHeaderARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  CFCParseHeaderARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  CFCParseHeaderARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 207 "../src/CFCParseHeader.y"

    CFCParser_set_errors(state, true);
#line 2254 "../src/CFCParseHeader.c"
  CFCParseHeaderARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  CFCParseHeaderARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  CFCParseHeaderARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "CFCParseHeaderAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void CFCParseHeader(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  CFCParseHeaderTOKENTYPE yyminor       /* The value for the token */
  CFCParseHeaderARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      /*memset(&yyminorunion, 0, sizeof(yyminorunion));*/
      yyminorunion = yyzerominor;
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  CFCParseHeaderARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact<YYNSTATE ){
      assert( !yyendofinput );  /* Impossible to shift the $ token */
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_reduce_action(
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor,yyminorunion);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
