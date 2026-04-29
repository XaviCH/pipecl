/**
 * Generic macros
 * 
 * TODO: Simplify macros to more generic ones.
 */

#ifndef BACKEND_SHADERS_MACROS_H
#define BACKEND_SHADERS_MACROS_H

// A preprocessor argument counter
#define COUNT(...) COUNT_I(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1,)
#define COUNT_I(_9,_8,_7,_6,_5,_4,_3,_2,_1,X,...) X
// Preprocessor paster
#define GLUE(A,B) GLUE_I(A,B)
#define GLUE_I(A,B) A##B


// 
#define JOIN_CHAIN(value, ...) GLUE(JOIN_CHAIN_,COUNT(__VA_ARGS__))(value, __VA_ARGS__)
// chain
#define JOIN_CHAIN_1(value, a) value##a
#define JOIN_CHAIN_2(value, a,...) value##a, JOIN_CHAIN_1(value, __VA_ARGS__)
#define JOIN_CHAIN_3(value, a,...) value##a, JOIN_CHAIN_2(value, __VA_ARGS__)
#define JOIN_CHAIN_4(value, a,...) value##a, JOIN_CHAIN_3(value, __VA_ARGS__)
#define JOIN_CHAIN_5(value, a,...) value##a, JOIN_CHAIN_4(value, __VA_ARGS__)
#define JOIN_CHAIN_6(value, a,...) value##a, JOIN_CHAIN_5(value, __VA_ARGS__)
#define JOIN_CHAIN_7(value, a,...) value##a, JOIN_CHAIN_6(value, __VA_ARGS__)
#define JOIN_CHAIN_8(value, a,...) value##a, JOIN_CHAIN_7(value, __VA_ARGS__)
#define JOIN_CHAIN_9(value, a,...) value##a, JOIN_CHAIN_8(value, __VA_ARGS__)

// Prepend a word to each element in a chain
#define PREPEND_CHAIN(value, ...) GLUE(PREPEND_CHAIN_,COUNT(__VA_ARGS__))(value, __VA_ARGS__)
#define PREPEND_CHAIN_1(value, a) value##a
#define PREPEND_CHAIN_2(value, a,...) value##a PREPEND_CHAIN_1(value, __VA_ARGS__)
#define PREPEND_CHAIN_3(value, a,...) value##a PREPEND_CHAIN_2(value, __VA_ARGS__)
#define PREPEND_CHAIN_4(value, a,...) value##a PREPEND_CHAIN_3(value, __VA_ARGS__)
#define PREPEND_CHAIN_5(value, a,...) value##a PREPEND_CHAIN_4(value, __VA_ARGS__)
#define PREPEND_CHAIN_6(value, a,...) value##a PREPEND_CHAIN_5(value, __VA_ARGS__)
#define PREPEND_CHAIN_7(value, a,...) value##a PREPEND_CHAIN_6(value, __VA_ARGS__)
#define PREPEND_CHAIN_8(value, a,...) value##a PREPEND_CHAIN_7(value, __VA_ARGS__)
#define PREPEND_CHAIN_9(value, a,...) value##a PREPEND_CHAIN_8(value, __VA_ARGS__)

// Append a word to each element in a chain
#define APPEND_CHAIN(value, ...) GLUE(APPEND_CHAIN_,COUNT(__VA_ARGS__))(value, __VA_ARGS__)
#define APPEND_CHAIN_1(value, a) a##value
#define APPEND_CHAIN_2(value, a,...) a##value APPEND_CHAIN_1(value, __VA_ARGS__)
#define APPEND_CHAIN_3(value, a,...) a##value APPEND_CHAIN_2(value, __VA_ARGS__)
#define APPEND_CHAIN_4(value, a,...) a##value APPEND_CHAIN_3(value, __VA_ARGS__)
#define APPEND_CHAIN_5(value, a,...) a##value APPEND_CHAIN_4(value, __VA_ARGS__)
#define APPEND_CHAIN_6(value, a,...) a##value APPEND_CHAIN_5(value, __VA_ARGS__)
#define APPEND_CHAIN_7(value, a,...) a##value APPEND_CHAIN_6(value, __VA_ARGS__)
#define APPEND_CHAIN_8(value, a,...) a##value APPEND_CHAIN_7(value, __VA_ARGS__)
#define APPEND_CHAIN_9(value, a,...) a##value APPEND_CHAIN_8(value, __VA_ARGS__)

// chained caller
#define PFRONT_CHAIN(word, ...) GLUE(PFRONT_CHAIN_,COUNT(__VA_ARGS__))(word, __VA_ARGS__)
#define PFRONT_CHAIN_1(word, a) word a;
#define PFRONT_CHAIN_2(word, a,...) word a; PFRONT_CHAIN_1(word, __VA_ARGS__)
#define PFRONT_CHAIN_3(word, a,...) word a; PFRONT_CHAIN_2(word, __VA_ARGS__)
#define PFRONT_CHAIN_4(word, a,...) word a; PFRONT_CHAIN_3(word, __VA_ARGS__)
#define PFRONT_CHAIN_5(word, a,...) word a; PFRONT_CHAIN_4(word, __VA_ARGS__)
#define PFRONT_CHAIN_6(word, a,...) word a; PFRONT_CHAIN_5(word, __VA_ARGS__)
#define PFRONT_CHAIN_7(word, a,...) word a; PFRONT_CHAIN_6(word, __VA_ARGS__)
#define PFRONT_CHAIN_8(word, a,...) word a; PFRONT_CHAIN_7(word, __VA_ARGS__)
#define PFRONT_CHAIN_9(word, a,...) word a; PFRONT_CHAIN_8(word, __VA_ARGS__)

// chained caller
#define STRUCT_CHAIN(type, ...) GLUE(STRUCT_CHAIN_,COUNT(__VA_ARGS__))(type, __VA_ARGS__)
// chain
#define STRUCT_CHAIN_1(type, a) type a;
#define STRUCT_CHAIN_2(type, a,...) type a; STRUCT_CHAIN_1(type, __VA_ARGS__)
#define STRUCT_CHAIN_3(type, a,...) type a; STRUCT_CHAIN_2(type, __VA_ARGS__)
#define STRUCT_CHAIN_4(type, a,...) type a; STRUCT_CHAIN_3(type, __VA_ARGS__)
#define STRUCT_CHAIN_5(type, a,...) type a; STRUCT_CHAIN_4(type, __VA_ARGS__)
#define STRUCT_CHAIN_6(type, a,...) type a; STRUCT_CHAIN_5(type, __VA_ARGS__)
#define STRUCT_CHAIN_7(type, a,...) type a; STRUCT_CHAIN_6(type, __VA_ARGS__)
#define STRUCT_CHAIN_8(type, a,...) type a; STRUCT_CHAIN_7(type, __VA_ARGS__)
#define STRUCT_CHAIN_9(type, a,...) type a; STRUCT_CHAIN_8(type, __VA_ARGS__)

// add comma and type to all parameters

#define COMMA_CHAIN(type, ...) GLUE(COMMA_CHAIN_,COUNT(__VA_ARGS__))(type, __VA_ARGS__)
// chain
#define COMMA_CHAIN_1(type, a) type a,
#define COMMA_CHAIN_2(type, a,...) type a, COMMA_CHAIN_1(type, __VA_ARGS__)
#define COMMA_CHAIN_3(type, a,...) type a, COMMA_CHAIN_2(type, __VA_ARGS__)
#define COMMA_CHAIN_4(type, a,...) type a, COMMA_CHAIN_3(type, __VA_ARGS__)
#define COMMA_CHAIN_5(type, a,...) type a, COMMA_CHAIN_4(type, __VA_ARGS__)
#define COMMA_CHAIN_6(type, a,...) type a, COMMA_CHAIN_5(type, __VA_ARGS__)
#define COMMA_CHAIN_7(type, a,...) type a, COMMA_CHAIN_6(type, __VA_ARGS__)
#define COMMA_CHAIN_8(type, a,...) type a, COMMA_CHAIN_7(type, __VA_ARGS__)
#define COMMA_CHAIN_9(type, a,...) type a, COMMA_CHAIN_8(type, __VA_ARGS__)

#define PARAM_SAMPLER_CHAIN(type0, type1, ...) GLUE(PARAM_SAMPLER_CHAIN_,COUNT(__VA_ARGS__))(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_1(type0, type1, a) type0 a, type1 gl_image_##a,
#define PARAM_SAMPLER_CHAIN_2(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_1(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_3(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_2(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_4(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_3(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_5(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_4(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_6(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_5(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_7(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_6(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_8(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_7(type0, type1, __VA_ARGS__)
#define PARAM_SAMPLER_CHAIN_9(type0, type1, a,...) type0 a, type1 gl_image_##a, PARAM_SAMPLER_CHAIN_8(type0, type1, __VA_ARGS__)

#define EQ_STRUCT_CHAIN(name, ...) GLUE(EQ_STRUCT_CHAIN_,COUNT(__VA_ARGS__))(name, __VA_ARGS__)
// chain
#define EQ_STRUCT_CHAIN_1(name, a) name.a=a;
#define EQ_STRUCT_CHAIN_2(name, a,...) name.a=a; EQ_STRUCT_CHAIN_1(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_3(name, a,...) name.a=a; EQ_STRUCT_CHAIN_2(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_4(name, a,...) name.a=a; EQ_STRUCT_CHAIN_3(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_5(name, a,...) name.a=a; EQ_STRUCT_CHAIN_4(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_6(name, a,...) name.a=a; EQ_STRUCT_CHAIN_5(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_7(name, a,...) name.a=a; EQ_STRUCT_CHAIN_6(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_8(name, a,...) name.a=a; EQ_STRUCT_CHAIN_7(name, __VA_ARGS__)
#define EQ_STRUCT_CHAIN_9(name, a,...) name.a=a; EQ_STRUCT_CHAIN_8(name, __VA_ARGS__)

/**
 * Kernel setters
 */

#define SET_ATTRIBUTE(attr) \
{ \
    gl_set_vertex_attribute(&attr, _##attr, vertex_attributes, vertex_attribute_datas, gl_attribute_location); \
    gl_attribute_location += 1; \
}

#define SET_ATTRIBUTE_CHAIN(...) GLUE(SET_ATTRIBUTE_CHAIN_,COUNT(__VA_ARGS__))(__VA_ARGS__)

#define SET_ATTRIBUTE_CHAIN_1(a)     SET_ATTRIBUTE(a);
#define SET_ATTRIBUTE_CHAIN_2(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_1(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_3(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_2(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_4(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_3(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_5(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_4(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_6(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_5(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_7(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_6(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_8(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_7(__VA_ARGS__)
#define SET_ATTRIBUTE_CHAIN_9(a,...) SET_ATTRIBUTE(a); SET_ATTRIBUTE_CHAIN_8(__VA_ARGS__)


/**
 * Vertex buffer getters
 */
#define SET_VARYING_F1_CHAIN(...) GLUE(SET_VARYING_F1_CHAIN_,COUNT(__VA_ARGS__))(__VA_ARGS__)

#define SET_VARYING_F1_CHAIN_1(a)     a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x;
#define SET_VARYING_F1_CHAIN_2(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_1(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_3(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_2(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_4(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_3(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_5(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_4(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_6(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_5(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_7(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_6(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_8(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_7(__VA_ARGS__)
#define SET_VARYING_F1_CHAIN_9(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).x; SET_VARYING_F1_CHAIN_8(__VA_ARGS__)

#define SET_VARYING_F2_CHAIN(...) GLUE(SET_VARYING_F2_CHAIN_,COUNT(__VA_ARGS__))(__VA_ARGS__)

#define SET_VARYING_F2_CHAIN_1(a)     a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy;
#define SET_VARYING_F2_CHAIN_2(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_1(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_3(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_2(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_4(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_3(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_5(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_4(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_6(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_5(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_7(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_6(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_8(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_7(__VA_ARGS__)
#define SET_VARYING_F2_CHAIN_9(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xy; SET_VARYING_F2_CHAIN_8(__VA_ARGS__)

#define SET_VARYING_F3_CHAIN(...) GLUE(SET_VARYING_F3_CHAIN_,COUNT(__VA_ARGS__))(__VA_ARGS__)

#define SET_VARYING_F3_CHAIN_1(a)     a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz;
#define SET_VARYING_F3_CHAIN_2(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_1(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_3(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_2(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_4(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_3(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_5(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_4(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_6(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_5(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_7(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_6(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_8(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_7(__VA_ARGS__)
#define SET_VARYING_F3_CHAIN_9(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer).xyz; SET_VARYING_F3_CHAIN_8(__VA_ARGS__)

#define SET_VARYING_CHAIN(...) GLUE(SET_VARYING_CHAIN_,COUNT(__VA_ARGS__))(__VA_ARGS__)

#define SET_VARYING_CHAIN_1(a)     a = interpolate_varying(varying, vert_idx, bary, vertex_buffer);
#define SET_VARYING_CHAIN_2(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_1(__VA_ARGS__)
#define SET_VARYING_CHAIN_3(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_2(__VA_ARGS__)
#define SET_VARYING_CHAIN_4(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_3(__VA_ARGS__)
#define SET_VARYING_CHAIN_5(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_4(__VA_ARGS__)
#define SET_VARYING_CHAIN_6(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_5(__VA_ARGS__)
#define SET_VARYING_CHAIN_7(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_6(__VA_ARGS__)
#define SET_VARYING_CHAIN_8(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_7(__VA_ARGS__)
#define SET_VARYING_CHAIN_9(a,...) a = interpolate_varying(varying, vert_idx, bary, vertex_buffer); SET_VARYING_CHAIN_8(__VA_ARGS__)
/**
 * Struct setters 
 */

// To name.a = (float4){a,value}; 
#define SET_STRUCT_CHAIN(name, ...) GLUE(SET_STRUCT_CHAIN_,COUNT(__VA_ARGS__))(name, __VA_ARGS__)

#define SET_STRUCT_CHAIN_1(name, a)     name.a=a;
#define SET_STRUCT_CHAIN_2(name, a,...) name.a=a; SET_STRUCT_CHAIN_1(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_3(name, a,...) name.a=a; SET_STRUCT_CHAIN_2(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_4(name, a,...) name.a=a; SET_STRUCT_CHAIN_3(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_5(name, a,...) name.a=a; SET_STRUCT_CHAIN_4(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_6(name, a,...) name.a=a; SET_STRUCT_CHAIN_5(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_7(name, a,...) name.a=a; SET_STRUCT_CHAIN_6(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_8(name, a,...) name.a=a; SET_STRUCT_CHAIN_7(name, __VA_ARGS__)
#define SET_STRUCT_CHAIN_9(name, a,...) name.a=a; SET_STRUCT_CHAIN_8(name, __VA_ARGS__)

// To name.a = (float4){a,value0, value1, value2}; 
#define SET_STRUCT_F4_F1_CHAIN(name, value0, value1, value2, ...) GLUE(SET_STRUCT_F4_F1_CHAIN_,COUNT(__VA_ARGS__))(name, value0, value1, value2, __VA_ARGS__)

#define SET_STRUCT_F4_F1_CHAIN_1(name, value0, value1, value2, a)     name.a= (float4){a,value0,value1, value2};
#define SET_STRUCT_F4_F1_CHAIN_2(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_1(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_3(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_2(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_4(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_3(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_5(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_4(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_6(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_5(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_7(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_6(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_8(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_7(name, value0, value1, value2, __VA_ARGS__)
#define SET_STRUCT_F4_F1_CHAIN_9(name, value0, value1, value2, a,...) name.a= (float4){a,value0,value1, value2}; SET_STRUCT_F4_F1_CHAIN_8(name, value0, value1, value2, __VA_ARGS__)

// To name.a = (float4){a,value0, value1}; 
#define SET_STRUCT_F4_F2_CHAIN(name, value0, value1, ...) GLUE(SET_STRUCT_F4_F2_CHAIN_,COUNT(__VA_ARGS__))(name, value0, value1, __VA_ARGS__)

#define SET_STRUCT_F4_F2_CHAIN_1(name, value0, value1, a)     name.a= (float4){a,value0,value1};
#define SET_STRUCT_F4_F2_CHAIN_2(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_1(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_3(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_2(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_4(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_3(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_5(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_4(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_6(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_5(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_7(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_6(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_8(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_7(name, value0, value1, __VA_ARGS__)
#define SET_STRUCT_F4_F2_CHAIN_9(name, value0, value1, a,...) name.a= (float4){a,value0,value1}; SET_STRUCT_F4_F2_CHAIN_8(name, value0, value1, __VA_ARGS__)

// To name.a = (float4){a,value}; 
#define SET_STRUCT_F4_F3_CHAIN(name, value, ...) GLUE(SET_STRUCT_F4_F3_CHAIN_,COUNT(__VA_ARGS__))(name, value, __VA_ARGS__)

#define SET_STRUCT_F4_F3_CHAIN_1(name, value, a)     name.a= (float4){a,value};
#define SET_STRUCT_F4_F3_CHAIN_2(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_1(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_3(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_2(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_4(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_3(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_5(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_4(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_6(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_5(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_7(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_6(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_8(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_7(name, value, __VA_ARGS__)
#define SET_STRUCT_F4_F3_CHAIN_9(name, value, a,...) name.a= (float4){a,value}; SET_STRUCT_F4_F3_CHAIN_8(name, value, __VA_ARGS__)




#endif

