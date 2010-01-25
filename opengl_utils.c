/*
 *  Copyright (c) 2007 Even Rouault
 *  Copyright (c) 2010 Intel
 *  Modified by Ian Molton 2010 <ian.molton@collabora.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opengl_func.h"
#include "log.h"

/* Sizes of types in opengl_func.h */
int tab_args_type_length[] =
{
  0,
  sizeof(char),
  sizeof(unsigned char),
  sizeof(short),
  sizeof(unsigned short),
  sizeof(int),
  sizeof(unsigned int),
  sizeof(float),
  sizeof(double),
  1 * sizeof(char),
  2 * sizeof(char),
  3 * sizeof(char),
  4 * sizeof(char),
  128 * sizeof(char),
  1 * sizeof(short),
  2 * sizeof(short),
  3 * sizeof(short),
  4 * sizeof(short),
  1 * sizeof(int),
  2 * sizeof(int),
  3 * sizeof(int),
  4 * sizeof(int),
  1 * sizeof(float),
  2 * sizeof(float),
  3 * sizeof(float),
  4 * sizeof(float),
  16 * sizeof(float),
  1 * sizeof(double),
  2 * sizeof(double),
  3 * sizeof(double),
  4 * sizeof(double),
  16 * sizeof(double),
  sizeof(int),
  sizeof(float),
  4 * sizeof(char),
  4 * sizeof(int),
  4 * sizeof(float),
  4 * sizeof(double),
  128 * sizeof(char),
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,

  /* the following sizes are the size of 1 element of the array */
  sizeof(char),
  sizeof(short),
  sizeof(int),
  sizeof(float),
  sizeof(double),
  sizeof(char),
  sizeof(short),
  sizeof(int),
  sizeof(float),
  sizeof(double),
};

GLint __glTexParameter_size(FILE* err_file, GLenum pname)
{
    switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
    case GL_TEXTURE_PRIORITY:
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_COMPARE_FAIL_VALUE_ARB:
/*      case GL_SHADOW_AMBIENT_SGIX:*/
    case GL_TEXTURE_MIN_LOD:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
    case GL_TEXTURE_CLIPMAP_FRAME_SGIX:
    case GL_TEXTURE_LOD_BIAS_S_SGIX:
    case GL_TEXTURE_LOD_BIAS_T_SGIX:
    case GL_TEXTURE_LOD_BIAS_R_SGIX:
    case GL_GENERATE_MIPMAP:
/*      case GL_GENERATE_MIPMAP_SGIS:*/
    case GL_TEXTURE_COMPARE_SGIX:
    case GL_TEXTURE_COMPARE_OPERATOR_SGIX:
    case GL_TEXTURE_MAX_CLAMP_S_SGIX:
    case GL_TEXTURE_MAX_CLAMP_T_SGIX:
    case GL_TEXTURE_MAX_CLAMP_R_SGIX:
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
    case GL_TEXTURE_LOD_BIAS:
/*      case GL_TEXTURE_LOD_BIAS_EXT:*/
    case GL_DEPTH_TEXTURE_MODE:
/*      case GL_DEPTH_TEXTURE_MODE_ARB:*/
    case GL_TEXTURE_COMPARE_MODE:
/*      case GL_TEXTURE_COMPARE_MODE_ARB:*/
    case GL_TEXTURE_COMPARE_FUNC:
/*      case GL_TEXTURE_COMPARE_FUNC_ARB:*/
    case GL_TEXTURE_UNSIGNED_REMAP_MODE_NV:
        return 1;
    case GL_TEXTURE_CLIPMAP_CENTER_SGIX:
    case GL_TEXTURE_CLIPMAP_OFFSET_SGIX:
        return 2;
    case GL_TEXTURE_CLIPMAP_VIRTUAL_DEPTH_SGIX:
        return 3;
    case GL_TEXTURE_BORDER_COLOR:
    case GL_POST_TEXTURE_FILTER_BIAS_SGIX:
    case GL_POST_TEXTURE_FILTER_SCALE_SGIX:
        return 4;
    default:
        fprintf(err_file, "unhandled pname = %d\n", pname);
        return 0;
    }
}

int __glLight_size(FILE* err_file, GLenum pname)
{
  switch(pname)
  {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_POSITION:
      return 4;
      break;

    case GL_SPOT_DIRECTION:
      return 3;
      break;

    case GL_SPOT_EXPONENT:
    case GL_SPOT_CUTOFF:
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
      return 1;
      break;

    default:
      fprintf(err_file, "unhandled pname = %d\n", pname);
      return 0;
  }
}

int __glMaterial_size(FILE* err_file, GLenum pname)
{
  switch(pname)
  {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
      return 4;
      break;

    case GL_SHININESS:
      return 1;
      break;

    case GL_COLOR_INDEXES:
      return 3;
      break;

    default:
      fprintf(err_file, "unhandled pname = %d\n", pname);
      return 0;
  }
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

int compute_arg_length(int func_number, Signature *signature, int arg_i, long* args)
{
  int* args_type = signature->args_type;

  switch (func_number)
  {
    case glProgramNamedParameter4fNV_func:
    case glProgramNamedParameter4dNV_func:
    case glProgramNamedParameter4fvNV_func:
    case glProgramNamedParameter4dvNV_func:
    case glGetProgramNamedParameterfvNV_func:
    case glGetProgramNamedParameterdvNV_func:
      if (arg_i == 2)
        return 1 * args[arg_i-1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glProgramStringARB_func:
    case glLoadProgramNV_func:
    case glGenProgramsNV_func:
    case glDeleteProgramsNV_func:
    case glGenProgramsARB_func:
    case glDeleteProgramsARB_func:
    case glRequestResidentProgramsNV_func:
    case glDrawBuffers_func:
    case glDrawBuffersARB_func:
    case glDrawBuffersATI_func:
    case glDeleteBuffers_func:
    case glDeleteBuffersARB_func:
    case glDeleteTextures_func:
    case glDeleteTexturesEXT_func:
    case glGenFramebuffersEXT_func:
    case glDeleteFramebuffersEXT_func:
    case glGenRenderbuffersEXT_func:
    case glDeleteRenderbuffersEXT_func:
    case glGenQueries_func:
    case glGenQueriesARB_func:
    case glDeleteQueries_func:
    case glDeleteQueriesARB_func:
    case glGenOcclusionQueriesNV_func:
    case glDeleteOcclusionQueriesNV_func:
    case glGenFencesNV_func:
    case glDeleteFencesNV_func:
    case glUniform1fv_func:
    case glUniform1iv_func:
    case glUniform1fvARB_func:
    case glUniform1ivARB_func:
    case glUniform1uivEXT_func:
    case glVertexAttribs1dvNV_func:
    case glVertexAttribs1fvNV_func:
    case glVertexAttribs1svNV_func:
    case glVertexAttribs1hvNV_func:
    case glWeightbvARB_func:
    case glWeightsvARB_func:
    case glWeightivARB_func:
    case glWeightfvARB_func:
    case glWeightdvARB_func:
    case glWeightubvARB_func:
    case glWeightusvARB_func:
    case glWeightuivARB_func:
    case glPixelMapfv_func:
    case glPixelMapuiv_func:
    case glPixelMapusv_func:
    case glProgramBufferParametersfvNV_func:
    case glProgramBufferParametersIivNV_func:
    case glProgramBufferParametersIuivNV_func:
    case glTransformFeedbackAttribsNV_func:
    case glTransformFeedbackVaryingsNV_func:
      if (arg_i == signature->nb_args - 1)
        return 1 * args[arg_i-1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniform2fv_func:
    case glUniform2iv_func:
    case glUniform2fvARB_func:
    case glUniform2ivARB_func:
    case glUniform2uivEXT_func:
    case glVertexAttribs2dvNV_func:
    case glVertexAttribs2fvNV_func:
    case glVertexAttribs2svNV_func:
    case glVertexAttribs2hvNV_func:
    case glDetailTexFuncSGIS_func:
    case glSharpenTexFuncSGIS_func:
      if (arg_i == signature->nb_args - 1)
        return 2 * args[arg_i-1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniform3fv_func:
    case glUniform3iv_func:
    case glUniform3fvARB_func:
    case glUniform3ivARB_func:
    case glUniform3uivEXT_func:
    case glVertexAttribs3dvNV_func:
    case glVertexAttribs3fvNV_func:
    case glVertexAttribs3svNV_func:
    case glVertexAttribs3hvNV_func:
      if (arg_i == signature->nb_args - 1)
        return 3 * args[arg_i-1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniform4fv_func:
    case glUniform4iv_func:
    case glUniform4fvARB_func:
    case glUniform4ivARB_func:
    case glUniform4uivEXT_func:
    case glVertexAttribs4dvNV_func:
    case glVertexAttribs4fvNV_func:
    case glVertexAttribs4svNV_func:
    case glVertexAttribs4hvNV_func:
    case glVertexAttribs4ubvNV_func:
    case glProgramParameters4fvNV_func:
    case glProgramParameters4dvNV_func:
    case glProgramLocalParameters4fvEXT_func:
    case glProgramEnvParameters4fvEXT_func:
    case glProgramLocalParametersI4ivNV_func:
    case glProgramLocalParametersI4uivNV_func:
    case glProgramEnvParametersI4ivNV_func:
    case glProgramEnvParametersI4uivNV_func:
      if (arg_i == signature->nb_args - 1)
        return 4 * args[arg_i-1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glPrioritizeTextures_func:
    case glPrioritizeTexturesEXT_func:
    case glAreProgramsResidentNV_func:
    case glAreTexturesResident_func:
    case glAreTexturesResidentEXT_func:
      if (arg_i == 1 || arg_i == 2)
        return args[0] * tab_args_type_length[args_type[arg_i]];
      break;

    case glLightfv_func:
    case glLightiv_func:
    case glGetLightfv_func:
    case glGetLightiv_func:
    case glFragmentLightfvSGIX_func:
    case glFragmentLightivSGIX_func:
    case glGetFragmentLightfvSGIX_func:
    case glGetFragmentLightivSGIX_func:
      if (arg_i == signature->nb_args - 1)
        return __glLight_size(get_err_file(), args[arg_i-1]) * tab_args_type_length[args_type[arg_i]];
      break;

    case glLightModelfv_func:
    case glLightModeliv_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_LIGHT_MODEL_AMBIENT) ? 4 : 1) * tab_args_type_length[args_type[arg_i]];
      break;

    case glFragmentLightModelfvSGIX_func:
    case glFragmentLightModelivSGIX_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX) ? 4 : 1) * tab_args_type_length[args_type[arg_i]];
      break;

    case glMaterialfv_func:
    case glMaterialiv_func:
    case glGetMaterialfv_func:
    case glGetMaterialiv_func:
    case glFragmentMaterialfvSGIX_func:
    case glFragmentMaterialivSGIX_func:
    case glGetFragmentMaterialfvSGIX_func:
    case glGetFragmentMaterialivSGIX_func:
      if (arg_i == signature->nb_args - 1)
        return __glMaterial_size(get_err_file(), args[arg_i-1]) * tab_args_type_length[args_type[arg_i]];
      break;

    case glTexParameterfv_func:
    case glTexParameteriv_func:
    case glGetTexParameterfv_func:
    case glGetTexParameteriv_func:
    case glTexParameterIivEXT_func:
    case glTexParameterIuivEXT_func:
    case glGetTexParameterIivEXT_func:
    case glGetTexParameterIuivEXT_func:
      if (arg_i == signature->nb_args - 1)
        return __glTexParameter_size(get_err_file(), args[arg_i-1]) * tab_args_type_length[args_type[arg_i]];
      break;

    case glFogiv_func:
    case glFogfv_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_FOG_COLOR) ? 4 : 1) * tab_args_type_length[args_type[arg_i]];
      break;

    case glTexGendv_func:
    case glTexGenfv_func:
    case glTexGeniv_func:
    case glGetTexGendv_func:
    case glGetTexGenfv_func:
    case glGetTexGeniv_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_TEXTURE_GEN_MODE) ? 1 : 4) * tab_args_type_length[args_type[arg_i]];
      break;

    case glTexEnvfv_func:
    case glTexEnviv_func:
    case glGetTexEnvfv_func:
    case glGetTexEnviv_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_TEXTURE_ENV_MODE) ? 1 : 4) * tab_args_type_length[args_type[arg_i]];
      break;

    case glConvolutionParameterfv_func:
    case glConvolutionParameteriv_func:
    case glGetConvolutionParameterfv_func:
    case glGetConvolutionParameteriv_func:
    case glConvolutionParameterfvEXT_func:
    case glConvolutionParameterivEXT_func:
    case glGetConvolutionParameterfvEXT_func:
    case glGetConvolutionParameterivEXT_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_CONVOLUTION_BORDER_COLOR ||
                 args[arg_i-1] == GL_CONVOLUTION_FILTER_SCALE ||
                 args[arg_i-1] == GL_CONVOLUTION_FILTER_BIAS) ? 4 : 1) * tab_args_type_length[args_type[arg_i]];
      break;

    case glGetVertexAttribfvARB_func:
    case glGetVertexAttribfvNV_func:
    case glGetVertexAttribfv_func:
    case glGetVertexAttribdvARB_func:
    case glGetVertexAttribdvNV_func:
    case glGetVertexAttribdv_func:
    case glGetVertexAttribivARB_func:
    case glGetVertexAttribivNV_func:
    case glGetVertexAttribiv_func:
    case glGetVertexAttribIivEXT_func:
    case glGetVertexAttribIuivEXT_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_CURRENT_VERTEX_ATTRIB_ARB) ? 4 : 1) * tab_args_type_length[args_type[arg_i]];
      break;


    case glPointParameterfv_func:
    case glPointParameterfvEXT_func:
    case glPointParameterfvARB_func:
    case glPointParameterfvSGIS_func:
    case glPointParameteriv_func:
    case glPointParameterivEXT_func:
      if (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_POINT_DISTANCE_ATTENUATION) ? 3 : 1) * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniformMatrix2fv_func:
    case glUniformMatrix2fvARB_func:
      if (arg_i == signature->nb_args - 1)
        return 2 * 2 * args[1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniformMatrix3fv_func:
    case glUniformMatrix3fvARB_func:
      if (arg_i == signature->nb_args - 1)
        return 3 * 3 * args[1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniformMatrix4fv_func:
    case glUniformMatrix4fvARB_func:
      if (arg_i == signature->nb_args - 1)
        return 4 * 4 * args[1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniformMatrix2x3fv_func:
    case glUniformMatrix3x2fv_func:
      if (arg_i == signature->nb_args - 1)
        return 2 * 3 * args[1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniformMatrix2x4fv_func:
    case glUniformMatrix4x2fv_func:
      if (arg_i == signature->nb_args - 1)
        return 2 * 4 * args[1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glUniformMatrix3x4fv_func:
    case glUniformMatrix4x3fv_func:
      if (arg_i == signature->nb_args - 1)
        return 3 * 4 * args[1] * tab_args_type_length[args_type[arg_i]];
      break;

    case glSpriteParameterivSGIX_func:
    case glSpriteParameterfvSGIX_func:
      if  (arg_i == signature->nb_args - 1)
        return ((args[arg_i-1] == GL_SPRITE_MODE_SGIX) ? 1 : 3) * tab_args_type_length[args_type[arg_i]];
      break;

    default:
      break;
  }

  return 0;
}


