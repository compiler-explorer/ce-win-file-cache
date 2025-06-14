# CE Win File Cache - Architecture Overview

## What is CE Win File Cache?

**CE Win File Cache** is a high-performance caching filesystem for Windows that accelerates access to compiler toolchains and development tools stored on network shares. It's specifically designed for distributed development teams where build tools are centrally hosted but local access needs to be fast.

## Problem It Solves

```
❌ BEFORE: Slow Network Access
Developer's Machine          Network Share
┌─────────────────┐          ┌─────────────────┐
│   Build Process │◄────────►│ \\server\msvc\  │
│                 │   Slow   │   ├─ bin/       │
│ - Compiling...  │ Network  │   ├─ include/   │
│ - Linking...    │ Latency  │   └─ lib/       │
│ - Waiting...    │          │                 │
└─────────────────┘          └─────────────────┘

✅ AFTER: Fast Cached Access  
Developer's Machine                    Network Share
┌─────────────────┐    ┌─────────────┐ ┌─────────────────┐
│   Build Process │◄──►│ File Cache  │◄│ \\server\msvc\  │
│                 │Fast│ Filesystem  │ │   ├─ bin/       │
│ - Compiling ✓   │    │             │ │   ├─ include/   │
│ - Linking ✓     │    │ ├─ Memory   │ │   └─ lib/       │
│ - Done!         │    │ ├─ Disk     │ │                 │
└─────────────────┘    │ └─ Async    │ └─────────────────┘
                       └─────────────┘
```

## Architecture Diagram

```mermaid
graph TB
    %% User Applications
    subgraph "User Applications"
        VS[Visual Studio]
        CL[cl.exe<br/>MSVC Compiler]
        BUILD[Build Tools]
    end

    %% Virtual Filesystem
    subgraph "Virtual Filesystem Layer"
        VFS[Virtual Drive<br/>Z:\msvc\]
        WINFSP[WinFsp Kernel Driver]
    end

    %% CE Win File Cache Main Components
    subgraph "CE Win File Cache"
        direction TB
        
        %% Filesystem Interface
        subgraph "Filesystem Interface"
            HFS[HybridFileSystem<br/>WinFsp Integration]
        end
        
        %% Cache Layer
        subgraph "Cache Layer"
            direction LR
            MCM[Memory Cache<br/>Manager]
            DC[Directory Cache<br/>Always Resident]
            ADM[Async Download<br/>Manager]
        end
        
        %% Network Layer  
        subgraph "Network Layer"
            direction LR
            NC[Network Client<br/>UNC Access]
            PR[Path Resolver<br/>Virtual→Network]
            GM[Glob Matcher<br/>Cache Patterns]
        end
        
        %% Configuration & Monitoring
        subgraph "Config & Monitoring"
            direction LR
            CP[Config Parser<br/>JSON]
            METRICS[Metrics Collector<br/>Prometheus]
            UTILS[String Utils]
        end
    end

    %% Network Storage
    subgraph "Network Storage"
        NS[Network Share<br/>\\server\compilers\]
        MSVC[msvc-14.40\<br/>├─ bin\cl.exe<br/>├─ include\<br/>└─ lib\]
        KITS[windows-kits\<br/>├─ Include\<br/>└─ Lib\]
        NINJA[ninja\ninja.exe]
    end

    %% Data Flow Connections
    VS --> VFS
    CL --> VFS  
    BUILD --> VFS
    VFS --> WINFSP
    WINFSP --> HFS

    HFS --> MCM
    HFS --> DC
    HFS --> ADM
    
    MCM --> NC
    DC --> NC
    ADM --> NC
    
    NC --> PR
    PR --> GM
    GM --> UTILS
    
    CP --> MCM
    CP --> ADM
    METRICS --> MCM
    METRICS --> ADM
    
    NC --> NS
    NS --> MSVC
    NS --> KITS  
    NS --> NINJA

    %% Styling
    classDef userApp fill:#e1f5fe
    classDef vfs fill:#f3e5f5
    classDef cache fill:#e8f5e8
    classDef network fill:#fff3e0
    classDef storage fill:#fce4ec
    classDef config fill:#f1f8e9

    class VS,CL,BUILD userApp
    class VFS,WINFSP vfs
    class MCM,DC,ADM,HFS cache
    class NC,PR,GM network
    class NS,MSVC,KITS,NINJA storage
    class CP,METRICS,UTILS config
```

### Data Flow Diagram

```mermaid
sequenceDiagram
    participant App as Application<br/>(cl.exe)
    participant VFS as Virtual FS<br/>(Z:\msvc\)
    participant HFS as HybridFileSystem
    participant MC as Memory Cache
    participant DC as Directory Cache
    participant ADM as Async Download
    participant NC as Network Client
    participant NS as Network Share

    App->>VFS: Open Z:\msvc\bin\cl.exe
    VFS->>HFS: File Open Request
    
    alt File in Memory Cache
        HFS->>MC: Check cache
        MC-->>HFS: Return cached file
        HFS-->>VFS: Fast response
    else File not cached
        HFS->>DC: Get directory info
        DC-->>HFS: Directory structure
        HFS->>NC: Check network file
        NC->>NS: Access \\server\msvc\bin\cl.exe
        NS-->>NC: File metadata
        NC-->>HFS: File available
        HFS->>ADM: Queue download
        ADM->>NC: Background download
        NC->>NS: Fetch file content
        NS-->>NC: File data
        NC-->>ADM: Download complete
        ADM->>MC: Store in cache
        MC-->>HFS: File ready
        HFS-->>VFS: Response (first time)
    end
    
    VFS-->>App: File handle ready
    
    Note over App,NS: Subsequent access to same file<br/>serves from memory cache
```

### Cache Policy Flow

```mermaid
flowchart TD
    START([File Access Request])
    
    CHECK_MEMORY{In Memory<br/>Cache?}
    CHECK_PATTERN{Matches Cache<br/>Pattern?}
    CHECK_SIZE{Within Size<br/>Limits?}
    
    SERVE_MEMORY[Serve from Memory<br/>⚡ Fast]
    SERVE_NETWORK[Serve from Network<br/>🌐 Slower]
    QUEUE_DOWNLOAD[Queue Background<br/>Download]
    STORE_CACHE[Store in Cache]
    EVICT_LRU[Evict LRU Items]
    
    START --> CHECK_MEMORY
    CHECK_MEMORY -->|Yes| SERVE_MEMORY
    CHECK_MEMORY -->|No| CHECK_PATTERN
    
    CHECK_PATTERN -->|No *.pdb, *.tmp| SERVE_NETWORK
    CHECK_PATTERN -->|Yes *.exe, *.dll| CHECK_SIZE
    
    CHECK_SIZE -->|Cache Full| EVICT_LRU
    CHECK_SIZE -->|Space Available| QUEUE_DOWNLOAD
    EVICT_LRU --> QUEUE_DOWNLOAD
    
    QUEUE_DOWNLOAD --> SERVE_NETWORK
    QUEUE_DOWNLOAD --> STORE_CACHE
    STORE_CACHE --> SERVE_MEMORY
    
    SERVE_MEMORY --> END([Response Ready])
    SERVE_NETWORK --> END
    
    classDef fast fill:#c8e6c9
    classDef slow fill:#ffcdd2
    classDef process fill:#e1f5fe
    
    class SERVE_MEMORY fast
    class SERVE_NETWORK slow
    class QUEUE_DOWNLOAD,STORE_CACHE,EVICT_LRU,CHECK_MEMORY,CHECK_PATTERN,CHECK_SIZE process
```

## Key Features

### 🚀 **Multi-Layer Caching**
- **Memory Cache**: Ultra-fast access for frequently used files
- **Directory Cache**: Always-cached directory structures for instant browsing
- **Disk Cache**: Persistent storage with LRU eviction

### ⚡ **Async Operations**
- Background downloads don't block file access
- Thread pool for concurrent network operations
- Non-blocking cache warming

### 🎯 **Smart Cache Policies**
- **Pattern-based**: Cache `*.exe`, `*.dll` but not `*.pdb`
- **Size-aware**: Different limits per compiler
- **Access-based**: LRU eviction keeps hot files in memory

### 📊 **Monitoring & Observability**
- Prometheus metrics for cache performance
- Hit/miss ratios, download speeds, queue depths
- Health monitoring for distributed teams

## Configuration Example

```json
{
  "global": {
    "total_cache_size_mb": 4096,
    "eviction_policy": "lru",
    "cache_directory": "./cache",
    "download_threads": 8
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "\\\\build-server\\msvc\\14.40",
      "cache_size_mb": 2048,
      "cache_always": ["*.exe", "*.dll", "*.lib"],
      "cache_never": ["*.pdb", "*.ilk"],
      "prefetch_patterns": ["bin/**/*.exe", "include/**/*.h"]
    },
    "windows-kits-10": {
      "network_path": "\\\\build-server\\kits\\10",
      "cache_size_mb": 1024,
      "cache_always": ["*.lib", "*.h"],
      "prefetch_patterns": ["Include/**/*.h", "Lib/**/*.lib"]
    }
  }
}
```

## Use Cases

### 🏢 **Enterprise Development**
- Central build servers with compiler toolchains
- Remote developers with slow network connections
- CI/CD pipelines needing fast tool access

### 🌐 **Distributed Teams**
- Geographically distributed development teams
- Consistent toolchain versions across locations
- Reduced bandwidth usage through intelligent caching

### 🔄 **Build Performance**
- Faster compilation times
- Reduced network latency for file access
- Improved developer productivity

## Performance Benefits

The caching system provides significant performance improvements by:

- **Memory cache hits**: Serve frequently accessed files directly from RAM
- **Directory structure caching**: Eliminate network round-trips for directory listings
- **Background downloads**: Async fetching doesn't block file access
- **Pattern-based caching**: Focus cache space on high-impact files (executables, libraries)
- **LRU eviction**: Keep most-used files readily available

## Technical Highlights

- **WinFsp Integration**: Kernel-level filesystem driver for transparency
- **Cross-Platform Testing**: Linux CI for core logic, Windows for full stack
- **Modern C++20**: Clean, maintainable codebase with strong typing
- **Comprehensive Testing**: Unit tests, integration tests, performance benchmarks
- **Production Ready**: Metrics, logging, error handling, graceful degradation

---

*CE Win File Cache transforms slow network-based development workflows into fast, responsive local experiences while maintaining the benefits of centralized tool management.*