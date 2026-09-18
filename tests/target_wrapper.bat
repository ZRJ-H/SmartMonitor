@echo off
SetLocal EnableDelayedExpansion
(set PATH=D:\Develop\QT6\6.11.2\mingw_64\bin;!PATH!)
if defined QT_PLUGIN_PATH (
    set QT_PLUGIN_PATH=D:\Develop\QT6\6.11.2\mingw_64\plugins;!QT_PLUGIN_PATH!
) else (
    set QT_PLUGIN_PATH=D:\Develop\QT6\6.11.2\mingw_64\plugins
)
%*
EndLocal
