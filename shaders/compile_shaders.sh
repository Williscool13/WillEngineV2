#!/bin/bash
# Store the root include path as a global variable
ROOT_INCLUDE_PATH="$PWD/include"

compile_shader() {
    local file="$1"
    local relative_path="${file#"$PWD"/}"  # Get relative path from current directory
    
    # Compile to /dev/null on Unix-like systems or NUL on Windows
    if glslc -I"$ROOT_INCLUDE_PATH" "$file" -o /dev/null 2>/dev/null; then
        echo "✓ Successfully compiled: $relative_path"
    else
        echo "✗ Failed to compile: $relative_path"
        # Compile again to show error messages
        glslc -I"$ROOT_INCLUDE_PATH" "$file" -o /dev/null
    fi
}

find_shaders() {
    local dir="$1"
    shopt -s nullglob
    
    # Find and compile all shader types
    for ext in comp vert frag tese tesc; do
        for file in "$dir"/*.$ext; do
            if [ -f "$file" ]; then
                compile_shader "$file"
            fi
        done
    done
    
    # Recursively process subdirectories
    for subdir in "$dir"/*/; do
        find_shaders "$subdir"
    done
}

echo "Checking shader compilation..."
find_shaders "$PWD"
echo "Shader check complete."

# Wait for user input before exiting
read -n 1 -s -r -p "Press any key to continue..."