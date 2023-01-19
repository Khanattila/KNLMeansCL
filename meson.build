project('KNLMeansCL',
        'cpp',
        default_options : ['cpp_std=gnu++11', 'buildtype=release'],
        license : 'GPL-3',
        version : '1.1.1')

avisynth = dependency('avisynth', required : false)
avisynth_src = ['KNLMeansCL/NLMAvisynth.h',
                'KNLMeansCL/NLMAvisynth.cpp']

vapoursynth = dependency('vapoursynth', required : false)
vapoursynth_src = ['KNLMeansCL/NLMVapoursynth.cpp',
                   'KNLMeansCL/NLMVapoursynth.h',]
deps = []

opencl_dep = dependency('OpenCL', required : false)
if not opencl_dep.found()
    opencl_dep = meson.get_compiler('cpp').find_library('OpenCL')
endif
boost_dep = dependency('boost', modules : ['filesystem', 'system'])
deps += [opencl_dep, boost_dep]

src = [
    'KNLMeansCL/NLMDefault.h',
    'KNLMeansCL/NLMKernel.cpp',
    'KNLMeansCL/NLMKernel.h',
    'KNLMeansCL/shared/common.cpp',
    'KNLMeansCL/shared/common.h',
    'KNLMeansCL/shared/ocl_utils.cpp',
    'KNLMeansCL/shared/ocl_utils.h',
    'KNLMeansCL/shared/startchar.cpp',
    'KNLMeansCL/shared/startchar.h']

if avisynth.found()
    src += [avisynth_src]
endif

if vapoursynth.found()
    src += [vapoursynth_src]
 endif

library(
    'knlmeanscl',
    src,
    dependencies : deps,
    install_dir : join_paths(get_option('prefix'), get_option('libdir'), 'vapoursynth'),
    install : true)
