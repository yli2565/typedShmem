#!/bin/bash

# Enable error handling to exit script on error
set -e

# Define the target Python version and environment name
DEFAULT_PYTHON_VERSION="3.9"
ENV_NAME="build_env_$$"

# Detect the operating system
OS=$(uname -s)

# Check for Python version passed as an argument
cleanup() {
    echo "Cleaning up: Deactivating and removing the environment $ENV_NAME..."
    conda deactivate || true  # Deactivate the environment, ignore errors if already deactivated
    conda env remove --name $ENV_NAME -y || true  # Remove the environment, ignore errors if already removed
    echo "Environment $ENV_NAME removed."
}

trap cleanup SIGINT SIGTERM EXIT


# Function to activate conda environment cross-platform
activate_env() {
    if [[ "$OS" == "Linux" ]]; then
        eval "$(conda shell.bash hook)" 
        conda activate $ENV_NAME
    elif [[ "$OS" == "Darwin" ]]; then
        eval "$(conda shell.zsh hook)" 
        conda activate $ENV_NAME
    else
        echo "Unsupported OS: $OS"
        exit 1
    fi
}

validate_python_version() {
    local version=$1
    # Regex to match Python version format (e.g., 3.9, 3.10)
    if [[ $version =~ ^[0-9]+\.[0-9]+$ ]]; then
        return 0  # Valid version format
    else
        return 1  # Invalid version format
    fi
}

if [[ $# -eq 1 ]]; then
    CLI_PYTHON_VERSION=$1
    # Validate the provided Python version
    if validate_python_version $CLI_PYTHON_VERSION; then
        PYTHON_VERSION=$CLI_PYTHON_VERSION
        echo "Using specified Python version: $PYTHON_VERSION"
    else
        echo "Invalid Python version format provided. Using default version: $DEFAULT_PYTHON_VERSION"
        PYTHON_VERSION=$DEFAULT_PYTHON_VERSION
    fi
else
    # Set default Python version if no argument is provided
    PYTHON_VERSION=$DEFAULT_PYTHON_VERSION
    echo "No Python version specified. Using default version: $PYTHON_VERSION"
fi

# Step 1: Create a clean environment with the target Python version
echo "Creating a clean environment with Python $PYTHON_VERSION..."
conda create --name $ENV_NAME python=$PYTHON_VERSION -y

# Step 2: Activate the environment
echo "Activating the environment..."
activate_env

# Step 3: Install GCC toolchain on Linux/macOS; skip on Windows
if [[ "$OS" == "Linux" || "$OS" == "Darwin" ]]; then
    echo "Installing GCC and related toolchain from conda-forge..."
    conda install -c conda-forge gcc_linux-64 gxx_linux-64 libstdcxx-ng -y
fi

# Step 4: Install necessary build dependencies
echo "Installing necessary build dependencies..."
pip install --upgrade pip setuptools wheel pybind11 cmake scikit-build build

# Step 5: Run the build command
echo "Running the build command..."
python -m build

# Step 6: Deactivate the environment after building
echo "Deactivating the environment..."
conda deactivate

# Step 7: Delete the temporary build environment
echo "Deleting the temporary environment $ENV_NAME..."
conda env remove --name $ENV_NAME -y

echo "Build process completed and build environment deleted successfully!"
