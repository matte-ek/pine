#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECTS_DIR="$SCRIPT_DIR/assets/projects"
EDITOR_BIN="$SCRIPT_DIR/Editor/Editor"

mkdir -p "$PROJECTS_DIR"

show_help() {
    echo "Pine Editor Manager"
    echo "Usage:"
    echo "  $0                         - Open the interactive menu"
    echo "  $0 <project_name>          - Directly launch a specific project"
    echo "  $0 launch <project_name>   - Explicitly launch a specific project"
    echo "  $0 create <project_name>   - Create a new project with assets & content folders"
    echo "  $0 list                    - List all existing projects"
    echo "  $0 -h | --help             - Show this help screen"
}

list_projects() {
    echo "=== Existing Projects ==="
    local count=0
    for dir in "$PROJECTS_DIR"/*/; do
        if [ -d "$dir" ]; then
            basename "$dir"
            ((count++))
        fi
    done
    
    if [ "$count" -eq 0 ]; then
        echo "(No projects found in $PROJECTS_DIR)"
    fi
}

create_project() {
    local name="$1"
    if [ -z "$name" ]; then
        echo "Error: Please provide a project name."
        exit 1
    fi

    local target_dir="$PROJECTS_DIR/$name"

    if [ -d "$target_dir" ]; then
        echo "Error: A project named '$name' already exists at $target_dir"
        exit 1
    fi

    echo "Creating project '$name'..."
    mkdir -p "$target_dir/assets"
    mkdir -p "$target_dir/content"
    
    echo "✓ Project '$name' initialized successfully."
    echo "  ↳ Location: $target_dir"
}

launch_project() {
    local name="$1"
    if [ -z "$name" ]; then
        echo "Error: No project specified to launch."
        exit 1
    fi

    local target_dir="$PROJECTS_DIR/$name"
    if [ ! -d "$target_dir" ]; then
        echo "Warning: Project folder '$name' does not exist at $target_dir"
        read -rp "Do you want to create it now? (y/N): " choice
        case "$choice" in
            [yY][eE][sS]|[yY]) create_project "$name" ;;
            *) echo "Aborting launch."; exit 1 ;;
        esac
    fi

    cd "$SCRIPT_DIR/assets" || {
        echo "Error: Could not find the assets directory at $SCRIPT_DIR/assets"
        exit 1
    }

    echo "Launching project '$name' in the Editor..."
    export PINE_X11=yes
    exec "$EDITOR_BIN" "$name"
}

interactive_menu() {
    while true; do
        echo ""
        echo "================================="
        echo "      PINE ENGINE MANAGER       "
        echo "================================="
        echo "1) Launch a Project"
        echo "2) Create a New Project"
        echo "3) List All Projects"
        echo "4) Exit"
        echo "---------------------------------"
        read -rp "Select an option [1-4]: " opt

        case "$opt" in
            1)
                echo ""
                list_projects
                echo ""
                read -rp "Enter the name of the project to launch: " p_name
                if [ -n "$p_name" ]; then
                    launch_project "$p_name"
                fi
                ;;
            2)
                read -rp "Enter a name for your new project: " p_name
                if [ -n "$p_name" ]; then
                    create_project "$p_name"
                fi
                ;;
            3)
                echo ""
                list_projects
                ;;
            4)
                echo "Goodbye!"
                exit 0
                ;;
            *)
                echo "Invalid option, try again."
                ;;
         esac
    done
}

case "$1" in
    -h|--help)
        show_help
        ;;
    list)
        list_projects
        ;;
    create)
        create_project "$2"
        ;;
    launch)
        launch_project "$2"
        ;;
    *)
        if [ -z "$1" ]; then
            interactive_menu
        else
            launch_project "$1"
        fi
        ;;
esac
