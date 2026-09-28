#pragma once

#include <windows.h>

#include <optional>

//  cqptur's own settings, stored per user as DWORD values under HKCU\Software\cqptur.
//  Only choices that differ from the defaults are stored, so a default setup leaves nothing
//  behind in the registry.

std::optional<DWORD> ReadSetting(const wchar_t* name);

void WriteSetting(const wchar_t* name, DWORD value);

//  Removes the value, and cqptur's key as well once nothing is left in it.
void DeleteSetting(const wchar_t* name);
