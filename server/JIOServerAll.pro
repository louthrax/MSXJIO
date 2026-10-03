# Graphical server (JIOServer.pro) and command line server (JIOServerCLI.pro) in one project, to open in
# Qt Creator. The build scripts (0_build_*.sh) build the two projects separately.

TEMPLATE = subdirs

gui.file = JIOServer.pro
cli.file = JIOServerCLI.pro

SUBDIRS = gui cli
