#!/bin/bash
# Wrapper script to convert WSL paths to Windows paths for MSVC

# MSVC cl.exe path
CL_PATH="/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/cl.exe"

# Convert arguments
ARGS=()
for arg in "$@"; do
    # Skip MSVC-style flags (starts with / and is not a file path)
    if [[ "$arg" == /* ]] && [[ ! -e "$arg" ]] && [[ ! "$arg" =~ ^/mnt/ ]]; then
        # This is likely an MSVC flag, keep as-is
        ARGS+=("$arg")
    elif [[ "$arg" == /* ]]; then
        # This is a file path, convert it
        if [[ "$arg" == /mnt/[a-z]/* ]]; then
            # Handle /mnt/X/ paths (Windows drive mounts)
            drive_letter=$(echo "$arg" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$arg" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
            ARGS+=("$win_path")
        else
            # Convert other absolute paths using wslpath
            if command -v wslpath > /dev/null 2>&1; then
                win_path=$(wslpath -m "$arg" 2>/dev/null)
                if [[ $? -eq 0 ]]; then
                    ARGS+=("$win_path")
                else
                    # Fallback: keep original path
                    ARGS+=("$arg")
                fi
            else
                ARGS+=("$arg")
            fi
        fi
    else
        ARGS+=("$arg")
    fi
done

# Execute cl.exe with converted arguments
exec "$CL_PATH" "${ARGS[@]}"