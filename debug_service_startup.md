# Service Startup Debugging Guide

## Error Analysis: Status e06d7363

The error code `0xe06d7363` is a **C++ exception code** that typically indicates:

1. **File access permissions issues**
2. **Locked files or directories** 
3. **Corrupted system files**
4. **Insufficient privileges**
5. **Resource conflicts**

## Based on Your Log Output

Your logs show successful initialization up to this point:
- ✅ Configuration loaded (compilers.json)
- ✅ Filesystem initialization 
- ✅ Metrics server started on 127.0.0.1:8080
- ✅ Cache directory creation (H:\opt\CompilerCache)
- ❌ **Service fails to start** with status e06d7363

## Immediate Troubleshooting Steps

### 1. Run as Administrator
```cmd
# Open Command Prompt as Administrator, then run:
cd H:\opt\ce-win-file-cache-v0.0.1-windows-x64
CeWinFileCacheFS --config compilers.json --mount M: -d -l debug
```

### 2. Check Directory Permissions
```cmd
# Verify cache directory permissions
icacls "H:\opt\CompilerCache"

# Verify config file permissions  
icacls "compilers.json"

# Verify mount point availability
dir M:
```

### 3. Check for File Locks
```cmd
# Check if mount point is already in use
net use

# Check for existing M: drive mappings
subst

# List all mounted file systems
mountvol
```

### 4. Test with Different Mount Point
```cmd
# Try a different drive letter
CeWinFileCacheFS --config compilers.json --mount N: -d -l debug

# Or try a directory mount
CeWinFileCacheFS --config compilers.json --mount C:\MountTest -d -l debug
```

### 5. Check WinFsp Installation
```cmd
# Verify WinFsp is properly installed
sc query WinFsp.Launcher
sc query WinFsp

# Check WinFsp version
reg query "HKLM\Software\WinFsp" /v Version
```

### 6. Enhanced Logging
```cmd
# Enable maximum logging
CeWinFileCacheFS --config compilers.json --mount M: -d -1 -l trace -o both -f debug.log

# Check the debug.log file for more details
type debug.log
```

### 7. Test Mode First
```cmd
# Try test mode to isolate WinFsp issues
CeWinFileCacheFS --test --config compilers.json -l debug

# Test specific components
CeWinFileCacheFS --test-config --config compilers.json -l debug
```

## Possible Root Causes

### A. Antivirus/Security Software
- **MalwareBytes**: Known to cause 0xe06d7363 errors
- **Windows Defender**: May block WinFsp operations
- **Corporate antivirus**: May interfere with filesystem drivers

**Solution**: Temporarily disable antivirus or add exceptions for:
- CeWinFileCacheFS.exe
- H:\opt\CompilerCache directory
- WinFsp drivers

### B. Drive Letter Conflicts
- M: drive may already be in use
- Network mappings conflict
- Subst mappings conflict

**Solution**: Use `net use` and `subst` to check for conflicts

### C. Insufficient Privileges
- Service needs administrator privileges
- WinFsp driver access requires elevation
- Cache directory write permissions

**Solution**: Run as Administrator

### D. WinFsp Driver Issues
- WinFsp not properly installed
- Driver not running
- Version compatibility issues

**Solution**: Reinstall WinFsp

### E. File System Resources
- Out of drive letters
- Insufficient memory
- Too many open file handles

**Solution**: Check system resources

## Advanced Diagnostics

### 1. Windows Event Viewer
```cmd
# Check System logs for WinFsp events
eventvwr.msc
# Navigate to: Windows Logs > System
# Look for WinFsp-related errors around the failure time
```

### 2. Process Monitor
```cmd
# Download Process Monitor from Microsoft Sysinternals
# Filter by Process Name: CeWinFileCacheFS.exe
# Look for file access denials or failures
```

### 3. WinFsp Debug Output
```cmd
# Enable WinFsp debug logging (requires DebugView)
# Download DebugView from Microsoft Sysinternals
# Run DebugView as Administrator
# Run CeWinFileCacheFS with -d -1 to see WinFsp internal messages
```

### 4. Dependency Walker
```cmd
# Use Dependency Walker to check for missing DLLs
# Load CeWinFileCacheFS.exe in Dependency Walker
# Look for red entries (missing dependencies)
```

## Configuration Verification

### Check compilers.json
```json
// Ensure network paths are accessible:
{
  "global": {
    "cache_directory": "H:\\opt\\CompilerCache",  // Must be writable
    "total_cache_size_mb": 1024
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "\\\\server\\share\\path",  // Must be accessible
      "cache_size_mb": 512
    }
  }
}
```

### Test Network Paths
```cmd
# Verify network paths in config are accessible
dir "\\server\share\path"

# Test with UNC paths
net use \\server\share

# Check network connectivity
ping server
telnet server 445
```

## Temporary Workarounds

### 1. Use Test Mode
```cmd
# Skip WinFsp mounting entirely
CeWinFileCacheFS --test --config compilers.json -l debug
```

### 2. Use Different Cache Location
```cmd
# Try local cache directory
# Edit compilers.json:
"cache_directory": "C:\\temp\\CompilerCache"
```

### 3. Disable Metrics
```cmd
# Edit compilers.json to disable metrics:
"metrics": {
  "enabled": false
}
```

## Next Steps Based on Results

1. **If running as Administrator fixes it**: Permissions issue
2. **If different mount point works**: Drive letter conflict  
3. **If test mode works**: WinFsp-specific issue
4. **If nothing works**: WinFsp installation problem

## Getting More Help

If these steps don't resolve the issue, please provide:

1. **Output of enhanced logging**: Run with `-l trace -o both -f debug.log`
2. **Windows Event Viewer entries**: Around the time of failure
3. **System information**:
   ```cmd
   systeminfo
   wmic os get caption,version,buildnumber
   ```
4. **WinFsp status**:
   ```cmd
   sc query WinFsp.Launcher
   sc query WinFsp
   ```
5. **Process Monitor trace**: During the failure
6. **Network path accessibility**: Results of testing UNC paths in config