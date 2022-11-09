find_program('linuxdeploy', required: get_option('build_appimage'))
find_program('appimagetool', required: get_option('build_appimage'))
if get_option('build_appimage')
  meson.add_install_script('build.sh', get_option('bindir'))
endif
