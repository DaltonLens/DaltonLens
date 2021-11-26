//
// Copyright (c) 2021, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the MIT license.  See the LICENSE file for details.
//

#pragma once

#include <string>

/*!
    Global access to user preferences.
*/
class CppUserPrefs
{
public:
    CppUserPrefs (const std::string& appName);
    ~CppUserPrefs ();

public:
    void sync ();

    void setBool (const std::string& key, bool value);
    void setInt (const std::string& key, int value);
    void setDouble (const std::string& key, double value);
    void setString (const std::string& key, const std::string& value);

    bool getBool (const std::string& key, bool defaultValue = false) const;
    int getInt (const std::string& key, int defaultValue = 0) const;
    double getDouble (const std::string& key, double defaultValue = 0.0) const;
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

private:
    struct Impl;
    Impl* impl = nullptr;
};

/************************************************
* Implementation
************************************************/

#include "tortellini.hh"

#include <cassert>
#include <fstream>

#if __APPLE__
# include <TargetConditionals.h>
#endif

#ifdef __linux__
# define CPPUSERPREFS_LINUX 1
#endif

#if TARGET_OS_OSX
# define CPPUSERPREFS_MACOS 1
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
# define CPPUSERPREFS_WINDOWS 1
#endif

#if CPPUSERPREFS_MACOS || CPPUSERPREFS_LINUX
# define CPPUSERPREFS_UNIX 1
#endif

#if CPPUSERPREFS_WINDOWS
# include <windows.h>
# include <shlobj.h>
#endif

#if CPPUSERPREFS_MACOS
#import <Foundation/Foundation.h>
#endif

// Not needed on macOS, and std::filesystem requires 10.15
#if !CPPUSERPREFS_MACOS
#include <filesystem>
namespace fs = std::filesystem;

fs::path getAppDataFolder()
{
#if CPPUSERPREFS_WINDOWS
    PWSTR path_tmp;

    // https://stackoverflow.com/questions/5920853/how-to-open-a-folder-in-appdata-with-c
    auto get_folder_path_ret = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path_tmp);

    /* Error check */
    if (get_folder_path_ret != S_OK) {
        CoTaskMemFree(path_tmp);
        return "";
    }

    /* Convert the Windows path type to a C++ path */
    fs::path path = path_tmp;

    /* Free memory :) */
    CoTaskMemFree(path_tmp);

    return path;

#else // CPPUSERPREFS_LINUX / other UNIX
    auto path = getenv( "XDG_CONFIG_HOME" );
    if (path && *path)
    {
        return fs::path(path);
    }
    else
    {
        path = getenv( "HOME" );
        assert(path && *path);
        return fs::path(path) / ".config";
    }
#endif
}

fs::path getCreatedAppDataFolderForName(const std::string& name)
{
    fs::path appData = getAppDataFolder();
    assert (!appData.empty());
    auto specificFolder = appData / name;
    std::filesystem::create_directories(specificFolder);
    return specificFolder;
}
#endif // !CPPUSERPREFS_MACOS

struct CppUserPrefs::Impl
{
#if CPPUSERPREFS_MACOS
    NSUserDefaults* userDefaults = nil;
#else
    tortellini::ini ini;
    fs::path iniPath;

    template <typename T>
    void setAny(const std::string& key, T value)
    {
        ini["UserPreferences"][key] = value;
    }

    template <typename T>
    T getAny(const std::string& key, T defaultValue)
    {
        return ini["UserPreferences"][key] | defaultValue;
    }
#endif
};

CppUserPrefs ::CppUserPrefs (const std::string& appName)
: impl (new Impl())
{
# if CPPUSERPREFS_MACOS
    impl->userDefaults = [NSUserDefaults standardUserDefaults];
#else
    impl->iniPath = getCreatedAppDataFolderForName (appName) / "preferences.ini";
    if (!fs::exists(impl->iniPath))
    {
        // Create an empty file.
        tortellini::ini ini;
        std::ofstream(impl->iniPath) << ini;
    }
    
    std::ifstream iniFile (impl->iniPath);
    iniFile >> impl->ini;
#endif
}

CppUserPrefs::~CppUserPrefs ()
{
    sync ();
    delete impl;
    impl = nullptr;
}

# if CPPUSERPREFS_MACOS

inline NSString* nsStr(const std::string& s)
{
    return [NSString stringWithUTF8String:s.c_str()];
}

// do nothing, sync is unnecessary on macOS
void CppUserPrefs::sync() {}

void CppUserPrefs::setBool(const std::string& key, bool value)
{
    [impl->userDefaults setBool:value forKey:nsStr(key)];
}

void CppUserPrefs::setInt(const std::string& key, int value)
{
    [impl->userDefaults setInteger:value forKey:nsStr(key)];
}

void CppUserPrefs::setDouble(const std::string& key, double value)
{
    [impl->userDefaults setDouble:value forKey:nsStr(key)];
}

void CppUserPrefs::setString(const std::string& key, const std::string& value)
{
    [impl->userDefaults setObject:[NSString stringWithUTF8String:value.c_str()]
                           forKey:nsStr(key)];
}

bool CppUserPrefs::getBool(const std::string& key, bool defaultValue) const
{
    NSObject* obj = [impl->userDefaults objectForKey:nsStr(key)];
    return obj == nil ? defaultValue : [(NSNumber*)(obj) boolValue];
}

int CppUserPrefs::getInt(const std::string& key, int defaultValue) const
{
    NSObject* obj = [impl->userDefaults objectForKey:nsStr(key)];
    return obj == nil ? defaultValue : [(NSNumber*)(obj) intValue];
}

double CppUserPrefs::getDouble(const std::string& key, double defaultValue) const
{
    NSObject* obj = [impl->userDefaults objectForKey:nsStr(key)];
    return obj == nil ? defaultValue : [(NSNumber*)(obj) doubleValue];
}

std::string CppUserPrefs::getString(const std::string& key, const std::string& defaultValue) const
{
    NSObject* obj = [impl->userDefaults objectForKey:nsStr(key)];
    return obj == nil ? defaultValue : [(NSString*)(obj) UTF8String];
}

#else

void CppUserPrefs::sync()
{
    std::ofstream(impl->iniPath) << impl->ini;
}

void CppUserPrefs::setBool(const std::string& key, bool value) { impl->setAny(key, value); }
void CppUserPrefs::setInt(const std::string& key, int value) { impl->setAny(key, value); }
void CppUserPrefs::setString(const std::string& key, const std::string& value) { impl->setAny(key, value); }
void CppUserPrefs::setDouble(const std::string& key, double value) { impl->setAny(key, value); }

bool CppUserPrefs::getBool(const std::string& key, bool defaultValue) const { return impl->getAny(key, defaultValue); }
int CppUserPrefs::getInt(const std::string& key, int defaultValue) const { return impl->getAny(key, defaultValue); }
double CppUserPrefs::getDouble(const std::string& key, double defaultValue) const { return impl->getAny(key, defaultValue); }

std::string CppUserPrefs::getString(const std::string& key, const std::string& defaultValue) const 
{ 
    return impl->getAny(key, std::string(defaultValue)).c_str(); 
}

#endif // not CPPUSERPREFS_MACOS
