
Debian
====================
This directory contains files used to package subid/subi-qt
for Debian-based Linux systems. If you compile subid/subi-qt yourself, there are some useful files here.

## subi: URI support ##


subi-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install subi-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your subi-qt binary to `/usr/bin`
and the `../../share/pixmaps/subi128.png` to `/usr/share/pixmaps`

subi-qt.protocol (KDE)

