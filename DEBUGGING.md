# Debugging Guide for CeWinFileCacheFS

## Generating Visual Studio Solution

To generate a Visual Studio solution for debugging:

```batch
generate-vs-debug.bat
```

This will create a `build-vs` directory with a Visual Studio 2022 solution file.

## Opening in Visual Studio

1. Open `build-vs\CeWinFileCacheFS.sln` in Visual Studio
2. The solution contains several projects:
   - **CeWinFileCacheFS**: Main filesystem executable
   - **DirectoryCacheDebugTest**: Test DirectoryCache in isolation
   - **IntegrationDebugTest**: Test full integration

## Key Areas to Debug

### 1. Directory Enumeration Issue

The current issue is that `dir M:` hangs. Key breakpoints to set:

**In `src/hybrid_filesystem.cpp`:**
- `ReadDirectoryEntry()` (line ~579) - Entry point for directory enumeration
- `normalizePath()` (line ~1063) - Path normalization logic
- Line ~604: `directory_cache.getDirectoryContents(L"/")` - Check if this returns empty

**In `src/directory_cache.cpp`:**
- `getDirectoryContents()` (line ~23) - Returns directory contents
- `findNode()` (line ~28) - Finds nodes with path normalization
- `buildDirectoryTreeFromConfig()` (line ~34) - Initial tree building
- `enumerateNetworkDirectory()` (line ~56) - Network enumeration

**In `src/directory_tree.cpp`:**
- `getDirectoryContents()` (line ~59) - Core directory enumeration
- `findOrCreatePath()` (line ~183) - Path resolution logic

### 2. Path Normalization

Check that paths are being normalized correctly:
- Windows uses `\` (backslash)
- DirectoryCache stores with `/` (forward slash)
- Both should work after normalization

### 3. DirectoryCache Population

Verify that the DirectoryCache is being populated:
1. Set breakpoint in `buildDirectoryTreeFromConfig()`
2. Check if compiler directories are added: `/msvc-14.40`, `/ninja`, etc.
3. Verify `enumerateNetworkDirectory()` is finding files

## Debug Tools

### DirectoryCacheDebugTest

This standalone executable tests DirectoryCache:

```batch
cd build-vs\bin
DirectoryCacheDebugTest.exe
```

Expected output:
- Should show 3 compiler directories in root
- Path normalization tests should pass
- Directory enumeration should return entries

### IntegrationDebugTest

Tests the full integration without mounting:

```batch
cd build-vs\bin
IntegrationDebugTest.exe
```

## Common Issues

### 1. Empty Root Directory

If `getDirectoryContents("/")` returns empty:
- Check if `addDirectory()` is called for each compiler
- Verify root node has children
- Check path normalization in `findOrCreatePath()`

### 2. Network Enumeration Fails

If network shares aren't accessible:
- Check network paths in `compilers.json`
- Verify `FindFirstFileW` succeeds
- May need to add mock data for testing

### 3. Path Mismatch

If paths don't match:
- Check normalization in both DirectoryCache and HybridFileSystem
- Ensure consistent use of normalized paths
- Debug `findNode()` with various path formats

## Debugging Tips

1. **Enable All Debug Output**: The code has many `std::wcout` statements
2. **Use Immediate Window**: Test path normalization directly
3. **Watch Variables**: Monitor `directory_tree` contents
4. **Step Through**: Follow the path from `ReadDirectoryEntry` to `getDirectoryContents`

## Key Variables to Watch

- `directory_cache.directory_tree.root->children` - Should contain compiler directories
- `normalized_path` in various functions - Check normalization results
- `contents.size()` in `getDirectoryContents` - Should be > 0 for root
- `file_desc->entry->virtual_path` - Current path being enumerated

## Testing Without Network

To test without network access, uncomment the mock data section in `enumerateNetworkDirectory()` or use the `addTestFile()` methods in the debug test.