project('Aegisub', ['c', 'cpp'],
        license: 'BSD-3-Clause',
        meson_version: '>=1.0.0',
        default_options: ['cpp_std=c++14', 'buildtype=debugoptimized'],
        version: '3.2.2')

cmake = import('cmake')

if host_machine.system() == 'windows'
    add_project_arguments('-DNOMINMAX', language: 'cpp')
    add_project_arguments('-DUNICODE', language: 'cpp')

    if not get_option('csri').disabled()
        add_global_arguments('-DCSRI_NO_EXPORT', language: 'c')
    endif

    sys_nasm = find_program('nasm', required: false)
    if not sys_nasm.found()
        nasm = subproject('nasm').get_variable('nasm')
        meson.override_find_program('nasm', nasm)
    endif
endif

if host_machine.system() == 'windows'
    version_sh = find_program('tools/version.ps1')
else
    version_sh = find_program('tools/version.sh')
endif
version_inc = include_directories('.')
version_h = custom_target('git_version.h',
                          command: [version_sh, meson.current_build_dir(), meson.current_source_dir()],
                          build_by_default: true,
                          build_always_stale: true, # has internal check whether target file will be refreshed
                          output: ['git_version.h'])

if host_machine.system() == 'darwin' and get_option('build_osx_bundle')
    prefix = meson.current_build_dir() / 'Aegisub.app' / 'Contents'
    bindir = prefix / 'MacOS'
    datadir = prefix / 'SharedSupport'
    localedir = prefix / 'Resources'
    dataroot = datadir
else
    prefix = get_option('prefix')
    bindir = prefix / get_option('bindir')
    datadir = prefix / get_option('datadir')
    localedir = prefix / get_option('localedir')
    dataroot = datadir / 'aegisub'
endif
docdir = prefix / 'doc'

# MSVC sets this automatically with -MDd, but it has a different meaning on other platforms
if get_option('debug') and host_machine.system() != 'windows'
    add_project_arguments('-D_DEBUG', language: 'cpp')
endif

if get_option('buildtype') == 'release'
    add_project_arguments('-DNDEBUG', language: 'cpp')
endif

conf = configuration_data()
conf.set_quoted('P_BIN', bindir)
conf.set_quoted('P_DATA', dataroot)
conf.set_quoted('P_LOCALE', localedir)
if get_option('credit') != ''
    conf.set_quoted('BUILD_CREDIT', get_option('credit'))
endif
if get_option('build_appimage')
    conf.set('APPIMAGE_BUILD', 1)
endif
conf.set('WITH_UPDATE_CHECKER', get_option('enable_update_checker'))

deps = []
deps_inc = []

if host_machine.system() == 'darwin'
    add_languages('objc', 'objcpp')
    add_project_arguments('-DGL_SILENCE_DEPRECATION', language: 'cpp')
    # meson neither supports objcpp_std nor inherits cpp_std https://github.com/mesonbuild/meson/issues/5495
    add_project_arguments('-std=c++11', language: 'objcpp')
elif host_machine.system() != 'windows'
    conf.set('WITH_FONTCONFIG', 1)
    deps += dependency('fontconfig')
endif

cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
deps += cc.find_library('m', required: false)
deps += cc.find_library('dl', required: false)

if meson.version().version_compare('>=0.60.0')
  iconv_dep = dependency('iconv', fallback: ['iconv', 'libiconv_dep'])
else
  iconv_dep = cc.find_library('iconv', required: false)
  if not (iconv_dep.found() or cc.has_function('iconv_open'))
      iconv_sp = subproject('iconv') # this really needs to be replaced with a proper port
      iconv_dep = iconv_sp.get_variable('libiconv_dep')
  endif
endif
deps += iconv_dep

deps += dependency('libass', version: '>=0.9.7',
                   fallback: ['libass', 'libass_dep'])

boost_modules = ['chrono', 'filesystem', 'thread', 'locale', 'regex']
if not get_option('local_boost')
    boost_dep = dependency('boost', version: '>=1.60.0',
                            modules: boost_modules + ['system'],
                            required: false,
                            static: get_option('default_library') == 'static')
endif

if get_option('local_boost') or not boost_dep.found()
    boost_dep = []
    boost = subproject('boost')
    foreach module: boost_modules
        boost_dep += boost.get_variable('boost_' + module + '_dep')
    endforeach
endif

deps += boost_dep
if host_machine.system() == 'windows'
    conf.set('BOOST_USE_WINDOWS_H', 1)
endif

deps += dependency('zlib')

wx_minver = '>=' + get_option('wx_version')
if host_machine.system() == 'darwin'
    wx_minver = '>=3.1.0'
endif
wx_dep = dependency('wxWidgets', version: wx_minver,
                    required: false,
                    modules: ['std', 'stc', 'gl'])

if wx_dep.found()
    deps += wx_dep
else
    build_shared = true
    if get_option('default_library') == 'static'
        build_shared = false
    endif
    build_type = 'Release'
    if get_option('buildtype') == 'debug'
        build_type = 'Debug'
    endif

    opt_var = cmake.subproject_options()
    opt_var.add_cmake_defines({
        'wxBUILD_INSTALL': false,
        'wxBUILD_PRECOMP': 'OFF',       # otherwise breaks project generation w/ meson
        'wxBUILD_SHARED': build_shared,

        'wxUSE_WEBVIEW': false,         # breaks build on linux
        'wxUSE_REGEX': 'builtin',
        'CMAKE_BUILD_TYPE': build_type,
        'wxUSE_IMAGE': true,
        'wxBUILD_MONOLITHIC': true      # otherwise breaks project generation w/ meson
    })

    if get_option('wx_version').version_compare('>=3.3.0')
        wx = cmake.subproject('wxWidgets-master', options: opt_var)
    else
        wx = cmake.subproject('wxWidgets', options: opt_var)
    endif

    deps += [
        wx.dependency('wxmono'),
        wx.dependency('wxregex'),
        wx.dependency('wxscintilla')
    ]

    if get_option('wx_version').version_compare('>=3.3.0')
        deps += [
            wx.dependency('wxlexilla')
        ]
    endif

    if host_machine.system() == 'windows' or host_machine.system() == 'darwin'
        deps += [
            wx.dependency('wxpng'),
        ]
    endif

    if host_machine.system() == 'windows'
        deps += [
            wx.dependency('wxzlib'),
            wx.dependency('wxexpat'),
        ]

        if cc.has_header('rpc.h')
            deps += cc.find_library('rpcrt4', required: true)
        else
            error('Missing Windows SDK RPC Library (rpc.h / rpcrt4.lib)')
        endif
        if cc.has_header('commctrl.h')
            deps += cc.find_library('comctl32', required: true)
        else
            error('Missing Windows SDK Common Controls Library (commctrl.h / comctl32.lib)')
        endif
    endif
endif

deps += dependency('icu-uc', version: '>=4.8.1.1')
deps += dependency('icu-i18n', version: '>=4.8.1.1')

dep_avail = []
foreach dep: [
    # audio, in order of precedence
    ['libpulse', [], 'PulseAudio', [], []],
    ['alsa', [], 'ALSA', [], []],
    ['portaudio-2.0', [], 'PortAudio', [], []],
    ['openal', '>=0.0.8', 'OpenAL', [], ['OpenAL']],
    # video
    ['ffms2', '>=2.22', 'FFMS2', ['ffms2', 'ffms2_dep'], []],
    # other
    ['fftw3', [], 'FFTW3', [], []],
    ['hunspell', [], 'Hunspell', ['hunspell', 'hunspell_dep'], []],
    ['uchardet', [], 'uchardet', ['uchardet', 'uchardet_dep'], []],
]
    optname = dep[0].split('-')[0]
    if not get_option(optname).disabled()
        # [provide] section is ignored if required is false;
        # must provided define fallback explicitly
        # (with meson 0.56 you can do allow_fallback: true):
        #d = dependency(dep[0], version: dep[1],
        #               required: false, allow_fallback: true)
        if dep[3].length() > 0
            d = dependency(dep[0], version: dep[1], fallback: dep[3])
        else
            d = dependency(dep[0], version: dep[1], required: false)
        endif

        if not d.found() and dep[4].length() > 0 and host_machine.system() == 'darwin'
            d = dependency('appleframeworks', modules: dep[4], required: false)
        endif

        if d.found()
            deps += d
            conf.set('WITH_@0@'.format(dep[0].split('-')[0].to_upper()), 1)
            dep_avail += dep[2]
        elif get_option(optname).enabled()
            error('@0@ enabled but not found'.format(dep[2]))
        endif
    endif
endforeach

needs_ffmpeg = false

if get_option('bestsource').enabled()
    conf.set('WITH_BESTSOURCE', 1)
    bs = subproject('bestsource')
    deps += bs.get_variable('bestsource_dep')
    dep_avail += 'BestSource'
    needs_ffmpeg = true
endif

if needs_ffmpeg
    conf.set('WITH_FFMPEG', 1)
    deps += [
      dependency('libavutil', default_options: ['tests=disabled']),
      dependency('libswscale', default_options: ['tests=disabled']),
    ]
endif

if get_option('avisynth').enabled()
    conf.set('WITH_AVISYNTH', 1) # bundled separately with installer
    dep_avail += 'AviSynth'

    avs_opt = cmake.subproject_options()
    avs_opt.add_cmake_defines({
        'HEADERS_ONLY': true
    })

    avs = cmake.subproject('avisynth', options: avs_opt)
    deps_inc += avs.include_directories('AviSynth-Headers')

    if host_machine.system() == 'windows'
      deps += cc.find_library('avifil32', required: true)
    endif
endif

if get_option('vapoursynth').enabled()
    conf.set('WITH_VAPOURSYNTH', 1)
    vs_sub = subproject('vapoursynth')
    deps_inc += vs_sub.get_variable('vs_inc')
    dep_avail += 'VapourSynth'
endif

if host_machine.system() == 'windows'
    if not get_option('directsound').disabled()
        dsound_dep = cc.find_library('dsound', required: get_option('directsound'))
        winmm_dep = cc.find_library('winmm', required: get_option('directsound'))
        ole32_dep = cc.find_library('ole32', required: get_option('directsound'))
        have_dsound_h = cc.has_header('dsound.h')
        if not have_dsound_h and get_option('directsound').enabled()
            error('DirectSound enabled but dsound.h not found')
        endif

        dxguid_dep = cc.find_library('dxguid', required: true)
        if dsound_dep.found() and winmm_dep.found() and ole32_dep.found() and dxguid_dep.found() and have_dsound_h
            deps += [dsound_dep, winmm_dep, ole32_dep, dxguid_dep]
            conf.set('WITH_DIRECTSOUND', 1)
            dep_avail += 'DirectSound'
        endif
    endif

    if not get_option('xaudio2').disabled()
        have_xaudio_h = cc.has_header('xaudio2.h')
        xaudio2_dep = cc.find_library('xaudio2', required: true)
        if have_xaudio_h and xaudio2_dep.found()
            deps += [xaudio2_dep]
            conf.set('WITH_XAUDIO2', 1)
            dep_avail += 'XAudio2'
            # XAudio2 needs Windows 8 or newer, so we tell meson not to define an older windows or else it can break things.
            add_project_arguments('-D_WIN32_WINNT=0x0602', language: 'cpp')
        else
            # Windows 8 not required if XAudio2 fails to be found. revert for compat.
            add_project_arguments('-D_WIN32_WINNT=0x0601', language: 'cpp')
        endif

        if not have_dsound_h and get_option('xaudio2').enabled()
            error('xaudio2 enabled but xaudio2.h not found')
        endif
    else
        # Windows 8 not required if XAudio2 is disabled. revert for compat.
        add_project_arguments('-D_WIN32_WINNT=0x0601', language: 'cpp')
    endif
endif

if host_machine.system() == 'windows' and cc.has_header('dwrite_3.h')
    add_project_arguments('-DHAVE_DWRITE_3', language: 'cpp')
endif

if host_machine.system() == 'darwin'
    frameworks_dep = dependency('appleframeworks', modules : ['CoreText', 'CoreFoundation', 'AppKit', 'Carbon', 'IOKit', 'QuartzCore'])
    deps += frameworks_dep
endif

# TODO: OSS

def_audio = get_option('default_audio_output')
if def_audio != 'auto'
    if not dep_avail.contains(def_audio)
        error('Default audio output "@0@" selected but not available'.format(def_audio))
    endif
elif dep_avail.length() != 0
    def_audio = dep_avail[0]
else
    def_audio = ''
endif

conf_platform = configuration_data()
conf_platform.set('DEFAULT_PLAYER_AUDIO', def_audio)

luajit = dependency('luajit', version: '>=2.0.0', required: get_option('system_luajit'))
if luajit.found() and luajit.type_name() != 'internal'
    luajit_test = cc.run('''#include <lauxlib.h>
int main(void)
{
    lua_State *L = luaL_newstate();
    if (!L) return 1;
    // This is valid in lua 5.2, but a syntax error in 5.1
    const char testprogram[] = "function foo() while true do break return end end";
    return luaL_loadstring(L, testprogram) == LUA_ERRSYNTAX;
}''', dependencies: luajit)

    if luajit_test.returncode() == 1
        if get_option('system_luajit')
            error('System luajit found but not compiled in 5.2 mode')
        else
            message('System luajit found but not compiled in 5.2 mode; using built-in luajit')
        endif
    else
        deps += luajit
    endif
else
    message('System luajit not found; using built-in luajit')
endif

if not deps.contains(luajit)
    luajit_sp = subproject('luajit')
    luajit_inc = luajit_sp.get_variable('src_inc')
    deps += luajit_sp.get_variable('luajit_dep')
else
    luajit_inc = include_directories(luajit.get_variable('includedir'))
endif
subdir('vendor/luabins/src')

dep_gl = dependency('gl', required: false)
if not dep_gl.found()
    if host_machine.system() == 'windows'
        dep_gl = cc.find_library('opengl32', required: false)
    else
        dep_gl = cc.find_library('GL', required: false)
    endif

    if not cc.has_header('GL/gl.h')
        dep_gl = dependency('', required: false)
    endif
endif
if host_machine.system() == 'darwin'
    conf.set('HAVE_OPENGL_GL_H', 1)
endif

if not dep_gl.found()
    error('OpenGL implementation not found')
endif

deps += dep_gl

if not get_option('csri').disabled() and host_machine.system() == 'windows'
    conf.set('WITH_CSRI', 1)

    csri_sp = subproject('csri')
    deps += csri_sp.get_variable('csri_dep')
endif

acconf = configure_file(output: 'acconf.h', configuration: conf)

subdir('automation')
subdir('libaegisub')
subdir('packages')
subdir('po')
subdir('src')

if not meson.is_cross_build()
    subdir('tests')
endif

aegisub_cpp_pch = ['src/include/agi_pre.h']
aegisub_c_pch = ['src/include/agi_pre_c.h']

link_args = []
link_depends = []
if host_machine.system() == 'windows'
    manifest_file = configure_file(copy: true, input: 'src/res/aegisub.exe.manifest', output: 'aegisub.exe.manifest')
    link_args += ['/MANIFEST:EMBED', '/MANIFESTINPUT:@0@'.format(manifest_file)]
    link_depends += manifest_file
endif

aegisub = executable('aegisub', aegisub_src, version_h, acconf, resrc,
                     link_with: [libresrc, libluabins, libaegisub],
                     link_args: link_args,
                     link_depends: link_depends,
                     include_directories: [libaegisub_inc, libresrc_inc, version_inc, deps_inc, include_directories('src')],
                     cpp_pch: aegisub_cpp_pch,
                     c_pch: aegisub_c_pch,
                     install: true,
                     install_dir: bindir,
                     dependencies: deps,
                     win_subsystem: 'windows')
