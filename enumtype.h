#ifndef __ENUMTYPE_H

enum
{
/* 0 */
  TYPE_NONE,
  TYPE_CHAR,
  TYPE_UNSIGNED_CHAR,
  TYPE_SHORT,
  TYPE_UNSIGNED_SHORT,
  TYPE_INT,
  TYPE_UNSIGNED_INT,
  TYPE_FLOAT,
  TYPE_DOUBLE,
  TYPE_1CHAR,
/* 10 */
  TYPE_2CHAR,
  TYPE_3CHAR,
  TYPE_4CHAR,
  TYPE_128UCHAR,
  TYPE_1SHORT,
  TYPE_2SHORT,
  TYPE_3SHORT,
  TYPE_4SHORT,
  TYPE_1INT,
  TYPE_2INT,
/* 20 */
  TYPE_3INT,
  TYPE_4INT,
  TYPE_1FLOAT,
  TYPE_2FLOAT,
  TYPE_3FLOAT,
  TYPE_4FLOAT,
  TYPE_16FLOAT,
  TYPE_1DOUBLE,
  TYPE_2DOUBLE,
  TYPE_3DOUBLE,
/* 30 */
  TYPE_4DOUBLE,
  TYPE_16DOUBLE,
  TYPE_OUT_1INT,
  TYPE_OUT_1FLOAT,
  TYPE_OUT_4CHAR,
  TYPE_OUT_4INT,
  TYPE_OUT_4FLOAT,
  TYPE_OUT_4DOUBLE,
  TYPE_OUT_128UCHAR,
  TYPE_CONST_CHAR,
/* 40 */
  TYPE_ARRAY_CHAR,
  TYPE_ARRAY_SHORT,
  TYPE_ARRAY_INT,
  TYPE_ARRAY_FLOAT,
  TYPE_ARRAY_DOUBLE,
  TYPE_IN_IGNORED_POINTER,
  TYPE_OUT_ARRAY_CHAR,
  TYPE_OUT_ARRAY_SHORT,
  TYPE_OUT_ARRAY_INT,
  TYPE_OUT_ARRAY_FLOAT,
/* 50 */
  TYPE_OUT_ARRAY_DOUBLE,
  TYPE_NULL_TERMINATED_STRING,

  TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
 /* .... */
  TYPE_LAST,
 /* .... */
  TYPE_1UCHAR = TYPE_CHAR,
  TYPE_1USHORT = TYPE_1SHORT,
  TYPE_1UINT = TYPE_1INT,
  TYPE_OUT_1UINT = TYPE_OUT_1INT,
  TYPE_OUT_4UCHAR = TYPE_OUT_4CHAR,
  TYPE_ARRAY_VOID = TYPE_ARRAY_CHAR,
  TYPE_ARRAY_SIGNED_CHAR = TYPE_ARRAY_CHAR,
  TYPE_ARRAY_UNSIGNED_CHAR = TYPE_ARRAY_CHAR,
  TYPE_ARRAY_UNSIGNED_SHORT = TYPE_ARRAY_SHORT,
  TYPE_ARRAY_UNSIGNED_INT = TYPE_ARRAY_INT,
  TYPE_OUT_ARRAY_VOID = TYPE_OUT_ARRAY_CHAR,
  TYPE_OUT_ARRAY_SIGNED_CHAR = TYPE_OUT_ARRAY_CHAR,
  TYPE_OUT_ARRAY_UNSIGNED_CHAR = TYPE_OUT_ARRAY_CHAR,
  TYPE_OUT_ARRAY_UNSIGNED_SHORT = TYPE_OUT_ARRAY_SHORT,
  TYPE_OUT_ARRAY_UNSIGNED_INT = TYPE_OUT_ARRAY_INT,
  TYPE_ARRAY_VOID_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_SIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_UNSIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_UNSIGNED_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_ARRAY_UNSIGNED_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_VOID_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_SIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_UNSIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_UNSIGNED_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_OUT_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
  TYPE_OUT_ARRAY_UNSIGNED_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS = TYPE_OUT_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
};

#define __ENUMTYPE_H
#endif // __ENUMTYPE_H
