project('KNLMeansCL',
        'cpp',
        default_options : ['cpp_std=gnu++11', 'buildtype=release'],
        license : 'GPL-3',
        version : '1.1.1')

# Dependencies
vapoursynth = dependency('vapoursynth')
opencl = meson.get_compiler('cpp').find_library('OpenCL')

# Sources
src = [
    'KNLMeansCL/NLMAvisynth.h',
    'KNLMeansCL/NLMAvisynth.cpp',
    'KNLMeansCL/NLMDefault.h',
    'KNLMeansCL/NLMKernel.cpp',
    'KNLMeansCL/NLMKernel.h',
    'KNLMeansCL/NLMVapoursynth.cpp',
    'KNLMeansCL/NLMVapoursynth.h',
    'KNLMeansCL/shared/common.cpp',
    'KNLMeansCL/shared/common.h',
    'KNLMeansCL/shared/ocl_utils.cpp',
    'KNLMeansCL/shared/ocl_utils.h',
    'KNLMeansCL/shared/startchar.cpp',
    'KNLMeansCL/shared/startchar.h']

# Libs
library(
    'knlmeanscl',
    src,
    dependencies : [vapoursynth, opencl],
    install_dir : join_paths(get_option('prefix'), get_option('libdir'), 'vapoursynth'),
    install : true
)
