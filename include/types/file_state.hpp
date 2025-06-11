#pragma once

namespace CeWinFileCache
{

enum class FileState
{
    VIRTUAL, // File exists in metadata/index only
    CACHED, // File is stored locally
    PLACEHOLDER, // File metadata exists, content fetched on first access
    FETCHING, // Currently downloading from network
    NETWORK_ONLY // Always fetch from network (no local caching)
};

enum class CachePolicy
{
    ALWAYS_CACHE, // Always cache locally
    ON_DEMAND, // Cache only after first access
    NEVER_CACHE // Never cache, always fetch from network
};

} // namespace CeWinFileCache