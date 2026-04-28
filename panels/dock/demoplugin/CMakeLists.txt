# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dock-demoplugin SHARED
    demoplugin.cpp
    demoplugin.h
    directorymodel.cpp
    directorymodel.h
)

target_link_libraries(dock-demoplugin PRIVATE
    dde-shell-frame
)

ds_install_package(PACKAGE org.deepin.ds.dock.demoplugin TARGET dock-demoplugin)
ds_handle_package_translation(PACKAGE org.deepin.ds.dock.demoplugin)
