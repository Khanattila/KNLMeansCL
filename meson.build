project('KNLMeansCL',
        'cpp',
        default_options : ['cpp_std=gnu++11', 'buildtype=release'],
        license : 'GPL-3',
        version : '1.1.1')

vapoursynth = dependency('vapoursynth')
deps = [vapoursynth]

opencl_dep = dependency('OpenCL', required : false)
if not opencl_dep.found()
    opencl_dep = meson.get_compiler('cpp').find_library('OpenCL')
endif
boost_dep = dependency('boost', modules : ['filesystem', 'system'])
deps += [opencl_dep, boost_dep]

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

library(
    'knlmeanscl',
    src,
    dependencies : deps,
    install_dir : join_paths(get_option('prefix'), get_option('libdir'), 'vapoursynth'),
    install : true)
