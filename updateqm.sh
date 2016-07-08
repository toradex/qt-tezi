#!/bin/sh

lupdate -no-obsolete tezi.pro || true
lrelease tezi.pro
