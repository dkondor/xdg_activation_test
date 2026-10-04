project(
	'xdgtest',
	'c',
	version: '0.0.1',
	license: 'MIT',
	meson_version: '>=0.50.0',
	default_options: [
		'warning_level=2',
		'werror=false',
	],
)


gtk = dependency('gtk+-3.0')
gdk = dependency('gdk-3.0')
layer_shell = dependency('gtk-layer-shell-0')

xdgtest = executable('xdgtest', ['xdgtest.c'], dependencies: [gtk, gdk, layer_shell])


gtk4 = dependency('gtk4')
layer_shell4 = dependency('gtk4-layer-shell-0')

xdgtest = executable('xdgtest4', ['xdgtest4.c'], dependencies: [gtk4, layer_shell4])
