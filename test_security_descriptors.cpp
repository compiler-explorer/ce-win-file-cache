#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <iostream>
#include <vector>
#include <iomanip>

#pragma comment(lib, "advapi32.lib")

void DumpSecurityDescriptor(const std::wstring& path, const std::wstring& type)
{
    std::wcout << L"\n=== " << type << L": " << path << L" ===" << std::endl;
    
    PSECURITY_DESCRIPTOR pSD = nullptr;
    DWORD dwSize = 0;
    
    // Get the security descriptor
    DWORD result = GetFileSecurityW(path.c_str(), 
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        nullptr, 0, &dwSize);
    
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        std::wcout << L"GetFileSecurity failed: " << GetLastError() << std::endl;
        return;
    }
    
    std::vector<BYTE> buffer(dwSize);
    pSD = (PSECURITY_DESCRIPTOR)buffer.data();
    
    if (!GetFileSecurityW(path.c_str(), 
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        pSD, dwSize, &dwSize))
    {
        std::wcout << L"GetFileSecurity failed: " << GetLastError() << std::endl;
        return;
    }
    
    std::wcout << L"Security descriptor size: " << dwSize << L" bytes" << std::endl;
    
    // Check if it's valid
    if (!IsValidSecurityDescriptor(pSD))
    {
        std::wcout << L"Security descriptor is NOT valid!" << std::endl;
        return;
    }
    
    std::wcout << L"Security descriptor is valid" << std::endl;
    
    // Check if it's self-relative
    SECURITY_DESCRIPTOR_CONTROL control;
    DWORD revision;
    if (GetSecurityDescriptorControl(pSD, &control, &revision))
    {
        if (control & SE_SELF_RELATIVE)
        {
            std::wcout << L"Format: Self-relative" << std::endl;
        }
        else
        {
            std::wcout << L"Format: Absolute" << std::endl;
        }
        std::wcout << L"Control flags: 0x" << std::hex << control << std::dec << std::endl;
    }
    
    // Get string representation
    LPWSTR pStringSD = nullptr;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(pSD, SDDL_REVISION_1, 
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &pStringSD, nullptr))
    {
        std::wcout << L"SDDL: " << pStringSD << std::endl;
        LocalFree(pStringSD);
    }
    
    // Dump raw bytes (first 64 bytes or all if smaller)
    std::wcout << L"Raw bytes (hex): ";
    size_t dumpSize = min(dwSize, 64UL);
    for (size_t i = 0; i < dumpSize; i++)
    {
        if (i > 0 && i % 16 == 0) std::wcout << L"\n                 ";
        std::wcout << std::hex << std::setw(2) << std::setfill(L'0') << buffer[i] << L" ";
    }
    if (dwSize > 64) std::wcout << L"... (" << (dwSize - 64) << L" more bytes)";
    std::wcout << std::dec << std::endl;
    
    // Try to get detailed info
    BOOL bDaclPresent = FALSE, bDaclDefaulted = FALSE;
    PACL pDacl = nullptr;
    if (GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted))
    {
        std::wcout << L"DACL present: " << (bDaclPresent ? L"Yes" : L"No") << std::endl;
        if (bDaclPresent && pDacl)
        {
            ACL_SIZE_INFORMATION aclInfo;
            if (GetAclInformation(pDacl, &aclInfo, sizeof(aclInfo), AclSizeInformation))
            {
                std::wcout << L"DACL ACE count: " << aclInfo.AceCount << std::endl;
            }
        }
        else if (bDaclPresent && !pDacl)
        {
            std::wcout << L"DACL is NULL (allows all access)" << std::endl;
        }
    }
}

int main()
{
    std::wcout << L"Windows Security Descriptor Analyzer\n" << std::endl;
    
    // Test various Windows objects
    DumpSecurityDescriptor(L"C:", L"Drive");
    DumpSecurityDescriptor(L"C:\\", L"Drive Root Directory");
    DumpSecurityDescriptor(L"C:\\Windows", L"System Directory");
    DumpSecurityDescriptor(L"C:\\Users", L"Users Directory");
    DumpSecurityDescriptor(L"C:\\Windows\\System32", L"System32 Directory");
    DumpSecurityDescriptor(L"C:\\Windows\\notepad.exe", L"System Executable");
    DumpSecurityDescriptor(L"C:\\Windows\\System32\\kernel32.dll", L"System DLL");
    
    // Test temp file
    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) && 
        GetTempFileNameW(tempPath, L"TSD", 0, tempFile))
    {
        // Create a simple file
        HANDLE hFile = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            WriteFile(hFile, "test", 4, nullptr, nullptr);
            CloseHandle(hFile);
            DumpSecurityDescriptor(tempFile, L"Temp File");
            DeleteFileW(tempFile);
        }
    }
    
    // Also test a directory we create
    std::wstring testDir = std::wstring(tempPath) + L"TestSecDir";
    if (CreateDirectoryW(testDir.c_str(), nullptr))
    {
        DumpSecurityDescriptor(testDir, L"Created Directory");
        RemoveDirectoryW(testDir.c_str());
    }
    
    return 0;
}