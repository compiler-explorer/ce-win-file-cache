#!/bin/bash
# Wrapper script to convert WSL paths to Windows paths for MSVC

# Debug: print all arguments
# echo "cl-wrapper.sh called with: $@" >&2

# MSVC cl.exe path
CL_PATH="/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/cl.exe"

# Convert arguments
ARGS=()
prev_arg=""
for arg in "$@"; do
    # echo "Processing arg: $arg" >&2
    # Handle -o output file (convert to /Fo for MSVC)
    if [[ "$arg" == "-o" ]]; then
        # Skip this arg, next arg will be the output file
        prev_arg="$arg"
        continue
    elif [[ "$prev_arg" == "-o" ]]; then
        # Convert output file path and add /Fo prefix
        if [[ "$arg" == /mnt/* ]]; then
            drive_letter=$(echo "$arg" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$arg" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
        elif [[ "$arg" == *\\* ]]; then
            # Handle Windows-style paths with backslashes
            win_path=$(echo "$arg" | sed 's|\\|/|g')
        else
            win_path="$arg"
        fi
        ARGS+=("/Fo$win_path")
        prev_arg=""
        continue
    # Handle /I include paths (MSVC style) - MUST come before general MSVC flag handling
    elif [[ "$arg" == /I/* ]]; then
        inc_path="${arg:2}"  # Remove /I prefix
        # echo "Processing /I path: $inc_path" >&2
        if [[ "$inc_path" == /mnt/[a-z]/* ]]; then
            # Handle /mnt/X/ paths (Windows drive mounts)
            drive_letter=$(echo "$inc_path" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$inc_path" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
            # echo "Converted to: /I$win_path" >&2
            ARGS+=("/I$win_path")
        else
            ARGS+=("$arg")
        fi
        prev_arg=""
    # Handle -I include paths
    elif [[ "$arg" == -I/* ]]; then
        inc_path="${arg:2}"  # Remove -I prefix
        if [[ "$inc_path" == /mnt/[a-z]/* ]]; then
            # Handle /mnt/X/ paths (Windows drive mounts)
            drive_letter=$(echo "$inc_path" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$inc_path" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
            ARGS+=("-I$win_path")
        else
            ARGS+=("$arg")
        fi
        prev_arg=""
    # Handle -external:I include paths  
    elif [[ "$arg" == -external:I/* ]]; then
        inc_path="${arg:11}"  # Remove -external:I prefix
        if [[ "$inc_path" == /mnt/[a-z]/* ]]; then
            # Handle /mnt/X/ paths (Windows drive mounts)
            drive_letter=$(echo "$inc_path" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$inc_path" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
            ARGS+=("-external:I$win_path")
        else
            ARGS+=("$arg")
        fi
        prev_arg=""
    # Skip MSVC-style flags (starts with / and is not a file path)
    elif [[ "$arg" == /* ]] && [[ ! -e "$arg" ]] && [[ ! "$arg" =~ ^/mnt/ ]]; then
        # This is likely an MSVC flag, keep as-is
        ARGS+=("$arg")
        prev_arg=""
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
        prev_arg=""
    else
        ARGS+=("$arg")
        prev_arg=""
    fi
done

# Debug: print converted arguments
# echo "Converted args: ${ARGS[@]}" >&2

# Execute cl.exe with converted arguments
exec "$CL_PATH" "${ARGS[@]}"