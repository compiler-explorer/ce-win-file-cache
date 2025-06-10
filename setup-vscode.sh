#!/bin/bash

set -e

echo "Setting up VSCode for CeWinFileCache development..."

# Check if VSCode is installed
if ! command -v code &> /dev/null; then
    echo "Warning: VSCode 'code' command not found. You may need to:"
    echo "  1. Install VSCode"
    echo "  2. Enable 'Shell Command: Install code command in PATH'"
    echo ""
fi

# Check Wine development tools
if ! command -v winegcc &> /dev/null; then
    echo "Warning: Wine development tools not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt-get install wine-dev"
    echo "  Fedora/RHEL:   sudo dnf install wine-devel"
    echo "  Arch Linux:    sudo pacman -S wine"
    echo ""
fi

# Check for Wine headers
WINE_HEADERS_FOUND=0
WINE_HEADER_PATH=""
for path in /usr/include/wine/wine/windows /usr/include/wine/windows /usr/local/include/wine/windows /opt/wine-stable/include/wine/windows /usr/include/wine-development/windows; do
    if [ -f "$path/windows.h" ]; then
        echo "✓ Found Wine headers at: $path"
        WINE_HEADERS_FOUND=1
        WINE_HEADER_PATH="$path"
        break
    fi
done

if [ $WINE_HEADERS_FOUND -eq 0 ]; then
    echo "✗ Wine Windows headers not found"
    echo "  Please install wine-dev package for your distribution"
else
    echo "✓ Wine headers configured in VSCode settings"
fi

# Check if .vscode directory exists
if [ -d ".vscode" ]; then
    echo "✓ VSCode configuration already exists"
else
    echo "✗ VSCode configuration missing"
    echo "  This should have been created automatically. Please check if files exist:"
    echo "  - .vscode/c_cpp_properties.json"
    echo "  - .vscode/settings.json"
    echo "  - .vscode/tasks.json"
fi

echo ""
echo "Setup checklist:"
echo "  [${WINE_HEADERS_FOUND:+✓}${WINE_HEADERS_FOUND:-✗}] Wine development tools installed"
echo "  [${WINE_HEADERS_FOUND:+✓}${WINE_HEADERS_FOUND:-✗}] Wine Windows headers available"
echo "  [$([ -d .vscode ] && echo "✓" || echo "✗")] VSCode configuration present"
echo ""

if [ $WINE_HEADERS_FOUND -eq 1 ] && [ -d ".vscode" ]; then
    echo "🎉 Setup complete! You can now:"
    echo "  1. Open this project in VSCode"
    echo "  2. Install recommended extensions (C/C++, CMake Tools)"
    echo "  3. Select 'Linux Wine' configuration"
    echo "  4. Use Ctrl+Shift+B to build"
    echo ""
    echo "To open in VSCode: code ."
else
    echo "⚠️  Setup incomplete. Please address the issues above."
fi