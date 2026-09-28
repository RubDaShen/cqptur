#include "settings.h"

#include "error.h"

namespace {

constexpr wchar_t kSettingsKey[] = L"Software\\cqptur";

} // namespace

std::optional<DWORD> ReadSetting(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kSettingsKey,
        name,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size
    );

    if (status != ERROR_SUCCESS) {
        return std::nullopt;
    }

    return value;
}

void WriteSetting(const wchar_t* name, DWORD value) {
    //  Creates the key the first time.
    const LSTATUS status = RegSetKeyValueW(
        HKEY_CURRENT_USER,
        kSettingsKey,
        name,
        REG_DWORD,
        &value,
        sizeof(value)
    );

    if (status != ERROR_SUCCESS) {
        ThrowWin32Error("Saving the setting", static_cast<DWORD>(status));
    }
}

void DeleteSetting(const wchar_t* name) {
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kSettingsKey, name);

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return; // no key, nothing to tidy up
    }

    DWORD subkeys = 0;
    DWORD values = 0;
    const LSTATUS status = RegQueryInfoKeyW(
        key,
        nullptr,
        nullptr,
        nullptr,
        &subkeys,
        nullptr,
        nullptr,
        &values,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    RegCloseKey(key);

    if ((status == ERROR_SUCCESS) && (subkeys == 0) && (values == 0)) {
        RegDeleteKeyW(HKEY_CURRENT_USER, kSettingsKey);
    }
}
