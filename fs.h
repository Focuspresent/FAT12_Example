#pragma once

#include "fo.h"

void ls(const char *dir);

void cd(const char *dir);

void mkdir(const char *dir);

void touch(const char *dir);

void rm(const char *dir);

void cat(const char *dir);

void hexdump(const char *dir);

u32 fs_get_mul_next_offset();
