# xdg_activation_test
Simple test program for the xdg-activation Wayland protocol.

This works by creating a parent and a child process, creating tokens in the parent (using `GdkAppLaunchContext`), passing them to the child and using them to try to activate the child's open window. Options allow testing a simple case (creating only one token), a case with an "expired" token (creating two tokens and trying to use the first one), and also trying to activate the child without a valid token.

Requirements:
 - meson for building
 - GTK+ 3.24

Building and running:
```
meson setup -Dbuildtype=debug build
ninja -C build
build/xdgtest
```
