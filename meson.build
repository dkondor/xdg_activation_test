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

sources = ['xdgtest.c']

xdgtest = executable('xdgtest', sources, dependencies: [gtk, gdk])

