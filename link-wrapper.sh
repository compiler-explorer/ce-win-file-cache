#!/bin/bash
# Wrapper script to convert WSL paths to Windows paths for MSVC linker

# MSVC link.exe path
LINK_PATH="/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/link.exe"

# Convert arguments
ARGS=()
for arg in "$@"; do
    # Skip MSVC-style flags (starts with / and is not a file path)
    if [[ "$arg" == /* ]] && [[ ! -e "$arg" ]] && [[ ! "$arg" =~ ^/mnt/ ]]; then
        # This is likely an MSVC flag, keep as-is
        ARGS+=("$arg")
    elif [[ "$arg" == /* ]]; then
        # This is a file path, convert it
        if [[ "$arg" == /mnt/* ]]; then
            # Handle /mnt/X/ paths
            drive_letter=$(echo "$arg" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$arg" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
        else
            # Convert other absolute paths using wslpath if available
            if command -v wslpath > /dev/null 2>&1; then
                win_path=$(wslpath -m "$arg" 2>/dev/null || echo "$arg")
            else
                win_path="$arg"
            fi
        fi
        ARGS+=("$win_path")
    else
        ARGS+=("$arg")
    fi
done

# Execute link.exe with converted arguments
exec "$LINK_PATH" "${ARGS[@]}"