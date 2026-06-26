#include <windows.h>
#include <string>
#include <fstream>
#include "miniz.h"

// Helper to check if a file exists
bool FileExists(const std::string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Helper to recursively create directories
void CreateDirectoryRecursive(const std::string& dir) {
    std::string path = "";
    for (size_t i = 0; i < dir.length(); ++i) {
        char c = dir[i];
        if (c == '/' || c == '\\') {
            if (!path.empty() && path.back() != ':') {
                CreateDirectoryA(path.c_str(), NULL);
            }
        }
        path += c;
    }
    if (!path.empty() && path.back() != ':') {
        CreateDirectoryA(path.c_str(), NULL);
    }
}

// Helper to get directory of a file path
std::string GetDirectoryOfFile(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("\\/");
    if (lastSlash == std::string::npos) {
        return "";
    }
    return filePath.substr(0, lastSlash);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // 1. Locate the embedded ZIP resource (resource ID 101, custom type RCDATA)
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(101), (LPCSTR)RT_RCDATA);
    if (!hRes) {
        MessageBoxA(NULL, "Critical Error: Embedded archive package not found inside binary.", "Motherboard Checker Loader", MB_ICONERROR);
        return 1;
    }

    DWORD zipSize = SizeofResource(NULL, hRes);
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        MessageBoxA(NULL, "Critical Error: Failed to load embedded archive resource.", "Motherboard Checker Loader", MB_ICONERROR);
        return 1;
    }

    void* pZipData = LockResource(hData);
    if (!pZipData) {
        MessageBoxA(NULL, "Critical Error: Failed to lock embedded archive resource.", "Motherboard Checker Loader", MB_ICONERROR);
        return 1;
    }

    // 2. Prepare paths in user TEMP directory
    char tempPath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tempPath)) {
        MessageBoxA(NULL, "Critical Error: Failed to retrieve system temporary directory path.", "Motherboard Checker Loader", MB_ICONERROR);
        return 1;
    }

    std::string outDir = std::string(tempPath) + "Motherboard_Resource_Checker_App";
    std::string mainExePath = outDir + "\\mb_resource_checker.exe";

    // 3. Optimize startup: check if main executable is already extracted and present
    if (!FileExists(mainExePath)) {
        // Create destination directory
        CreateDirectoryRecursive(outDir);

        // 4. Initialize zip reader from memory
        mz_zip_archive zipArchive;
        memset(&zipArchive, 0, sizeof(zipArchive));
        if (!mz_zip_reader_init_mem(&zipArchive, pZipData, zipSize, 0)) {
            MessageBoxA(NULL, "Critical Error: Failed to initialize package decompression reader.", "Motherboard Checker Loader", MB_ICONERROR);
            return 1;
        }

        // Get total number of files
        mz_uint fileCount = mz_zip_reader_get_num_files(&zipArchive);
        for (mz_uint i = 0; i < fileCount; ++i) {
            mz_zip_archive_file_stat fileStat;
            if (!mz_zip_reader_file_stat(&zipArchive, i, &fileStat)) {
                continue;
            }

            // Normalize the path separators
            std::string relPath = fileStat.m_filename;
            for (size_t j = 0; j < relPath.length(); ++j) {
                if (relPath[j] == '/') {
                    relPath[j] = '\\';
                }
            }

            std::string targetPath = outDir + "\\" + relPath;

            if (fileStat.m_is_directory) {
                CreateDirectoryRecursive(targetPath);
            } else {
                // Ensure parent directory exists
                std::string fileDir = GetDirectoryOfFile(targetPath);
                if (!fileDir.empty()) {
                    CreateDirectoryRecursive(fileDir);
                }

                // Extract file
                if (!mz_zip_reader_extract_to_file(&zipArchive, i, targetPath.c_str(), 0)) {
                    std::string errMsg = "Critical Error: Failed to extract file '" + relPath + "' from archive package.";
                    MessageBoxA(NULL, errMsg.c_str(), "Motherboard Checker Loader", MB_ICONERROR);
                    mz_zip_reader_end(&zipArchive);
                    return 1;
                }
            }
        }
        mz_zip_reader_end(&zipArchive);
    }

    // 5. Run the main checker application
    STARTUPINFOA siExe = { sizeof(siExe) };
    PROCESS_INFORMATION piExe;
    
    char exeBuf[MAX_PATH];
    strcpy_s(exeBuf, mainExePath.c_str());

    if (CreateProcessA(NULL, exeBuf, NULL, NULL, TRUE, 0, NULL, outDir.c_str(), &siExe, &piExe)) {
        // Close handles immediately as we don't need to block or wait in this portable shell
        CloseHandle(piExe.hProcess);
        CloseHandle(piExe.hThread);
    } else {
        MessageBoxA(NULL, "Critical Error: Failed to execute main Motherboard Resource Checker binary.", "Motherboard Checker Loader", MB_ICONERROR);
        return 1;
    }

    return 0;
}

