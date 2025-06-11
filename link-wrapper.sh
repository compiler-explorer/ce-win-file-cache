#!/bin/bash
# Wrapper script to convert WSL paths to Windows paths for MSVC linker

# MSVC link.exe path
LINK_PATH="/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/link.exe"

# Convert arguments, filtering out compiler-specific flags
ARGS=()
OUTPUT_FILE=""
prev_arg=""
for arg in "$@"; do
    # Skip compiler-only flags
    if [[ "$arg" == "/EHsc" ]] || [[ "$arg" == "/MD" ]] || [[ "$arg" == "/std:c++"* ]] || [[ "$arg" == /I* ]]; then
        continue
    # Handle /LIBPATH: arguments
    elif [[ "$arg" == /LIBPATH:/* ]]; then
        libpath="${arg:9}"  # Remove /LIBPATH: prefix
        if [[ "$libpath" == /mnt/[a-z]/* ]]; then
            # Handle /mnt/X/ paths (Windows drive mounts)
            drive_letter=$(echo "$libpath" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$libpath" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
            ARGS+=("/LIBPATH:$win_path")
        else
            ARGS+=("$arg")
        fi
    # Handle -o output argument (convert to /OUT:)
    elif [[ "$arg" == "-o" ]]; then
        # Next argument will be the output file
        continue
    elif [[ "$OUTPUT_FILE" == "" ]] && [[ "$prev_arg" == "-o" ]]; then
        # Convert output file path and add /OUT: prefix
        if [[ "$arg" == /mnt/* ]]; then
            drive_letter=$(echo "$arg" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$arg" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
        else
            win_path="$arg"
        fi
        ARGS+=("/OUT:$win_path")
        OUTPUT_FILE="$win_path"
    # Handle -l library arguments (convert to .lib format)
    elif [[ "$arg" == -l* ]]; then
        libname="${arg:2}"  # Remove -l prefix
        ARGS+=("${libname}.lib")
    # Skip MSVC-style linker flags that are valid
    elif [[ "$arg" == /NOLOGO ]] || [[ "$arg" == /MANIFEST:NO ]]; then
        ARGS+=("$arg")
    # Handle .obj and .lib files (convert paths)
    elif [[ "$arg" == *.obj ]] || [[ "$arg" == *.lib ]]; then
        if [[ "$arg" == /mnt/* ]]; then
            # Handle /mnt/X/ paths
            drive_letter=$(echo "$arg" | cut -d'/' -f3 | tr '[:lower:]' '[:upper:]')
            rest_of_path=$(echo "$arg" | cut -d'/' -f4-)
            win_path="${drive_letter}:/${rest_of_path}"
        elif [[ "$arg" == *\\* ]]; then
            # Handle Windows-style paths with backslashes - convert to forward slashes first
            temp_path=$(echo "$arg" | sed 's|\\|/|g')
            if [[ "$temp_path" == [A-Za-z]:/* ]]; then
                # Already a Windows drive path, just fix slashes
                win_path=$(echo "$arg" | sed 's|\\|/|g')
            else
                # Relative path, convert to absolute Windows path
                win_path="D:/opt/ce-win-file-cache/build-msvc/$temp_path"
            fi
        else
            win_path="$arg"
        fi
        ARGS+=("$win_path")
    # Skip other compiler flags or unknown options
    else
        # Only add if it looks like a valid linker argument
        if [[ ! "$arg" == /* ]] || [[ "$arg" == /*.obj ]] || [[ "$arg" == /*.lib ]]; then
            ARGS+=("$arg")
        fi
    fi
    prev_arg="$arg"
done

# Execute link.exe with converted arguments
exec "$LINK_PATH" "${ARGS[@]}"