/**
 * Generic macros
 */

#ifndef BACKEND_SHADERS_MACROS_H
#define BACKEND_SHADERS_MACROS_H

// Preprocessor argument counter
#define COUNT(...) COUNT_I(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,)
#define COUNT_I(_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,X,...) X

// Preprocessor paster
#define GLUE(A,B) GLUE_I(A,B)
#define GLUE_I(A,B) A##B

// Preprocessor list indexer
#define IOTA(n) GLUE(IOTA_, n)
#define IOTA_1 0
#define IOTA_2 0, 1
#define IOTA_3 0, 1, 2
#define IOTA_4 0, 1, 2, 3
#define IOTA_5 0, 1, 2, 3, 4
#define IOTA_6 0, 1, 2, 3, 4, 5
#define IOTA_7 0, 1, 2, 3, 4, 5, 6
#define IOTA_8 0, 1, 2, 3, 4, 5, 6, 7
#define IOTA_9 0, 1, 2, 3, 4, 5, 6, 7, 8
#define IOTA_10 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
#define IOTA_11 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
#define IOTA_12 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
#define IOTA_13 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
#define IOTA_14 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
#define IOTA_15 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
#define IOTA_16 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15

// --- FOR_EACH engines -------------------------------------------------------
// Apply OP(ctx, x) to each variadic arg. `ctx` is one token threaded unchanged
// to every call (pass ~ when unused; bundle several fixed args as a paren tuple
// and unpack with UNPAREN inside the op). Two flavours:
//   FOR_EACH   - concatenative: OP supplies its own trailing punctuation.
//   FOR_EACH_C - comma-infix: OP results joined by `,`, with NO trailing comma.

#define FOR_EACH(op, ctx, ...) GLUE(FOR_EACH_, COUNT(__VA_ARGS__))(op, ctx, __VA_ARGS__)
#define FOR_EACH_1(op, ctx, a)      op(ctx, a)
#define FOR_EACH_2(op, ctx, a, ...) op(ctx, a) FOR_EACH_1(op, ctx, __VA_ARGS__)
#define FOR_EACH_3(op, ctx, a, ...) op(ctx, a) FOR_EACH_2(op, ctx, __VA_ARGS__)
#define FOR_EACH_4(op, ctx, a, ...) op(ctx, a) FOR_EACH_3(op, ctx, __VA_ARGS__)
#define FOR_EACH_5(op, ctx, a, ...) op(ctx, a) FOR_EACH_4(op, ctx, __VA_ARGS__)
#define FOR_EACH_6(op, ctx, a, ...) op(ctx, a) FOR_EACH_5(op, ctx, __VA_ARGS__)
#define FOR_EACH_7(op, ctx, a, ...) op(ctx, a) FOR_EACH_6(op, ctx, __VA_ARGS__)
#define FOR_EACH_8(op, ctx, a, ...) op(ctx, a) FOR_EACH_7(op, ctx, __VA_ARGS__)
#define FOR_EACH_9(op, ctx, a, ...) op(ctx, a) FOR_EACH_8(op, ctx, __VA_ARGS__)
#define FOR_EACH_10(op, ctx, a, ...) op(ctx, a) FOR_EACH_9(op, ctx, __VA_ARGS__)
#define FOR_EACH_11(op, ctx, a, ...) op(ctx, a) FOR_EACH_10(op, ctx, __VA_ARGS__)
#define FOR_EACH_12(op, ctx, a, ...) op(ctx, a) FOR_EACH_11(op, ctx, __VA_ARGS__)
#define FOR_EACH_13(op, ctx, a, ...) op(ctx, a) FOR_EACH_12(op, ctx, __VA_ARGS__)
#define FOR_EACH_14(op, ctx, a, ...) op(ctx, a) FOR_EACH_13(op, ctx, __VA_ARGS__)
#define FOR_EACH_15(op, ctx, a, ...) op(ctx, a) FOR_EACH_14(op, ctx, __VA_ARGS__)
#define FOR_EACH_16(op, ctx, a, ...) op(ctx, a) FOR_EACH_15(op, ctx, __VA_ARGS__)

#define FOR_EACH_C(op, ctx, ...) GLUE(FOR_EACH_C_, COUNT(__VA_ARGS__))(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_1(op, ctx, a)      op(ctx, a)
#define FOR_EACH_C_2(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_1(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_3(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_2(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_4(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_3(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_5(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_4(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_6(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_5(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_7(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_6(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_8(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_7(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_9(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_8(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_10(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_9(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_11(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_10(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_12(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_11(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_13(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_12(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_14(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_13(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_15(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_14(op, ctx, __VA_ARGS__)
#define FOR_EACH_C_16(op, ctx, a, ...) op(ctx, a), FOR_EACH_C_15(op, ctx, __VA_ARGS__)

// Strip one layer of parens off a tuple, and re-invoke a macro after its args
// have been expanded (lets an op unpack a paren-bundled multi-arg ctx).
#define UNPAREN(...) __VA_ARGS__
#define APPLY(f, ...) f(__VA_ARGS__)

// --- per-element ops --------------------------------------------------------
#define OP_PASTE_PRE(v, a) v##a               // value##a  (JOIN, PREPEND)
#define OP_DECL(type, a)   type a;            // `type a;` (STRUCT)
#define OP_DECL_C(type, a) type a,            // `type a,` (COMMA)
#define OP_MEMBER(name, a) name.a=a;          // `name.a=a;` (SET_STRUCT)

// --- chain families ---------------------------------------------------------
// Join words into a comma list (no trailing comma): value##a, value##b, ...
#define JOIN_CHAIN(value, ...)    FOR_EACH_C(OP_PASTE_PRE, value, __VA_ARGS__)
// Prepend a word to each element (space separated): value##a value##b ...
#define PREPEND_CHAIN(value, ...) FOR_EACH(OP_PASTE_PRE, value, __VA_ARGS__)
// `type a;` per element (struct member declarations)
#define STRUCT_CHAIN(type, ...)   FOR_EACH(OP_DECL, type, __VA_ARGS__)
// `type a,` per element (parameter lists; keeps the trailing comma)
#define COMMA_CHAIN(type, ...)    FOR_EACH(OP_DECL_C, type, __VA_ARGS__)

#endif // BACKEND_SHADERS_MACROS_H
