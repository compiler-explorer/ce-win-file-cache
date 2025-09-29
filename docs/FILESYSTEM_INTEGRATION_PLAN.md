# Filesystem Integration Plan: Full Network Share Access

## Current Problem
The filesystem currently only shows directories defined in `compilers.json` as cached entries. Users cannot access the full network share content - only the pre-configured cached paths are visible. This defeats the purpose of having a complete virtual filesystem.

## Goal
Integrate the DirectoryTree system into HybridFileSystem to provide **full network share access** while maintaining the caching benefits for frequently accessed files defined in compilers.json.

## Architecture Overview

### Current State
- `compilers.json` defines paths that are cached in-memory by default
- Only these cached paths are visible in the filesystem
- Network shares are not fully accessible

### Target State  
- `compilers.json` defines paths that are **prioritized for caching**
- **All network share content** is accessible through the virtual filesystem
- DirectoryTree provides the complete virtual file hierarchy
- HybridFileSystem routes requests through DirectoryTree for path resolution
- Caching system works transparently behind the scenes

## Implementation Plan

### Phase 1: DirectoryTree Integration in HybridFileSystem

#### 1.1 Add DirectoryTree to HybridFileSystem
```cpp
class HybridFileSystem {
private:
    DirectoryTree virtual_tree;  // Add this
    // existing members...
};
```

#### 1.2 Initialize DirectoryTree in Initialize()
- Create root nodes for each compiler network share
- Map compiler base paths to network roots
- Build initial tree structure from network discovery

#### 1.3 Update Path Resolution Methods
- Modify `getCacheEntry()` to first check DirectoryTree
- If path exists in tree but not in cache_entries, create dynamic cache entry
- Maintain backward compatibility with existing cache_entries

### Phase 2: Network Share Discovery and Tree Building

#### 2.1 Network Share Enumeration
- Implement network directory scanning for each compiler base path
- Use `FindFirstFile`/`FindNextFile` with UNC paths
- Handle network timeouts and errors gracefully
- Cache directory structure to avoid repeated network calls

#### 2.2 Dynamic Tree Population
- Populate DirectoryTree with discovered network content
- Update tree incrementally as directories are accessed
- Handle both files and subdirectories
- Maintain file metadata (size, timestamps, attributes)

#### 2.3 Lazy Loading Strategy
- Load directory contents on first access (like `ls` command)
- Cache directory listings for performance
- Implement TTL for directory cache to handle network changes

### Phase 3: Enhanced Path Resolution

#### 3.1 Unified Path Lookup
```cpp
CacheEntry* getCacheEntry(const std::wstring& virtual_path) {
    // 1. Check existing cache_entries first (fast path)
    auto it = cache_entries.find(virtual_path);
    if (it != cache_entries.end()) {
        return it->second.get();
    }
    
    // 2. Check DirectoryTree for path existence
    DirectoryNode* node = virtual_tree.findNode(virtual_path);
    if (!node) {
        return nullptr;  // Path doesn't exist
    }
    
    // 3. Create dynamic cache entry from DirectoryNode
    return createDynamicCacheEntry(node);
}
```

#### 3.2 Dynamic Cache Entry Creation
- Convert DirectoryNode to CacheEntry on demand
- Determine caching policy based on compilers.json configuration
- Default to ON_DEMAND caching for non-configured paths
- Preserve network_path mapping from DirectoryNode

### Phase 4: Directory Enumeration Enhancement

#### 4.1 Update ReadDirectoryEntry()
- Use DirectoryTree for all directory enumeration
- Remove hardcoded root directory handling
- Support nested directory browsing
- Handle both cached and non-cached content uniformly

#### 4.2 Implement Tree-Based Directory Listing
```cpp
NTSTATUS ReadDirectoryEntry(...) {
    // Get directory contents from DirectoryTree
    std::vector<DirectoryNode*> contents = 
        virtual_tree.getDirectoryContents(virtual_path);
    
    // Convert to filesystem directory entries
    // Handle both files and subdirectories
    // Maintain enumeration state properly
}
```

### Phase 5: Caching Policy Integration

#### 5.1 Policy-Driven Caching
- Files in compilers.json paths: Use configured caching policy
- Other files: Use default ON_DEMAND policy  
- Maintain memory cache for frequently accessed files
- Implement LRU eviction for non-prioritized files

#### 5.2 Cache Entry Management
- Distinguish between "prioritized" and "dynamic" cache entries
- Allow dynamic entries to be evicted under memory pressure
- Keep prioritized entries (from compilers.json) in memory longer

### Phase 6: Performance Optimizations

#### 6.1 Network Call Optimization
- Batch network operations where possible
- Use parallel directory scanning for multiple compiler paths
- Implement connection pooling for network shares
- Cache network metadata with TTL

#### 6.2 Memory Management
- Implement DirectoryTree node eviction under memory pressure
- Keep recently accessed paths in memory
- Lazy-load file metadata until actually needed

## Configuration Changes

### compilers.json Evolution
The configuration file role changes from "what's visible" to "what's prioritized":

```json
{
  "compilers": {
    "msvc-14.40": {
      "network_path": "\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40",
      "cache_policy": "PRELOAD",  // Prioritized for caching
      "virtual_path": "\\msvc-14.40"
    }
  }
}
```

### New Configuration Options
- `scan_network_on_startup`: Whether to scan network shares at startup
- `directory_cache_ttl`: How long to cache directory listings
- `max_dynamic_entries`: Limit for dynamic cache entries
- `network_timeout`: Timeout for network operations

## Implementation Order

1. **Add DirectoryTree member to HybridFileSystem**
2. **Update Initialize() to populate DirectoryTree from network shares**
3. **Modify getCacheEntry() to use DirectoryTree as fallback**
4. **Implement createDynamicCacheEntry() for on-demand entries**
5. **Update ReadDirectoryEntry() to use DirectoryTree**
6. **Add network share scanning functionality**
7. **Implement caching policy differentiation**
8. **Add performance optimizations and error handling**
9. **Update configuration handling**
10. **Add comprehensive testing**

## Testing Strategy

### Unit Tests
- DirectoryTree integration with HybridFileSystem
- Dynamic cache entry creation
- Path resolution with mixed cached/uncached content
- Network error handling

### Integration Tests  
- Full network share browsing
- File access for both cached and uncached files
- Directory enumeration at all levels
- Performance under load

### Manual Testing
- Browse complete network share in Windows Explorer
- Access files at various directory levels
- Verify caching behavior for prioritized vs. non-prioritized files
- Test network disconnection scenarios

## Success Criteria

1. **Complete Network Access**: Users can browse and access all files in the network shares, not just cached ones
2. **Transparent Caching**: Caching works behind the scenes without limiting visibility
3. **Performance**: Cached files perform better, but non-cached files are still accessible
4. **Backward Compatibility**: Existing compilers.json configurations continue to work
5. **Scalability**: System handles large network shares efficiently

## Risk Mitigation

- **Network Timeouts**: Implement proper timeout handling and fallback mechanisms
- **Memory Usage**: Implement bounds on DirectoryTree size and dynamic cache entries
- **Performance**: Avoid blocking operations on main filesystem threads
- **Error Handling**: Graceful degradation when network shares are unavailable

This plan transforms the filesystem from a "cache-only view" to a "full network share with intelligent caching" system.